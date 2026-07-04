#include "ui.h"
#include "solder.h"
#include <stdlib.h>
#include <stdio.h>
#include "debug.h"

// Font định nghĩa
#define FONT_TEMP_BIG    &lv_font_montserrat_36
#define FONT_INFO_MID    &lv_font_montserrat_20
#define FONT_LABEL_SMALL &lv_font_montserrat_12

extern volatile sensor_values_struct sensor_values;
#define COLOR_PHASE_A  0xFF9100 // Màu Cam
#define COLOR_PHASE_B  0x00FFFF // Màu Xanh Cyan

static lv_obj_t * screens[SCREEN_COUNT];
static lv_group_t * groups[SCREEN_COUNT];
static lv_chart_series_t * ser_target;
static lv_chart_series_t * ser_limit_up;
static lv_chart_series_t * ser_limit_dn;

static uint32_t overheat_start_time = 0;   // Lưu mốc thời gian bắt đầu quá nhiệt
static float overheat_safety_threshold = 0; // Lưu ngưỡng nhiệt độ an toàn
static bool is_chart_in_alert_mode = false; // Đánh dấu đã đổi màu nền chart chưa
static bool alert_is_active = false; // Chốt chặn quan trọng nhất
static bool is_overheat_latched = false; // Chốt chặn quan trọng


// Thêm biến static để quản lý popup trong ui.c
static lv_obj_t * overheat_msgbox = NULL;

static lv_chart_series_t * ser_now;  // Màu rực (Hiện tại)
static lv_chart_series_t * ser_past; // Màu tối (Quá khứ)
#define COLOR_NOW    0xFFFF00  // Vàng chanh rực (Hiện tại)
#define COLOR_PAST   0x004400  // Xanh lá cực tối (Quá khứ - bóng ma)
#define COLOR_CURSOR 0xFFFFFF  // Con trỏ màu Trắng (Dẫn đầu cho gắt)
#define CHART_POINTS         120       // 5 giây (120 điểm * 40ms)
#define COLOR_LIMIT          0xFF8000  // Màu cam cho đường giới hạn

#define CHART_Y_RANGE_LOW  300  // Nhiệt độ thấp hơn Set Temp
#define CHART_Y_RANGE_HIGH 200 // Nhiệt độ cao hơn Set Temp (Bạn đang để 200)

// --- Objects màn hình Solder ---
static lv_obj_t * lbl_v_val, * lbl_a_val, * lbl_w_val;
static lv_obj_t * lbl_set_temp;
static lv_obj_t * lbl_status_icon;
static lv_obj_t * solder_chart;
static lv_chart_series_t * ser_power;
// Thêm vào chỗ khai báo static cùng với ser_temp
static lv_obj_t * chart_cursor = NULL;
static lv_obj_t * power_bar = NULL;

#define Y_LABEL_SPACE 35  // Khoảng trống dành cho chữ số (0, 100, 200...)
#define BAR_WIDTH     12
#define CHART_WIDTH   (320 - Y_LABEL_SPACE - BAR_WIDTH - 15) // Tự động tính chiều rộng còn lại
#define CHART_HEIGHT  160

typedef enum {
    CHART_MODE_OVERVIEW, // Phase 1: 0-CHART_POINTS điểm, range rộng
    CHART_MODE_FOCUS     // Phase 2: Chạy vòng tròn, range hẹp
} chart_mode_t;

static chart_mode_t current_chart_mode = CHART_MODE_OVERVIEW;
static uint16_t chart_point_idx = 0;

static bool is_running = false;   // Trạng thái Run/Pause
static uint32_t last_move_time = 0;
static lv_timer_t * blink_timer = NULL;

//==================================================

typedef struct {
    lv_obj_t * labels[8];
    uint8_t count;
    char unit_char[4]; // Lưu trữ đơn vị
} ui_fixed_num_t;

static ui_fixed_num_t temp_display;
static ui_fixed_num_t curr_display;
static ui_fixed_num_t volt_display;
static ui_fixed_num_t watt_display;
static ui_fixed_num_t perc_display;


void ui_create_fixed_num(
        ui_fixed_num_t * num, lv_obj_t * parent, const lv_font_t * font,
        uint32_t color, int16_t x_right, int16_t y,
        int16_t spacing, uint8_t count, const char * unit)
{
    num->count = count;
    strncpy(num->unit_char, unit, sizeof(num->unit_char));
    int16_t current_x = x_right;

    // Duyệt ngược từ phải sang trái để bám sát lề phải
    for (int i = count - 1; i >= 0; i--) {
        num->labels[i] = lv_label_create(parent);

        lv_obj_set_style_pad_top(num->labels[i], 4, 0);
        lv_obj_set_style_pad_bottom(num->labels[i], 2, 0);
        lv_obj_set_style_clip_corner(num->labels[i], false, 0);

        // QUAN TRỌNG: Xóa chữ "Text" mặc định của LVGL
        lv_label_set_text(num->labels[i], "");

        lv_obj_set_style_text_color(num->labels[i], lv_color_hex(color), 0);
        lv_obj_set_style_text_align(num->labels[i], LV_TEXT_ALIGN_CENTER, 0);

        int16_t char_width = spacing;

        // Ký tự cuối cùng là ĐƠN VỊ (V, A, W, %, o)
        if (i == count - 1) {
            char_width = 10; // Đơn vị V, A, W chỉ cần 10px chiều rộng
            lv_obj_set_style_text_font(num->labels[i], &lv_font_montserrat_12, 0); // Dùng font 12 cho nhỏ hẳn
            lv_obj_set_pos(num->labels[i], 0, y - 5);
        }
        else if (i == count - 3) {
            char_width = 6; // Dấu chấm ép xuống 6px (vừa đủ nhìn)
            lv_obj_set_style_text_font(num->labels[i], font, 0);
            lv_obj_set_y(num->labels[i], y);
        }
        else {
            lv_obj_set_style_text_font(num->labels[i], font, 0);
            lv_obj_set_y(num->labels[i], y);
        }

        current_x -= char_width;
        lv_obj_set_x(num->labels[i], current_x);
        lv_obj_set_width(num->labels[i], char_width);
    }
}
void ui_update_fixed_num(ui_fixed_num_t * num, const char * str) {
    // str truyền vào chỉ cần là phần số (VD: " 24.1")
    int str_len = strlen(str);

    for (int i = 0; i < num->count; i++) {
        // Nhãn cuối cùng luôn hiển thị đơn vị
        if (i == num->count - 1) {
            if (strcmp(num->unit_char, "o") == 0) {
                lv_label_set_text(num->labels[i], "o"); // Giả lập dấu độ
            } else {
                lv_label_set_text(num->labels[i], num->unit_char);
            }
        }
        // Các nhãn phía trước hiển thị số và dấu chấm
        else {
            if (i < str_len) {
                if (str[i] == ' ') {
                    lv_label_set_text(num->labels[i], ""); // Xóa nhãn nếu là khoảng trắng
                } else {
                    char buf[2] = {str[i], '\0'};
                    lv_label_set_text(num->labels[i], buf);
                }
            } else {
                lv_label_set_text(num->labels[i], ""); // Xóa nhãn nếu chuỗi ngắn hơn dự kiến
            }
        }
    }
}

// Hàm tính gia tốc thông minh
int16_t get_accelerated_diff(int16_t raw_diff) {
    uint32_t now = lv_tick_get();
    uint32_t dt = now - last_move_time;
    last_move_time = now;

    if (dt < 30) return raw_diff * 10; // Xoay cực nhanh -> nhảy 10 đơn vị
    if (dt < 80) return raw_diff * 5;  // Xoay vừa -> nhảy 5 đơn vị
    return raw_diff;                   // Xoay chậm -> nhảy 1 đơn vị
}

// Callback nhấp nháy khi chốt số
static uint32_t last_encoder_tick = 0;
static bool is_setting_temp = false;

static void temp_blink_cb(lv_timer_t * timer) {
    lv_obj_t * obj = (lv_obj_t *)timer->user_data;
    static bool visible = true;

    // Trong LVGL v8, nếu timer bị pause, callback sẽ KHÔNG được gọi.
    // Vì vậy, ta chỉ xử lý việc nháy ở đây.
    // Việc "Chốt" màu ta sẽ làm ở hàm dừng timer (ở bước sau).

    visible = !visible;
    if (visible) {
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_text_color(obj, lv_palette_main(LV_PALETTE_YELLOW), 0);
    } else {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
}

// Thêm hàm này để chốt nhiệt độ
void ui_solder_confirm_temp(void) {
    if(blink_timer) {
      lv_timer_pause(blink_timer);
    }

    if(lbl_set_temp) {
      lv_obj_clear_flag(lbl_set_temp, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_style_text_color(lbl_set_temp, lv_color_hex(0x80FFFF), 0);
    }

    if (solder_chart) {
      int16_t confirmed_t = (int16_t)sensor_values.set_temperature;

      // ÉP CHUYỂN MODE: Khi đã chốt nhiệt độ, ta nên coi như đã vào mode Focus
      // để tránh việc hàm update_chart quay lại dùng range 0-500 của Phase 1
      current_chart_mode = CHART_MODE_FOCUS;

      // Cập nhật Range mới
      lv_chart_set_range(solder_chart, LV_CHART_AXIS_PRIMARY_Y,
                         confirmed_t - CHART_Y_RANGE_LOW,
                         confirmed_t + CHART_Y_RANGE_HIGH);

      ui_solder_update_limits(confirmed_t, 15);
      lv_chart_refresh(solder_chart);
    }
}


// Hàm callback để thay đổi độ mờ (đã nói ở trên)
static void anim_opa_cb(void * var, int32_t v) {
    lv_obj_set_style_opa((lv_obj_t *)var, v, 0);
}

// Hàm dừng mọi animation đang chạy trên icon status
static void stop_status_anim(void) {
    lv_anim_del(lbl_status_icon, (lv_anim_exec_xcb_t)anim_opa_cb);
    lv_obj_set_style_opa(lbl_status_icon, LV_OPA_COVER, 0);
}
// Hàm cập nhật trạng thái UI
void ui_update_status_style(int state) {
    static int last_state = -1;
    if(state == last_state) return; // Tránh khởi tạo lại anim nếu trạng thái không đổi
    last_state = state;

    stop_status_anim();

    if (state == 0) { // SLEEP (Xanh Cyan - Nhịp thở chậm)
      debug_log("SLEEP\r\n");
        lv_label_set_text(lbl_status_icon, LV_SYMBOL_PAUSE);
        lv_obj_set_style_text_color(lbl_status_icon, lv_color_hex(0x00FFFF), 0);

        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, lbl_status_icon);
        lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_30);
        lv_anim_set_time(&a, 1500);
        lv_anim_set_playback_time(&a, 1000);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)anim_opa_cb);
        lv_anim_start(&a);
    }
    else if (state == 1) { // HEATING
      debug_log("HEATING\r\n");
        lv_label_set_text(lbl_status_icon, LV_SYMBOL_CHARGE);
        lv_obj_set_style_text_color(lbl_status_icon, lv_palette_main(LV_PALETTE_ORANGE), 0);

        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, lbl_status_icon);
        lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_20);
        lv_anim_set_time(&a, 800);           // Chậm lại (0.8s mỗi chu kỳ)
        lv_anim_set_playback_time(&a, 400);  // Sáng lại nhanh hơn một chút để tạo nhịp gắt
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)anim_opa_cb);
        lv_anim_start(&a);
    }
    else if (state == 2) { // READY (Xanh lá - Đứng yên)
      debug_log("READY\r\n");
        lv_label_set_text(lbl_status_icon, LV_SYMBOL_OK);
        lv_obj_set_style_text_color(lbl_status_icon, lv_palette_main(LV_PALETTE_GREEN), 0);
        // Không chạy anim, để sáng rực cố định
    }
}

static void solder_screen_event_cb(lv_event_t * e) {
  //debug_log("solder_screen_event_cb()");
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * obj = lv_event_get_target(e);
    static uint32_t press_start_tick = 0;

    // Lấy giá trị hiện tại từ sensor_values
    float target_temp = sensor_values.set_temperature;

    // 1. XỬ LÝ XOAY ENCODER
    if(code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);

        if(key == LV_KEY_RIGHT || key == LV_KEY_UP || key == LV_KEY_LEFT || key == LV_KEY_DOWN) {
            int16_t raw_diff = (key == LV_KEY_RIGHT || key == LV_KEY_UP) ? 1 : -1;
            int16_t final_diff = get_accelerated_diff(raw_diff);
            target_temp += final_diff;

            // Giới hạn nhiệt độ (Ví dụ 50 - 450)
            if(target_temp > 450) target_temp = 450;
            if(target_temp < 20) target_temp = 20;

            // --- QUAN TRỌNG: CẬP NHẬT TRẠNG THÁI "ĐANG CHỈNH" ---
            last_encoder_tick = lv_tick_get(); // Đánh dấu thời điểm vặn cuối cùng
            is_setting_temp = true;            // Dựng cờ đang chỉnh

            // Cập nhật hiển thị con số
            ui_solder_set_target_temp(target_temp);

            // Ép màu VÀNG khi đang vặn
            lv_obj_set_style_text_color(lbl_set_temp, lv_palette_main(LV_PALETTE_YELLOW), 0);

            // Lưu giá trị vào biến hệ thống
            sensor_values.set_temperature = target_temp;

            // Kích hoạt nháy số
            if(blink_timer) {
                lv_timer_reset(blink_timer);
                lv_timer_resume(blink_timer);
            }
        }
    }

    // 2. XỬ LÝ NÚT BẤM
    if(code == LV_EVENT_PRESSED) {
        press_start_tick = lv_tick_get();
    }

    if(code == LV_EVENT_RELEASED) {
        uint32_t duration = lv_tick_elaps(press_start_tick);

        if(duration >= 1200) { // LONG PRESS -> Về Home
            is_running = false;
            //sensor_values.current_state = HALTED;
            change_state(HALTED);
            ui_update_status_style(0);

            lv_group_t * g = lv_obj_get_group(obj);
            if(g) lv_group_set_editing(g, false);

            ui_change_screen(SCREEN_MAIN);
        }
        else if(duration > 50) { // CLICK -> Run/Pause
            is_running = !is_running;

            if(is_running) {
                ui_update_status_style(1); // Chuyển sang Heating (Cam nháy)
                change_state(RUN);
                //sensor_values.current_state = RUN;
            } else {
                ui_update_status_style(0); // Chuyển sang Sleep (Xanh Cyan thở)
                //sensor_values.current_state = SLEEP;
                change_state(SLEEP);
            }
        }
    }
}

// --- Callbacks điều hướng ---
static void btn_event_cb(lv_event_t * e) {
    screen_id_t target_id = (screen_id_t)(uintptr_t)lv_event_get_user_data(e);
    ui_change_screen(target_id);
}


void ui_change_screen(screen_id_t screen_id) {
    if(screen_id < SCREEN_COUNT && screens[screen_id]) {
        lv_scr_load_anim(screens[screen_id], LV_SCR_LOAD_ANIM_FADE_ON, 200, 0, false);

        lv_indev_t * indev = lv_indev_get_next(NULL);
        if(indev && groups[screen_id]) {
            lv_indev_set_group(indev, groups[screen_id]);

            lv_group_focus_freeze(groups[screen_id], false);
            lv_group_focus_obj(screens[screen_id]);

            // Chỉ ép Editing cho màn hình Solder để xoay được ngay
            if(screen_id == SCREEN_SOLDER) {
                lv_group_set_editing(groups[screen_id], true);
            } else {
                lv_group_set_editing(groups[screen_id], false);
            }
        }
    }
}

static void solder_timeout_monitor_cb(lv_timer_t * t) {
    if(is_setting_temp && lv_tick_elaps(last_encoder_tick) > 1500) {
        ui_solder_confirm_temp(); // Gọi hàm chốt màu
        is_setting_temp = false;  // Reset cờ
    }
}

// =========================================================
// KHỞI TẠO MÀN HÌNH SOLDER
// =========================================================
void create_screen_solder(void) {
    screens[SCREEN_SOLDER] = lv_obj_create(NULL);
    groups[SCREEN_SOLDER] = lv_group_create();
    lv_obj_set_style_bg_color(screens[SCREEN_SOLDER], lv_color_hex(0x000000), 0);
    lv_obj_set_style_pad_all(screens[SCREEN_SOLDER], 0, 0);

    // 1. TOP BAR
    lv_obj_t * top_bar = lv_obj_create(screens[SCREEN_SOLDER]);
    lv_obj_set_size(top_bar, 320, 24);
    lv_obj_set_style_bg_color(top_bar, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_border_width(top_bar, 0, 0);
    lv_obj_set_style_radius(top_bar, 0, 0);

    lv_obj_t * lbl_tip = lv_label_create(top_bar);
    lv_label_set_text(lbl_tip, "eStation   c245");
    lv_obj_set_style_text_color(lbl_tip, lv_palette_main(LV_PALETTE_INDIGO), 0);
    lv_obj_align(lbl_tip, LV_ALIGN_LEFT_MID, 5, 0);

    lbl_status_icon = lv_label_create(top_bar);
    lv_label_set_text(lbl_status_icon, LV_SYMBOL_PAUSE);
    lv_obj_align(lbl_status_icon, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_text_font(lbl_status_icon, FONT_INFO_MID, 0);
    lv_obj_set_style_text_color(lbl_status_icon, lv_color_hex(0x00FFFF), 0);

    lbl_set_temp = lv_label_create(top_bar);
    lv_obj_set_style_text_font(lbl_set_temp, FONT_INFO_MID, 0);
    //lv_obj_set_style_text_color(lbl_set_temp, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_color(lbl_set_temp, lv_color_hex(0x90EE90), 0); // Màu Light Green
    lv_obj_align_to(lbl_set_temp, lbl_status_icon, LV_ALIGN_OUT_LEFT_MID, -28, 0);

// ==============================================================================
// 3. CẬP NHẬT BỐ CỤC PHÍA DƯỚI: CHART VÀ CÔNG SUẤT BAR
// ==============================================================================
    power_bar = lv_bar_create(screens[SCREEN_SOLDER]);
    lv_obj_set_size(power_bar, 12, 160); // Rộng 12, Cao 160

    // Căn thanh Bar vào góc Dưới - Phải của màn hình, cách lề 5px
    lv_obj_align(power_bar, LV_ALIGN_BOTTOM_RIGHT, -5, -2);

    lv_bar_set_range(power_bar, 0, 100);
    lv_obj_set_style_bg_color(power_bar, lv_color_hex(0x1A1A1A), LV_PART_MAIN);
    lv_obj_set_style_radius(power_bar, 2, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(power_bar, lv_palette_main(LV_PALETTE_GREEN), LV_PART_INDICATOR);

// ==============================================================================
// 4. CHART AREA - TỐI ƯU HIỆU NĂNG
// ==============================================================================
    solder_chart = lv_chart_create(screens[SCREEN_SOLDER]);
    lv_obj_clear_flag(screens[SCREEN_SOLDER], LV_OBJ_FLAG_SCROLLABLE); // Tắt cuộn
    lv_obj_set_size(solder_chart, CHART_WIDTH, CHART_HEIGHT);
    lv_obj_align(solder_chart, LV_ALIGN_BOTTOM_LEFT, Y_LABEL_SPACE, -2);
    lv_obj_set_style_border_width(solder_chart, 0, 0);
    lv_obj_set_style_pad_all(solder_chart, 0, 0);
    //lv_obj_set_style_pad_left(solder_chart, 0, 0);
    //lv_obj_set_style_pad_right(solder_chart, 0, 0);
    lv_chart_set_axis_tick(solder_chart, LV_CHART_AXIS_PRIMARY_Y, 5, 2, 6, 2, true, 30);

    // 7 đường dọc sẽ tạo ra 6 khoảng trống, mỗi khoảng đúng 25 điểm = 1 giây
    lv_chart_set_div_line_count(solder_chart, 5, 6);
    lv_obj_set_style_anim_speed(solder_chart, 0, 0);

    lv_chart_set_type(solder_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(solder_chart, CHART_POINTS);
    lv_chart_set_range(solder_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 500);
    lv_chart_set_update_mode(solder_chart, LV_CHART_UPDATE_MODE_CIRCULAR);

    lv_obj_set_style_line_width(solder_chart, 1, LV_PART_ITEMS);
    lv_obj_set_style_width(solder_chart, 0, LV_PART_INDICATOR);
    lv_obj_set_style_height(solder_chart, 0, LV_PART_INDICATOR);
    lv_obj_set_style_text_font(solder_chart, FONT_LABEL_SMALL, LV_PART_TICKS);

    lv_obj_set_style_line_color(solder_chart, lv_color_hex(0x151515), LV_PART_MAIN);
    lv_obj_set_style_bg_color(solder_chart, lv_color_hex(0x050505), 0);
    lv_obj_set_style_border_width(solder_chart, 0, 0);

    // 1. Đường Target (Màu trắng mờ)
    ser_target = lv_chart_add_series(solder_chart, lv_color_hex(0x888888), LV_CHART_AXIS_PRIMARY_Y);
    // 2. Đường Limit Trên/Dưới (Màu đỏ/xanh rất mờ)
    ser_limit_up = lv_chart_add_series(solder_chart, lv_color_hex(0xFF9F00), LV_CHART_AXIS_PRIMARY_Y);
    ser_limit_dn = lv_chart_add_series(solder_chart, lv_color_hex(0xFF9F00), LV_CHART_AXIS_PRIMARY_Y);


    //ser_temp = lv_chart_add_series(solder_chart, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y);
    ser_power = lv_chart_add_series(solder_chart, lv_palette_main(LV_PALETTE_GREEN), LV_CHART_AXIS_SECONDARY_Y);

    ser_now = lv_chart_add_series(solder_chart, lv_color_hex(COLOR_NOW), LV_CHART_AXIS_PRIMARY_Y);
    ser_past = lv_chart_add_series(solder_chart, lv_color_hex(COLOR_PAST), LV_CHART_AXIS_PRIMARY_Y);

    for(int i = 0; i < CHART_POINTS; i++) {
        ser_now->y_points[i] = LV_CHART_POINT_NONE;
        ser_past->y_points[i] = LV_CHART_POINT_NONE;
        ser_power->y_points[i] = LV_CHART_POINT_NONE;
    }

    // 2. INFO AREA
    lv_obj_t * info_cont = lv_obj_create(screens[SCREEN_SOLDER]);
    lv_obj_set_size(info_cont, 320, 80);
    lv_obj_set_pos(info_cont, 10, 24);
    lv_obj_set_style_bg_opa(info_cont, 0, 0);
    lv_obj_set_style_border_width(info_cont, 0, 0);
    lv_obj_set_style_pad_all(info_cont, 0, 0);

    lv_obj_t * elec_cont = lv_obj_create(info_cont);
    lv_obj_set_size(elec_cont, 200, 40); // Chỉ cho phép rộng 200px để tránh đè Temp
    lv_obj_align(elec_cont, LV_ALIGN_TOP_LEFT, 0, 5); // Đẩy sát lên trên một chút
    lv_obj_set_style_bg_opa(elec_cont, 0, 0);
    lv_obj_set_style_border_width(elec_cont, 0, 0);
    lv_obj_set_style_pad_all(elec_cont, 0, 0);

  // Volt: Kết thúc tại X=60
  ui_create_fixed_num(&volt_display, elec_cont, FONT_INFO_MID, 0x2196F3, 70, 10, 14, 5, "V");

  // Ampe: Kết thúc tại X=125 (Cách Volt 65px)
  ui_create_fixed_num(&curr_display, elec_cont, FONT_INFO_MID, 0x4CAF50, 135, 10, 14, 5, "A");

  // Watt: Kết thúc tại X=190 (Cách Ampe 65px)
  ui_create_fixed_num(&watt_display, elec_cont, FONT_INFO_MID, 0x9C27B0, 190, 10, 14, 5, "W");

  // --- NHIỆT ĐỘ HIỆN TẠI (Đẩy sát về mép phải 320px) ---
  // Dùng x_right = 318 để sát sạt lề phải
  ui_create_fixed_num(&temp_display, info_cont, FONT_TEMP_BIG, 0xFF4500, 300, 8, 22, 6, "o");

  // 3. Khởi tạo giá trị mặc định (Target 350, Limit +-50)
  ui_solder_update_limits(350, 50);

  // 1. Cho phép màn hình nhận tương tác
  lv_obj_add_flag(screens[SCREEN_SOLDER], LV_OBJ_FLAG_CLICKABLE);

  // 2. Thêm màn hình vào Group (để Encoder biết đường gửi tín hiệu vào)
  lv_group_add_obj(groups[SCREEN_SOLDER], screens[SCREEN_SOLDER]);

  // 3. QUAN TRỌNG: Ép Focus vào màn hình ngay từ đầu
  lv_group_focus_obj(screens[SCREEN_SOLDER]);

  // 4. Gắn callback xử lý sự kiện
  lv_obj_add_event_cb(screens[SCREEN_SOLDER], solder_screen_event_cb, LV_EVENT_ALL, NULL);

  // Khởi tạo timer nháy
  blink_timer = lv_timer_create(temp_blink_cb, 200, lbl_set_temp);
  lv_timer_pause(blink_timer);

  // Timer này chạy mỗi 100ms để kiểm tra việc chốt nhiệt độ
  lv_timer_create(solder_timeout_monitor_cb, 100, NULL);
}

void ui_solder_update_power_bar(uint8_t pc) {
    if(!power_bar) return;
    lv_bar_set_value(power_bar, pc, LV_ANIM_OFF);

    // Đổi màu theo mức độ %
    lv_color_t color = lv_palette_main(LV_PALETTE_GREEN);
    if(pc > 80) color = lv_palette_main(LV_PALETTE_RED);
    else if(pc > 50) color = lv_palette_main(LV_PALETTE_ORANGE);
    else if(pc > 25) color = lv_palette_main(LV_PALETTE_YELLOW);

    lv_obj_set_style_bg_color(power_bar, color, LV_PART_INDICATOR);
}

// --- Placeholder các màn khác ---
void create_screen_scope(void) {
    screens[SCREEN_SCOPE] = lv_obj_create(NULL);
    groups[SCREEN_SCOPE] = lv_group_create();
    lv_obj_t * b = lv_btn_create(screens[SCREEN_SCOPE]);
    lv_obj_center(b);
    lv_obj_add_event_cb(b, btn_event_cb, LV_EVENT_CLICKED, (void*)SCREEN_MAIN);
    lv_group_add_obj(groups[SCREEN_SCOPE], b);
}

void create_screen_meter(void) {
    screens[SCREEN_METER] = lv_obj_create(NULL);
    groups[SCREEN_METER] = lv_group_create();
    lv_obj_t * b = lv_btn_create(screens[SCREEN_METER]);
    lv_obj_center(b);
    lv_obj_add_event_cb(b, btn_event_cb, LV_EVENT_CLICKED, (void*)SCREEN_MAIN);
    lv_group_add_obj(groups[SCREEN_METER], b);
}

// --- INIT ---
void ui_init(void) {
    screens[SCREEN_MAIN] = lv_obj_create(NULL);
    groups[SCREEN_MAIN] = lv_group_create();
    lv_obj_t * cont = lv_obj_create(screens[SCREEN_MAIN]);
    lv_obj_set_size(cont, 320, 240);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    const char * names[] = {"SOLDER", "SCOPE", "METER", "SETTING"};
    screen_id_t ids[] = {SCREEN_SOLDER, SCREEN_SCOPE, SCREEN_METER, SCREEN_METER};

    for(int i = 0; i < 4; i++) {
        lv_obj_t * btn = lv_btn_create(cont);
        lv_obj_set_size(btn, 120, 80);
        lv_obj_t * lbl = lv_label_create(btn);
        lv_label_set_text(lbl, names[i]);
        lv_obj_center(lbl);
        lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)ids[i]);
        lv_group_add_obj(groups[SCREEN_MAIN], btn);
    }
    create_screen_solder();
    create_screen_scope();
    create_screen_meter();

    change_state(HALTED);
    ui_solder_set_target_temp(sensor_values.set_temperature);
    ui_change_screen(SCREEN_MAIN);

}

// --- UPDATE ---
// Hàm bổ trợ chuyển số nguyên thành chuỗi cực nhanh (thay cho sprintf)`
static char* fast_itoa(int val, char* buf, int digits, bool leading_zero) {
    buf[digits] = '\0';
    for (int i = digits - 1; i >= 0; i--) {
        if (!leading_zero && val == 0 && i < digits - 1) buf[i] = ' ';
        else buf[i] = (val % 10) + '0';
        val /= 10;
    }
    return buf + digits;
}

// Thêm các define để dễ điều chỉnh tỉ lệ
#define LIMIT_OFFSET_RATIO_IN   1.5f  // Khi ổn định: Biên đồ thị = 1.5 * Limit
#define LIMIT_OFFSET_RATIO_OUT  0.5f  // Khi vượt ngưỡng: Biên đồ thị = T_thực + 0.5 * Limit

void ui_solder_update_dynamic_range(int16_t current_temp) {
    if(!solder_chart) return;

    int16_t set_t = (int16_t)sensor_values.set_temperature;
    int16_t limit = 15; // Ngưỡng Control Limit
    int16_t y_min, y_max;

    // TRƯỜNG HỢP 1: Nhiệt độ nằm trong vùng Limit (Ổn định)
    if (current_temp <= (set_t + limit) && current_temp >= (set_t - limit)) {
        // Khoảng cách từ Set đến mép = 1.5 * Limit
        // Điều này đảm bảo đường Limit cách mép bằng 1/2 khoảng cách từ Limit đến Set
        y_max = set_t + (int16_t)(limit * 1.5f);
        y_min = set_t - (int16_t)(limit * 1.5f);
    }
    // TRƯỜNG HỢP 2: Nhiệt độ vọt ra ngoài vùng Limit (Biến động)
    else {
        if (current_temp > (set_t + limit)) {
            // Vượt ngưỡng trên: Mép trên = Temp + 0.5 * Limit
            y_max = current_temp + (int16_t)(limit * 0.5f);
            y_min = set_t - (y_max - set_t); // Giữ Set nhiệt ở giữa (đối xứng)
        } else {
            // Vượt ngưỡng dưới: Mép dưới = Temp - 0.5 * Limit
            y_min = current_temp - (int16_t)(limit * 0.5f);
            y_max = set_t + (set_t - y_min); // Giữ Set nhiệt ở giữa
        }
    }

    // Cập nhật Range thực tế vào Chart
    lv_chart_set_range(solder_chart, LV_CHART_AXIS_PRIMARY_Y, y_min, y_max);

    // Cập nhật lại các đường kẻ Target và Limit trên đồ thị để trượt theo dải mới
    ui_solder_update_limits(set_t, limit);
}

void ui_solder_update_dynamic_range_v2(int16_t current_temp) {
    if(!solder_chart) return;

    // Lấy Range hiện tại của Chart để so sánh
    lv_chart_t * chart = (lv_chart_t *)solder_chart;
    int16_t cur_y_min = chart->ymin[LV_CHART_AXIS_PRIMARY_Y];
    int16_t cur_y_max = chart->ymax[LV_CHART_AXIS_PRIMARY_Y];

    int16_t set_t = (int16_t)sensor_values.set_temperature;
    int16_t limit = 15;
    bool need_update = false;

    // 1. KIỂM TRA BIÊN TỨC THỜI: Nếu vọt ra ngoài khung hiện tại thì phải nới khung ngay
    // Thêm biên an toàn 5 độ để tránh việc vừa chạm mép đã nhảy
    if (current_temp > (cur_y_max - 5) || current_temp < (cur_y_min + 5)) {
        need_update = true;
    }

    // 2. KIỂM TRA KHI KẾT THÚC CHU KỲ (Chạm mép phải)
    // chart_point_idx == 0 nghĩa là vừa quay đầu vòng mới
    if (chart_point_idx == 0) {
        need_update = true;
    }

    // 3. THỰC HIỆN CẬP NHẬT TRỤC TỌA ĐỘ
    if (need_update) {
        // Tìm Max/Min thực tế trong dữ liệu để tính toán khung mới
        int16_t data_max = current_temp;
        int16_t data_min = current_temp;

        for (int i = 0; i < CHART_POINTS; i++) {
            if (ser_now->y_points[i] != LV_CHART_POINT_NONE) {
                if (ser_now->y_points[i] > data_max) data_max = ser_now->y_points[i];
                if (ser_now->y_points[i] < data_min) data_min = ser_now->y_points[i];
            }
            if (ser_past->y_points[i] != LV_CHART_POINT_NONE) {
                if (ser_past->y_points[i] > data_max) data_max = ser_past->y_points[i];
                if (ser_past->y_points[i] < data_min) data_min = ser_past->y_points[i];
            }
        }

        // Tính toán dải Y_Max/Y_Min mới (giữ logic đối xứng quanh set_t của bạn)
        int16_t target_max = data_max + (int16_t)(limit * 0.5f);
        int16_t target_min = data_min - (int16_t)(limit * 0.5f);

        // Đảm bảo khung hình tối thiểu luôn bao phủ vùng Set_T +- 1.5*Limit
        int16_t default_upper = set_t + (int16_t)(limit * 1.5f);
        int16_t default_lower = set_t - (int16_t)(limit * 1.5f);

        int16_t final_y_max = (target_max > default_upper) ? target_max : default_upper;
        int16_t final_y_min = (target_min < default_lower) ? target_min : default_lower;

        // Chỉ gọi hàm set_range khi giá trị thực sự khác biệt đáng kể (ví dụ > 2 độ)
        if (abs(final_y_max - cur_y_max) > 2 || abs(final_y_min - cur_y_min) > 2) {
            lv_chart_set_range(solder_chart, LV_CHART_AXIS_PRIMARY_Y, final_y_min, final_y_max);
            ui_solder_update_limits(set_t, limit);
        }
    }
}
void ui_solder_update_dynamic_range_v2_old(int16_t current_temp) {
    if(!solder_chart || !ser_now || !ser_past) return;

    int16_t set_t = (int16_t)sensor_values.set_temperature;
    int16_t limit = 15;

    // Tìm Max/Min trong các điểm đang hiển thị (cả ser_now và ser_past)
    int16_t max_line = current_temp;
    int16_t min_line = current_temp;

    for (int i = 0; i < CHART_POINTS; i++) {
        // Kiểm tra series hiện tại
        if (ser_now->y_points[i] != LV_CHART_POINT_NONE) {
            if (ser_now->y_points[i] > max_line) max_line = ser_now->y_points[i];
            if (ser_now->y_points[i] < min_line) min_line = ser_now->y_points[i];
        }
        // Kiểm tra series quá khứ (bóng ma)
        if (ser_past->y_points[i] != LV_CHART_POINT_NONE) {
            if (ser_past->y_points[i] > max_line) max_line = ser_past->y_points[i];
            if (ser_past->y_points[i] < min_line) min_line = ser_past->y_points[i];
        }
    }

    // Tính toán mép trên (Max của 2 điều kiện)
    int16_t cond_a_top = set_t + (int16_t)(limit * 1.5f);
    int16_t cond_b_top = max_line + (int16_t)(limit * 0.5f);
    int16_t y_max = (cond_a_top > cond_b_top) ? cond_a_top : cond_b_top;

    // Tính toán mép dưới (Min của 2 điều kiện)
    int16_t cond_a_bot = set_t - (int16_t)(limit * 1.5f);
    int16_t cond_b_bot = min_line - (int16_t)(limit * 0.5f);
    int16_t y_min = (cond_a_bot < cond_b_bot) ? cond_a_bot : cond_b_bot;

    // Cập nhật range
    lv_chart_set_range(solder_chart, LV_CHART_AXIS_PRIMARY_Y, y_min, y_max);

    // Cập nhật các đường Limit màu đỏ
    ui_solder_update_limits(set_t, limit);
}

void ui_solder_set_target_temp(int16_t new_set_t) {
    if(!screens[SCREEN_SOLDER]) return;
    static int16_t last_ui_set_t = -1;

    if (new_set_t != last_ui_set_t) {
        // ... (Giữ nguyên phần format chuỗi fast_itoa của bạn) ...
        char s_buf[8];
        s_buf[0] = 's'; s_buf[1] = ':'; s_buf[2] = ' ';
        s_buf[3] = (new_set_t / 100) + '0';
        s_buf[4] = ((new_set_t / 10) % 10) + '0';
        s_buf[5] = (new_set_t % 10) + '0';
        s_buf[6] = '\0';
        lv_label_set_text(lbl_set_temp, s_buf);

        // --- PHẦN FIX CHO PHASE 2 ---
//        if (current_chart_mode == CHART_MODE_FOCUS) {
//            // Cập nhật lại trục Y của Chart theo Set Temp mới
//            lv_chart_set_range(solder_chart, LV_CHART_AXIS_PRIMARY_Y,
//                               new_set_t - CHART_Y_RANGE_LOW,
//                               new_set_t + CHART_Y_RANGE_HIGH);
//
//            // Cập nhật lại 3 đường kẻ (Target, Limit Up, Limit Dn) trượt theo
//            ui_solder_update_limits(new_set_t, 15);
//        }
        // ----------------------------

        last_ui_set_t = new_set_t;
    }

    // ... (Giữ nguyên phần cập nhật icon status bên dưới) ...
//    if (!is_running) {
//        ui_update_status_style(0);
//        change_state(SLEEP);
//    } else {
//        ui_update_status_style(1);
//        change_state(RUN);
//    }
}

void ui_solder_update_values(float v, float a, float p, float cur_t, int16_t set_t) {
  //debug_log("ui_solder_update_values()");
    if(!screens[SCREEN_SOLDER]) return;

    static int16_t l_v = -1, l_a = -1, l_p = -1;
    static int32_t l_ct_fixed = -1;
    char buf[16];

    // --- LUÔN CẬP NHẬT SỐ LIỆU (Bất kể Run hay Pause) ---

    // 1. Volt
    int16_t v_fix = (int16_t)(v * 10);
    if (v_fix != l_v) {
        snprintf(buf, sizeof(buf), "%2d.%d", v_fix / 10, abs(v_fix % 10));
        ui_update_fixed_num(&volt_display, buf);
        l_v = v_fix;
    }

    // 2. Ampe
    int16_t a_fix = (int16_t)(a * 100);
    if (a_fix != l_a) {
        snprintf(buf, sizeof(buf), "%1d.%02d", a_fix / 100, abs(a_fix % 100));
        ui_update_fixed_num(&curr_display, buf);
        l_a = a_fix;
    }

    // 3. Watt
    int16_t p_fix = (int16_t)(p * 10);
    if (p_fix != l_p) {
        snprintf(buf, sizeof(buf), "%2d.%d", p_fix / 10, abs(p_fix % 10));
        ui_update_fixed_num(&watt_display, buf);
        l_p = p_fix;
    }

    // 4. Nhiệt độ hiện tại (Quan trọng nhất)
    int32_t cur_t_fixed = (int32_t)(cur_t * 10.0f);
    if (cur_t_fixed != l_ct_fixed) {
        snprintf(buf, sizeof(buf), "%3d.%d", (int)(cur_t_fixed / 10), abs((int)(cur_t_fixed % 10)));
        ui_update_fixed_num(&temp_display, buf);
        l_ct_fixed = cur_t_fixed;
    }

    // 2. GIỮ NGUYÊN LOGIC AUTO-CONFIRM ENCODER
    // Nếu đang chỉnh nhiệt độ mà buông tay sau 1.5s thì tự xác nhận
    if(is_setting_temp && (lv_tick_get() - last_encoder_tick > 1500)) {
        ui_solder_confirm_temp();
        is_setting_temp = false;
    }

    // 3. QUẢN LÝ ICON TRẠNG THÁI DỰA TRÊN STATE MACHINE

    // TRƯỜNG HỢP QUÁ NHIỆT (EMERGENCY_SLEEP)
    if (sensor_values.current_state == EMERGENCY_SLEEP) {
        // Khóa chặt icon Warning đỏ, dừng mọi animation nháy
        stop_status_anim();
        lv_label_set_text(lbl_status_icon, LV_SYMBOL_WARNING);
        lv_obj_set_style_text_color(lbl_status_icon, lv_palette_main(LV_PALETTE_RED), 0);

        // Thoát sớm tại đây để các logic hiển thị SLEEP/RUN bên dưới
        // không thể ghi đè lên icon Warning này.
        return;
    }

    // TRƯỜNG HỢP NGHỈ (SLEEP hoặc HALTED)
    if (sensor_values.current_state == SLEEP || sensor_values.current_state == HALTED) {
        ui_update_status_style(0); // Icon Pause (Xanh Cyan)
    }
    // TRƯỜNG HỢP ĐANG HOẠT ĐỘNG (RUN hoặc STANDBY)
    else {
        int16_t diff = abs((int16_t)cur_t - set_t);
        if (diff <= 5) {
            ui_update_status_style(2); // READY (Xanh lá)
        } else {
            ui_update_status_style(1); // HEATING (Cam nháy)
        }
    }
}
void ui_solder_update_limits(int16_t set_t, int16_t limit) {
    if(!ser_target || !ser_limit_up || !ser_limit_dn) return;

    for(int i = 0; i < CHART_POINTS; i++) {
        ser_target->y_points[i]   = set_t;
        ser_limit_up->y_points[i] = set_t + limit;
        ser_limit_dn->y_points[i] = set_t - limit;
    }
    // Không cần gọi refresh ở đây vì hàm update_chart sẽ gọi sau cùng
}

void ui_solder_update_chart_phase2(int16_t current_temp) {
    if(!solder_chart || !ser_now || !ser_past) return;

    // 1. Xóa điểm quá khứ tại vị trí này để lộ ra màu của ser_now và tránh bị đè
    ser_past->y_points[chart_point_idx] = LV_CHART_POINT_NONE;

    // 2. Ghi điểm nhiệt độ hiện tại (Màu vàng rực)
    ser_now->y_points[chart_point_idx] = current_temp;

    // 3. Cập nhật Range Dynamic (Max/Min logic của bạn)
    if (!is_setting_temp) {
      ui_solder_update_dynamic_range_v2(current_temp);
    }

    // 4. Tính toán Cursor
    lv_chart_t * chart_data = (lv_chart_t *)solder_chart;
    int32_t y_min = chart_data->ymin[LV_CHART_AXIS_PRIMARY_Y];
    int32_t y_max = chart_data->ymax[LV_CHART_AXIS_PRIMARY_Y];
    int32_t y_span = y_max - y_min;
    if(y_span <= 0) y_span = 1;

    int32_t chart_h = lv_obj_get_content_height(solder_chart);
    int32_t chart_w = lv_obj_get_content_width(solder_chart);

    int32_t x_pixel = (chart_point_idx * chart_w) / (CHART_POINTS - 1);
    int32_t y_pixel = chart_h - (((int32_t)current_temp - y_min) * chart_h) / y_span;

    if(chart_cursor) {
        // Cập nhật vị trí cursor dẫn đầu
        lv_obj_align(chart_cursor, LV_ALIGN_TOP_LEFT, x_pixel - 3, y_pixel - 3);
        // Đưa cursor lên trên cùng của các đối tượng con trong chart nếu cần
        lv_obj_move_foreground(chart_cursor);
    }

    // 5. Quản lý vòng lặp quét
    chart_point_idx++;
    if (chart_point_idx >= CHART_POINTS) {
        chart_point_idx = 0;
        // Hết vòng: Chuyển toàn bộ Now sang Past (làm mờ đi)
        for(int i = 0; i < CHART_POINTS; i++) {
            ser_past->y_points[i] = ser_now->y_points[i];
            ser_now->y_points[i] = LV_CHART_POINT_NONE;
        }
    }

    // Cuối cùng mới vẽ lại toàn bộ
    lv_chart_refresh(solder_chart);
}


void ui_solder_update_chart(int16_t current_temp, uint8_t power_pc) {
    if(!solder_chart || !ser_now || !ser_past) return;

    // --- 1. KHỞI TẠO CURSOR (Nếu chưa có) ---
    if(chart_cursor == NULL) {
        chart_cursor = lv_obj_create(solder_chart);
        lv_obj_set_size(chart_cursor, 6, 6);
        lv_obj_set_style_radius(chart_cursor, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(chart_cursor, lv_color_hex(0xFFFFFF), 0); // Trắng
        lv_obj_set_style_border_width(chart_cursor, 1, 0);
        lv_obj_set_style_border_color(chart_cursor, lv_color_hex(0xFFFF00), 0); // Viền vàng
        lv_obj_clear_flag(chart_cursor, LV_OBJ_FLAG_CLICKABLE);
    }

    // Lấy kích thước vùng vẽ
    int32_t chart_h = lv_obj_get_content_height(solder_chart);
    int32_t chart_w = lv_obj_get_content_width(solder_chart);

    if (current_chart_mode == CHART_MODE_OVERVIEW) {
        // --- PHASE 1: CHẾ ĐỘ TOÀN CẢNH ---

        ser_now->y_points[chart_point_idx] = current_temp;
        ser_past->y_points[chart_point_idx] = LV_CHART_POINT_NONE;

        // TÍNH TOÁN TỌA ĐỘ CURSOR CHO PHASE 1
        // Giả sử range mặc định lúc khởi tạo là 0 - 500
        int32_t y_min = 0;
        int32_t y_max = 500;
        int32_t y_span = y_max - y_min;

        int32_t x_pixel = (chart_point_idx * chart_w) / (CHART_POINTS - 1);
        int32_t y_pixel = chart_h - (((int32_t)current_temp - y_min) * chart_h) / y_span;

        // Giới hạn trong khung
        if(y_pixel < 0) y_pixel = 0;
        if(y_pixel > chart_h) y_pixel = chart_h;

        lv_obj_align(chart_cursor, LV_ALIGN_TOP_LEFT, x_pixel - 3, y_pixel - 3);

        chart_point_idx++;

        if (chart_point_idx >= CHART_POINTS) {
            // CHUYỂN SANG PHASE 2
            current_chart_mode = CHART_MODE_FOCUS;
            int16_t current_set = (int16_t)sensor_values.set_temperature;

            // Set range Focus dùng các define chung của mình
            lv_chart_set_range(solder_chart, LV_CHART_AXIS_PRIMARY_Y,
                               current_set - CHART_Y_RANGE_LOW,
                               current_set + CHART_Y_RANGE_HIGH);

            ui_solder_update_limits(current_set, 15);

            for(int i = 0; i < CHART_POINTS; i++) {
                ser_past->y_points[i] = ser_now->y_points[i];
                ser_now->y_points[i] = LV_CHART_POINT_NONE;
            }
            chart_point_idx = 0;
        }
    } else {
        // --- PHASE 2: CHẾ ĐỘ QUÉT VÒNG TRÒN (Đã có logic xóa cũ vẽ mới) ---
        ui_solder_update_chart_phase2(current_temp);
    }

    lv_chart_refresh(solder_chart);
}

/**
 * @brief Cập nhật thanh Bar công suất PWM
 * @param percentage: Giá trị từ 0 đến 100
 */
/**
 * API cập nhật thanh công suất PWM
 */
void ui_solder_set_power(float percentage) {
    if (power_bar == NULL) return;

    if (percentage > 100) percentage = 100;
    lv_bar_set_value(power_bar, percentage, LV_ANIM_OFF);

    // Đổi màu theo %
    if (percentage > 80) {
        lv_obj_set_style_bg_color(power_bar, lv_palette_main(LV_PALETTE_RED), LV_PART_INDICATOR);
    } else if (percentage > 40) {
        lv_obj_set_style_bg_color(power_bar, lv_palette_main(LV_PALETTE_ORANGE), LV_PART_INDICATOR);
    } else {
        lv_obj_set_style_bg_color(power_bar, lv_palette_main(LV_PALETTE_GREEN), LV_PART_INDICATOR);
    }
}


static void overheat_monitor_timer_cb(lv_timer_t * t) {
    // 1. Xử lý đóng Popup sau 3 giây
    if (overheat_msgbox != NULL && lv_tick_elaps(overheat_start_time) >= 3000) {
        lv_msgbox_close(overheat_msgbox);
        overheat_msgbox = NULL;

        // Sau khi đóng popup, PHẢI giữ nền đỏ hoặc chuyển sang màu đỏ tối để báo hiệu vẫn đang nóng
        if (solder_chart) {
            lv_obj_set_style_bg_color(solder_chart, lv_color_make(60, 0, 0), 0); // Đỏ tối
            is_chart_in_alert_mode = true;
        }
    }

    // 2. Xử lý khôi phục khi nhiệt độ đã an toàn (solder.c đổi sang SLEEP)
    if (sensor_values.current_state != EMERGENCY_SLEEP) {
        // Trả lại nền đen cho Chart
        if (solder_chart) {
            lv_obj_set_style_bg_color(solder_chart, lv_color_hex(0x000000), 0);
            is_chart_in_alert_mode = false;
        }

        // Nếu popup chưa kịp đóng (nhiệt hạ quá nhanh) thì đóng nốt
        if (overheat_msgbox != NULL) {
            lv_msgbox_close(overheat_msgbox);
            overheat_msgbox = NULL;
        }

        lv_timer_del(t); // Xóa timer
        debug_log("Overheat UI: Recovered to normal.");
    }
}

void ui_solder_trigger_overheat(float target_temp) {
    if (overheat_msgbox != NULL) return;

    overheat_start_time = lv_tick_get();

    // Đổi màu nền chart sang đỏ rực ngay khi trigger
    if (solder_chart) {
        lv_obj_set_style_bg_color(solder_chart, lv_palette_main(LV_PALETTE_RED), 0);
        is_chart_in_alert_mode = true;
    }

    static const char * btns[] = {""};
    overheat_msgbox = lv_msgbox_create(NULL, LV_SYMBOL_WARNING " OVERHEAT",
                                     "Critical Temp! System Halted.\nWaiting for cooling...",
                                     btns, false);
    lv_obj_center(overheat_msgbox);
    lv_obj_set_style_bg_color(overheat_msgbox, lv_palette_main(LV_PALETTE_RED), 0);

    lv_timer_create(overheat_monitor_timer_cb, 200, NULL);
}
