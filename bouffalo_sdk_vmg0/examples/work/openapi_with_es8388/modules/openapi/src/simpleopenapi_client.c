/**
 * @file simpleopenapi_client.c
 * @brief Client implementation for interacting with the SimpleOpenAPI.
 *
 * Handles formatting requests, sending them via the 'requests' module,
 * and parsing the JSON response to extract the assistant's message.
 */

#include "FreeRTOS.h"
#include "task.h"
#include "requests.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"
#include "log.h"
#include "mem.h"
#include "cJSON.h"
#include "simpleopenapi_client.h"

// --- Configuration Constants ---
#define SIMPLE_OPENAPI_URL                  "http://140.143.223.43:8000/chat" // SimpleOpenAPI endpoint
#define SIMPLE_OPENAPI_KEY                  "testkey1"                        // API Key

// Request body JSON format template (uses %s for model and user content)
#define SIMPLE_OPENAPI_HEADER_KEY           "X-API-Key: testkey1\r\n"
#define FIXED_SESSION_ID                    "testsession1" // 新增：定义固定的 Session ID
#define SIMPLE_OPENAPI_RESPONSE_BUFFER_SIZE 4096           // Size of the buffer for the response body
#define SIMPLE_OPENAPI_TASK_STACK_SIZE      8192           // Stack size for the background task (HTTPS needs more)
#define SIMPLE_OPENAPI_TASK_PRIORITY        10             // Priority of the background task
#define SIMPLE_OPENAPI_REQUEST_TIMEOUT_MS   25000          // Request timeout in milliseconds
#define SIMPLE_OPENAPI_MAX_HEADER_LEN       128            // Buffer size for HTTP headers

// --- Internal Task Data Structure ---
typedef struct {
    char *user_message; // Pointer to dynamically allocated user message
} simple_openapi_task_params_t;

// --- WiFi State Check Function (Declaration) ---
extern int check_wifi_state(void);

// --- 对外接口函数 ---
int simple_openapi_chat_async(const char *user_message)
{
    // 1. Basic input validation
    if (!user_message || strlen(user_message) == 0) {
        LOG_E("simple_openapi_chat_async: user_message is NULL or empty.\r\n");
        return REQ_ERR_BAD_REQUEST; // Return bad request error code
    }

    // 2. Allocate memory for task parameters structure
    simple_openapi_task_params_t *params = malloc(sizeof(simple_openapi_task_params_t));
    if (!params) {
        LOG_E("simple_openapi_chat_async: Failed to allocate memory for task params.\r\n");
        return REQ_ERR_MEM; // Return memory error code
    }

    // 3. Duplicate the user message string (caller's buffer might be temporary)
    params->user_message = strdup(user_message); // Use strdup for safe copy
    if (!params->user_message) {
        LOG_E("simple_openapi_chat_async: Failed to duplicate user_message.\r\n");
        free(params);       // Free the partially allocated params struct
        return REQ_ERR_MEM; // Return memory error code
    }

    // 4. Create the background FreeRTOS task
    BaseType_t task_ret = xTaskCreate(
        simple_openapi_chat_task,       // Pointer to the task function
        "simple_openapi_task",          // Name of the task (for debugging)
        SIMPLE_OPENAPI_TASK_STACK_SIZE, // Stack size allocated for the task
        params,                         // Pointer to the task parameters (ownership transferred)
        SIMPLE_OPENAPI_TASK_PRIORITY,   // Task priority
        NULL                            // No task handle needed here
    );

    // 5. Check if the task was created successfully
    if (task_ret != pdPASS) {
        // Task creation failed
        LOG_E("simple_openapi_chat_async: Failed to create task.\r\n");
        // Must free the allocated memory as the task won't run to do it
        free(params->user_message); // Free the duplicated message
        free(params);               // Free the parameter struct
        return -1;                  // Return a generic error code for task creation failure
    }

    // Task created successfully
    LOG_I("simple_openapi_chat_async: Task created for message: \"%s\"\r\n", user_message);
    return 0; // Return 0 indicating success
}

// --- 任务函数 ---
static void simple_openapi_chat_task(void *pvParameters)
{
    simple_openapi_task_params_t *params = (simple_openapi_task_params_t *)pvParameters;
    char *request_payload = NULL;    // Buffer for JSON request body
    uint8_t *response_buffer = NULL; // Buffer for HTTP response body
    char *header = NULL;             // Buffer for Authorization header
    int final_status = REQ_ERR_INTERNAL;
    cJSON *json_req_root = NULL;    // cJSON object for the request
    cJSON *json_resp_root = NULL;   // cJSON object for the response
    char *assistant_content = NULL; // Pointer to assistant's message in response JSON
    uint64_t ai_start_us = bflb_mtimer_get_time_us();

    // --- Parameter Validation ---
    if (!params || !params->user_message) {
        LOG_E("SimpleOpenAPI Task: Invalid parameters.\r\n");
        if (params) {
            if (params->user_message)
                free(params->user_message);
            free(params);
        }
        vTaskDelete(NULL);
        return;
    }
    LOG_I("SimpleOpenAPI Task: Started for message: \"%s\"\r\n", params->user_message);

    // --- WiFi Check ---
    if (check_wifi_state() != 0) {
        LOG_E("SimpleOpenAPI Task: WiFi not connected!\r\n");
        final_status = REQ_ERR_CONNECT;
        goto cleanup;
    }

    // --- Memory Allocation ---
    LOG_I("SimpleOpenAPI Task: Free heap before response_buffer alloc: %d bytes\r\n", kfree_size());
    response_buffer = malloc(SIMPLE_OPENAPI_RESPONSE_BUFFER_SIZE);
    LOG_I("SimpleOpenAPI Task: Free heap after response_buffer alloc (ptr: %p): %d bytes\r\n", response_buffer, kfree_size());
    header = malloc(SIMPLE_OPENAPI_MAX_HEADER_LEN);
    LOG_I("SimpleOpenAPI Task: Free heap after header alloc (ptr: %p): %d bytes\r\n", header, kfree_size());

    if (!response_buffer || !header) {
        LOG_E("SimpleOpenAPI Task: Memory alloc failed.\r\n");
        final_status = REQ_ERR_MEM;
        goto cleanup;
    }
    response_buffer[0] = '\0';
    header[0] = '\0';
    snprintf(header, SIMPLE_OPENAPI_MAX_HEADER_LEN, SIMPLE_OPENAPI_HEADER_KEY);

    // 构造 JSON 请求体
    json_req_root = cJSON_CreateObject();
    if (!json_req_root) {
        LOG_E("SimpleOpenAPI Task: Failed to create cJSON object.\r\n");
        final_status = REQ_ERR_MEM;
        goto cleanup;
    }
    cJSON_AddStringToObject(json_req_root, "user_message", params->user_message);
    cJSON_AddStringToObject(json_req_root, "session_id", FIXED_SESSION_ID);
    request_payload = cJSON_PrintUnformatted(json_req_root);
    if (!request_payload) {
        LOG_E("SimpleOpenAPI Task: Failed to print JSON.\r\n");
        final_status = REQ_ERR_MEM;
        goto cleanup;
    }
    int payload_len = strlen(request_payload);
    LOG_D("SimpleOpenAPI Task: JSON Payload created (%d bytes):\r\n%s\r\n", payload_len, request_payload);

    // 组装请求参数
    RequestOptions opts = {
        .method = "POST",
        .url = SIMPLE_OPENAPI_URL,
        .custom_headers = header,
        .content_type = "application/json",
        .request_body = request_payload,
        .body_len = payload_len,
        .timeout_ms = SIMPLE_OPENAPI_REQUEST_TIMEOUT_MS
    };
    Response resp = {
        .response_buf = response_buffer,
        .response_buf_len = SIMPLE_OPENAPI_RESPONSE_BUFFER_SIZE,
        .status_code = 0,
        .actual_resp_len = 0
    };
    response_buffer[0] = '\0';

    // --- Send Request ---
    LOG_I("SimpleOpenAPI Task: Sending request to %s...\r\n", SIMPLE_OPENAPI_URL);
    final_status = send_request(&opts, &resp);
    LOG_I("SimpleOpenAPI Task: Request finished.\r\n");
    // 记录完整AI响应结束时间并输出
    uint64_t ai_end_us = bflb_mtimer_get_time_us();
    printf("[CHAT] AI response total elapsed time: %llu ms\r\n", (ai_end_us - ai_start_us) / 1000ULL);

    // --- Process the Response ---
    printf("\r\n========= SimpleOpenAPI Response =========\r\n");
    printf("Last Message Sent: \"%s\"\r\n", params->user_message);
    printf("HTTP Status Code: %d\r\n", resp.status_code);

    if (resp.status_code >= 200 && resp.status_code < 300 && resp.actual_resp_len > 0) {
        // --- JSON Parsing Logic ---
        json_resp_root = cJSON_ParseWithLength((const char *)resp.response_buf, resp.actual_resp_len);
        if (!json_resp_root) {
            printf("Could not parse JSON response.\r\n");
        } else {
            const cJSON *ai_response = cJSON_GetObjectItemCaseSensitive(json_resp_root, "ai_response");
            if (cJSON_IsString(ai_response)) {
                assistant_content = ai_response->valuestring;
            }
            // Print the reply
            printf("AI Response:\r\n");
            printf("-----------------------------------------\r\n");
            if (assistant_content) {
                printf("%s", assistant_content);
                if (strlen(assistant_content) > 0 && assistant_content[strlen(assistant_content) - 1] != '\n') {
                    printf("\r\n");
                }
            } else {
                printf("(No AI response found)\r\n");
            }
            printf("-----------------------------------------\r\n");

            // Clean up the response cJSON object
            cJSON_Delete(json_resp_root);
            json_resp_root = NULL;
            LOG_D("Response cJSON object deleted successfully.\r\n");
        }
    } else if (resp.actual_resp_len > 0) {
        // Handle HTTP errors with body
        LOG_E("Received HTTP error status %d.\r\n", resp.status_code);
        printf("Actual Response Length: %d bytes\r\n", resp.actual_resp_len);
        printf("Error Response Body (Hex Dump):\r\n<<<\r\n");
        if (resp.response_buf && resp.actual_resp_len > 0) {
            for (size_t i = 0; i < resp.actual_resp_len; ++i) {
                printf("%02X ", resp.response_buf[i]);
                if ((i + 1) % 16 == 0)
                    printf("\r\n");
            }
            printf("\r\n");
        } else
            printf("(Buffer empty or length zero)\r\n");
        printf(">>>\r\n");
        printf("-----------------------------------------\r\n");
        json_resp_root = cJSON_ParseWithLength((const char *)resp.response_buf, resp.actual_resp_len);
        if (json_resp_root) {
            char *error_json_string = cJSON_Print(json_resp_root);
            if (error_json_string) {
                printf("(Parsed Error Body):\r\n%s\r\n", error_json_string);
                free(error_json_string);
            } else
                printf("(Could not print parsed error JSON)\r\n");
            cJSON_Delete(json_resp_root);
            json_resp_root = NULL;
        } else
            printf("(Error body is not valid JSON)\r\n");
        printf("-----------------------------------------\r\n");

    } else {
        // Handle no response body
        printf("Response Body: (empty or N/A for status %d)\r\n", resp.status_code);
    }

    // --- Print Client Errors ---
    if (final_status < 0 && final_status != resp.status_code) {
        printf("Client Error Code: %d\r\n", final_status);
    }
    printf("=========================================\r\n");

// --- Cleanup Label ---
cleanup:
    LOG_D("SimpleOpenAPI Task: Cleaning up resources...\r\n");
    // Free dynamically allocated memory
    if (request_payload)
        free(request_payload);
    if (response_buffer)
        free(response_buffer);
    if (header)
        free(header);
    if (json_req_root)
        cJSON_Delete(json_req_root);
    if (json_resp_root)
        cJSON_Delete(json_resp_root);
    // Free task parameters
    if (params) {
        if (params->user_message)
            free(params->user_message);
        free(params);
    }

    LOG_I("SimpleOpenAPI Task Finished.\r\n");
    vTaskDelete(NULL); // Task deletes itself
}