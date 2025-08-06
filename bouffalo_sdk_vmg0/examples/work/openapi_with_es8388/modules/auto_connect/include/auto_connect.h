#ifndef AUTO_CONNECT_H
#define AUTO_CONNECT_H

/**
 * @brief Initializes the auto-connect task.
 *
 * This function sets up the necessary resources for the Wi-Fi auto-connection feature.
 *
 * @param ssid The SSID of the target Wi-Fi network.
 * @param key The password/key for the target Wi-Fi network.
 */
void auto_connect_init(const char *ssid, const char *key);


/**
 * @brief Signals that the Wi-Fi system is initialized and ready.
 *
 * This function should be called (typically from the Wi-Fi event handler)
 * to notify the auto-connect task that it can proceed with connection attempts.
 */
void auto_connect_signal_wifi_ready(void);

#endif // AUTO_CONNECT_H