#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "driver/uart.h"

#include "terminal.h"

#include <string.h>
#include <stdio.h>


#define STORAGE_NAMESPACE "storage"

static void initialise_wifi(void);
static esp_err_t event_handler(void *ctx, system_event_t *event);

static wifi_config_t sta_config = {
	.sta = {
		.ssid = CONFIG_WIFI_SSID,
		.password = CONFIG_WIFI_PASSWORD,
	},
};

static const char *TAG = "roomba-wifi";

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT = BIT0;


/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        /* This is a workaround as ESP32 WiFi libs don't currently
           auto-reassociate. */
        //esp_wifi_connect();
    	ESP_LOGE(TAG, "disconnected!");
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        break;
    default:
        break;
    }
    return ESP_OK;
}

static void command_config_wifi(char* args, void* unused)
{
	char* tmp;
	char* ssid = args;
	char* password = strchr((const char*)args, ' ');


	if (args == NULL)
	{
		ESP_LOGW(TAG, "No ssid found");
		ESP_LOGW(TAG, "usage: setup_wifi SSID PASSWORD");
		return;
	}

	if (password == NULL)
	{
		ESP_LOGW(TAG, "No password found");
		ESP_LOGW(TAG, "usage: setup_wifi SSID PASSWORD");
		return;
	}

	// seperate the SSID string
	*password =  '\0';

	// sanitize password, set start right
	do
	{
		password++;
	}
	while (*password == ' ');

	// set end to \0
	tmp = password;
	while (*tmp != '\r' && *tmp != '\n' && *tmp != '\0')
		tmp++;

	*tmp = '\0';

	strcpy(&sta_config.sta.ssid, ssid);
	strcpy(&sta_config.sta.password, password);

    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", sta_config.sta.ssid);
	ESP_ERROR_CHECK( esp_wifi_disconnect() );
	ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &sta_config) );
	ESP_ERROR_CHECK( esp_wifi_connect() );

}

void app_main(void)
{
    nvs_flash_init();

	//1. Setup terminal

    terminal_init();
    terminal_register_command("setup_wifi", command_config_wifi, NULL);

    initialise_wifi();

    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                        false, true, portMAX_DELAY);
}

/*
static esp_err_t get_wifi_args_from_nvs(wifi_config_t* config)
{

    nvs_handle my_handle;
    esp_err_t err;
    size_t blob_size = sizeof(sta_config);

    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    	return err;

    return nvs_get_blob(my_handle, "wifi_config", (void*)config, &blob_size);
}*/


static void initialise_wifi(void)
{
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_FLASH) );
    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", sta_config.sta.ssid);
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &sta_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
    esp_log_level_set("wifi", ESP_LOG_WARN);      // enable WARN logs from WiFi stack
}

