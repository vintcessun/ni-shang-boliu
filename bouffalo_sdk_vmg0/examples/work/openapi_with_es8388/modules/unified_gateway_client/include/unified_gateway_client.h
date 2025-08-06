#ifndef UNIFIED_GATEWAY_CLIENT_H
#define UNIFIED_GATEWAY_CLIENT_H

/**
 * @brief 启动一个全新的机器人MQTT会话任务。
 * @note 调用此函数前，请确保Wi-Fi已连接，并且音频采集已启动。
 */
void robot_session_start(void);

#endif // UNIFIED_GATEWAY_CLIENT_H