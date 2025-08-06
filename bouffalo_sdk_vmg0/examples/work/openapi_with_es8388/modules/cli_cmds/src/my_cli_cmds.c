// my_cli_cmds.c - Shell 命令实现与网络请求测试
#include "my_cli_cmds.h"

#include <lwip/netdb.h>
#include <lwip/sockets.h>
#include <lwip/tcpip.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "es8388_driver.h"
#include "http_client.h"
#include "https.h"
#include "https_client.h"
#include "log.h"
#include "requests.h"
#include "semphr.h"
#include "simpleopenapi_client.h"
#include "task.h"
#include "unified_gateway_client.h"
#include "url_parser.h"
#include "wifi_mgmr_ext.h"
#ifdef CONFIG_SHELL
#include "shell.h"
#endif

#define TEST_RESP_BUF_SIZE 4096
#define TASK_STACK_SIZE_NET 8192
#define CURL_MAX_HEADERS_LEN 512
#define CURL_DEFAULT_TIMEOUT_MS 15000

extern volatile uint32_t wifi_state;

int check_wifi_state(void) { return (wifi_state == 1) ? 0 : 1; }

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

static void request_test_task(void *pvParameters) {
	fixed_request_args_t *args = (fixed_request_args_t *)pvParameters;
	uint8_t *resp_buf_ptr = NULL;
	int final_status = REQ_ERR_INTERNAL;

	if (check_wifi_state() != 0) {
		LOG_E("%s %s Task Error: WiFi not connected!\r\n", args->method,
			  args->url);
		goto task_exit_fixed;
	}

	LOG_I("%s %s Test Task Started (using send_request)...\r\n", args->method,
		  args->url);
	resp_buf_ptr = malloc(TEST_RESP_BUF_SIZE);
	if (!resp_buf_ptr) {
		LOG_E("Failed to allocate response buffer (%d bytes)\r\n",
			  TEST_RESP_BUF_SIZE);
		goto task_exit_fixed;
	}
	LOG_D("Response buffer allocated (%d bytes) at %p\r\n", TEST_RESP_BUF_SIZE,
		  resp_buf_ptr);
	resp_buf_ptr[0] = '\0';

	RequestOptions opts = {.method = args->method,
						   .url = args->url,
						   .timeout_ms = args->timeout_ms,
						   .content_type = args->content_type,
						   .request_body = args->body,
						   .body_len = args->body ? strlen(args->body) : 0,
						   .custom_headers = NULL};
	Response resp = {.response_buf = resp_buf_ptr,
					 .response_buf_len = TEST_RESP_BUF_SIZE,
					 .status_code = 0,
					 .actual_resp_len = 0};

	final_status = send_request(&opts, &resp);

	LOG_I("send_request returned: %d\r\n", final_status);
	LOG_I("Response status_code: %d, Actual length: %d\r\n", resp.status_code,
		  resp.actual_resp_len);

	if (resp.status_code >= 200 && resp.status_code < 300) {
		LOG_I("%s %s Success! Status Code: %d\r\n", args->method, args->url,
			  resp.status_code);
		if (resp.actual_resp_len > 0) {
			printf(
				"--- Start Response Body (%d bytes, newlines replaced with "
				"space) ---\r\n",
				resp.actual_resp_len);
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
		LOG_E("%s %s Failed! HTTP Status Code: %d\r\n", args->method, args->url,
			  resp.status_code);
		if (resp.actual_resp_len > 0) {
			printf(
				"--- Start Error Body (%d bytes, newlines replaced with space) "
				"---\r\n",
				resp.actual_resp_len);
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
		LOG_E("%s %s Client Failed! Error code: %d (%s)\r\n", args->method,
			  args->url, resp.status_code,
			  resp.status_code == REQ_ERR_TIMEOUT	? "Timeout"
			  : resp.status_code == REQ_ERR_CONNECT ? "Connection/TLS Failed"
			  : resp.status_code == REQ_ERR_DNS		? "DNS Failed"
			  : resp.status_code == REQ_ERR_BAD_REQUEST
				  ? "Bad Request/URL Parse Failed"
			  : resp.status_code == REQ_ERR_MEM ? "Memory Allocation Failed"
			  : resp.status_code == REQ_ERR_RESP_TOO_LARGE
				  ? "Response Too Large"
			  : resp.status_code == REQ_ERR_RESP_PARSE ? "Response Parse Failed"
													   : "Other Client Error");
	}

task_exit_fixed:
	if (resp_buf_ptr) {
		LOG_D("Freeing fixed request response buffer at %p\r\n", resp_buf_ptr);
		free(resp_buf_ptr);
	}
	LOG_I("%s %s Test Task Finished.\r\n", args->method, args->url);
	vTaskDelete(NULL);
}

static void http_get_test_task(void *pvParameters) {
	static fixed_request_args_t args = {
		"GET", "http://httpbin.org/get?source=bl618_send_request_http", NULL,
		NULL, 15000};
	request_test_task(&args);
}

static void https_get_test_task(void *pvParameters) {
	static fixed_request_args_t args = {"GET", "https://httpbin.org/get", NULL,
										NULL, 20000};
	request_test_task(&args);
}

static void http_post_test_task(void *pvParameters) {
	static fixed_request_args_t args = {
		"POST", "http://httpbin.org/post", "application/json",
		"{\"chip\":\"BL618_send_req_http\", \"message\":\"Hello again!\"}",
		15000};
	request_test_task(&args);
}

static void https_post_test_task(void *pvParameters) {
	static fixed_request_args_t args = {
		"POST", "https://httpbin.org/post", "application/json",
		"{\"source\":\"BL618_send_request\", \"secure\":true}", 20000};
	request_test_task(&args);
}

#ifdef CONFIG_SHELL

// Shell 命令：打印问候语
enum { GREET_OK = 0, GREET_ARGERR = -1 };
static int cmd_greet(int argc, char **argv) {
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
static int cmd_robot_action(int argc, char **argv) {
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
static int cmd_test_url_parse(int argc, char **argv) {
	if (argc != 2) {
		LOG_E("Usage: test_parseurl <url_string>\r\n");
		return -1;
	}
	parsed_url_t parsed_result;
	int ret = parse_url(argv[1], &parsed_result);
	if (ret == URL_PARSE_OK) {
		LOG_I("URL OK: scheme=%s, host=%s, port=%u, path=%s\r\n",
			  parsed_result.scheme, parsed_result.host, parsed_result.port,
			  parsed_result.path);
	} else {
		LOG_E("URL parse error: %d\r\n", ret);
		return -1;
	}
	return 0;
}

// Shell 命令：测试中文输出
static int cmd_chinese_test(int argc, char **argv) {
	LOG_I("测试中文输出：你好，世界！\r\n");
	return 0;
}

static int cmd_test_http_get(int argc, char **argv) {
	BaseType_t task_ret = xTaskCreate(http_get_test_task, "http_get_test",
									  TASK_STACK_SIZE_NET, NULL, 10, NULL);
	if (task_ret != pdPASS) {
		printf("Error: Failed to create HTTP GET test task! (ret=%ld)\r\n",
			   task_ret);
		return -1;
	}
	printf(
		"HTTP GET test task created. Check logs for results "
		"(httpbin.org/get).\r\n");
	return 0;
}

static int cmd_test_https_get(int argc, char **argv) {
	BaseType_t task_ret = xTaskCreate(https_get_test_task, "https_get_test",
									  TASK_STACK_SIZE_NET, NULL, 10, NULL);
	if (task_ret != pdPASS) {
		printf("Error: Failed to create HTTPS GET test task! (ret=%ld)\r\n",
			   task_ret);
		return -1;
	}
	printf(
		"HTTPS GET test task created. Check logs for results "
		"(httpbin.org/get).\r\n");
	return 0;
}

static int cmd_test_http_post(int argc, char **argv) {
	BaseType_t task_ret = xTaskCreate(http_post_test_task, "http_post_test",
									  TASK_STACK_SIZE_NET, NULL, 10, NULL);
	if (task_ret != pdPASS) {
		printf("Error: Failed to create HTTP POST test task! (ret=%ld)\r\n",
			   task_ret);
		return -1;
	}
	printf("HTTP POST test task created. Check logs for results.\r\n");
	return 0;
}

static int cmd_test_https_post(int argc, char **argv) {
	BaseType_t task_ret = xTaskCreate(https_post_test_task, "https_post_test",
									  TASK_STACK_SIZE_NET, NULL, 10, NULL);
	if (task_ret != pdPASS) {
		printf("Error: Failed to create HTTPS POST test task! (ret=%ld)\r\n",
			   task_ret);
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

static void curl_task(void *pvParameters) {
	curl_task_params_t *params = (curl_task_params_t *)pvParameters;
	uint8_t *resp_buf_ptr = NULL;
	const int resp_buf_size = TEST_RESP_BUF_SIZE;
	int final_status = REQ_ERR_INTERNAL;

	Response resp = {.response_buf = NULL,
					 .response_buf_len = resp_buf_size,
					 .status_code = 0,
					 .actual_resp_len = 0};

	if (!params || !params->url || !params->method) {
		LOG_E("Curl Task: Invalid parameters received.\r\n");
		goto task_cleanup_curl;
	}

	if (check_wifi_state() != 0) {
		LOG_E("Curl Task: WiFi not connected! Aborting request to %s\r\n",
			  params->url);
		final_status = REQ_ERR_CONNECT;
		goto task_cleanup_curl;
	}

	LOG_I("Curl Task: Starting %s request to %s\r\n", params->method,
		  params->url);

	resp_buf_ptr = malloc(resp_buf_size);
	if (!resp_buf_ptr) {
		LOG_E("Curl Task: Failed to allocate response buffer (%d bytes)\r\n",
			  resp_buf_size);
		final_status = REQ_ERR_MEM;
		goto task_cleanup_curl;
	}
	LOG_D("Curl Task: Response buffer allocated (%d bytes) at %p\r\n",
		  resp_buf_size, resp_buf_ptr);
	resp_buf_ptr[0] = '\0';
	resp.response_buf = resp_buf_ptr;

	RequestOptions opts = {
		.method = params->method,
		.url = params->url,
		.custom_headers = params->headers,
		.content_type = params->content_type,
		.request_body = params->body,
		.body_len = (params->body ? strlen(params->body) : 0),
		.timeout_ms = params->timeout_ms};

	resp.response_buf = resp_buf_ptr;
	resp.response_buf_len = resp_buf_size;
	resp.status_code = 0;
	resp.actual_resp_len = 0;

	TickType_t tick_start = xTaskGetTickCount();
	LOG_I("Curl Task: Sending request...\r\n");
	final_status = send_request(&opts, &resp);
	TickType_t tick_end = xTaskGetTickCount();
	uint32_t elapsed_ms = (tick_end - tick_start) * 1000 / configTICK_RATE_HZ;
	LOG_I("Curl Task: Request finished. Time elapsed: %lu ms\r\n",
		  (unsigned long)elapsed_ms);

	printf("\r\n-------------------- Curl Result --------------------\r\n");
	printf("URL: %s\r\n", params->url);
	printf("Method: %s\r\n", params->method);
	printf("HTTP Status Code: %d\r\n", resp.status_code);
	printf("Time elapsed: %lu ms\r\n", (unsigned long)elapsed_ms);

	if (resp.actual_resp_len > 0) {
		printf("Response Body (%d bytes, newlines replaced with space):\r\n",
			   resp.actual_resp_len);
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
			   final_status == REQ_ERR_TIMEOUT	 ? "Timeout"
			   : final_status == REQ_ERR_CONNECT ? "Connection/TLS Failed"
			   : final_status == REQ_ERR_DNS	 ? "DNS Failed"
			   : final_status == REQ_ERR_BAD_REQUEST
				   ? "Bad Request/URL Parse Failed"
			   : final_status == REQ_ERR_MEM ? "Memory Allocation Failed"
			   : final_status == REQ_ERR_RESP_TOO_LARGE ? "Response Too Large"
			   : final_status == REQ_ERR_RESP_PARSE ? "Response Parse Failed"
													: "Other Client Error");
	}
	printf("-----------------------------------------------------\r\n");

task_cleanup_curl:
	if (resp_buf_ptr) {
		LOG_D("Curl Task: Freeing response buffer at %p\r\n", resp_buf_ptr);
		free(resp_buf_ptr);
	}
	if (params) {
		LOG_D("Curl Task: Freeing task parameters...\r\n");
		if (params->url) free(params->url);
		if (params->headers) free(params->headers);
		if (params->body) free(params->body);
		free(params);
	}

	LOG_I("Curl Task Finished.\r\n");
	vTaskDelete(NULL);
}

static int cmd_curl(int argc, char **argv) {
	if (argc < 2) {
		printf(
			"Usage: curl <URL> [-H \"Header: Value\"] [-d 'data'] "
			"[timeout_ms]\r\n");
		printf("Example:\r\n");
		printf("  curl http://httpbin.org/get\r\n");
		printf(
			"  curl https://httpbin.org/post -H \"Content-Type: "
			"application/json\" -d '{\"value\":1}'\r\n");
		printf(
			"  curl https://api.deepseek.com/... -H \"Authorization: Bearer "
			"sk-...\" -H \"Content-Type: application/json\" -d "
			"'{\"model\":...}' 20000\r\n");
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
		printf(
			"Error: Cannot allocate memory for task parameters or headers\r\n");
		if (params) free(params);
		if (headers_arg) free(headers_arg);
		return -1;
	}
	headers_arg[0] = '\0';
	memset(params, 0, sizeof(curl_task_params_t));

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-H") == 0) {
			if (i + 1 < argc) {
				i++;
				size_t header_len = strlen(argv[i]);
				if (headers_current_len + header_len + 2 >=
					CURL_MAX_HEADERS_LEN) {
					printf(
						"Error: Total header length exceeds limit (%d "
						"bytes)\r\n",
						CURL_MAX_HEADERS_LEN);
					goto parse_error_curl;
				}
				strcat(headers_arg, argv[i]);
				strcat(headers_arg, "\r\n");
				headers_current_len += header_len + 2;

				if (content_type_arg == NULL &&
					strncasecmp(argv[i], "Content-Type:", 13) == 0) {
					const char *ct_start = argv[i] + 13;
					while (*ct_start == ' ') ct_start++;
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
					printf(
						"Error: Multiple -d arguments are not supported\r\n");
					goto parse_error_curl;
				}
				body_arg = strdup(argv[i]);
				if (!body_arg) {
					printf(
						"Error: Cannot allocate memory for request body\r\n");
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
				printf("Error: Unknown argument or URL already set: %s\r\n",
					   argv[i]);
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

	BaseType_t task_ret = xTaskCreate(curl_task, "curl_task",
									  TASK_STACK_SIZE_NET, params, 10, NULL);

	if (task_ret != pdPASS) {
		printf("Error: Failed to create curl task (ret=%ld)\r\n", task_ret);
		if (params) {
			if (params->url) free(params->url);
			if (params->headers) free(params->headers);
			if (params->body) free(params->body);
			free(params);
		}
		return -1;
	}

	printf("Curl task created for %s. Check logs for progress...\r\n", url_arg);
	return 0;

parse_error_curl:
	printf("Cleaning up due to parsing error...\r\n");
	if (url_arg) free(url_arg);
	if (headers_arg) free(headers_arg);
	if (body_arg) free(body_arg);
	if (params) free(params);
	return -1;
}

// Shell 命令：调用 Chat 聊天
static int cmd_simpleopenapi_chat(int argc, char **argv) {
	if (argc < 2) {
		printf("Usage: chat <user_message>\r\n");
		return -1;
	}
	size_t total_len = 0;
	for (int i = 1; i < argc; i++) total_len += strlen(argv[i]) + 1;
	if (total_len == 0) return -1;
	char *full_message = malloc(total_len);
	if (!full_message) return -1;
	full_message[0] = '\0';
	for (int i = 1; i < argc; i++) {
		strcat(full_message, argv[i]);
		if (i < argc - 1) strcat(full_message, " ");
	}
	int ret = simple_openapi_chat_async(full_message);
	free(full_message);
	if (ret != 0) {
		printf("Error: Failed to initiate Chat request (Error code: %d)\r\n",
			   ret);
		return -1;
	}
	printf("Chat request initiated. Check logs for response...\r\n");
	return 0;
}

// --- 新增: ES8388 音频控制 Shell 命令 ---

// 配置接收端 PC 的 IP 地址和端口
#define LAN_PCM_SERVER_IP \
	"192.168.85.89"	 // <<--- 【已更新为您日志中提供的IP地址】
#define LAN_PCM_SERVER_PORT 80
#define LAN_PCM_SERVER_PATH \
	"/api/stt"	// 与Python服务器脚本中的 UPLOAD_PATH 一致

// 定义发送时单次从音频驱动获取的数据块大小
#define LAN_AUDIO_FETCH_CHUNK_SIZE (32000)
#define DEFAULT_AUDIO_CHUNKS_TO_SEND \
	30	// 默认发送30个数据块 (约 15 秒音频: 30 * 0.5s)

static uint8_t lan_audio_chunk_buffer[LAN_AUDIO_FETCH_CHUNK_SIZE];
// --- 新增: 为 multipart/form-data 请求体创建一个更大的缓冲区 ---
// 需要容纳音频数据本身 + 所有 multipart 头部信息（大约需要额外300-400字节）
#define LAN_MULTIPART_BODY_BUFFER_SIZE (LAN_AUDIO_FETCH_CHUNK_SIZE + 512)
static uint8_t multipart_body_buffer[LAN_MULTIPART_BODY_BUFFER_SIZE];

int http_post_audio(const char *url, const uint8_t *audio_data,
					size_t audio_len, uint8_t *response, size_t max_len) {
	LOG_I("[HTTP-POST] Starting request to: %s (audio length: %zu bytes)\n",
		  url, audio_len);

	// 1. 解析URL（提取host、path、port）
	char host[128] = {0}, path[256] = {0};
	int port = 80;	// 默认HTTP端口
	char *p1 = NULL, *p2 = NULL;

	// 处理协议头（http://）
	if (strncmp(url, "http://", 7) == 0) {
		p1 = (char *)url + 7;
		LOG_I("[HTTP-POST] URL protocol: HTTP\n");
	} else {
		LOG_I("[HTTP-POST] ERROR: Only HTTP protocol is supported\n");
		return -1;
	}

	// 提取host和path（支持带端口的host，如host:port）
	p2 = strchr(p1, '/');			   // 查找路径分隔符
	char *port_sep = strchr(p1, ':');  // 查找端口分隔符（若有）
	if (port_sep && (!p2 || port_sep < p2)) {
		// 解析端口（如 host:8080/path）
		strncpy(host, p1, port_sep - p1);
		host[port_sep - p1] = '\0';
		port = atoi(port_sep + 1);	// 转换端口号
		if (p2) {
			strncpy(path, p2, sizeof(path) - 1);  // 提取path
		} else {
			strcpy(path, "/");
		}
	} else if (p2) {
		// 无端口，直接分割host和path（如 host/path）
		strncpy(host, p1, p2 - p1);
		host[p2 - p1] = '\0';
		strncpy(path, p2, sizeof(path) - 1);
	} else {
		// 无path（如 host）
		strncpy(host, p1, sizeof(host) - 1);
		strcpy(path, "/");
	}
	LOG_I("[HTTP-POST] Parsed URL - host: %s, port: %d, path: %s\n", host, port,
		  path);

	// 2. DNS解析（获取服务器IP）
	LOG_I("[HTTP-POST] Resolving host: %s\n", host);
	struct hostent *server = gethostbyname(host);
	if (server == NULL) {
		LOG_I("[HTTP-POST] ERROR: Failed to resolve host (errno: %d)\n",
			  h_errno);
		return -1;
	}
	LOG_I("[HTTP-POST] Host resolved - IP: %s\n",
		  inet_ntoa(*(struct in_addr *)server->h_addr));

	// 3. 创建Socket
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		LOG_I("[HTTP-POST] ERROR: Failed to create socket (errno: %d)\n",
			  errno);
		return -1;
	}
	LOG_I("[HTTP-POST] Socket created (fd: %d)\n", sockfd);

	// 4. 连接服务器
	struct sockaddr_in serv_addr;
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);  // 端口转换为网络字节序
	memcpy(&serv_addr.sin_addr.s_addr, server->h_addr,
		   server->h_length);  // IP地址

	LOG_I("[HTTP-POST] Connecting to %s:%d...\n", inet_ntoa(serv_addr.sin_addr),
		  ntohs(serv_addr.sin_port));
	int connect_result =
		connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	if (connect_result < 0) {
		LOG_I("[HTTP-POST] ERROR: Connection failed (errno: %d)\n", errno);
		close(sockfd);
		return -1;
	}
	LOG_I("[HTTP-POST] Connected successfully\n");

	// 5. 构建POST请求头（关键：包含Content-Length和正确的HTTP版本）
	char request_header[512] = {0};
	// 使用HTTP/1.1（避免HTTP/1.0的短连接等待问题），显式指定Content-Length和连接方式
	snprintf(request_header, sizeof(request_header),
			 "POST %s HTTP/1.1\r\n"
			 "Host: %s\r\n"
			 "Content-Type: application/octet-stream\r\n"  // 音频二进制流类型
			 "Content-Length: %zu\r\n"	// 关键：告知服务端音频数据长度
			 "Connection: Close\r\n"	// 关键修复：使用短连接而非长连接
			 "\r\n",					// 头部结束标志（空行）
			 path, host, audio_len);	// 传入path、host、端口、音频长度
	LOG_I("[HTTP-POST] Request header:\n%s", request_header);

	// 6. 发送请求（先发送头，再发送音频数据）
	// 6.1 发送请求头
	ssize_t header_sent = write(sockfd, request_header, strlen(request_header));
	if (header_sent < 0) {
		LOG_I("[HTTP-POST] ERROR: Failed to send header (errno: %d)\n", errno);
		close(sockfd);
		return -1;
	}
	LOG_I("[HTTP-POST] Sent %zd bytes of header\n", header_sent);

	// 6.2 发送音频二进制数据（关键：确保完整发送）
	if (audio_len > 0 && audio_data != NULL) {
		ssize_t body_sent = write(sockfd, audio_data, audio_len);
		if (body_sent < 0) {
			LOG_I("[HTTP-POST] ERROR: Failed to send audio data (errno: %d)\n",
				  errno);
			close(sockfd);
			return -1;
		}
		if ((size_t)body_sent != audio_len) {
			LOG_I(
				"[HTTP-POST] WARNING: Audio data not fully sent (sent: "
				"%zd/%zu)\n",
				body_sent, audio_len);
		} else {
			LOG_I("[HTTP-POST] Sent all %zu bytes of audio data\n", audio_len);
		}
	} else {
		LOG_I("[HTTP-POST] No audio data to send (length: 0)\n");
	}

	// 7. 接收响应（同http_get逻辑，处理头部和响应体）
	LOG_I("[HTTP-POST] Receiving response...\n");
	size_t total_recv = 0;
	int n;

	// 循环接收响应（直到缓冲区满或连接关闭）
	while ((n = read(sockfd, response + total_recv, max_len - total_recv - 1)) >
		   0) {
		total_recv += n;
		if (total_recv >= max_len - 1) {
			LOG_I(
				"[HTTP-POST] WARNING: Response buffer full (max: %zu bytes)\n",
				max_len);
			break;
		}
	}
	if (n < 0) {
		LOG_I("[HTTP-POST] ERROR: Failed to receive response (errno: %d)\n",
			  errno);
		close(sockfd);
		return -1;
	}
	response[total_recv] = '\0';  // 确保字符串结束
	LOG_I("[HTTP-POST] Received %zu bytes of response\n", total_recv);

	// 8. 解析响应（提取响应体，同http_get逻辑）
	uint8_t *body_start = (uint8_t *)strstr((char *)response, "\r\n\r\n");
	if (body_start) {
		body_start += 4;  // 跳过"\r\n\r\n"分隔符
		LOG_I("[HTTP-POST] Response headers:\n%s\n", response);	 // 打印头部
		LOG_I("[HTTP-POST] Response body (first 200 bytes):\n%.200s\n",
			  body_start);
		// 将响应体移动到缓冲区起始位置（方便上层处理）
		size_t body_len = total_recv - (body_start - response);
		memmove(response, body_start, body_len + 1);
		total_recv = body_len;
	} else {
		LOG_I("[HTTP-POST] WARNING: No header/body separator found\n");
	}

	// 9. 清理资源
	close(sockfd);
	LOG_I("[HTTP-POST] Socket closed\n");
	LOG_I("[HTTP-POST] Request completed - total response body: %zu bytes\n",
		  total_recv);

	return 0;
}

/**
 * @brief Shell command to initialize and start ES8388 audio capture.
 */
static int cmd_audio_start(int argc, char **argv) {
	LOG_I("Attempting to initialize and start audio capture...\r\n");

	// 获取当前状态，避免重复初始化或启动
	audio_module_state_t current_state = es8388_audio_get_state();

	if (current_state == AUDIO_STATE_UNINITIALIZED) {
		if (es8388_audio_init() != 0) {
			LOG_E("ES8388 audio initialization failed.\r\n");
			return -1;
		}
		LOG_I("ES8388 audio module initialized.\r\n");
	} else if (current_state == AUDIO_STATE_CAPTURING) {
		LOG_W("Audio capture is already active.\r\n");
		return 0;
	}
	// 如果是 AUDIO_STATE_INITIALIZED 状态，则直接尝试启动

	if (es8388_audio_start_capture() != 0) {
		LOG_E("Failed to start ES8388 audio capture.\r\n");
		return -2;	// 使用不同的错误码以便区分
	}
	LOG_I("ES8388 audio capture started.\r\n");

	char server_url[128];
	snprintf(server_url, sizeof(server_url), "http://%s:%d%s",
			 LAN_PCM_SERVER_IP, LAN_PCM_SERVER_PORT, LAN_PCM_SERVER_PATH);
	LOG_I("目标服务器 URL: %s\r\n", server_url);

	uint32_t fetched_data_len = 0;
	int audio_ret = -1;

	int num_chunks_to_send = 100;
	int successfully_sent_chunks = 0;

	for (int i = 0; i < num_chunks_to_send; i++) {
		char exact_url[128];
		sprintf(exact_url, "%s/%d", server_url, i);
		LOG_I("正在准备第 %d/%d 个数据块...\r\n", i + 1, num_chunks_to_send);
		TickType_t get_data_timeout = pdMS_TO_TICKS(1000);
		audio_ret = es8388_audio_get_data(lan_audio_chunk_buffer,
										  LAN_AUDIO_FETCH_CHUNK_SIZE,
										  &fetched_data_len, get_data_timeout);

		// int play_ret =
		//	es8388_audio_play(lan_audio_chunk_buffer, fetched_data_len);

		// if (play_ret != 0) {
		//	LOG_E("音频播放失败");
		// }

		if (audio_ret != 0 || fetched_data_len == 0) {
			LOG_E("获取音频数据失败 (返回: %d)。\r\n", audio_ret);
			if (i == 0) return -2;
			continue;
		}

		uint8_t http_resp_buf[1000];

		LOG_I("正在发送第 %d/%d 个数据块 (%lu 字节) 到 %s...\r\n", i + 1,
			  num_chunks_to_send, fetched_data_len, server_url);

		http_post_audio(server_url, lan_audio_chunk_buffer, fetched_data_len,
						http_resp_buf, sizeof(http_resp_buf));

		LOG_I("请求返回：%s\n", http_resp_buf);
	}

	return 0;
}
/**
 * @brief Shell command to stop ES8388 audio capture and deinitialize the
 * module.
 */
static int cmd_audio_stop(int argc, char **argv) {
	LOG_I("Attempting to stop audio capture and deinitialize module...\r\n");

	audio_module_state_t current_state = es8388_audio_get_state();

	if (current_state == AUDIO_STATE_CAPTURING) {
		if (es8388_audio_stop_capture() != 0) {
			LOG_E(
				"Failed to stop ES8388 audio capture, but will attempt "
				"deinit.\r\n");
			// 即使停止失败，也尝试反初始化
		} else {
			LOG_I("ES8388 audio capture stopped.\r\n");
		}
	} else if (current_state == AUDIO_STATE_UNINITIALIZED) {
		LOG_W(
			"Audio module is not initialized or already "
			"deinitialized.\r\n");
		return 0;  // 无需操作
	} else {
		LOG_I("Audio capture was not active, proceeding to deinit.\r\n");
	}

	es8388_audio_deinit();	// 清理资源
	LOG_I("ES8388 audio module deinitialized.\r\n");
	return 0;
}

/**
 * @brief Shell command to get a chunk of audio data (for testing).
 */
static int cmd_audio_get_data(int argc, char **argv) {
#define TEMP_AUDIO_CHUNK_SIZE 1024	// 定义一个临时缓冲区大小用于测试
	static uint8_t temp_audio_buffer[TEMP_AUDIO_CHUNK_SIZE];
	uint32_t data_len = 0;

	LOG_I("Attempting to get audio data...\r\n");
	if (es8388_audio_get_state() != AUDIO_STATE_CAPTURING) {
		LOG_E(
			"Audio capture is not active. Start capture first with "
			"'audio_start'.\r\n");
		return -1;
	}
	TickType_t get_data_timeout =
		pdMS_TO_TICKS(1000);  // 或者给1秒的富余，等待DMA填充完毕
	int ret = es8388_audio_get_data(temp_audio_buffer, TEMP_AUDIO_CHUNK_SIZE,
									&data_len, get_data_timeout);
	if (ret == 0 && data_len > 0) {
		LOG_I("Successfully retrieved %lu bytes of audio data.\r\n", data_len);
		// 为简单起见，这里只打印前几个字节作为示例
		// 注意：直接打印二进制音频数据到控制台可能不可读
		printf("First few bytes (hex): ");
		for (int i = 0; i < 16 && i < data_len; i++) {
			printf("%02X ", temp_audio_buffer[i]);
		}
		printf("\r\n");
	} else if (ret == -3) {
		LOG_W(
			"Audio data not ready yet (DMA transfer might be "
			"ongoing).\r\n");
	} else {
		LOG_E("Failed to get audio data, error code: %d, data_len: %lu\r\n",
			  ret, data_len);
	}
	return ret;
}

/**
 * @brief Shell command to send multiple chunks of captured audio data to a
 * LAN server. [全新方案] 将 session_id 放在 URL 中，请求体为原始二进制流。
 * Usage: send_audio_lan [num_chunks]
 */
static int cmd_send_audio_lan(int argc, char **argv) {
	if (es8388_audio_get_state() != AUDIO_STATE_CAPTURING) {
		LOG_E("音频未在采集中，请先运行 'audio_start' 命令。\r\n");
		return -1;
	}

	int num_chunks_to_send = DEFAULT_AUDIO_CHUNKS_TO_SEND;
	if (argc > 1) {
		int arg_chunks = atoi(argv[1]);
		if (arg_chunks > 0) {
			num_chunks_to_send = arg_chunks;
		}
	}
	LOG_I("准备发送 %d 个音频数据块 (每块 %d 字节)。\r\n", num_chunks_to_send,
		  LAN_AUDIO_FETCH_CHUNK_SIZE);

	// 1. 生成本次流式会话的 session_id
	char session_id_str[24];
	snprintf(session_id_str, sizeof(session_id_str), "%llu",
			 bflb_mtimer_get_time_us());
	LOG_I("本次会话 Session ID: %s\r\n", session_id_str);

	// ★★★★★【核心修改】★★★★★
	// 2. 将 session_id 拼接到 URL 路径的末尾
	char server_url[128];
	snprintf(server_url, sizeof(server_url), "http://%s:%d%s",
			 LAN_PCM_SERVER_IP, LAN_PCM_SERVER_PORT, LAN_PCM_SERVER_PATH);
	LOG_I("目标服务器 URL: %s\r\n", server_url);

	uint32_t fetched_data_len = 0;
	int audio_ret = -1;
	int successfully_sent_chunks = 0;

	for (int i = 0; i < num_chunks_to_send; i++) {
		LOG_I("正在准备第 %d/%d 个数据块...\r\n", i + 1, num_chunks_to_send);
		TickType_t get_data_timeout = pdMS_TO_TICKS(1000);
		audio_ret = es8388_audio_get_data(lan_audio_chunk_buffer,
										  LAN_AUDIO_FETCH_CHUNK_SIZE,
										  &fetched_data_len, get_data_timeout);

		if (audio_ret != 0 || fetched_data_len == 0) {
			LOG_E("获取音频数据失败 (返回: %d)。\r\n", audio_ret);
			if (i == 0) return -2;
			goto send_summary_lan_v2;
		}

		// 3. ★恢复到最原始的
		// RequestOptions★，不再需要任何复杂的头部和body拼接！
		RequestOptions opts = {
			.method = "POST",
			.url = server_url,
			.custom_headers =
				"Connection: Keep-Alive\r\n",  // 可以保留，或设为 NULL
			.content_type = "application/octet-stream",	 // 发送原始二进制流
			.request_body = (const char *)lan_audio_chunk_buffer,
			.body_len = fetched_data_len,
			.timeout_ms = 10000};

		uint8_t http_resp_buf[256];
		Response http_resp_data = {.response_buf = http_resp_buf,
								   .response_buf_len = sizeof(http_resp_buf)};

		LOG_I("正在发送第 %d/%d 个数据块 (%lu 字节) 到 %s...\r\n", i + 1,
			  num_chunks_to_send, fetched_data_len, server_url);
		int send_status = send_request(&opts, &http_resp_data);

		if (send_status >= 200 && send_status < 300) {
			LOG_I("第 %d/%d 个数据块发送成功。HTTP 状态码: %d。\r\n", i + 1,
				  num_chunks_to_send, send_status);
			if (http_resp_data.actual_resp_len > 0) {
				LOG_I("服务器响应: %.*s\r\n", http_resp_data.actual_resp_len,
					  http_resp_data.response_buf);
			}
			successfully_sent_chunks++;
		} else {
			LOG_E("第 %d/%d 个数据块发送失败。send_request 返回: %d\r\n", i + 1,
				  num_chunks_to_send, send_status);
			goto send_summary_lan_v2;
		}
	}

send_summary_lan_v2:
	LOG_I("发送总结：总共尝试发送 %d 个数据块，成功发送 %d 个。\r\n",
		  num_chunks_to_send, successfully_sent_chunks);
	return 0;
}

static int cmd_robot_start(int argc, char **argv) {
	printf("Command: robot_start\r\n");

	// 步骤 1: 启动音频采集 (由“专人”负责)
	// 检查是否已经初始化，避免重复操作
	if (es8388_audio_get_state() == AUDIO_STATE_UNINITIALIZED) {
		printf("Audio not initialized. Initializing...\r\n");
		if (es8388_audio_init() != 0) {
			printf("Error: Audio module initialization failed.\r\n");
			return -1;
		}
	}
	if (es8388_audio_get_state() != AUDIO_STATE_CAPTURING) {
		printf("Audio not capturing. Starting capture...\r\n");
		if (es8388_audio_start_capture() != 0) {
			printf("Error: Failed to start audio capture.\r\n");
			return -2;
		}
	}
	printf("Audio system is ready.\r\n");

	// 步骤 2: 调用我们新的MQTT会话启动函数
	printf("Starting MQTT session task...\r\n");
	robot_session_start();

	return 0;
}

// Shell 命令注册
SHELL_CMD_EXPORT_ALIAS(cmd_greet, greet, Greet the user : greet[name]);
SHELL_CMD_EXPORT_ALIAS(cmd_robot_action, action,
					   Robot action : action<move | turn | dance>[value]);
SHELL_CMD_EXPORT_ALIAS(cmd_test_http_get, test_http_get,
					   Test HTTP GET to httpbin.org / get);
SHELL_CMD_EXPORT_ALIAS(cmd_test_https_get, test_https_get,
					   Test HTTPS GET to httpbin.org / get);
SHELL_CMD_EXPORT_ALIAS(cmd_test_http_post, test_http_post,
					   Test HTTP POST to httpbin.org / post);
SHELL_CMD_EXPORT_ALIAS(cmd_test_https_post, test_https_post,
					   Test HTTPS POST to httpbin.org / post);
SHELL_CMD_EXPORT_ALIAS(cmd_test_url_parse, test_parseurl,
					   Test the URL parser : test_parseurl<url>);
SHELL_CMD_EXPORT_ALIAS(cmd_chinese_test, test_chinese,
					   Test Chinese output : test_chinese);
SHELL_CMD_EXPORT_ALIAS(
	cmd_curl, curl, "Simple curl: curl <URL> [-H H:V] [-d 'data'] [timeout]");
SHELL_CMD_EXPORT_ALIAS(cmd_simpleopenapi_chat, chat,
					   Send message to Chat API : chat<message>);
SHELL_CMD_EXPORT_ALIAS(cmd_audio_start, audio_start,
					   Initialize and start ES8388 audio capture);
SHELL_CMD_EXPORT_ALIAS(cmd_audio_stop, audio_stop,
					   Stop ES8388 audio capture and deinitialize);
SHELL_CMD_EXPORT_ALIAS(cmd_audio_get_data, audio_get, Get a chunk of captured audio data (for testing));
SHELL_CMD_EXPORT_ALIAS(cmd_send_audio_lan, send_audio_lan,
					   Send audio to LAN.Usage : send_audio_lan[num_chunks]);
SHELL_CMD_EXPORT_ALIAS(cmd_robot_start, robot_start,
					   Start the main conversation loop of the robot);
#endif