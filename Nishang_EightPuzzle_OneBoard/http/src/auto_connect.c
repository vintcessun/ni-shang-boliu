#include "auto_connect.h"  // 包含自己的头文件

#include "FreeRTOS.h"		// FreeRTOS 内核
#include "log.h"			// 日志库
#include "semphr.h"			// FreeRTOS 信号量
#include "string.h"			// 字符串操作
#include "task.h"			// FreeRTOS 任务管理
#include "wifi_mgmr.h"		// WiFi 管理器
#include "wifi_mgmr_ext.h"	// WiFi 管理器扩展

// --- 配置 ---

#define TASK_NAME "auto_conn_327"			   // 任务名
#define TASK_STACK_SIZE (1024 * 2)			   // 任务堆栈大小 (保守给大一点)
#define TASK_PRIORITY (tskIDLE_PRIORITY + 16)  // 任务优先级 (比 IDLE 高即可)

// --- 内部变量 ---
// 用于同步的二进制信号量：等待 WiFi 初始化完成
static SemaphoreHandle_t wifi_ready_sem = NULL;
// 使用静态数组存储 SSID 和 Key，确保任务运行时它们是有效的
static char g_ssid[MGMR_SSID_LEN + 1] = {0};  // 根据 wifi_mgmr_ext.h 定义的大小
static char g_key[MGMR_KEY_LEN + 1] = {0};	  // 根据 wifi_mgmr_ext.h 定义的大小
static uint8_t g_ssid_len = 0;
static uint8_t g_key_len = 0;

static void auto_connect_task(void *pvParameters) {
	LOG_I("[%s] Task started. Waiting for WiFi initialization signal...\r\n",
		  TASK_NAME);

	if (xSemaphoreTake(wifi_ready_sem, portMAX_DELAY) == pdTRUE) {
		LOG_I(
			"[%s] WiFi ready signal received. Preparing to connect to "
			"[%s]...\r\n",
			TASK_NAME, g_ssid);

		// 使能 SDK 的自动连接功能 (保持不变)
		LOG_I("[%s] Enabling WiFi Auto-Connect feature...\r\n", TASK_NAME);
		int auto_conn_res = wifi_mgmr_sta_autoconnect_enable();
		if (auto_conn_res == 0) {
			LOG_I("[%s] Auto-Connect enabled successfully.\r\n", TASK_NAME);
		} else {
			LOG_E("[%s] Failed to enable Auto-Connect, error code: %d\r\n",
				  TASK_NAME, auto_conn_res);
		}

		// --- 修改处：尝试调用 wifi_sta_connect() 而不是
		// wifi_mgmr_sta_connect() ---
		LOG_I("[%s] Sending connection request using wifi_sta_connect()...\r\n",
			  TASK_NAME);
		LOG_I("[%s] SSID: %s, Key: %s\r\n", TASK_NAME, g_ssid, g_key);
		// 调用参数更直接的函数版本
		// 对于不确定的参数，我们先尝试传递 NULL 或 0，让函数使用可能的默认值
		int result = wifi_sta_connect(
			g_ssid,	 // ssid
			g_key,	 // key (密码)
			NULL,	 // bssid - 先不指定，让它自动找
			NULL,	 // akm_str - 先不指定，让它自动协商 (通常是 WPA/WPA2)
			1,	   // pmf_cfg - PMF 通常设 0 (Disabled) 或 1 (Optional)，先用 0
			NULL,  // freq1 - 先不指定，让它扫描
			NULL,  // freq2 - 先不指定
			1	   // use_dhcp - 通常设 1 来自动获取 IP
		);
		// -------------------------------------------------------------------------

		if (result == 0) {
			LOG_I(
				"[%s] Connection request (wifi_sta_connect) sent "
				"successfully.\r\n",
				TASK_NAME);
			// 看看这次是否能像手动输入命令一样连接成功
			// (可能也需要第二次尝试，由 autoconnect 处理)
		} else {
			LOG_E(
				"[%s] Failed to send connection request (wifi_sta_connect), "
				"error code: %d\r\n",
				TASK_NAME, result);
		}
	} else {
		LOG_E("[%s] Error waiting for semaphore!\r\n", TASK_NAME);
	}

	LOG_I("[%s] Auto-connect initiated. Task will delete itself.\r\n",
		  TASK_NAME);
	vTaskDelete(NULL);
}

// --- 公共函数 ---
/**
 * @brief 初始化自动连接功能：创建信号量和任务
 */
void auto_connect_init(const char *ssid, const char *key)  // 接收 SSID 和 Key
{
	if (ssid == NULL || key == NULL) {
		LOG_E("[%s] SSID or Key is NULL during init!\r\n", TASK_NAME);
		return;
	}

	// 创建信号量 (如果还没创建)
	if (wifi_ready_sem == NULL) {
		wifi_ready_sem = xSemaphoreCreateBinary();
		if (wifi_ready_sem == NULL) {
			LOG_E("[%s] Failed to create semaphore!\r\n", TASK_NAME);
			return;
		}
	}

	// 保存 SSID 和 Key 到静态变量中
	strncpy(g_ssid, ssid, sizeof(g_ssid) - 1);
	strncpy(g_key, key, sizeof(g_key) - 1);
	g_ssid[sizeof(g_ssid) - 1] = '\0';	// 确保 null 结尾
	g_key[sizeof(g_key) - 1] = '\0';	// 确保 null 结尾
	g_ssid_len = strlen(g_ssid);
	g_key_len = strlen(g_key);

	// 创建任务
	xTaskCreate(auto_connect_task, TASK_NAME, TASK_STACK_SIZE, NULL,
				TASK_PRIORITY, NULL);
	LOG_I("[%s] Auto connect task created for SSID: %s\r\n", TASK_NAME, g_ssid);
}

/**
 * @brief 从 ISR 或其他任务中安全地发出 WiFi 就绪信号
 */
void auto_connect_signal_wifi_ready(void) {
	if (wifi_ready_sem != NULL) {
		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
		// 尝试释放信号量
		xSemaphoreGiveFromISR(wifi_ready_sem, &xHigherPriorityTaskWoken);
		// 如果需要，进行上下文切换 (通常在 FreeRTOS 的 portYIELD_FROM_ISR
		// 中处理)
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
		LOG_I("[Signal] Signaled wifi ready semaphore.\r\n");
	} else {
		LOG_E("[Signal] Semaphore handle is NULL!\r\n");
	}
}