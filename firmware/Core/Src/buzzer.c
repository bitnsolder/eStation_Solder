#include "buzzer.h"

extern TIM_HandleTypeDef htim8;

static uint32_t b_last_tick = 0;
static uint32_t b_interval = 50; // Nhịp chuẩn 100ms
static uint8_t  b_n_remaining = 0;
static uint8_t  b_is_silent = 0;
static uint8_t  b_is_active = 0;

void set_tone(float frequency, float time_ms) {
    uint32_t arr = (uint32_t)(1000000.0f / frequency) - 1;
    TIM8->ARR = (arr > 65535) ? 65535 : arr;
    b_interval = (uint32_t)time_ms; // Cập nhật nhịp theo yêu cầu
    TIM8->EGR |= TIM_EGR_UG;
}

void beep_n(float buzzer_enabled, uint8_t n) {
    if (buzzer_enabled <= 0 || n == 0) return;

    TIM8->CCR4 = (uint32_t)((TIM8->ARR / 2) * buzzer_enabled);

    b_n_remaining = n;
    b_is_silent = 0;
    b_is_active = 1;
    b_last_tick = HAL_GetTick(); // Đánh dấu mốc thời gian bắt đầu

    HAL_TIMEx_PWMN_Start(&htim8, TIM_CHANNEL_4);
}

// Giữ nguyên interface cho bạn
void beep(float buzzer_enabled) { beep_n(buzzer_enabled, 1); }
void beep_double(float buzzer_enabled) { beep_n(buzzer_enabled, 2); }

/**
 * @brief HÀM UPDATE: Gọi trong while(1)
 */
void buzzer_update(void) {
    if (!b_is_active) return;

    // Kiểm tra xem đã đến lúc chuyển trạng thái chưa (sau 100ms)
    if (HAL_GetTick() - b_last_tick >= b_interval) {
        b_last_tick = HAL_GetTick(); // Reset mốc thời gian

        if (b_is_silent == 0) {
            // Đang kêu -> Chuyển sang tắt
            HAL_TIMEx_PWMN_Stop(&htim8, TIM_CHANNEL_4);
            b_n_remaining--;

            if (b_n_remaining > 0) {
                b_is_silent = 1;
            } else {
                b_is_active = 0; // Xong toàn bộ chu trình
            }
        } else {
            // Đang nghỉ -> Chuyển sang kêu
            HAL_TIMEx_PWMN_Start(&htim8, TIM_CHANNEL_4);
            b_is_silent = 0;
        }
    }
}
