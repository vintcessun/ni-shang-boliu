#include "wifi.h"
#include "bl_fw_api.h"
#include "wifi_mgmr_ext.h"
#include "bflb_irq.h"
#include "bl616_glb.h"
#include "rfparam_adapter.h"
#include "board.h"
#include "log.h"
#include "FreeRTOS.h"
#include "task.h"
#include "lwip/sockets.h"

#define DBG_TAG "WIFI"

static TaskHandle_t wifi_fw_task;
static wifi_event_cb_t user_event_cb = NULL;

static wifi_conf_t conf = {
    .country_code = "CN",
    .sta_auto_reconnect = 1,
    .sta_auto_reconnect_timeout = 5,
    .ap_ssid_hidden = 0,
    .ap_max_conn = 4,
    .ap_channel = 6,
    .ap_bandwidth = WIFI_BW_HT20
};

static void wifi_event_handler(uint32_t code)
{
    if (user_event_cb) {
        user_event_cb(code);
    }
    
    switch (code) {
        case CODE_WIFI_ON_INIT_DONE:
            LOG_I("[WIFI] Init done");
            wifi_mgmr_init(&conf);
            break;
        case CODE_WIFI_ON_MGMR_DONE:
            LOG_I("[WIFI] Manager ready");
            break;
        case CODE_WIFI_ON_CONNECTED:
            LOG_I("[WIFI] Connected");
            break;
        case CODE_WIFI_ON_GOT_IP:
            LOG_I("[WIFI] Got IP");
            break;
        case CODE_WIFI_ON_DISCONNECT:
            LOG_I("[WIFI] Disconnected");
            break;
        default:
            LOG_I("[WIFI] Unknown event: %lu", code);
    }
}

int wifi_init(wifi_event_cb_t event_cb)
{
    LOG_I("Initializing WiFi...");
    
    user_event_cb = event_cb;

    /* Enable WiFi clock */
    GLB_PER_Clock_UnGate(GLB_AHB_CLOCK_IP_WIFI_PHY | 
                        GLB_AHB_CLOCK_IP_WIFI_MAC_PHY | 
                        GLB_AHB_CLOCK_IP_WIFI_PLATFORM);
    GLB_AHB_MCU_Software_Reset(GLB_AHB_MCU_SW_WIFI);

    /* Set EM Size */
    GLB_Set_EM_Sel(GLB_WRAM160KB_EM0KB);

    if (rfparam_init(0, NULL, 0) != 0) {
        LOG_E("RF init failed");
        return -1;
    }

    /* Enable WiFi IRQ */
    extern void interrupt0_handler(void);
    bflb_irq_attach(WIFI_IRQn, (irq_callback)interrupt0_handler, NULL);
    bflb_irq_enable(WIFI_IRQn);

    /* Create WiFi task */
    xTaskCreate(wifi_main, "WiFi", WIFI_STACK_SIZE, NULL, WIFI_TASK_PRIORITY, &wifi_fw_task);

    /* Initialize TCP/IP stack */
    tcpip_init(NULL, NULL);

    return 0;
}

int wifi_connect(const char *ssid, const char *passwd)
{
    wifi_interface_t wifi_interface = wifi_mgmr_sta_enable();
    return wifi_mgmr_sta_connect(wifi_interface, ssid, passwd, NULL, NULL, 0, 0);
}

int wifi_disconnect(void)
{
    return wifi_mgmr_sta_disconnect();
}

int wifi_send_game_data(const char *server_ip, uint16_t port, const char *data)
{
    struct sockaddr_in server_addr;
    int sockfd;
    int ret;

    /* Create socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        LOG_E("Socket creation failed");
        return -1;
    }

    /* Configure server address */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(server_ip);

    /* Connect to server */
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        LOG_E("Connection failed");
        close(sockfd);
        return -1;
    }

    /* Send game data */
    ret = send(sockfd, data, strlen(data), 0);
    if (ret < 0) {
        LOG_E("Send failed");
        close(sockfd);
        return -1;
    }

    close(sockfd);
    return ret;
}

int wifi_receive_game_data(const char *server_ip, uint16_t port, char *buffer, int max_len)
{
    struct sockaddr_in server_addr;
    int sockfd;
    int ret;

    /* Create socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        LOG_E("Socket creation failed");
        return -1;
    }

    /* Configure server address */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(server_ip);

    /* Connect to server */
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        LOG_E("Connection failed");
        close(sockfd);
        return -1;
    }

    /* Receive game data */
    ret = recv(sockfd, buffer, max_len-1, 0);
    if (ret < 0) {
        LOG_E("Receive failed");
        close(sockfd);
        return -1;
    }
    buffer[ret] = '\0';

    close(sockfd);
    return ret;
}
