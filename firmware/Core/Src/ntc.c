/*
 * ntc.c
 *
 *  Created on: Mar 27, 2026
 *      Author: tuant
 */

#include <math.h>
#include <ntc.h>
#include "debug.h"

// Thông số NTC MF58 103F3950
#define NTC_R0      10000.0f    // 10k Ohm tại 25 độ C
#define NTC_B       3950.0f     // Hệ số B
#define T0          298.15f     // 25 độ C đổi sang Kelvin (273.15 + 25)
#define R_UP        10000.0f    // Điện trở treo trên (Pull-up) 10k Ohm

/**
 * @brief Tính nhiệt độ từ giá trị ADC Raw (đã qua Oversampling 16x)
 * @param adc_raw: Giá trị từ DMA (0-4095 nếu shift 4-bit, hoặc 0-65535 nếu không shift)
 * @return Nhiệt độ Celsius (float)
 */
float Calculate_NTC_Temp(uint16_t adc_raw) {
    // 1. Tính điện trở NTC hiện tại
    // Công thức cho mạch cầu phân áp (VCC -- R_UP --|ADC|-- NTC -- GND)
    float v_ratio = (float)adc_raw / 4095.0f;
    float r_ntc = R_UP * (v_ratio / (1.0f - v_ratio));
    // 2. Áp dụng công thức Beta (Steinhart-Hart rút gọn)
    float steinhart;
    steinhart = r_ntc / NTC_R0;             // (R/Ro)
    steinhart = logf(steinhart);             // ln(R/Ro) - Dùng logf cho FPU
    steinhart /= NTC_B;                      // 1/B * ln(R/Ro)
    steinhart += 1.0f / T0;                  // + (1/To)
    steinhart = 1.0f / steinhart;            // Nghịch đảo để lấy T (Kelvin)

    return steinhart - 273.15f;              // Chuyển sang độ C
}
