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

#include <string.h>
#include <stdio.h>

static void initialise_wifi(void);
static esp_err_t event_handler(void *ctx, system_event_t *event);

static wifi_config_t sta_config = {
	.sta = {
		.bssid_set = false
	}
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

void app_main(void)
{
    nvs_flash_init();

	//1. Setup UART

	#define UART_INTR_NUM 17                                //choose one interrupt number from soc.h

    //a. Set UART parameter
	int uart_num = 0;                                       //uart port number
	uart_config_t uart_config = {
	 .baud_rate = 115200,                    //baudrate
	 .data_bits = UART_DATA_8_BITS,                       //data bit mode
	 .parity = UART_PARITY_DISABLE,                       //parity mode
	 .stop_bits = UART_STOP_BITS_1,                       //stop bit mode
	 .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,               //hardware flow control(cts/rts)
	 .rx_flow_ctrl_thresh = 120,                          //flow control threshold
	};
	uart_param_config(uart_num, &uart_config);
	//b1. Setup UART driver(with UART queue)
	QueueHandle_t uart_queue;
	//parameters here are just an example, tx buffer size is 2048
	uart_driver_install(uart_num, 1024 * 2, 1024 * 2, 10, UART_INTR_NUM, &uart_queue);
	//3. Read data from UART.
	uint8_t data[128];
	int length = 0;
	int totallength = 0;

	ESP_LOGI(TAG, "enter ssid: ");
    do {
		length = uart_read_bytes(uart_num, data, sizeof(data), 100);
		if (length > 0)
		{
			memcpy(&sta_config.sta.ssid + totallength, data, length);
			totallength += length;
			uart_write_bytes(uart_num, (const char*)data, length);
		}
    } while (length == 0 || data[length -1] != '\r');

    // replace carriage return with 0
    sta_config.sta.ssid[totallength -1] = '\0';

    totallength = 0;
    ESP_LOGI(TAG, "enter password: ");
    do {
		length = uart_read_bytes(uart_num, data, sizeof(data), 100);
		if (length > 0)
		{
			memcpy(&sta_config.sta.password + totallength, data, length);
			totallength += length;
			uart_write_bytes(uart_num, (const char*)data, length);
		}
    } while (length == 0 || data[length -1] != '\r');

    // replace carriage return with 0
    sta_config.sta.password[totallength -1] = '\0';

    initialise_wifi();

    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                        false, true, portMAX_DELAY);
}


static void initialise_wifi(void)
{
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", sta_config.sta.ssid);
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &sta_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
    esp_log_level_set("wifi", ESP_LOG_WARN);      // enable WARN logs from WiFi stack
}

