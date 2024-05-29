/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdio.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_adc/adc_continuous.h"

#define EXAMPLE_ADC_UNIT                    ADC_UNIT_1
#define _EXAMPLE_ADC_UNIT_STR(unit)         #unit
#define EXAMPLE_ADC_UNIT_STR(unit)          _EXAMPLE_ADC_UNIT_STR(unit)
#define EXAMPLE_ADC_CONV_MODE               ADC_CONV_SINGLE_UNIT_1
#define EXAMPLE_ADC_ATTEN                   ADC_ATTEN_DB_0
#define EXAMPLE_ADC_BIT_WIDTH               SOC_ADC_DIGI_MAX_BITWIDTH

#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
#define EXAMPLE_ADC_OUTPUT_TYPE             ADC_DIGI_OUTPUT_FORMAT_TYPE1
#define EXAMPLE_ADC_GET_CHANNEL(p_data)     ((p_data)->type1.channel)
#define EXAMPLE_ADC_GET_DATA(p_data)        ((p_data)->type1.data)
#else
#define EXAMPLE_ADC_OUTPUT_TYPE             ADC_DIGI_OUTPUT_FORMAT_TYPE2
#define EXAMPLE_ADC_GET_CHANNEL(p_data)     ((p_data)->type2.channel)
#define EXAMPLE_ADC_GET_DATA(p_data)        ((p_data)->type2.data)
#endif

#define EXAMPLE_READ_LEN                    512

#if CONFIG_IDF_TARGET_ESP32
static adc_channel_t channel[1] = {ADC_CHANNEL_6};
#else
static adc_channel_t channel[1] = {ADC_CHANNEL_2};
#endif

static TaskHandle_t s_task_handle;
static const char *TAG = "EXAMPLE";

void int_to_4char(int num, char* buffer) {
    sprintf(buffer, "%04d", num);
}

static void format_measurements(uint8_t* int_array, size_t samples_amount){
    // Each integer will be converted to a 4-character string
    size_t char_array_size = (samples_amount+1) * 4 + 1; // +1 for the null terminator
    char* char_array = (char*)malloc(char_array_size);

    // Initialize the char array with null terminators
    memset(char_array, 0, char_array_size);

    // Pointer to traverse the char array
    char* ptr = char_array;

    char small_buffer[5];
    int_to_4char(samples_amount, small_buffer);
    strcat(ptr, small_buffer);
    ptr += 4;

    for (size_t i = 1; i < samples_amount+1; ++i) {
        // Buffer to hold the 4-character string
        char buffer[5]; // 4 characters + 1 for null terminator
        int_to_4char(int_array[i], buffer);

        // Append the 4-character string to the char array
        strcat(ptr, buffer);

        // Move the pointer forward by 4 characters
        ptr += 4;
    }
    ESP_LOGI(TAG, "%s", char_array);
}

static bool IRAM_ATTR s_conv_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data)
{
    BaseType_t mustYield = pdFALSE;
    //Notify that ADC continuous driver has done enough number of conversions
    vTaskNotifyGiveFromISR(s_task_handle, &mustYield);

    return (mustYield == pdTRUE);
}

static void continuous_adc_init(adc_channel_t *channel, uint8_t channel_num, adc_continuous_handle_t *out_handle)
{
    adc_continuous_handle_t handle = NULL;

    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = 2048,
        .conv_frame_size = EXAMPLE_READ_LEN,
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &handle));

    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = 5 * 1000,
        .conv_mode = EXAMPLE_ADC_CONV_MODE,
        .format = EXAMPLE_ADC_OUTPUT_TYPE,
    };

    adc_digi_pattern_config_t adc_pattern[SOC_ADC_PATT_LEN_MAX] = {0};
    dig_cfg.pattern_num = channel_num;
    for (int i = 0; i < channel_num; i++) {
        adc_pattern[i].atten = EXAMPLE_ADC_ATTEN;
        adc_pattern[i].channel = channel[i] & 0x7;
        adc_pattern[i].unit = EXAMPLE_ADC_UNIT;
        adc_pattern[i].bit_width = EXAMPLE_ADC_BIT_WIDTH;

        ESP_LOGI(TAG, "adc_pattern[%d].atten is :%"PRIx8, i, adc_pattern[i].atten);
        ESP_LOGI(TAG, "adc_pattern[%d].channel is :%"PRIx8, i, adc_pattern[i].channel);
        ESP_LOGI(TAG, "adc_pattern[%d].unit is :%"PRIx8, i, adc_pattern[i].unit);
    }
    dig_cfg.adc_pattern = adc_pattern;
    ESP_ERROR_CHECK(adc_continuous_config(handle, &dig_cfg));

    *out_handle = handle;
}

void app_main(void)
{
    esp_err_t ret;
    uint32_t ret_num = 0;
    uint8_t result[EXAMPLE_READ_LEN];
    memset(result, 0xcc, EXAMPLE_READ_LEN);

    int array_size = EXAMPLE_READ_LEN*2+1;
    char char_array[array_size];

    s_task_handle = xTaskGetCurrentTaskHandle();

    adc_continuous_handle_t handle = NULL;
    continuous_adc_init(channel, sizeof(channel) / sizeof(adc_channel_t), &handle);

    adc_continuous_evt_cbs_t cbs = {
        .on_conv_done = s_conv_done_cb,
    };

    ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(handle, &cbs, NULL));
    ESP_ERROR_CHECK(adc_continuous_start(handle));

    // vTaskDelay(10);

    for(int i = 0; i < 10; i++){
        // char* char_array_2 = (char*) malloc((EXAMPLE_READ_LEN*2+1) * sizeof(char));
        // memset(char_array, 0, sizeof(char_array));
        // ESP_LOGI(TAG, "test 3");

        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        // ESP_LOGI(TAG, "test 4");
        while(1){

            char* ptr = char_array;
            ret = adc_continuous_read(handle, result, EXAMPLE_READ_LEN, &ret_num, 0);
            if (ret == ESP_OK){
                ESP_LOGI("TASK", "ret is %x, ret_num is %"PRIu32" bytes", ret, ret_num);
                for (int i = 0; i < ret_num; i += SOC_ADC_DIGI_RESULT_BYTES) {

                    adc_digi_output_data_t *p = (adc_digi_output_data_t*)&result[i];
                    uint16_t data = (p)->type1.data;

                    /* Check the channel number validation, the data is invalid if the channel num exceed the maximum channel */
                    if ((p)->type1.channel < SOC_ADC_CHANNEL_NUM(EXAMPLE_ADC_UNIT)) {
                        char buffer[5];
                        sprintf(buffer, "%04d", data);

                        if ((ptr - char_array) + strlen(buffer) < array_size) {
                            strcpy(ptr, buffer);
                            ptr += strlen(buffer);
                        } else {
                            ESP_LOGE("TASK", "Buffer overflow prevented");
                            break;
                        }
                    }
                }
                ESP_LOGI(TAG, "%s", char_array);
                // free(char_array);
                // vTaskDelay(10);
            }
            else if (ret == ESP_ERR_TIMEOUT) {
                //We try to read `EXAMPLE_READ_LEN` until API returns timeout, which means there's no available data
                break;
            }
        }
    }

    ESP_LOGI(TAG, "STOPPING");
    ESP_ERROR_CHECK(adc_continuous_stop(handle));
    ESP_ERROR_CHECK(adc_continuous_deinit(handle));
}
