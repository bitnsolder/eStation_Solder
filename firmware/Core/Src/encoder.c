/*
 * encoder.c
 *
 *  Created on: Mar 21, 2026
 *      Author: tuant
 */
#include "encoder.h"

extern TIM_HandleTypeDef htim3;


void encoder_read(lv_indev_drv_t * drv, lv_indev_data_t * data)
{
    static int16_t last_cnt = 0;

    // 1. Lấy giá trị thô từ Timer (Đã bật Filter 15 trong CubeMX)
    int16_t current_cnt = (int16_t)__HAL_TIM_GET_COUNTER(&htim3);
    int16_t diff = current_cnt - last_cnt;

    // 2. Trả về diff ngay lập tức, không dùng Delay/Timeout ở đây
    if (diff != 0) {
        data->enc_diff = diff;
        last_cnt = current_cnt;
    } else {
        data->enc_diff = 0;
    }

    // 3. Nút bấm
    data->state = (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_3) == GPIO_PIN_RESET) ?
                  LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
}

//
//void encoder_read(lv_indev_drv_t * drv, lv_indev_data_t * data)
//{
//    static int16_t last_cnt = 0;
//    static uint32_t last_move_time = 0;
//    uint32_t now = HAL_GetTick();
//
//    // 1. Lấy giá trị hiện tại từ Timer
//    int16_t current_cnt = (int16_t)__HAL_TIM_GET_COUNTER(&htim3);
//    int16_t diff = current_cnt - last_cnt;
//
//    // 2. Logic chốt chặn: Chỉ xử lý nếu có sự thay đổi VÀ đã qua 500ms
//    if (diff != 0) {
//        if (now - last_move_time > 1000) {
//            // Có chuyển động và đủ thời gian chờ -> Cho phép nhảy 1 nấc
//            data->enc_diff = (diff > 0) ? 1 : -1;
//
//            last_move_time = now;
//            // Cập nhật mốc đếm để tính diff lần sau
//            last_cnt = current_cnt;
//        } else {
//            // Có xoay nhưng chưa đủ 0.5s -> Chặn đứng, không cho nhảy menu
//            data->enc_diff = 0;
//        }
//    } else {
//        data->enc_diff = 0;
//    }
//
//    // 3. Xử lý nút bấm (Switch) - Giữ nguyên logic đơn giản cho nhạy
//    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_3) == GPIO_PIN_RESET) {
//        data->state = LV_INDEV_STATE_PR;
//    } else {
//        data->state = LV_INDEV_STATE_REL;
//    }
//}

