#ifndef INC_DEBUG_H_
#define INC_DEBUG_H_

#include "stm32g4xx_hal.h"
#include "string.h"

typedef enum{
	DEBUG_OFF = 0,
	DEBUG_INFO,
	DEBUG_WARNING,
	DEBUG_ERROR,
	DEBUG_MAX_LEVELS
}DEBUG_VERBOSITY_t;

void heartbeat();
HAL_StatusTypeDef debug_print_str(DEBUG_VERBOSITY_t verbosity, char * str);
HAL_StatusTypeDef debug_print_int(DEBUG_VERBOSITY_t verbosity, char * str, int i);
HAL_StatusTypeDef debug_log(const char* fmt, ...);
#endif /* INC_DEBUG_H_ */
