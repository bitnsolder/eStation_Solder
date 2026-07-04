#ifndef UI_H
#define UI_H

#include "lvgl/lvgl.h"

// ID màn hình
typedef enum {
    SCREEN_MAIN = 0,
    SCREEN_SOLDER,
    SCREEN_SCOPE,
    SCREEN_METER,
    SCREEN_SETTING,
    SCREEN_COUNT
} screen_id_t;

// Khởi tạo và điều hướng
void ui_init(void);
void ui_change_screen(screen_id_t screen_id);

// --- API Cập Nhật Dữ Liệu ---
// 1. Cập nhật các con số (Volt, Curr, Power, Temp_Cur, Temp_Set)
void ui_solder_update_values(float v, float a, float p, float cur_t, int16_t set_t);

// 2. Đẩy dữ liệu vào đồ thị (Nhiệt độ thực, % công suất)
void ui_solder_update_chart(int16_t current_temp, uint8_t power_pc);
void ui_solder_update_limits(int16_t set_t, int16_t limit);
void ui_solder_set_target_temp(int16_t new_set_t);
void ui_scope_update(int16_t adc_val);
void ui_solder_set_power(float percentage);
void ui_update_status_style(int state);
void ui_solder_trigger_overheat(float target_temp);
#endif
