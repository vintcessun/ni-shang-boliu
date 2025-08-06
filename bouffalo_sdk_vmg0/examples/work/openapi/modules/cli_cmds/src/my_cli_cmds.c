// my_cli_cmds.c - Shell 命令实现与网络请求测试
#include "requests.h"
#include "my_cli_cmds.h"
#include "log.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>
#include <stdlib.h>
#include "semphr.h"
#include "https.h"
#include "wifi_mgmr_ext.h"
#include "http_client.h"
#include "https_client.h"
#include "simpleopenapi_client.h"
#include "url_parser.h"
#ifdef CONFIG_SHELL
#include "shell.h"
#endif

#define TEST_RESP_BUF_SIZE      4096
#define TASK_STACK_SIZE_NET     8192
#define CURL_MAX_HEADERS_LEN    512
#define CURL_DEFAULT_TIMEOUT_MS 15000

extern volatile uint32_t wifi_state;

int check_wifi_state(void)
{
    return (wifi_state == 1) ? 0 : 1;
}

// 固定参数网络请求任务
typedef struct {
    const char *method;
    const char *url;
    const char *content_type;
    const char *body;
    int timeout_ms;
} fixed_request_args_t;
static void request_test_task(void *pvParameters);
static void http_get_test_task(void *pvParameters);
static void https_get_test_task(void *pvParameters);
static void http_post_test_task(void *pvParameters);
static void https_post_test_task(void *pvParameters);

static void request_test_task(void *pvParameters)
{
    fixed_request_args_t *args = (fixed_request_args_t *)pvParameters;
    uint8_t *resp_buf_ptr = NULL;
    int final_status = REQ_ERR_INTERNAL;

    if (check_wifi_state() != 0) {
        LOG_E("%s %s Task Error: WiFi not connected!\r\n", args->method, args->url);
        goto task_exit_fixed;
    }

    LOG_I("%s %s Test Task Started (using send_request)...\r\n", args->method, args->url);
    resp_buf_ptr = malloc(TEST_RESP_BUF_SIZE);
    if (!resp_buf_ptr) {
        LOG_E("Failed to allocate response buffer (%d bytes)\r\n", TEST_RESP_BUF_SIZE);
        goto task_exit_fixed;
    }
    LOG_D("Response buffer allocated (%d bytes) at %p\r\n", TEST_RESP_BUF_SIZE, resp_buf_ptr);
    resp_buf_ptr[0] = '\0';

    RequestOptions opts = {
        .method = args->method,
        .url = args->url,
        .timeout_ms = args->timeout_ms,
        .content_type = args->content_type,
        .request_body = args->body,
        .body_len = args->body ? strlen(args->body) : 0,
        .custom_headers = NULL
    };
    Response resp = {
        .response_buf = resp_buf_ptr,
        .response_buf_len = TEST_RESP_BUF_SIZE,
        .status_code = 0,
        .actual_resp_len = 0
    };

    final_status = send_request(&opts, &resp);

    LOG_I("send_request returned: %d\r\n", final_status);
    LOG_I("Response status_code: %d, Actual length: %d\r\n", resp.status_code, resp.actual_resp_len);

    if (resp.status_code >= 200 && resp.status_code < 300) {
        LOG_I("%s %s Success! Status Code: %d\r\n", args->method, args->url, resp.status_code);
        if (resp.actual_resp_len > 0) {
            printf("--- Start Response Body (%d bytes, newlines replaced with space) ---\r\n", resp.actual_resp_len);
            for (int i = 0; i < resp.actual_resp_len; i++) {
                char current_char = resp.response_buf[i];
                if (current_char != '\n' && current_char != '\r') {
                    printf("%c", current_char);
                } else {
                    printf(" ");
                }
            }
            printf("\r\n--- End Response Body ---\r\n");
        } else {
            LOG_I("Response body is empty.\r\n");
        }
    } else if (resp.status_code > 0) {
        LOG_E("%s %s Failed! HTTP Status Code: %d\r\n", args->method, args->url, resp.status_code);
        if (resp.actual_resp_len > 0) {
            printf("--- Start Error Body (%d bytes, newlines replaced with space) ---\r\n", resp.actual_resp_len);
            for (int i = 0; i < resp.actual_resp_len; i++) {
                char current_char = resp.response_buf[i];
                if (current_char != '\n' && current_char != '\r') {
                    printf("%c", current_char);
                } else {
                    printf(" ");
                }
            }
            printf("\r\n--- End Error Body ---\r\n");
        }
    } else {
        LOG_E("%s %s Client Failed! Error code: %d (%s)\r\n", args->method, args->url, resp.status_code,
              resp.status_code == REQ_ERR_TIMEOUT        ? "Timeout" :
              resp.status_code == REQ_ERR_CONNECT        ? "Connection/TLS Failed" :
              resp.status_code == REQ_ERR_DNS            ? "DNS Failed" :
              resp.status_code == REQ_ERR_BAD_REQUEST    ? "Bad Request/URL Parse Failed" :
              resp.status_code == REQ_ERR_MEM            ? "Memory Allocation Failed" :
              resp.status_code == REQ_ERR_RESP_TOO_LARGE ? "Response Too Large" :
              resp.status_code == REQ_ERR_RESP_PARSE     ? "Response Parse Failed" :
                                                           "Other Client Error");
    }

task_exit_fixed:
    if (resp_buf_ptr) {
        LOG_D("Freeing fixed request response buffer at %p\r\n", resp_buf_ptr);
        free(resp_buf_ptr);
    }
    LOG_I("%s %s Test Task Finished.\r\n", args->method, args->url);
    vTaskDelete(NULL);
}

static void http_get_test_task(void *pvParameters)
{
    static fixed_request_args_t args = { "GET", "http://httpbin.org/get?source=bl618_send_request_http", NULL, NULL, 15000 };
    request_test_task(&args);
}

static void https_get_test_task(void *pvParameters)
{
    static fixed_request_args_t args = { "GET", "https://httpbin.org/get", NULL, NULL, 20000 };
    request_test_task(&args);
}

static void http_post_test_task(void *pvParameters)
{
    static fixed_request_args_t args = { "POST", "http://httpbin.org/post", "application/json", "{\"chip\":\"BL618_send_req_http\", \"message\":\"Hello again!\"}", 15000 };
    request_test_task(&args);
}

static void https_post_test_task(void *pvParameters)
{
    static fixed_request_args_t args = { "POST", "https://httpbin.org/post", "application/json", "{\"source\":\"BL618_send_request\", \"secure\":true}", 20000 };
    request_test_task(&args);
}

#ifdef CONFIG_SHELL

// Shell 命令：打印问候语
enum { GREET_OK = 0,
       GREET_ARGERR = -1 };
static int cmd_greet(int argc, char **argv)
{
    if (argc == 1) {
        LOG_I("Hello from your companion robot!\r\n");
    } else if (argc == 2) {
        LOG_I("Hello %s, nice to meet you!\r\n", argv[1]);
    } else {
        LOG_E("Usage: greet [name]\r\n");
        return GREET_ARGERR;
    }
    return GREET_OK;
}

// Shell 命令：机器人动作模拟
static int cmd_robot_action(int argc, char **argv)
{
    if (argc < 2) {
        LOG_E("Usage: action <move|turn|dance> [value]\r\n");
        return -1;
    }
    const char *action_type = argv[1];
    if (strcmp(action_type, "move") == 0 && argc == 3) {
        int steps = atoi(argv[2]);
        LOG_I("Executing move forward: %d steps (simulated)\r\n", steps);
    } else if (strcmp(action_type, "turn") == 0 && argc == 3) {
        int angle = atoi(argv[2]);
        LOG_I("Executing turn left: %d degrees (simulated)\r\n", angle);
    } else if (strcmp(action_type, "dance") == 0 && argc == 3) {
        int pattern = atoi(argv[2]);
        LOG_I("Executing dance pattern: %d (simulated)\r\n", pattern);
    } else {
        LOG_E("Unknown or invalid action.\r\n");
        return -1;
    }
    return 0;
}

// Shell 命令：测试 URL 解析
static int cmd_test_url_parse(int argc, char **argv)
{
    if (argc != 2) {
        LOG_E("Usage: test_parseurl <url_string>\r\n");
        return -1;
    }
    parsed_url_t parsed_result;
    int ret = parse_url(argv[1], &parsed_result);
    if (ret == URL_PARSE_OK) {
        LOG_I("URL OK: scheme=%s, host=%s, port=%u, path=%s\r\n", parsed_result.scheme, parsed_result.host, parsed_result.port, parsed_result.path);
    } else {
        LOG_E("URL parse error: %d\r\n", ret);
        return -1;
    }
    return 0;
}

// Shell 命令：测试中文输出
static int cmd_chinese_test(int argc, char **argv)
{
    LOG_I("测试中文输出：你好，世界！\r\n");
    return 0;
}

static int cmd_test_http_get(int argc, char **argv)
{
    BaseType_t task_ret = xTaskCreate(http_get_test_task, "http_get_test", TASK_STACK_SIZE_NET, NULL, 10, NULL);
    if (task_ret != pdPASS) {
        printf("Error: Failed to create HTTP GET test task! (ret=%ld)\r\n", task_ret);
        return -1;
    }
    printf("HTTP GET test task created. Check logs for results (httpbin.org/get).\r\n");
    return 0;
}

static int cmd_test_https_get(int argc, char **argv)
{
    BaseType_t task_ret = xTaskCreate(https_get_test_task, "https_get_test", TASK_STACK_SIZE_NET, NULL, 10, NULL);
    if (task_ret != pdPASS) {
        printf("Error: Failed to create HTTPS GET test task! (ret=%ld)\r\n", task_ret);
        return -1;
    }
    printf("HTTPS GET test task created. Check logs for results (httpbin.org/get).\r\n");
    return 0;
}

static int cmd_test_http_post(int argc, char **argv)
{
    BaseType_t task_ret = xTaskCreate(http_post_test_task, "http_post_test", TASK_STACK_SIZE_NET, NULL, 10, NULL);
    if (task_ret != pdPASS) {
        printf("Error: Failed to create HTTP POST test task! (ret=%ld)\r\n", task_ret);
        return -1;
    }
    printf("HTTP POST test task created. Check logs for results.\r\n");
    return 0;
}

static int cmd_test_https_post(int argc, char **argv)
{
    BaseType_t task_ret = xTaskCreate(https_post_test_task, "https_post_test", TASK_STACK_SIZE_NET, NULL, 10, NULL);
    if (task_ret != pdPASS) {
        printf("Error: Failed to create HTTPS POST test task! (ret=%ld)\r\n", task_ret);
        return -1;
    }
    printf("HTTPS POST test task created. Check logs for results.\r\n");
    return 0;
}

// curl 命令后台任务与参数解析
typedef struct {
    char *url;
    char *method;
    char *headers;
    char *body;
    char *content_type;
    int timeout_ms;
} curl_task_params_t;
static void curl_task(void *pvParameters);
static int cmd_curl(int argc, char **argv);

static void curl_task(void *pvParameters)
{
    curl_task_params_t *params = (curl_task_params_t *)pvParameters;
    uint8_t *resp_buf_ptr = NULL;
    const int resp_buf_size = TEST_RESP_BUF_SIZE;
    int final_status = REQ_ERR_INTERNAL;

    Response resp = {
        .response_buf = NULL,
        .response_buf_len = resp_buf_size,
        .status_code = 0,
        .actual_resp_len = 0
    };

    if (!params || !params->url || !params->method) {
        LOG_E("Curl Task: Invalid parameters received.\r\n");
        goto task_cleanup_curl;
    }

    if (check_wifi_state() != 0) {
        LOG_E("Curl Task: WiFi not connected! Aborting request to %s\r\n", params->url);
        final_status = REQ_ERR_CONNECT;
        goto task_cleanup_curl;
    }

    LOG_I("Curl Task: Starting %s request to %s\r\n", params->method, params->url);

    resp_buf_ptr = malloc(resp_buf_size);
    if (!resp_buf_ptr) {
        LOG_E("Curl Task: Failed to allocate response buffer (%d bytes)\r\n", resp_buf_size);
        final_status = REQ_ERR_MEM;
        goto task_cleanup_curl;
    }
    LOG_D("Curl Task: Response buffer allocated (%d bytes) at %p\r\n", resp_buf_size, resp_buf_ptr);
    resp_buf_ptr[0] = '\0';
    resp.response_buf = resp_buf_ptr;

    RequestOptions opts = {
        .method = params->method,
        .url = params->url,
        .custom_headers = params->headers,
        .content_type = params->content_type,
        .request_body = params->body,
        .body_len = (params->body ? strlen(params->body) : 0),
        .timeout_ms = params->timeout_ms
    };

    resp.response_buf = resp_buf_ptr;
    resp.response_buf_len = resp_buf_size;
    resp.status_code = 0;
    resp.actual_resp_len = 0;

    TickType_t tick_start = xTaskGetTickCount();
    LOG_I("Curl Task: Sending request...\r\n");
    final_status = send_request(&opts, &resp);
    TickType_t tick_end = xTaskGetTickCount();
    uint32_t elapsed_ms = (tick_end - tick_start) * 1000 / configTICK_RATE_HZ;
    LOG_I("Curl Task: Request finished. Time elapsed: %lu ms\r\n", (unsigned long)elapsed_ms);

    printf("\r\n-------------------- Curl Result --------------------\r\n");
    printf("URL: %s\r\n", params->url);
    printf("Method: %s\r\n", params->method);
    printf("HTTP Status Code: %d\r\n", resp.status_code);
    printf("Time elapsed: %lu ms\r\n", (unsigned long)elapsed_ms);

    if (resp.actual_resp_len > 0) {
        printf("Response Body (%d bytes, newlines replaced with space):\r\n", resp.actual_resp_len);
        printf("<<<--- START BODY --->>>\r\n");
        for (int i = 0; i < resp.actual_resp_len; i++) {
            char current_char = resp.response_buf[i];
            if (current_char != '\n' && current_char != '\r') {
                printf("%c", current_char);
            } else {
                printf(" ");
            }
        }
        printf("\r\n<<<--- END BODY --->>>\r\n");
    } else {
        printf("Response Body: (empty)\r\n");
    }

    if (final_status < 0 && final_status != resp.status_code) {
        printf("Client Error Code: %d (%s)\r\n", final_status,
               final_status == REQ_ERR_TIMEOUT        ? "Timeout" :
               final_status == REQ_ERR_CONNECT        ? "Connection/TLS Failed" :
               final_status == REQ_ERR_DNS            ? "DNS Failed" :
               final_status == REQ_ERR_BAD_REQUEST    ? "Bad Request/URL Parse Failed" :
               final_status == REQ_ERR_MEM            ? "Memory Allocation Failed" :
               final_status == REQ_ERR_RESP_TOO_LARGE ? "Response Too Large" :
               final_status == REQ_ERR_RESP_PARSE     ? "Response Parse Failed" :
                                                        "Other Client Error");
    }
    printf("-----------------------------------------------------\r\n");

task_cleanup_curl:
    if (resp_buf_ptr) {
        LOG_D("Curl Task: Freeing response buffer at %p\r\n", resp_buf_ptr);
        free(resp_buf_ptr);
    }
    if (params) {
        LOG_D("Curl Task: Freeing task parameters...\r\n");
        if (params->url)
            free(params->url);
        if (params->headers)
            free(params->headers);
        if (params->body)
            free(params->body);
        free(params);
    }

    LOG_I("Curl Task Finished.\r\n");
    vTaskDelete(NULL);
}

static int cmd_curl(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: curl <URL> [-H \"Header: Value\"] [-d 'data'] [timeout_ms]\r\n");
        printf("Example:\r\n");
        printf("  curl http://httpbin.org/get\r\n");
        printf("  curl https://httpbin.org/post -H \"Content-Type: application/json\" -d '{\"value\":1}'\r\n");
        printf("  curl https://api.deepseek.com/... -H \"Authorization: Bearer sk-...\" -H \"Content-Type: application/json\" -d '{\"model\":...}' 20000\r\n");
        return -1;
    }

    curl_task_params_t *params = NULL;
    char *url_arg = NULL;
    char *method_arg = "GET";
    char *body_arg = NULL;
    char *headers_arg = NULL;
    char *content_type_arg = NULL;
    int timeout_arg = CURL_DEFAULT_TIMEOUT_MS;
    size_t headers_current_len = 0;

    headers_arg = malloc(CURL_MAX_HEADERS_LEN);
    params = malloc(sizeof(curl_task_params_t));
    if (!params || !headers_arg) {
        printf("Error: Cannot allocate memory for task parameters or headers\r\n");
        if (params)
            free(params);
        if (headers_arg)
            free(headers_arg);
        return -1;
    }
    headers_arg[0] = '\0';
    memset(params, 0, sizeof(curl_task_params_t));

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-H") == 0) {
            if (i + 1 < argc) {
                i++;
                size_t header_len = strlen(argv[i]);
                if (headers_current_len + header_len + 2 >= CURL_MAX_HEADERS_LEN) {
                    printf("Error: Total header length exceeds limit (%d bytes)\r\n", CURL_MAX_HEADERS_LEN);
                    goto parse_error_curl;
                }
                strcat(headers_arg, argv[i]);
                strcat(headers_arg, "\r\n");
                headers_current_len += header_len + 2;

                if (content_type_arg == NULL && strncasecmp(argv[i], "Content-Type:", 13) == 0) {
                    const char *ct_start = argv[i] + 13;
                    while (*ct_start == ' ')
                        ct_start++;
                    content_type_arg = (char *)ct_start;
                }
            } else {
                printf("Error: -H requires an argument\r\n");
                goto parse_error_curl;
            }
        } else if (strcmp(argv[i], "-d") == 0) {
            if (i + 1 < argc) {
                i++;
                if (body_arg != NULL) {
                    printf("Error: Multiple -d arguments are not supported\r\n");
                    goto parse_error_curl;
                }
                body_arg = strdup(argv[i]);
                if (!body_arg) {
                    printf("Error: Cannot allocate memory for request body\r\n");
                    goto parse_error_curl;
                }
                method_arg = "POST";
            } else {
                printf("Error: -d requires an argument\r\n");
                goto parse_error_curl;
            }
        } else {
            char *endptr;
            long val = strtol(argv[i], &endptr, 10);
            if (*endptr == '\0' && argv[i][0] != '\0' && val > 0) {
                timeout_arg = (int)val;
            } else if (url_arg == NULL) {
                url_arg = strdup(argv[i]);
                if (!url_arg) {
                    printf("Error: Cannot allocate memory for URL\r\n");
                    goto parse_error_curl;
                }
            } else {
                printf("Error: Unknown argument or URL already set: %s\r\n", argv[i]);
                goto parse_error_curl;
            }
        }
    }

    if (url_arg == NULL) {
        printf("Error: URL is required\r\n");
        goto parse_error_curl;
    }

    params->url = url_arg;
    params->method = method_arg;
    if (headers_current_len > 0) {
        params->headers = headers_arg;
    } else {
        free(headers_arg);
        params->headers = NULL;
        headers_arg = NULL;
    }
    params->body = body_arg;
    params->content_type = content_type_arg;
    params->timeout_ms = timeout_arg;

    BaseType_t task_ret = xTaskCreate(curl_task,
                                      "curl_task",
                                      TASK_STACK_SIZE_NET,
                                      params,
                                      10,
                                      NULL);

    if (task_ret != pdPASS) {
        printf("Error: Failed to create curl task (ret=%ld)\r\n", task_ret);
        if (params) {
            if (params->url)
                free(params->url);
            if (params->headers)
                free(params->headers);
            if (params->body)
                free(params->body);
            free(params);
        }
        return -1;
    }

    printf("Curl task created for %s. Check logs for progress...\r\n", url_arg);
    return 0;

parse_error_curl:
    printf("Cleaning up due to parsing error...\r\n");
    if (url_arg)
        free(url_arg);
    if (headers_arg)
        free(headers_arg);
    if (body_arg)
        free(body_arg);
    if (params)
        free(params);
    return -1;
}

// Shell 命令：调用 Chat 聊天
static int cmd_simpleopenapi_chat(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: chat <user_message>\r\n");
        return -1;
    }
    size_t total_len = 0;
    for (int i = 1; i < argc; i++)
        total_len += strlen(argv[i]) + 1;
    if (total_len == 0)
        return -1;
    char *full_message = malloc(total_len);
    if (!full_message)
        return -1;
    full_message[0] = '\0';
    for (int i = 1; i < argc; i++) {
        strcat(full_message, argv[i]);
        if (i < argc - 1)
            strcat(full_message, " ");
    }
    int ret = simple_openapi_chat_async(full_message);
    free(full_message);
    if (ret != 0) {
        printf("Error: Failed to initiate Chat request (Error code: %d)\r\n", ret);
        return -1;
    }
    printf("Chat request initiated. Check logs for response...\r\n");
    return 0;
}

// Shell 命令注册
SHELL_CMD_EXPORT_ALIAS(cmd_greet, greet, Greet the user : greet[name]);
SHELL_CMD_EXPORT_ALIAS(cmd_robot_action, action, Robot action : action<move | turn | dance>[value]);
SHELL_CMD_EXPORT_ALIAS(cmd_test_http_get, test_http_get, Test HTTP GET to httpbin.org / get);
SHELL_CMD_EXPORT_ALIAS(cmd_test_https_get, test_https_get, Test HTTPS GET to httpbin.org / get);
SHELL_CMD_EXPORT_ALIAS(cmd_test_http_post, test_http_post, Test HTTP POST to httpbin.org / post);
SHELL_CMD_EXPORT_ALIAS(cmd_test_https_post, test_https_post, Test HTTPS POST to httpbin.org / post);
SHELL_CMD_EXPORT_ALIAS(cmd_test_url_parse, test_parseurl, Test the URL parser : test_parseurl<url>);
SHELL_CMD_EXPORT_ALIAS(cmd_chinese_test, test_chinese, Test Chinese output : test_chinese);
SHELL_CMD_EXPORT_ALIAS(cmd_curl, curl, "Simple curl: curl <URL> [-H H:V] [-d 'data'] [timeout]");
SHELL_CMD_EXPORT_ALIAS(cmd_simpleopenapi_chat, chat, Send message to Chat API : chat<message>);
#endif