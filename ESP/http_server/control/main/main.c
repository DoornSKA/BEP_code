/* Simple HTTP Server Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_mac.h"

#include "lwip/err.h"
#include "lwip/sys.h"


#include <string.h>
#include <stdlib.h>
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

#define OUT_PULSE_1 GPIO_NUM_1
#define OUT_PULSE_2 GPIO_NUM_2

#define LED_BUILTIN GPIO_NUM_2

#define MAX_VOLTAGE (250)

char WEB_on_resp[] = R"rawliteral(
<!DOCTYPE html>
<html>
   <head>
      <style type=\"text/css\">html {  font-family: Arial;  display: inline-block;  margin: 0px auto;  text-align: center;}h1{  color: #070812;  padding: 2vh;}.button {  display: inline-block;  background-color: #b30000; //red color  border: none;  border-radius: 4px;  color: white;  padding: 16px 40px;  text-decoration: none;  font-size: 30px;  margin: 2px;  cursor: pointer;}.button2 {  background-color: #364cf4; //blue color}.content {   padding: 50px;}.card-grid {  max-width: 800px;  margin: 0 auto;  display: grid;  grid-gap: 2rem;  grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));}.card {  background-color: white;  box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5);}.card-title {  font-size: 1.2rem;  font-weight: bold;  color: #034078}</style>
      <title>ESP32 WEB SERVER</title>
      <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">
      <link rel=\"icon\" href=\"data:,\">
      <link rel=\"stylesheet\" href=\"https://use.fontawesome.com/releases/v5.7.2/css/all.css\"    integrity=\"sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr\" crossorigin=\"anonymous\">
      <link rel=\"stylesheet\" type=\"text/css\" >
   </head>
   <body>
      <h2>ESP32 WEB SERVER</h2>
      <div class=\"content\">
      <div class=\"card-grid\">
         <div class=\"card\">
            <p><i class=\"fas fa-lightbulb fa-2x\" style=\"color:#c81919;\"></i>     <strong>GPIO2</strong></p>
            <p>GPIO state: <strong> ON</strong></p>
            <p>          <a href=\"/led2on\"><button class=\"button\">ON</button></a>          <a href=\"/led2off\"><button class=\"button button2\">OFF</button></a>        </p>
         </div>
         </  div>
      </div>
   </body>
</html>)rawliteral";

char WEB_off_resp[] = R"rawliteral(
<!DOCTYPE html>
<html>
   <head>
      <style type=\"text/css\">html {  font-family: Arial;  display: inline-block;  margin: 0px auto;  text-align: center;}h1{  color: #070812;  padding: 2vh;}.button {  display: inline-block;  background-color: #b30000; //red color  border: none;  border-radius: 4px;  color: white;  padding: 16px 40px;  text-decoration: none;  font-size: 30px;  margin: 2px;  cursor: pointer;}.button2 {  background-color: #364cf4; //blue color}.content {   padding: 50px;}.card-grid {  max-width: 800px;  margin: 0 auto;  display: grid;  grid-gap: 2rem;  grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));}.card {  background-color: white;  box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5);}.card-title {  font-size: 1.2rem;  font-weight: bold;  color: #034078}</style>
      <title>ESP32 WEB SERVER</title>
      <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">
      <link rel=\"icon\" href=\"data:,\">
      <link rel=\"stylesheet\" href=\"https://use.fontawesome.com/releases/v5.7.2/css/all.css\"    integrity=\"sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr\" crossorigin=\"anonymous\">
      <link rel=\"stylesheet\" type=\"text/css\">
   </head>
   <body>
      <h2>ESP32 WEB SERVER</h2>
      <div class=\"content\">
         <div class=\"card-grid\">
            <div class=\"card\">
               <p><i class=\"fas fa-lightbulb fa-2x\" style=\"color:#c81919;\"></i>     <strong>GPIO2</strong></p>
               <p>GPIO state: <strong> OFF</strong></p>
               <p>          <a href=\"/led2on\"><button class=\"button\">ON</button></a>          <a href=\"/led2off\"><button class=\"button button2\">OFF</button></a>        </p>
            </div>
         </div>
      </div>
   </body>
</html>)rawliteral";

char WEB_test[] = R"rawliteral(
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
      border: 1px solid #ccc;
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
      text-align: center;
  }


  </style>
  </head>
  <body>
  <header>
      <h1>WORMINATOR</h1>
  </header>

  <form action="/values">
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

  <div class="button-cont">

    <a href="/stimulate" class="button" onclick="WARNING - STIMULATION">RUN STIMULATION</a>
  </div>


  </body>
  </html>)rawliteral";

char WEB_input_test[] = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>ESP Input Form</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  </head><body>
  <form action="/testInput">
    input1: <input type="text" name="cap_value">
    <input type="submit" value="Submit">
  </form><br>
</body></html>)rawliteral";

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

struct userData{
    int Voltage1;
    int Voltage2;
    int MuS;
};

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

static esp_err_t input_handler(httpd_req_t *req)
{
    const char* target = "cap_value";
    const char* uri = "testInput";
    int input_values[3];

    const char* uri2 = req->uri;

    ESP_LOGI(TAG, "buffer: %s", uri2);
    char* box_input = &uri2[strlen(target)+1];
    ESP_LOGI(TAG, "print value: %s", box_input);

    // format the http request and put the form inputs into input_values
    format_inputs(input_values, box_input);

    return httpd_resp_send(req, WEB_input_test, HTTPD_RESP_USE_STRLEN);
}


esp_err_t get_req_handler(httpd_req_t *req)
{
    esp_err_t response = httpd_resp_send(req, WEB_off_resp, HTTPD_RESP_USE_STRLEN);
    ESP_LOGI(TAG, "GPIO%i", LED_BUILTIN);
    return response;
}
esp_err_t led_on_handler(httpd_req_t *req)
{
    esp_err_t response = httpd_resp_send(req, WEB_on_resp, HTTPD_RESP_USE_STRLEN);
    gpio_set_level(LED_BUILTIN, 1);
    ESP_LOGI(TAG, "GPIO%i set to on", LED_BUILTIN);
    return response;

}
esp_err_t led_off_handler(httpd_req_t *req)
{
    esp_err_t response = httpd_resp_send(req, WEB_off_resp, HTTPD_RESP_USE_STRLEN);
    gpio_set_level(LED_BUILTIN, 0);
    ESP_LOGI(TAG, "GPIO%i set to off", LED_BUILTIN);
    return response;
}

esp_err_t test_handler(httpd_req_t *req)
{
    const char* uri_test = req->uri;
    ESP_LOGI(TAG, "buffer: %s", uri_test);
    esp_err_t response = httpd_resp_send(req, WEB_test, HTTPD_RESP_USE_STRLEN);
    ESP_LOGI(TAG, "testing");
    return response;
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

static const httpd_uri_t input = {
  .uri       = "/testInput",
  .method    = HTTP_GET,
  .handler   = input_handler,
  .user_ctx  = NULL
};

static const httpd_uri_t uri_get = {
  .uri       = "/",
  .method    = HTTP_GET,
  .handler   = test_handler,
  .user_ctx  = NULL
};

static const httpd_uri_t ht_led_on = {
    .uri       = "/led2on",
    .method    = HTTP_GET,
    .handler   = led_on_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t ht_led_off = {
    .uri       = "/led2off" ,
    .method    = HTTP_GET,
    .handler   = led_off_handler,
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
        httpd_register_uri_handler(server, &ht_led_off);
        httpd_register_uri_handler(server, &ht_led_on);
        httpd_register_uri_handler(server, &uri_get);
        httpd_register_uri_handler(server, &input);
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
    gpio_set_direction(LED_BUILTIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_BUILTIN, 0);
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
