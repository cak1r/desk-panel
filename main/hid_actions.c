#include "hid_actions.h"
#include "usb_hid.h"

#include "class/hid/hid.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include <string.h>

#define QUEUE_LEN          8
#define TEXT_MAX           128
#define RUN_DIALOG_WAIT_MS 450

typedef enum {
    ACTION_KEY,
    ACTION_CONSUMER,
    ACTION_TEXT,
    ACTION_LAUNCH,
} action_type_t;

typedef struct {
    action_type_t type;
    uint8_t modifier;
    uint8_t keycode;
    uint16_t usage;
    char text[TEXT_MAX];
} action_t;

static const char *TAG = "hid_actions";
static QueueHandle_t s_queue;

static void worker(void *arg)
{
    action_t a;
    for (;;) {
        if (xQueueReceive(s_queue, &a, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        switch (a.type) {
        case ACTION_KEY:
            usb_hid_send_key(a.modifier, a.keycode);
            break;
        case ACTION_CONSUMER:
            usb_hid_send_consumer(a.usage);
            break;
        case ACTION_TEXT:
            usb_hid_send_text(a.text);
            break;
        case ACTION_LAUNCH:
            usb_hid_send_key(KEYBOARD_MODIFIER_LEFTGUI, HID_KEY_R);
            vTaskDelay(pdMS_TO_TICKS(RUN_DIALOG_WAIT_MS));
            usb_hid_send_text(a.text);
            usb_hid_send_key(0, HID_KEY_ENTER);
            break;
        }
    }
}

static void enqueue(const action_t *a)
{
    if (!s_queue || xQueueSend(s_queue, a, 0) != pdTRUE) {
        ESP_LOGW(TAG, "HID action dropped (queue full or not initialized)");
    }
}

void hid_actions_init(void)
{
    s_queue = xQueueCreate(QUEUE_LEN, sizeof(action_t));
    xTaskCreate(worker, "hid_actions", 3072, NULL, 4, NULL);
}

void hid_actions_key(uint8_t modifier, uint8_t keycode)
{
    action_t a = { .type = ACTION_KEY, .modifier = modifier, .keycode = keycode };
    enqueue(&a);
}

void hid_actions_consumer(uint16_t usage)
{
    action_t a = { .type = ACTION_CONSUMER, .usage = usage };
    enqueue(&a);
}

static void enqueue_text(action_type_t type, const char *text)
{
    action_t a = { .type = type };
    strncpy(a.text, text, sizeof(a.text) - 1);
    enqueue(&a);
}

void hid_actions_text(const char *text)
{
    enqueue_text(ACTION_TEXT, text);
}

void hid_actions_launch(const char *command)
{
    enqueue_text(ACTION_LAUNCH, command);
}
