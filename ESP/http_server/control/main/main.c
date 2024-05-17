/* WORMINATOR CODE:
    structure:
      headers & constanst
      frontend
      wifi setup
      data_formatting
      timer functions/callbacks
      backend
      gpio functions
      main
*/

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_mac.h"

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

#include "driver/gpio.h"

#if !CONFIG_IDF_TARGET_LINUX
#include <esp_wifi.h>
#include <esp_system.h>
#include "nvs_flash.h"
#include "esp_eth.h"
#endif  // !CONFIG_IDF_TARGET_LINUX

#define EXAMPLE_HTTP_QUERY_KEY_MAX_LEN  (64)
#define EXAMPLE_ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS      CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_WIFI_CHANNEL   CONFIG_ESP_WIFI_CHANNEL
#define EXAMPLE_MAX_STA_CONN       CONFIG_ESP_MAX_STA_CONN

#define GPIO_PULSE_1 GPIO_NUM_1
#define GPIO_PULSE_2 GPIO_NUM_2
#define GPIO_CHARGE_1 GPIO_NUM_3
#define GPIO_CHARGE_2 GPIO_NUM_4

#define SAMPLES 80
static char recording_data_array[SAMPLES*5];

// ---------------------------------------------------------------
/*
 * frontend
 */

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
 * Data formatting
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

 static void format_measurements(void){
     int i, j = 0;
     for (i = 0; i < SAMPLES; i++) {
         // Convert the integer to characters
         int len = snprintf(NULL, 0, "%d", i*5); // Determine the length of the integer string
         char temp[len + 1]; // Create a temporary buffer to store the integer string
         snprintf(temp, len + 1, "%d", i*5); // Convert the integer to a string

         // Append the characters of the integer to the character array
         int k;
         for (k = 0; temp[k] != '\0'; k++) {
             recording_data_array[j++] = temp[k];
         }

         // Append a comma if it's not the last integer
         if (i != SAMPLES - 1) {
             recording_data_array[j++] = ',';
         }
     }

     // Null-terminate the character array
     recording_data_array[j] = '\0';
 }


// ---------------------------------------------------------------
/*
 * Timers
 */

static void callback_interpulse(void* arg){
    gpio_set_level(GPIO_PULSE_2, 1);
}

static void callback_open_charge_1(void* arg){
  gpio_set_level(GPIO_CHARGE_1, 0);
}

static void callback_open_charge_2(void* arg){
  gpio_set_level(GPIO_CHARGE_2, 0);
}

static void callback_open_pulse_1(void* arg){
  gpio_set_level(GPIO_CHARGE_1, 0);
}

static void callback_open_pulse_2(void* arg){
  gpio_set_level(GPIO_CHARGE_2, 0);
}

// ---------------------------------------------------------------
/*
 * Backend
 */

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

esp_err_t handler_run_all(httpd_req_t *req){
    return httpd_resp_send(req, recording_data_array, HTTPD_RESP_USE_STRLEN);
}

esp_err_t handler_test_chunk(httpd_req_t *req){
    char temp_chunk[51];

    for(int i=0; i < 5; i++){
        for(int j = 0; j < 50; j++){
            temp_chunk[j] = i + '0';
        }
        temp_chunk[50] = '\0';
        httpd_resp_send_chunk(req, temp_chunk, HTTPD_RESP_USE_STRLEN);
        ESP_LOGI(TAG, "sent array chunk filled with %d", i);
    }
    return httpd_resp_send_chunk(req, temp_chunk, 0); // TODO: invalid size
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

static const httpd_uri_t handler_run_all_t = {
  .uri       = "/run_all" ,
  .method    = HTTP_GET,
  .handler   = handler_run_all,
  .user_ctx  = NULL
};

static const httpd_uri_t handler_test_chunk_t = {
  .uri       = "/chunk" ,
  .method    = HTTP_GET,
  .handler   = handler_test_chunk,
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
        httpd_register_uri_handler(server, &handler_run_all_t);
        httpd_register_uri_handler(server, &handler_test_chunk_t);
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
 * GPIO
 */

 static void control_led_init(void)
{
  //zero-initialize the config structure.
    gpio_set_direction(GPIO_CHARGE_2, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_CHARGE_2, 0);
}

void app_main(void)
{
    static httpd_handle_t server = NULL;

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
