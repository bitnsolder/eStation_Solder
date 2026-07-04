/* Includes ------------------------------------------------------------------*/
#include "hysteresis.h"
#include <math.h> // Để dùng hàm fabsf (trị tuyệt đối)

void Hysteresis_Init(Hysteresis_FilterTypeDef* hysteresis_struct, float hysteresis)
{
    hysteresis_struct->hysteresis = hysteresis;
    hysteresis_struct->previous_value = 0.0f;
    hysteresis_struct->first_run = 1;  // Đánh dấu: "Chưa có dữ liệu cũ"
}

float Hysteresis_Add(float new_value, Hysteresis_FilterTypeDef* hysteresis_struct)
{
    // 1. Nếu là lần đầu tiên chạy, gán luôn giá trị mới và thoát
    if (hysteresis_struct->first_run) {
        hysteresis_struct->previous_value = new_value;
        hysteresis_struct->first_run = 0; // Tắt cờ, các lần sau sẽ bắt đầu so sánh
        return new_value;
    }

    // 2. Kiểm tra độ lệch (dùng hàm trị tuyệt đối fabsf cho nhanh và sạch)
    // Nếu |mới - cũ| >= biên độ trễ, thì cập nhật số mới
    if (fabsf(new_value - hysteresis_struct->previous_value) >= hysteresis_struct->hysteresis) {
        hysteresis_struct->previous_value = new_value;
    }

    // 3. Trả về giá trị cũ nếu lệch ít, hoặc giá trị mới nếu lệch nhiều
    return hysteresis_struct->previous_value;
}

