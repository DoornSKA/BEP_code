/* Simple HTTP Server Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_mac.h"

#include "sdkconfig.h"

#include "lwip/err.h"
#include "lwip/sys.h"


#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "esp_netif.h"
#include "protocol_examples_common.h"
#include "protocol_examples_utils.h"
#include "esp_tls_crypto.h"
#include <esp_http_server.h>
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_tls.h"

#include "esp_adc/adc_continuous.h"
#include "driver/gpio.h"

#include "event_source.h"
#include "esp_event_base.h"

#if !CONFIG_IDF_TARGET_LINUX
#include <esp_wifi.h>
#include <esp_system.h>
#include "nvs_flash.h"
#include "esp_eth.h"
#endif  // !CONFIG_IDF_TARGET_LINUX


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

#define EXAMPLE_READ_LEN                    256
#define ADC_BUFFER_SIZE                     1024

#define EXAMPLE_HTTP_QUERY_KEY_MAX_LEN  (64)
#define EXAMPLE_ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS      CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_WIFI_CHANNEL   CONFIG_ESP_WIFI_CHANNEL
#define EXAMPLE_MAX_STA_CONN       CONFIG_ESP_MAX_STA_CONN

#define OUT_PULSE_1 GPIO_NUM_1
#define OUT_PULSE_2 GPIO_NUM_2

#define LED_BUILTIN GPIO_NUM_2

#define SAMPLES 80
static char recording_data_array[SAMPLES*5];

#if CONFIG_IDF_TARGET_ESP32
static adc_channel_t channel[1] = {ADC_CHANNEL_6};
#else
static adc_channel_t channel[1] = {ADC_CHANNEL_2};
#endif

static TaskHandle_t s_task_handle;
static adc_continuous_handle_t adc_handle;


esp_event_loop_handle_t loop_with_task;
esp_event_loop_handle_t loop_without_task;

char WORMINATOR[] = R"rawliteral(
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Header and form</title>
<style>
/* CSS styles */
body {
    font-family: OCR A Std, monospace;
    margin: 0;
    padding: 0;
    background-color: #94ddc9
}

header {
    background-color: #998adb;
    color: #fff;
    padding: 10px 20px;
    text-align: center;
}

form {
    margin: 20px;
    display: flex;
    flex-direction: column;
}

label {
    margin-bottom: 5px;
}

input[type="Voltage1"], input[type="Voltage2"], input[type="Interpulse"] {
    padding: 10px;
    margin-bottom: 10px;
    border: 1px solid black;
    background-color: #f8f9f4;
    border-radius: 5px;
    font-size: 16px;
}

input[type="submit"], .button {
    padding: 10px 20px;
    background-color: #998adb;
    color: #fff;
    border: none;
    border-radius: 5px;
    font-size: 16px;
    cursor: pointer;
}

.input-container {
    margin: auto;
    margin-right: ;
    margin-bottom: 10px;
}

.button-container {
    margin: auto;
    margin-bottom: 40px;
    text-align: center;
}

.button-cont {
    margin: auto;
    margin-bottom: 40px;
    text-align: center;
}

.box {
    margin-left: 60px;
    margin-right: 60px;
    text-align: left;
    background-color: #f8f9f4;
    padding: 60px;
    border: 3px solid black;
    border-radius: 5px;
}
</style>
</head>
<body>
<header>
    <h1>WORMINATOR</h1>
</header>

<form action="/submit">
    <div class="input-container">
        <label for="Voltage1">[V]</label>
        Voltage1: <input type="Voltage1" id="Voltage1" name="Voltage1" required>
    </div>
    <div class="input-container">
        <label for="Voltage2">[V]</label>
        Voltage2: <input type="Voltage2" id="Voltage2" name="Voltage2" required>
    </div>
    <div class="input-container">
        <label for="Interpulse">[us]</label>
        Interpulse: <input type="Interpulse" id="Interpulse" name="Interpulse" required>
    </div>
    <div class="button-container">
        <input type="submit" value="Submit">
    </div>
</form>

<div>
  <div class="button-cont">
  <a href="/stimulate" class="button" onclick="WARNING - STIMULATION">RUN STIMULATION</a>
  </div>
  <div>
    <div class="box">
    <p>USER GUIDE:</p>
    <p>-set the value of the voltages of the coil</p>
      <p>-set the values of the duration between the firing of the two pulses</p>
  </div>
  </div>
</div>
</body>
</html>)rawliteral";

static const char *TAG = "example";
// SoftAP
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

void wifi_init_softap(void)
{
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .channel = EXAMPLE_ESP_WIFI_CHANNEL,
            .password = EXAMPLE_ESP_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,
#ifdef CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT
            .authmode = WIFI_AUTH_WPA3_PSK,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
#else /* CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT */
            .authmode = WIFI_AUTH_WPA2_PSK,
#endif
            .pmf_cfg = {
                    .required = true,
            },
        },
    };
    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS, EXAMPLE_ESP_WIFI_CHANNEL);
}

// ---------------------------------------------------------------
/*
 * Web page
 */

struct userData_t{
    int Voltage1;
    int Voltage2;
    int Interpulse;
};

struct userData_t userData = {0, 0, 0};

static void format_inputs(int* input_values, char* input_string){
    // Make sure input_values has 3 items

    int fm_count = 0; // Counter for the array

    // Tokenize the input string
    char *fm_token = strtok(input_string, "&");

    ESP_LOGI(TAG, "test 3");

    while(fm_token != NULL && fm_count < 3) {
        // Find the position of '='
        char *pos = strchr(fm_token, '=');

        if (pos != NULL) {
            // Extract the value
            char *value = pos + 1;

            // Convert the value to an integer and store it in the array
            input_values[fm_count] = atoi(value);
            fm_count++;
        }

        // Move to the next token
        fm_token = strtok(NULL, "&");
    }

    // Output the values stored in the array
    ESP_LOGI(TAG, "input1: %d, input2: %d, input3 = %d", input_values[0], input_values[1], input_values[2]);
}

void int_to_4char(int num, char* buffer) {
    sprintf(buffer, "%04d", num);
}

static bool IRAM_ATTR s_conv_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data)
{
    BaseType_t mustYield = pdFALSE;
    // Notify that ADC continuous driver has done enough number of conversions
    vTaskNotifyGiveFromISR(s_task_handle, &mustYield);
    return (mustYield == pdTRUE);
}

static void format_measurements(httpd_req_t *req, uint8_t* int_array, size_t samples_amount){
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
    ESP_LOGI(TAG, "sending chunck");
    ESP_ERROR_CHECK(httpd_resp_send_chunk(req, char_array, HTTPD_RESP_USE_STRLEN));
}

esp_err_t handler_main(httpd_req_t *req){
    esp_err_t response = httpd_resp_send(req, WORMINATOR, HTTPD_RESP_USE_STRLEN);
    ESP_LOGI(TAG, "New user signed on");
    return response;
}

esp_err_t handler_stimulate(httpd_req_t *req){
    esp_err_t response = httpd_resp_send(req, WORMINATOR, HTTPD_RESP_USE_STRLEN);
    ESP_LOGI(TAG, "COIL GOES BRR");
    return response;
}

esp_err_t handler_submit(httpd_req_t *req){
    const char* target = "submit";
    int input_values[3];

    const char* uri = req->uri;

    ESP_LOGI(TAG, "buffer: %s", uri);
    char* box_input = &uri[strlen(target)+1];
    ESP_LOGI(TAG, "print value: %s", box_input);

    // format the http request and put the form inputs into input_values
    format_inputs(input_values, box_input);
    userData.Voltage1 = input_values[0];
    userData.Voltage2 = input_values[1];
    userData.Interpulse = input_values[2];

    ESP_LOGI(TAG, "Voltage 1: %d, Voltage 2: %d, Interpulse: %d", userData.Voltage1 ,userData.Voltage2 , userData.Interpulse);
    return httpd_resp_send(req, WORMINATOR, HTTPD_RESP_USE_STRLEN);
}

esp_err_t handler_get_data(httpd_req_t *req){
    // format_measurements();
    return httpd_resp_send(req, recording_data_array, HTTPD_RESP_USE_STRLEN);
}

esp_err_t handler_read_test(httpd_req_t *req){
    ESP_LOGI(TAG, "starting /read_test");
    char dummy[0];
    esp_err_t ret;
    uint32_t ret_num = 0;
    uint8_t result[EXAMPLE_READ_LEN] = {0};

    s_task_handle = xTaskGetCurrentTaskHandle();
    adc_continuous_evt_cbs_t cbs = {
        .on_conv_done = s_conv_done_cb,
    };
    ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(adc_handle, &cbs, NULL));
    ESP_ERROR_CHECK(adc_continuous_start(adc_handle));

    for(int i = 0; i < 10; i++){
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        ret = adc_continuous_read(adc_handle, result, EXAMPLE_READ_LEN, &ret_num, 0);
        if (ret == ESP_OK){
            ESP_LOGI(TAG, "sending chunck %d", i + 1);
            format_measurements(req, result, ret_num);
            vTaskDelay(10/portTICK_PERIOD_MS);
        }
        else{
            ESP_ERROR_CHECK(ret);
        }
    }
    ESP_LOGI(TAG, "sending final chunck");
    ret = httpd_resp_send_chunk(req, dummy, 0);

    ESP_ERROR_CHECK(adc_continuous_stop(adc_handle));
    ESP_ERROR_CHECK(adc_continuous_flush_pool(adc_handle));
    return ret;

}

/* This handler allows the custom error handling functionality to be
 * tested from client side. For that, when a PUT request 0 is sent to
 * URI /ctrl, the /hello and /echo URIs are unregistered and following
 * custom error handler http_404_error_handler() is registered.
 * Afterwards, when /hello or /echo is requested, this custom error
 * handler is invoked which, after sending an error message to client,
 * either closes the underlying socket (when requested URI is /echo)
 * or keeps it open (when requested URI is /hello). This allows the
 * client to infer if the custom error handler is functioning as expected
 * by observing the socket state.
 */
esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    if (strcmp("/hello", req->uri) == 0) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "/hello URI is not available");
        /* Return ESP_OK to keep underlying socket open */
        return ESP_OK;
    } else if (strcmp("/echo", req->uri) == 0) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "/echo URI is not available");
        /* Return ESP_FAIL to close underlying socket */
        return ESP_FAIL;
    }
    /* For any other URI send 404 and close socket */
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Some 404 error message");
    return ESP_FAIL;
}

static const httpd_uri_t handler_main_t = {
  .uri       = "/",
  .method    = HTTP_GET,
  .handler   = handler_main,
  .user_ctx  = NULL
};

static const httpd_uri_t handler_stimulate_t = {
  .uri       = "/stimulate" ,
  .method    = HTTP_GET,
  .handler   = handler_stimulate,
  .user_ctx  = NULL
};

static const httpd_uri_t handler_submit_t = {
  .uri       = "/submit" ,
  .method    = HTTP_GET,
  .handler   = handler_submit,
  .user_ctx  = NULL
};

static const httpd_uri_t handler_get_data_t = {
  .uri       = "/get_data" ,
  .method    = HTTP_GET,
  .handler   = handler_get_data,
  .user_ctx  = NULL
};

static const httpd_uri_t handler_read_test_t = {
  .uri       = "/read_test" ,
  .method    = HTTP_GET,
  .handler   = handler_read_test,
  .user_ctx  = NULL
};

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
#if CONFIG_IDF_TARGET_LINUX
    // Setting port as 8001 when building for Linux. Port 80 can be used only by a priviliged user in linux.
    // So when a unpriviliged user tries to run the application, it throws bind error and the server is not started.
    // Port 8001 can be used by an unpriviliged user as well. So the application will not throw bind error and the
    // server will be started.
    config.server_port = 8001;
#endif // !CONFIG_IDF_TARGET_LINUX
    config.lru_purge_enable = true;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &handler_stimulate_t);
        httpd_register_uri_handler(server, &handler_submit_t);
        httpd_register_uri_handler(server, &handler_main_t);
        httpd_register_uri_handler(server, &handler_get_data_t);
        httpd_register_uri_handler(server, &handler_read_test_t);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

#if !CONFIG_IDF_TARGET_LINUX
static esp_err_t stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    return httpd_stop(server);
}

static void disconnect_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server) {
        ESP_LOGI(TAG, "Stopping webserver");
        if (stop_webserver(*server) == ESP_OK) {
            *server = NULL;
        } else {
            ESP_LOGE(TAG, "Failed to stop http server");
        }
    }
}

static void connect_handler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server == NULL) {
        ESP_LOGI(TAG, "Starting webserver");
        *server = start_webserver();
    }
}
#endif // !CONFIG_IDF_TARGET_LINUX

// -------------------------------------------------------------
/*
 * GPIO & ADC
 */


 static void continuous_adc_init(adc_channel_t *channel, uint8_t channel_num, adc_continuous_handle_t *out_handle)
 {
     adc_continuous_handle_t handle = NULL;

     adc_continuous_handle_cfg_t adc_config = {
         .max_store_buf_size = 1024,
         .conv_frame_size = EXAMPLE_READ_LEN,
     };
     ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &handle));

     adc_continuous_config_t dig_cfg = {
         .sample_freq_hz = 8 * 1000,
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

 static void control_led_init(void)
{
  //zero-initialize the config structure.
    gpio_set_direction(LED_BUILTIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_BUILTIN, 0);
}

void app_main(void)
{
    static httpd_handle_t server = NULL;
    adc_handle = NULL;

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    control_led_init();

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */

    ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");
    wifi_init_softap();

    // ADC
    continuous_adc_init(channel, sizeof(channel) / sizeof(adc_channel_t), &adc_handle);

    // adc_continuous_evt_cbs_t cbs = {
    //     .on_conv_done = s_conv_done_cb,
    // };
    //
    // ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(adc_handle, &cbs, NULL));

    /* Register event handlers to stop the server when Wi-Fi or Ethernet is disconnected,
     * and re-start it upon connection.
     */
#if !CONFIG_IDF_TARGET_LINUX
#ifdef CONFIG_EXAMPLE_CONNECT_WIFI
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &server));
#endif // CONFIG_EXAMPLE_CONNECT_WIFI
#ifdef CONFIG_EXAMPLE_CONNECT_ETHERNET
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ETHERNET_EVENT_DISCONNECTED, &disconnect_handler, &server));
#endif // CONFIG_EXAMPLE_CONNECT_ETHERNET
#endif // !CONFIG_IDF_TARGET_LINUX

    /* Start the server for the first time */
    server = start_webserver();

    while (server) {
        sleep(5);
    }
}
