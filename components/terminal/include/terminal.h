
#ifndef __CORETERM_H__
#define __CORETERM_H__

#include 

typedef void (*command_function_t)(void* args)


esp_err_t terminal_register_command(const char* command, command_function_t command_function, void* args_to_give);



#endif /* __CORETERM_H__ */
