/* esp_timer (high resolution timer) example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "sdkconfig.h"
#include "driver/gpio.h"

#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "event_source.h"
#include "esp_event_base.h"

typedef struct timer_GPIO_item{
    int gpio;
    uint8_t level;
    uint64_t measurement; // 0 for no measurement, set to 1 to get a measurement
}timer_GPIO_item;

static void oneshot_timer_callback(void* arg);

static const char* TAG = "timers";

int creation = 12;

void app_main(void)
{
    /* Create two timers:
     * 1. a periodic timer which will run every 0.5s, and print a message
     * 2. a one-shot timer which will fire after 5s, and re-start periodic
     *    timer with period of 1s.
     */
    int64_t t_base;

    timer_GPIO_item timer_item_1 = {GPIO_NUM_1, 0, 0};
    timer_GPIO_item timer_item_2 = {GPIO_NUM_2, 0, 0};
    timer_GPIO_item timer_item_3 = {GPIO_NUM_3, 0, 0};

    ESP_LOGI(TAG, "measurement: %lld", timer_item_1.measurement);

    const esp_timer_create_args_t oneshot_timer_args_1 = {
            .callback = &oneshot_timer_callback,
            /* argument specified here will be passed to timer callback function */
            .name = "one-shot_1",
            .arg = (void*)&timer_item_1,
            .dispatch_method = ESP_TIMER_ISR
    };

    const esp_timer_create_args_t oneshot_timer_args_2 = {
            .callback = &oneshot_timer_callback,
            /* argument specified here will be passed to timer callback function */
            .name = "one-shot_2",
            .arg = (void*)&timer_item_2,
            .dispatch_method = ESP_TIMER_ISR
    };

    const esp_timer_create_args_t oneshot_timer_args_3 = {
            .callback = &oneshot_timer_callback,
            /* argument specified here will be passed to timer callback function */
            .name = "one-shot_3",
            .arg = (void*)&timer_item_3,
            .dispatch_method = ESP_TIMER_ISR
    };

    esp_timer_handle_t oneshot_timer_1;
    esp_timer_handle_t oneshot_timer_2;
    esp_timer_handle_t oneshot_timer_3;

    ESP_ERROR_CHECK(esp_timer_create(&oneshot_timer_args_1, &oneshot_timer_1));
    ESP_ERROR_CHECK(esp_timer_create(&oneshot_timer_args_2, &oneshot_timer_2));
    ESP_ERROR_CHECK(esp_timer_create(&oneshot_timer_args_3, &oneshot_timer_3));

    gpio_set_direction(timer_item_1.gpio, GPIO_MODE_OUTPUT);
    gpio_set_direction(timer_item_2.gpio, GPIO_MODE_OUTPUT);
    gpio_set_direction(timer_item_3.gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(timer_item_1.gpio, 0);
    gpio_set_level(timer_item_2.gpio, 0);
    gpio_set_level(timer_item_3.gpio, 0);

    usleep(400);
    // temp = 4996
    /* Start the timers */
    ESP_ERROR_CHECK(esp_timer_start_once(oneshot_timer_1, 60000));
    t_base = esp_timer_get_time();
    usleep(1500);
    ESP_ERROR_CHECK(esp_timer_start_once(oneshot_timer_2, 60000-(esp_timer_get_time()-t_base) - creation + 200));
    usleep(1500);
    ESP_ERROR_CHECK(esp_timer_start_once(oneshot_timer_3, 60000-(esp_timer_get_time()-t_base) - creation + 400));

    usleep(200000);
    // ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    ESP_ERROR_CHECK(esp_timer_delete(oneshot_timer_1));
    ESP_ERROR_CHECK(esp_timer_delete(oneshot_timer_2));
    ESP_ERROR_CHECK(esp_timer_delete(oneshot_timer_3));


    ESP_LOGI(TAG, "measurement: %lld", timer_item_1.measurement);

    ESP_LOGI(TAG, "difference1: %lld\ndifference2: %lld", (timer_item_3.measurement - timer_item_2.measurement), (timer_item_2.measurement - timer_item_3.measurement));
    ESP_LOGI(TAG, "difference1: %lld\ndifference2: %lld\ndifference3: %lld", (timer_item_1.measurement-t_base), (timer_item_2.measurement-t_base), (timer_item_3.measurement-t_base));

    ESP_LOGI(TAG, "Stopped and deleted timers");
}

static void oneshot_timer_callback(void* arg)
{
    uint64_t t_cb = esp_timer_get_time();
    timer_GPIO_item* e = (timer_GPIO_item*)arg;
    gpio_set_level(e->gpio, e->level);
    e->measurement = t_cb;
    // ESP_LOGI(TAG, "%d", &arg);

    /* To start the timer which is running, need to stop it first */
}
