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
	puts("List of commands:");
	for (int i = 0; i < num_commands_registered; i++)
	{
		printf("    * %s\n", terminal_data[i].command);
	}
	putchar('\n');
}


static bool find_command()
{
	// full command received, let's check which
	// replace \r by \0 at the end
	command_data[command_data_total_length-1] = '\0';

	char* first_space_index = strchr(command_data, ' ');

	if (first_space_index != NULL)
	{
		// change the first space to a \0 to make the strcmp function work
		*first_space_index = '\0';
		first_space_index++;
	}


	if (strlen(command_data) > 0)
	{
		bool found = false;
		for (int i = 0; i < num_commands_registered; i++)
		{
			if (strcmp(command_data, terminal_data[i].command) == 0)
			{
				found = true;
				terminal_data[i].command_function(first_space_index, terminal_data[i].stuff_to_call_command_with);
				break;
			}
		}

		if (!found)
		{
			printf("%s", NOT_FOUND_STR);
		}

		return found;
	}
	else
	{
		return false;
	}
}

static void terminal_task(void *arg)
{
	//3. Read data from UART.
	memset(command_data, 0, sizeof(command_data));
	command_data_total_length = 0;

	while(1)
	{
		do
		{
			assert(command_data_total_length < UART_QUEUE_RX_BUF_SIZE);

			command_data[command_data_total_length] = getchar();
			if (command_data[command_data_total_length] != (char)EOF)
			{
				//printf("%d", command_data_total_length);
				putchar(command_data[command_data_total_length]);
				command_data_total_length++;

				if (command_data[command_data_total_length - 1] == '\r')
				{
					putchar('\n');
					find_command();
					command_data_total_length = 0;
				}
			}
		} while (command_data[command_data_total_length] != (char)EOF);

		fflush(stdout);
		vTaskDelay(100);
	}

}

esp_err_t terminal_init()
{
	esp_err_t error;
	//1. Setup UART

	num_commands_registered = 0;


    //a. Set UART parameter                                     //uart port number
	uart_config_t uart_config = {
	 .baud_rate = 115200,                    //baudrate
	 .data_bits = UART_DATA_8_BITS,                       //data bit mode
	 .parity = UART_PARITY_DISABLE,                       //parity mode
	 .stop_bits = UART_STOP_BITS_1,                       //stop bit mode
	 .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,               //hardware flow control(cts/rts)
	 .rx_flow_ctrl_thresh = 120,                          //flow control threshold
	};
	//uart_param_config(TERMINAL_UART_NUM, &uart_config);
	//b1. Setup UART driver(with UART queue)

	//error = uart_driver_install(TERMINAL_UART_NUM, UART_QUEUE_RX_BUF_SIZE, UART_QUEUE_TX_BUF_SIZE, 10, &uart_queue, 0);

	//if (error)
	//	return error;

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

