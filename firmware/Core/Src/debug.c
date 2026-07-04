#include "debug.h"
#include "string.h"
#include <stdio.h>
#include <stdarg.h>
extern UART_HandleTypeDef huart1;
extern DEBUG_VERBOSITY_t debugLevel;
static char buffer[64];

uint32_t heartbeat_ = 0;
volatile uint8_t uart_ready = 1; // 1 là rảnh, 0 là đang bận

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART1) {
        uart_ready = 1; // Giải phóng cho lần log tiếp theo
    }
}

void heartbeat() {
  // Heartbeat LED (Nháy mỗi 500ms)
  if(HAL_GetTick() - heartbeat_ > 500) {
    heartbeat_ = HAL_GetTick();
      HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_6);
  }
}

static const char *debug_level_str[] = {
	"",
	"[I]: ",
	"[W]: ",
	"[E]: ",
};


HAL_StatusTypeDef debug_log(const char* fmt, ...) {
  HAL_StatusTypeDef ret = HAL_OK;
#ifdef DEBUG
  //if (uart_ready != 1) return ret; uart_ready = 0;
  va_list args;
  va_start(args, fmt);
  int len = vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);

  ret = HAL_UART_Transmit(&huart1, (uint8_t *) buffer, (uint16_t)len,100);
  //HAL_UART_Transmit_DMA(&huart1, (uint8_t*)buffer, len);
#endif
  return ret;
}


HAL_StatusTypeDef debug_print_str(DEBUG_VERBOSITY_t verbosity, char * str){
	HAL_StatusTypeDef ret = HAL_OK;
#ifdef DEBUG
  //if (uart_ready != 1) return ret; uart_ready = 0;
	if(debugLevel > 0 && verbosity >= debugLevel){
		memset(buffer, '\0', sizeof(buffer));
		sprintf(buffer, "%s %s\n", debug_level_str[verbosity], str);
		ret = HAL_UART_Transmit(&huart1, (uint8_t *) buffer, (uint16_t)strlen(buffer),100);
		//HAL_UART_Transmit_DMA(&huart1, (uint8_t*)buffer, (uint16_t)strlen(buffer));
	}
#endif
	return ret;
}

HAL_StatusTypeDef debug_print_int(DEBUG_VERBOSITY_t verbosity, char * str, int i){
	HAL_StatusTypeDef ret = HAL_OK;
#ifdef DEBUG
  //if (uart_ready != 1) return ret; uart_ready = 0;
	if(debugLevel > 0 && verbosity >= debugLevel){
		memset(&buffer, '\0', sizeof(buffer));
		sprintf(buffer, "%s %s %i\n", debug_level_str[verbosity], str, i);
		ret = HAL_UART_Transmit(&huart1, (uint8_t *) buffer, (uint16_t)strlen(buffer),100);
		//HAL_UART_Transmit_DMA(&huart1, (uint8_t*)buffer, (uint16_t)strlen(buffer));
	}
#endif
	return ret;
}

HAL_StatusTypeDef debug_print_int_int(DEBUG_VERBOSITY_t verbosity, char * str, int i, int j){
	HAL_StatusTypeDef ret = HAL_OK;
#ifdef DEBUG
  //if(uart_ready != 1) return ret;
  uart_ready = 0;
	if(debugLevel > 0 && verbosity >= debugLevel){
		memset(buffer, '\0', sizeof(buffer));
		sprintf(buffer, "%s %s %i %i\n", debug_level_str[verbosity], str, i, j);
		ret = HAL_UART_Transmit(&huart1, (uint8_t *) buffer, (uint16_t)strlen(buffer),100);
		//HAL_UART_Transmit_DMA(&huart1, (uint8_t*)buffer, (uint16_t)strlen(buffer));
	}
#endif
	return ret;
}

//HAL_StatusTypeDef debug_print_pdos(DEBUG_VERBOSITY_t verbosity, PDO_container_t *pdos){
//	HAL_StatusTypeDef ret = HAL_OK;
//#ifdef DEBUG
//	if(debugLevel > 0 && verbosity >= debugLevel){
//		char tx[256];
//		memset(tx, '\0', sizeof(tx));
//		for(int i = 0 ; i < pdos->numPDOs; i++){
//			memset(buffer, '\0', sizeof(buffer));
//			sprintf(buffer, "%s PDO %i %.2fA %.2fV\n", debug_level_str[verbosity], i, (double)(pdos->pdos[i].current)*0.01, (double)(pdos->pdos[i].voltage)*0.05);
//			strcat(tx, buffer);
//		}
//		ret = HAL_UART_Transmit(&huart1, (uint8_t *) tx, (uint16_t)strlen(tx),100);
//	}
//#endif
//	return ret;
//}

