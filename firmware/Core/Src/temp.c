/*
 * temp.c
 *
 *  Created on: Mar 20, 2026
 *      Author: tuant
 */


#include "temp.h"
#include "debug.h"
//
//static lv_color_t buf1[320 * 40];
//static lv_color_t buf2[320 * 40];
extern lv_disp_drv_t *disp_drv_p_flush;
extern volatile bool is_flushing; // Khai báo extern từ tft.c
extern TIM_HandleTypeDef htim3;
extern SPI_HandleTypeDef hspi2;

int32_t last_counter_value = 0;
lv_indev_state_t encoder_state = LV_INDEV_STATE_REL;


uint32_t log_timer = 0;   // ĐÃ KHAI BÁO Ở ĐÂY
/* Biến đo tốc độ */
uint32_t flush_count;   // Đếm số lần gọi hàm flush
uint32_t last_log_time = 0; // Hẹn giờ 1 giây để in log
extern ADC_HandleTypeDef hadc1;

// Hàm lấy random trong khoảng [min, max]
int get_hardware_random(int min, int max) {
    return (rand() % (max - min + 1)) + min;
}

void temp_main(){
  // 1. Quản lý UI LVGL
  //lv_timer_handler();

  // 2. Heartbeat LED (Nháy mỗi 500ms)
  //Heartbeat();

  // 4. test temporary solution
  //temp();

}

void ST7789_DrawImage(const uint16_t *img)
{
    ST7789_SetAddr(0,0,239,319);

    CS_LOW();
    DC_HIGH();

    for(int y=0;y<320;y++)
    {
        HAL_SPI_Transmit(&hspi2,
                        (uint8_t*)&img[y*240],
                        240*2,
                        HAL_MAX_DELAY);
    }

    CS_HIGH();
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
//
//            //My_Log("Step OK! Time: %lu | Diff: %d\r\n", now, data->enc_diff);
//            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_6);
//        } else {
//            // Có xoay nhưng chưa đủ 0.5s -> Chặn đứng, không cho nhảy menu
//            data->enc_diff = 0;
//            // Không cập nhật last_cnt ở đây để "dồn tích" nếu vặn tiếp
//            //My_Log("Blocked! Waiting for 1s...\r\n");
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


float read_chip_temp(void) {
    // 1. Các địa chỉ lưu giá trị hiệu chuẩn của hãng (tra datasheet G474)
    // TS_CAL1: Giá trị ADC tại 30°C
    // TS_CAL2: Giá trị ADC tại 110°C
    #define TS_CAL1 *((uint16_t*)((uint32_t)0x1FFF75A8))
    #define TS_CAL2 *((uint16_t*)((uint32_t)0x1FFF75CA))
    #define VREFINT_CAL *((uint16_t*)((uint32_t)0x1FFF75AA))

    // 2. Đọc ADC kênh cảm biến nhiệt
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 10);
    uint32_t adc_val = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);

    // 3. Công thức nội suy từ giá trị Calibration của ST
    float temp = ((110.0f - 30.0f) / (float)(TS_CAL2 - TS_CAL1)) * (float)(adc_val - TS_CAL1) + 30.0f;

    return temp;
}
//
//void temp(void) {
//  uint32_t now = HAL_GetTick();
//  static uint32_t last_solder_update = 0;
//  if(HAL_GetTick() - last_solder_update > 35) {
//    last_solder_update = HAL_GetTick();
//
//    // -- BỘ SỐ FAKE CHUẨN ĐỂ TEST UI (Pixel Perfect) --
////    float mock_v = 24.1f;
////    float mock_a = 1.25f;
////    float mock_w = 30.1f;
//    int16_t mock_target_t = 350;
//
//    // Đọc nhiệt độ chip thực tế để làm số "Current Temp" nhảy cho vui mắt
//    //float real_chip_temp = read_chip_temp();
//
//    // Giả lập logic READY/HEAT để đổi màu số
//    //int16_t mock_cur_t = (real_chip_temp > 35) ? 350 : (int16_t)real_chip_temp + 300;
//
//    // --- GỌI BỘ API MỚI ---
//    // 1. Cập nhật các con số lên màn hình
//    ui_solder_update_values(get_hardware_random(19,24), get_hardware_random(1,10), get_hardware_random(1,99), get_hardware_random(340,360), mock_target_t);
//
//    // 2. Đẩy dữ liệu vào Chart (Nhiệt độ thực, % Power)
//    ui_solder_update_chart(get_hardware_random(340,360), get_hardware_random(1,99));
//  }
//  if (now - log_timer >= 1000) {
//      log_timer = now;
//
//      // Lấy chỉ số rảnh rỗi của LVGL (0 = Quá tải, 100 = Rảnh tuyệt đối)
//      int cpu_idle = lv_timer_get_idle();
//
//      //My_Log("Flush Rate: %lu/s | CPU Idle: %d%%\r\n", flush_count, cpu_idle);
//      debug_log("Flush Rate: %lu/s | CPU Idle: %d%%\r\n", flush_count, cpu_idle);
//      flush_count = 0; // Reset bộ đếm
//  }
//
//}

//void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi) {
//    if(hspi == &hspi2) {
//        // 1. Nâng CS lên báo kết thúc truyền dữ liệu
//        HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);
//
//        // 2. Giải phóng cờ bận
//        is_flushing = false;
//
//        // 3. Báo cho LVGL biết đã sẵn sàng cho lượt tiếp theo
//        if(disp_drv_p_flush != NULL) {
//            lv_disp_flush_ready(disp_drv_p_flush);
//        }
//    }
//}
void temp_init() {
//  // 1. Hardware Start
//  HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL);
//  ST7789_Init();
//
//  // 2. LVGL Core Init
//  lv_init();
//
//  // 3. Display Buffer & Driver
//  static lv_disp_draw_buf_t draw_buf;
//  //static lv_color_t buf1[320 * 20];
//  lv_disp_draw_buf_init(&draw_buf, buf1, buf2, 320 * 40);
//
//  static lv_disp_drv_t disp_drv;
//  lv_disp_drv_init(&disp_drv);
//  disp_drv.hor_res = 320;
//  disp_drv.ver_res = 240;
//  disp_drv.flush_cb = my_disp_flush;
//  disp_drv.draw_buf = &draw_buf;
//  Debug_Init(&disp_drv);
//  lv_disp_drv_register(&disp_drv);
//
//  // 4. Input Driver (Encoder)
//  static lv_indev_drv_t indev_drv;
//  lv_indev_drv_init(&indev_drv);
//  indev_drv.type = LV_INDEV_TYPE_ENCODER;
//  indev_drv.read_cb = encoder_read;
//  lv_indev_drv_register(&indev_drv);
//
//  // 5. UI Init (Gọi hàm mình vừa viết)
//  ui_init();
}
