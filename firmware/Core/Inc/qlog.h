/*
 * qlog.h
 *
 *  Created on: Apr 22, 2026
 *      Author: tuant
 */

#ifndef INC_QLOG_H_
#define INC_QLOG_H_


#include "stm32g4xx_hal.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

// Config: Tăng size nếu log quá dài hoặc tần suất quá cao
#define LOG_BUF_SIZE 2048

void QuickLog_Init(void);
void QuickLog_Write(const char* fmt, ...); // Ghi log vào Buffer (dùng như printf)
void QuickLog_Process(void);               // Gọi trong vòng lặp while(1) để đẩy log qua USB


#endif /* INC_QLOG_H_ */
