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

#define TAG "terminal"

#define NOT_FOUND_STR "\tUnknown command\n"


typedef struct
{
	command_function_t command_function;
	const char* command;
	void* stuff_to_call_command_with;
} terminal_data_t;

static terminal_data_t terminal_data[MAX_NUM_COMMANDS];

static unsigned int num_commands_registered = 0;

static QueueHandle_t uart_queue;
static xTaskHandle  xTerminalTaskHandle = NULL;

static char command_data[UART_QUEUE_RX_BUF_SIZE];
static int command_data_total_length = 0;

static void command_help(char* args, void* unused)
{
	for (int i = 0; i < num_commands_registered; i++)
	{
		uart_write_bytes(TERMINAL_UART_NUM, "    ", 4);
		uart_write_bytes(TERMINAL_UART_NUM, terminal_data[i].command, strlen(terminal_data[i].command));
		uart_write_bytes(TERMINAL_UART_NUM, "\n", 1);
	}
}

static void terminal_task(void *arg)
{
	//3. Read data from UART.
	uint8_t data[128];
	int length;

	while(1)
	{

		length = uart_read_bytes(TERMINAL_UART_NUM, data, sizeof(data), 0);

		if (length > 0)
		{
			// echo back chars
			puts("test\n");
			//uart_write_bytes(TERMINAL_UART_NUM, (const char*)data, length);
/*
			assert((length + command_data_total_length) < UART_QUEUE_RX_BUF_SIZE);

			memcpy((void*)&command_data[command_data_total_length], data, length);
			command_data_total_length += length;

			if (command_data[command_data_total_length -1] == '\n' ||
				command_data[command_data_total_length -1] == '\r')
			{

				// full command received, let's check which
				// add \0 to the end
				command_data[command_data_total_length] = '\0';

				char* first_space_index = strchr(command_data, ' ');

				if (first_space_index == NULL)
					first_space_index = command_data + command_data_total_length;

				// change the first space to a \0 to make the strcmp function work
				*first_space_index = '\0';

				bool found = false;
				for (int i = 0; i < num_commands_registered; i++)
				{
					if (strcmp(command_data, terminal_data[i].command) == 0)
					{
						found = true;
						terminal_data[i].command_function(first_space_index +1, terminal_data[i].stuff_to_call_command_with);
						command_data_total_length = 0;
						break;
					}
				}

				if (!found)
					uart_write_bytes(TERMINAL_UART_NUM, NOT_FOUND_STR, strlen(NOT_FOUND_STR));
			}*/
		}
		vTaskDelay(1000);
	}

}

esp_err_t terminal_init()
{
	esp_err_t error;
	//1. Setup UART


    //a. Set UART parameter                                     //uart port number
	uart_config_t uart_config = {
	 .baud_rate = 115200,                    //baudrate
	 .data_bits = UART_DATA_8_BITS,                       //data bit mode
	 .parity = UART_PARITY_DISABLE,                       //parity mode
	 .stop_bits = UART_STOP_BITS_1,                       //stop bit mode
	 .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,               //hardware flow control(cts/rts)
	 .rx_flow_ctrl_thresh = 120,                          //flow control threshold
	};
	uart_param_config(TERMINAL_UART_NUM, &uart_config);
	//b1. Setup UART driver(with UART queue)

	error = uart_driver_install(TERMINAL_UART_NUM, UART_QUEUE_RX_BUF_SIZE, UART_QUEUE_TX_BUF_SIZE, 10, UART_INTR_NUM, &uart_queue);

	if (error)
		return error;

	terminal_register_command("help", command_help, NULL);

	xTaskCreate(terminal_task, "Terminal_task", TERMINAL_TASK_STACK_SIZE, NULL, TERMINAL_TASK_PRIO, &xTerminalTaskHandle);

	return ESP_OK;
}

esp_err_t terminal_register_command(const char* command, command_function_t command_function, void* args_to_give)
{
	assert(num_commands_registered < MAX_NUM_COMMANDS);

	terminal_data[num_commands_registered].command = command;
	terminal_data[num_commands_registered].command_function = command_function;
	terminal_data[num_commands_registered].stuff_to_call_command_with = args_to_give;

	num_commands_registered++;

	return ESP_OK;
}

