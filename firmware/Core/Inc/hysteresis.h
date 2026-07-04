#ifndef INC_HYSTERESIS_H
#define INC_HYSTERESIS_H

/* Includes ------------------------------------------------------------------*/
#include "stdint.h"
#include <math.h>

/* TypeDefs ------------------------------------------------------------------*/
typedef struct {
    float hysteresis;
    float previous_value;
    int first_run;         // Thêm cái này để đánh dấu lần chạy đầu
} Hysteresis_FilterTypeDef;

/* Function prototypes -------------------------------------------------------*/
void Hysteresis_Init(Hysteresis_FilterTypeDef* hysteresis_struct, float hysteresis);
float Hysteresis_Add(float new_value, Hysteresis_FilterTypeDef* hysteresis_struct);

#endif
