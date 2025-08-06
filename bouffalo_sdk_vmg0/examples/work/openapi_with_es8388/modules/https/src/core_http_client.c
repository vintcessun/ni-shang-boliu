// core_http_client.c
#include "core_http_client.h"

#include "FreeRTOS.h"
#include "bflb_mtimer.h"  // For bflb_mtimer_get_time_us
#include "bl_utils.h"
#include "log.h"
#include "task.h"

#ifndef DBG_TAG
#define DBG_TAG "CORE_HTTP"
#endif

#define DEFAULT_USER_AGENT "BL-HttpClient/1.0"

/**
 * @brief 通用 HTTP 响应接收与解析
 *
 * 支持自定义接收函数，循环接收数据，处理超时、缓冲区溢出、连接关闭等情况，解析
 * HTTP 状态码与响应体。
 *
 * @param recv_func      实际读取数据的函数指针
 * @param fd             Socket 或 TLS 句柄
 * @param response_buf   响应缓冲区
 * @param response_buf_len 缓冲区大小
 * @param actual_resp_len_ptr [OUT] 实际接收的响应体长度
 * @param start_us       操作开始时间戳（微秒）
 * @param timeout_ms     超时时间（毫秒）
 * @return int           HTTP 状态码（>=200）或负数错误码
 */
static int _process_response(int (*recv_func)(int32_t fd, uint8_t *buf,
											  uint16_t len),
							 int32_t fd, uint8_t *response_buf,
							 int response_buf_len, int *actual_resp_len_ptr,
							 uint64_t start_us, int timeout_ms) {
	int ret_code = REQ_ERR_INTERNAL;
	int total_received = 0;
	*actual_resp_len_ptr = 0;

	while (1) {
		if ((bflb_mtimer_get_time_us() - start_us) / 1000 > timeout_ms) {
			if (ret_code >= 0 || ret_code == REQ_ERR_INTERNAL)
				ret_code = REQ_ERR_TIMEOUT;
			break;
		}
		int space_left = response_buf_len - 1 - total_received;
		if (space_left <= 0) {
			if (ret_code >= 0 || ret_code == REQ_ERR_INTERNAL)
				ret_code = REQ_ERR_RESP_TOO_LARGE;
			break;
		}

		uint16_t len_to_read =
			(space_left > UINT16_MAX) ? UINT16_MAX : (uint16_t)space_left;
		int read_ret =
			recv_func(fd, response_buf + total_received, len_to_read);

		if (read_ret > 0) {
			total_received += read_ret;
		} else if (read_ret == 0) {
			vTaskDelay(pdMS_TO_TICKS(50));
			continue;
		} else if (read_ret == REQ_ERR_RECV_CLOSED) {
			if (ret_code == REQ_ERR_INTERNAL) ret_code = REQ_OK;
			break;
		} else {
			if (ret_code >= 0 || ret_code == REQ_ERR_INTERNAL)
				ret_code = read_ret;
			break;
		}
	}

	if (total_received < response_buf_len) {
		response_buf[total_received] = '\0';
	} else {
		response_buf[response_buf_len - 1] = '\0';
		if (ret_code == REQ_OK || ret_code == REQ_ERR_INTERNAL) {
			ret_code = REQ_ERR_RESP_TOO_LARGE;
		}
	}
	*actual_resp_len_ptr = total_received;

	if (ret_code >= 0 &&
		(bflb_mtimer_get_time_us() - start_us) / 1000 > timeout_ms) {
		ret_code = REQ_ERR_TIMEOUT;
	}

	int http_status = -1;
	char *body_ptr = NULL;
	int resp_header_len = 0;
	int resp_body_len = 0;

	if (*actual_resp_len_ptr > 0 && ret_code >= 0) {
		char *header_end_ptr =
			memmem(response_buf, *actual_resp_len_ptr, "\r\n\r\n", 4);
		if (header_end_ptr != NULL) {
			resp_header_len = (header_end_ptr - (char *)response_buf) + 4;
			body_ptr = header_end_ptr + 4;
			resp_body_len = *actual_resp_len_ptr - resp_header_len;
			if (resp_body_len < 0) resp_body_len = 0;

			if (sscanf((char *)response_buf, "HTTP/%*d.%*d %d", &http_status) ==
				1) {
				ret_code = http_status;
			} else {
				ret_code = REQ_ERR_RESP_PARSE;
				resp_body_len = 0;
			}

			if (resp_body_len > 0) {
				memmove(response_buf, body_ptr, resp_body_len);
			}
			*actual_resp_len_ptr = resp_body_len;
			response_buf[resp_body_len] = '\0';

		} else {
			if (ret_code >= 0) ret_code = REQ_ERR_RESP_PARSE;
			*actual_resp_len_ptr = 0;
			response_buf[0] = '\0';
		}
	} else if (ret_code >= 0) {
		ret_code = REQ_ERR_RECV;
	}

	return ret_code;
}

/**
 * @brief 构建 HTTP 请求头字符串
 *
 * 自动拼接标准 HTTP/1.1 请求头，包括
 * Host、Connection、User-Agent、Content-Type、Content-Length 及自定义头部。
 *
 * @param method         HTTP 方法
 * @param path           请求路径
 * @param server         服务器域名或IP
 * @param content_type   Content-Type 字符串
 * @param body_len       请求体长度
 * @param custom_headers 额外自定义头部字符串
 * @param buffer         输出缓冲区
 * @param buffer_size    缓冲区大小
 * @return int           实际写入的头部长度，或负数错误码
 */
int _build_http_request_header(const char *method, const char *path,
							   const char *server, const char *content_type,
							   int body_len, const char *custom_headers,
							   char *buffer, int buffer_size) {
	int written_len = 0;
	int ret;

	if (!method || !path || !server || !buffer || buffer_size <= 0) {
		return REQ_ERR_BAD_REQUEST;
	}
	bool is_post_like =
		(strcmp(method, "POST") == 0 || strcmp(method, "PUT") == 0);
	if (is_post_like && (!content_type || body_len < 0)) {
		return REQ_ERR_BAD_REQUEST;
	}

	memset(buffer, 0, buffer_size);

	ret = snprintf(buffer, buffer_size, "%s %s HTTP/1.1\r\n", method, path);
	if (ret < 0 || ret >= buffer_size) {
		return REQ_ERR_BAD_REQUEST;
	}
	written_len += ret;

	ret = snprintf(buffer + written_len, buffer_size - written_len,
				   "Host: %s\r\n", server);
	if (ret < 0 || ret >= (buffer_size - written_len)) {
		return REQ_ERR_BAD_REQUEST;
	}
	written_len += ret;

	ret = snprintf(buffer + written_len, buffer_size - written_len,
				   "Connection: Keep-Alive\r\n");
	if (ret < 0 || ret >= (buffer_size - written_len)) {
		return REQ_ERR_BAD_REQUEST;
	}
	written_len += ret;

	ret = snprintf(buffer + written_len, buffer_size - written_len,
				   "User-Agent: %s\r\n", DEFAULT_USER_AGENT);
	if (ret < 0 || ret >= (buffer_size - written_len)) {
		return REQ_ERR_BAD_REQUEST;
	}
	written_len += ret;

	if (is_post_like) {
		ret = snprintf(buffer + written_len, buffer_size - written_len,
					   "Content-Type: %s\r\n", content_type);
		if (ret < 0 || ret >= (buffer_size - written_len)) {
			return REQ_ERR_BAD_REQUEST;
		}
		written_len += ret;

		ret = snprintf(buffer + written_len, buffer_size - written_len,
					   "Content-Length: %d\r\n", body_len);
		if (ret < 0 || ret >= (buffer_size - written_len)) {
			return REQ_ERR_BAD_REQUEST;
		}
		written_len += ret;
	}

	if (custom_headers && strlen(custom_headers) > 0) {
		size_t custom_len = strlen(custom_headers);
		if (written_len + custom_len < buffer_size) {
			strcat(buffer + written_len, custom_headers);
			written_len += custom_len;
		} else {
			return REQ_ERR_BAD_REQUEST;
		}
	}

	if (written_len + 2 < buffer_size) {
		strcat(buffer + written_len, "\r\n");
		written_len += 2;
	} else {
		return REQ_ERR_BAD_REQUEST;
	}

	return written_len;
}

/**
 * @brief 辅助发送函数，处理发送循环与超时
 *
 * @param fd        连接句柄
 * @param send_func 实际发送函数指针
 * @param data      待发送数据
 * @param len       数据长度
 * @param start_us  操作开始时间戳（微秒）
 * @param timeout_ms 超时时间（毫秒）
 * @return int      发送结果（REQ_OK 或负数错误码）
 */
static int _send_data_internal(int32_t fd,
							   int (*send_func)(int32_t fd, const uint8_t *data,
												size_t len, int flags),
							   const uint8_t *data, int len, uint64_t start_us,
							   int timeout_ms) {
	int bytes_sent_total = 0;
	while (bytes_sent_total < len) {
		if ((bflb_mtimer_get_time_us() - start_us) / 1000 > timeout_ms) {
			return REQ_ERR_TIMEOUT;
		}

		int remaining_len = len - bytes_sent_total;
		int send_ret = send_func(fd, data + bytes_sent_total, remaining_len, 0);

		if (send_ret > 0) {
			bytes_sent_total += send_ret;
		} else if (send_ret == 0) {
			vTaskDelay(pdMS_TO_TICKS(50));
			continue;
		} else {
			return send_ret;
		}
	}
	return REQ_OK;
}

/**
 * @brief HTTP 请求主流程
 *
 * 封装 HTTP
 * 请求的完整流程，包括连接、请求头构建与发送、请求体发送、响应接收与解析、资源清理等。
 *
 * @param config HTTP 请求配置
 * @param ops    协议操作函数集
 * @return int   HTTP 状态码（>=200）或负数错误码
 */
int _http_request_internal(const http_request_config_t *config,
						   const http_proto_ops_t *ops) {
	int ret_code = REQ_ERR_INTERNAL;
	int32_t fd = -1;
	char *request_header_buf = NULL;
	int header_len = 0;
	uint64_t start_us = bflb_mtimer_get_time_us();

	if (!config || !ops || !config->server || !config->path ||
		!config->method || !config->response_buf ||
		config->response_buf_len <= 0 || !config->actual_resp_len_ptr ||
		!ops->connect || !ops->send || !ops->recv || !ops->disconnect) {
		return REQ_ERR_BAD_REQUEST;
	}
	*config->actual_resp_len_ptr = 0;
	config->response_buf[0] = '\0';

	int connect_status =
		ops->connect(config->server, config->port, config->timeout_ms, &fd);
	if (connect_status != REQ_OK) {
		ret_code = connect_status;
		fd = -1;
		goto cleanup_internal;
	}

	if ((bflb_mtimer_get_time_us() - start_us) / 1000 > config->timeout_ms) {
		ret_code = REQ_ERR_TIMEOUT;
		goto cleanup_internal;
	}

	const int MAX_HEADER_BUF_SIZE = 1024;
	request_header_buf = (char *)malloc(MAX_HEADER_BUF_SIZE);
	if (!request_header_buf) {
		ret_code = REQ_ERR_MEM;
		goto cleanup_internal;
	}

	header_len = _build_http_request_header(
		config->method, config->path, config->server, config->content_type,
		config->body_len, config->custom_headers, request_header_buf,
		MAX_HEADER_BUF_SIZE);

	if (header_len < 0) {
		ret_code = header_len;
		goto cleanup_internal;
	}

	if ((bflb_mtimer_get_time_us() - start_us) / 1000 > config->timeout_ms) {
		ret_code = REQ_ERR_TIMEOUT;
		goto cleanup_internal;
	}

	ret_code =
		_send_data_internal(fd, ops->send, (const uint8_t *)request_header_buf,
							header_len, start_us, config->timeout_ms);

	free(request_header_buf);
	request_header_buf = NULL;

	if (ret_code != REQ_OK) {
		goto cleanup_internal;
	}

	if (config->request_body && config->body_len > 0) {
		ret_code = _send_data_internal(
			fd, ops->send, (const uint8_t *)config->request_body,
			config->body_len, start_us, config->timeout_ms);
		if (ret_code != REQ_OK) {
			goto cleanup_internal;
		}
	}

	ret_code = _process_response(
		ops->recv, fd, config->response_buf, config->response_buf_len,
		config->actual_resp_len_ptr, start_us, config->timeout_ms);

cleanup_internal:
	if (request_header_buf) {
		free(request_header_buf);
		request_header_buf = NULL;
	}
	if (fd != -1) {
		ops->disconnect(fd);
	}

	return ret_code;
}
