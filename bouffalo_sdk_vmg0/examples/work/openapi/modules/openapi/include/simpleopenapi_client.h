#ifndef SIMPLEOPENAPI_CLIENT_H
#define SIMPLEOPENAPI_CLIENT_H

#include <stdint.h>
#include <stddef.h>

/**
 * @brief Asynchronously sends a chat message to the SimpleOpenAPI.
 *
 * This function formats the request, creates a background task to send it
 * using the 'requests' module, and returns immediately. The result will
 * be printed to the log/console by the background task.
 *
 * @param user_message The message content from the user.
 * @return int 0 on success (task created), negative error code on failure
 * (e.g., memory allocation error, task creation error).
 */
int simple_openapi_chat_async(const char *user_message);
static void simple_openapi_chat_task(void *pvParameters);
#endif // SIMPLEOPENAPI_CLIENT_H