// =================================================================================================
// =================================================================================================
//
//              unified_gateway_client.c - FINAL ARCHITECTURE - THE PLAYER TASK IS KING
//
//                        (This is the one. It has to be.)
//
// =================================================================================================
// =================================================================================================

#include "unified_gateway_client.h"
#include "FreeRTOS.h"
#include "task.h"
#include "bflb_mtimer.h"
#include "es8388_driver.h"
#include "log.h"
#include "mqtt.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <lwip/errno.h>
#include <netdb.h>
#include <queue.h>

#define MQTT_BROKER_HOSTNAME    "140.143.223.43"
#define MQTT_BROKER_PORT        "1883"
#define AUDIO_CHUNK_SIZE        (6400)
#define SESSION_TASK_STACK_SIZE (4096 * 2)
#define SESSION_TASK_PRIORITY   10
#define MQTT_SENDBUF_SIZE       (AUDIO_CHUNK_SIZE * 4)
#define MQTT_RECVBUF_SIZE       (4096 + 512)

static QueueHandle_t mqtt_publish_queue = NULL;
static QueueHandle_t audio_play_queue = NULL;

typedef struct {
    uint8_t *data;
    uint32_t len;
} audio_chunk_t;

// ★★★ SPECIAL COMMANDS for the player task ★★★
#define PLAYER_CMD_PURGE   ((uint8_t *)0x01)
#define PLAYER_CMD_SILENCE ((uint8_t *)0x02)

static uint8_t sendbuf[MQTT_SENDBUF_SIZE];
static uint8_t recvbuf[MQTT_RECVBUF_SIZE];

static struct mqtt_client client;
static TaskHandle_t client_daemon_handle = NULL;
static TaskHandle_t audio_player_handle = NULL;
static int g_sockfd = -1;
static volatile bool g_is_session_active = false;
static volatile bool g_stop_publishing = false;

typedef enum {
    STATE_RECORDING,
    STATE_PLAYING
} robot_state_t;

static volatile robot_state_t g_robot_state = STATE_RECORDING;

static int open_nb_socket(const char *addr, const char *port)
{
    struct addrinfo hints = { 0 };
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    int sockfd = -1, rv;
    struct addrinfo *p, *servinfo;

    if ((rv = getaddrinfo(addr, port, &hints, &servinfo)) != 0)
        return -1;
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
            continue;
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            sockfd = -1;
            continue;
        }
        break;
    }
    freeaddrinfo(servinfo);
    if (sockfd != -1) {
        int iMode = 1;
        ioctlsocket(sockfd, FIONBIO, &iMode);
    }
    return sockfd;
}

// =================================================================================================
// ★★★★★★★★★★★★★★★★★ 1. THE PLAYER TASK - NOW WITH COMMAND HANDLING ★★★★★★★★★★★★★★★★
// =================================================================================================
static void audio_player_task(void *pvParameters)
{
    LOG_I("[PlayerTask] Audio Player Task started and owns the audio driver.\r\n");
    audio_chunk_t received_item;

    while (1) {
        if (xQueueReceive(audio_play_queue, &received_item, portMAX_DELAY) == pdPASS) {
            // Check if the received item is a command or audio data
            if (received_item.data == PLAYER_CMD_PURGE) {
                // Command to purge all pending audio data in the queue
                LOG_I("[PlayerTask] Received PURGE command. Clearing my own queue...\r\n");
                audio_chunk_t stale_chunk;
                while (xQueueReceive(audio_play_queue, &stale_chunk, (TickType_t)0) == pdPASS) {
                    if (stale_chunk.data != NULL && stale_chunk.data != PLAYER_CMD_PURGE && stale_chunk.data != PLAYER_CMD_SILENCE) {
                        free(stale_chunk.data);
                    }
                }
                LOG_I("[PlayerTask] Queue purged.\r\n");

            } else if (received_item.data == PLAYER_CMD_SILENCE) {
                // Command to fill DMA buffers with silence
                LOG_I("[PlayerTask] Received SILENCE command. Filling DMA with silence...\r\n");
                es8388_audio_fill_silence();

            } else if (received_item.data != NULL) {
                // It's a regular audio chunk
                int play_ret = es8388_audio_play(received_item.data, received_item.len);
                if (play_ret != 0) {
                    printf("[PlayerTask] Error: es8388_audio_play failed with code %d.\r\n", play_ret);
                }
                free(received_item.data); // Free the memory after playing
            }
        }
    }
}

// =================================================================================================
// ★★★★★★★★★★★★★★★ 2. THE CALLBACK - NOW SENDS COMMANDS, NOT CALLS ★★★★★★★★★★★★★★
// =================================================================================================
static void incoming_publish_callback(void **unused, struct mqtt_response_publish *published)
{
    char topic_buf[128];
    int topic_len = published->topic_name_size > sizeof(topic_buf) - 1 ? sizeof(topic_buf) - 1 : published->topic_name_size;
    memcpy(topic_buf, published->topic_name, topic_len);
    topic_buf[topic_len] = '\0';

    if (strstr(topic_buf, "/audio/response/") != NULL) {
        uint8_t *audio_data_copy = (uint8_t *)malloc(published->application_message_size);
        if (audio_data_copy == NULL) {
            LOG_E("[Callback] Failed to malloc for audio chunk!\r\n");
            return;
        }
        memcpy(audio_data_copy, published->application_message, published->application_message_size);
        audio_chunk_t chunk_to_play = { .data = audio_data_copy, .len = published->application_message_size };
        if (xQueueSend(audio_play_queue, &chunk_to_play, (TickType_t)0) != pdPASS) {
            LOG_W("[Callback] Audio play queue is full, discarding chunk.\r\n");
            free(audio_data_copy);
        }

    } else if (strstr(topic_buf, "/control/") != NULL) {
        char *msg = (char *)published->application_message;
        int msg_len = published->application_message_size;

        if (strncmp(msg, "{\"action\":\"prepare_to_play\"}", msg_len) == 0) {
            printf("[CONTROL] Received 'prepare_to_play'. Sending PURGE command and switching state.\r\n");
            audio_chunk_t cmd = { .data = PLAYER_CMD_PURGE, .len = 0 };
            xQueueSendToFront(audio_play_queue, &cmd, (TickType_t)0); // Send command with high priority
            g_robot_state = STATE_PLAYING;

        } else if (strncmp(msg, "{\"action\":\"play_finished_go_ahead\"}", msg_len) == 0) {
            printf("[CONTROL] Received 'play_finished_go_ahead'. Sending SILENCE command and switching state.\r\n");
            audio_chunk_t cmd = { .data = PLAYER_CMD_SILENCE, .len = 0 };
            xQueueSend(audio_play_queue, &cmd, (TickType_t)0); // Send command with normal priority
            g_robot_state = STATE_RECORDING;

        } else if (strncmp(msg, "stop", msg_len) == 0) {
            printf("[MQTT] 'stop' command received. Shutting down.\r\n");
            g_stop_publishing = true;
        }

    } else if (strstr(topic_buf, "/result/") != NULL) {
        printf("\r\n================ FINAL RESPONSE (TEXT) ================\r\n");
        printf("%.*s\r\n", (int)published->application_message_size, (char *)published->application_message);
        printf("=====================================================\r\n\r\n");
    }
}

// =================================================================================================
// ★★★★★★★★★★★★★★★★★★ 3. THE NETWORK TASK - UNCHANGED, ROBUST ★★★★★★★★★★★★★★★★★★
// =================================================================================================
static void client_refresher(void *arg)
{
    const char *topic_audio = (const char *)arg;
    audio_chunk_t chunk_to_publish;
    bool has_pending_chunk = false;
    uint32_t congestion_count = 0;
    const TickType_t base_delay = pdMS_TO_TICKS(10);
    while (1) {
        if (!has_pending_chunk) {
            if (xQueueReceive(mqtt_publish_queue, &chunk_to_publish, pdMS_TO_TICKS(5)) == pdPASS) {
                has_pending_chunk = true;
            }
        }
        if (has_pending_chunk) {
            if (client.error == MQTT_OK) {
                mqtt_publish(&client, topic_audio, chunk_to_publish.data, chunk_to_publish.len, MQTT_PUBLISH_QOS_0);
            }
        }
        mqtt_sync(&client);
        if (client.error != MQTT_OK) {
            if (client.error == MQTT_ERROR_SEND_BUFFER_IS_FULL) {
                LOG_W("[Refresher] Network congested. Will retry automatically.\r\n");
                client.error = MQTT_OK;
                congestion_count++;
            } else {
                LOG_E("[Refresher] Unrecoverable MQTT error: %s. Terminating session.\r\n", mqtt_error_str(client.error));
                g_stop_publishing = true;
                if (has_pending_chunk) {
                    free(chunk_to_publish.data);
                }
                break;
            }
        } else {
            if (congestion_count > 0) {
                LOG_I("[Refresher] Network congestion cleared.\r\n");
                congestion_count = 0;
            }
            if (has_pending_chunk) {
                free(chunk_to_publish.data);
                has_pending_chunk = false;
            }
        }
        TickType_t dynamic_delay = base_delay;
        if (congestion_count > 0) {
            uint32_t backoff_delay_ms = (congestion_count < 10) ? (congestion_count * 20) : 200;
            dynamic_delay = pdMS_TO_TICKS(backoff_delay_ms);
        }
        vTaskDelay(dynamic_delay);
    }
    printf("[Refresher] Task finished.\r\n");
    client_daemon_handle = NULL;
    vTaskDelete(NULL);
}

// =================================================================================================
// ★★★★★★★★★★★ 4. THE MAIN SESSION TASK - NOW SIMPLER, NO MORE PURGING ★★★★★★★★★★★
// =================================================================================================
static void robot_session_task(void *pvParameters)
{
    char session_id[24] = { 0 };
    char topic_audio[64], topic_control[64], topic_result[64], topic_audio_response[64];

    mqtt_publish_queue = xQueueCreate(5, sizeof(audio_chunk_t));
    if (mqtt_publish_queue == NULL) {
        printf("[MQTT] Failed to create publish queue.\r\n");
        goto cleanup_no_tasks;
    }
    audio_play_queue = xQueueCreate(15, sizeof(audio_chunk_t)); // Increased size slightly
    if (audio_play_queue == NULL) {
        printf("[MQTT] Failed to create audio play queue.\r\n");
        goto cleanup;
    }
    if (xTaskCreate(audio_player_task, "audio_player", 4096, NULL, SESSION_TASK_PRIORITY + 1, &audio_player_handle) != pdPASS) {
        printf("[MQTT] Failed to create audio_player_task.\r\n");
        goto cleanup;
    }
    g_sockfd = open_nb_socket(MQTT_BROKER_HOSTNAME, MQTT_BROKER_PORT);
    if (g_sockfd < 0) {
        printf("[MQTT] Failed to open socket.\r\n");
        goto cleanup_task;
    }
    mqtt_init(&client, g_sockfd, sendbuf, sizeof(sendbuf), recvbuf, sizeof(recvbuf), incoming_publish_callback);
    g_robot_state = STATE_RECORDING;
    snprintf(session_id, sizeof(session_id), "%llu", bflb_mtimer_get_time_us());
    char client_id[32];
    snprintf(client_id, sizeof(client_id), "bl618-robot-%s", session_id);
    mqtt_connect(&client, client_id, NULL, NULL, 0, NULL, NULL, MQTT_CONNECT_CLEAN_SESSION, 400);
    snprintf(topic_audio, sizeof(topic_audio), "robot/audio/stream/%s", session_id);
    if (xTaskCreate(client_refresher, "mqtt_network", 8192, (void *)topic_audio, SESSION_TASK_PRIORITY, &client_daemon_handle) != pdPASS) {
        printf("[MQTT] Failed to create client_refresher task.\r\n");
        goto cleanup_task;
    }
    printf("[MQTT] Waiting for connection to be established by background task...\r\n");
    int connect_timeout_ms = 10000;
    while (client.error != MQTT_OK && connect_timeout_ms > 0) {
        if (g_stop_publishing) {
            printf("[MQTT] Connection failed because background task exited.\r\n");
            goto cleanup_task;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
        connect_timeout_ms -= 100;
    }
    if (client.error != MQTT_OK) {
        printf("[MQTT] Connection failed after waiting: %s\r\n", mqtt_error_str(client.error));
        goto cleanup_task;
    }
    printf("[MQTT] Connection established! Subscribing to topics...\r\n");
    snprintf(topic_control, sizeof(topic_control), "robot/control/%s", session_id);
    snprintf(topic_result, sizeof(topic_result), "robot/result/%s", session_id);
    snprintf(topic_audio_response, sizeof(topic_audio_response), "robot/audio/response/%s", session_id);
    mqtt_subscribe(&client, topic_control, 0);
    mqtt_subscribe(&client, topic_result, 0);
    mqtt_subscribe(&client, topic_audio_response, 0);
    printf("[MQTT] Ready! Starting perpetual audio stream...\r\n");
    g_stop_publishing = false;

    // The producer loop is now extremely simple. It doesn't need to know about purging.
    while (!g_stop_publishing) {
        if (g_robot_state == STATE_RECORDING) {
            uint8_t *audio_buffer = (uint8_t *)malloc(AUDIO_CHUNK_SIZE);
            if (audio_buffer == NULL) {
                LOG_W("Failed to alloc audio buffer, retrying...\r\n");
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }
            uint32_t fetched_len = 0;
            int audio_ret = es8388_audio_get_data(audio_buffer, AUDIO_CHUNK_SIZE, &fetched_len, portMAX_DELAY);
            if (audio_ret == 0 && fetched_len > 0) {
                audio_chunk_t chunk = { .data = audio_buffer, .len = fetched_len };
                if (xQueueSend(mqtt_publish_queue, &chunk, pdMS_TO_TICKS(100)) != pdPASS) {
                    LOG_W("MQTT publish queue is full. Discarding audio packet.\r\n");
                    free(audio_buffer);
                }
            } else {
                free(audio_buffer);
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }

cleanup_task:
    if (client_daemon_handle)
        vTaskDelete(client_daemon_handle);
    if (audio_player_handle)
        vTaskDelete(audio_player_handle);
cleanup:
    if (g_sockfd != -1)
        close(g_sockfd);
    if (mqtt_publish_queue) {
        audio_chunk_t leftover_chunk;
        while (xQueueReceive(mqtt_publish_queue, &leftover_chunk, (TickType_t)0) == pdPASS)
            free(leftover_chunk.data);
        vQueueDelete(mqtt_publish_queue);
    }
    if (audio_play_queue) {
        audio_chunk_t leftover_chunk;
        while (xQueueReceive(audio_play_queue, &leftover_chunk, (TickType_t)0) == pdPASS) {
            if (leftover_chunk.data != NULL && leftover_chunk.data != PLAYER_CMD_PURGE && leftover_chunk.data != PLAYER_CMD_SILENCE) {
                free(leftover_chunk.data);
            }
        }
        vQueueDelete(audio_play_queue);
    }
cleanup_no_tasks:
    g_is_session_active = false;
    printf("[MQTT] Task finished and cleaned up.\r\n");
    vTaskDelete(NULL);
}

void robot_session_start(void)
{
    if (g_is_session_active) {
        printf("[Launcher] A session is already active.\r\n");
        return;
    }
    if (xTaskCreate(robot_session_task, "robot_session", SESSION_TASK_STACK_SIZE, NULL, SESSION_TASK_PRIORITY, NULL) != pdPASS) {
        printf("[Launcher] Failed to create robot_session_task.\r\n");
    } else {
        g_is_session_active = true;
    }
}

#ifdef CONFIG_SHELL
#include <shell.h>

int cmd_test_player(int argc, char **argv)
{
    if (!g_is_session_active || client.error != MQTT_OK) {
        printf("Error: Main session not started or not connected. Please run 'robot_start' first.\r\n");
        return -1;
    }

    const char *test_topic = "robot/audio/test_playback";
    printf("Subscribing to '%s' using the main MQTT client...\r\n", test_topic);
    int ret = mqtt_subscribe(&client, test_topic, 0);

    if (ret != MQTT_OK) {
        printf("Error: Failed to subscribe to test topic. MQTT error: %s\r\n", mqtt_error_str(client.error));
        return -1;
    }

    printf("Successfully subscribed to '%s'.\r\n", test_topic);
    printf("On your PC, run: mosquitto_pub -h your_broker_ip -t '%s' -f your_audio_file.pcm\r\n", test_topic);

    return 0;
}

SHELL_CMD_EXPORT_ALIAS(cmd_test_player, test_player, Test audio playback by hijacking main connection);

#endif // CONFIG_SHELL