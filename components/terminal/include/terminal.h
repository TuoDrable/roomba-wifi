
#ifndef __CORETERM_H__
#define __CORETERM_H__

#include "esp_err.h"

#define MAX_NUM_COMMANDS	20
#define TERMINAL_UART_NUM	0
#define UART_INTR_NUM 17   //choose one interrupt number from soc.h
#define UART_QUEUE_TX_BUF_SIZE		256
#define UART_QUEUE_RX_BUF_SIZE		1024
#define TERMINAL_TASK_STACK_SIZE	512
#define TERMINAL_TASK_PRIO			(configMAX_PRIORITIES - 15)

typedef void (*command_function_t)(char* args, void* other_stuff_you_want_to_get_called_with);


esp_err_t terminal_init();

esp_err_t terminal_register_command(const char* command, command_function_t command_function, void* args_to_give);



#endif /* __CORETERM_H__ */
