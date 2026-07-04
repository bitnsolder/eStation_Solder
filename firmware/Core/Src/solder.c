#include "solder.h"
#include "main.h"
#include "string.h"
#include "pid.h"
#include "moving_average.h"
#include "hysteresis.h"
#include "type_packers.h"
#include "buzzer.h"
#include "debug.h"
#include "flash.h"
#include <math.h>
#include <stdio.h>
#include <float.h>
#include <stdbool.h>
#include "tft.h"
#include "lvgl/lvgl.h"
#include "ui.h"
#include "encoder.h"
#include "ntc.h"
#include "qlog.h"

/* Cartridge type specific PID parameters */
#define KP_NT115          5
#define KI_NT115          2
#define KD_NT115          0.3
#define MAX_I_NT115         300

#define KP_T210           7
#define KI_T210           4
#define KD_T210           0.3
#define MAX_I_T210          300

//#define KP_T245           8
//#define KI_T245           2
//#define KD_T245           0.5
//#define MAX_I_T245          300
#define KP_T245    8.0f
#define KI_T245    19.0f
#define KD_T245    0.1f
#define MAX_I_T245 100.0f

#define KP_No_name          8
#define KI_No_name          2
#define KD_No_name          0.5
#define MAX_I_No_name         300


/* General PID parameters */
float PID_NEG_ERROR_I_MULT = 7;   // Axxsolder: 7, cân nhắc 3
float PID_NEG_ERROR_I_BIAS = 1;   // Axxsolder: 1, cân nhắc 0, thực tế ko dùng

extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim6;
extern TIM_HandleTypeDef htim17;
//extern HRTIM_HandleTypeDef hhrtim1;
extern TIM_HandleTypeDef htim1;
extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;
extern UART_HandleTypeDef huart1;
extern DAC_HandleTypeDef hdac2;
extern ADC_HandleTypeDef hadc3;
extern ADC_HandleTypeDef hadc4;
extern DMA_HandleTypeDef hdma_adc3;
static lv_color_t buf1[320 * 40];
//static lv_color_t buf2[320 * 40];

volatile uint16_t pid_flag = 0;
volatile uint16_t ui_flag = 0;

#define ADC2_DMA_BUF_SIZE 5
volatile uint16_t adc2_dma_buf_raw[ADC2_DMA_BUF_SIZE];

#define ADC4_DMA_BUF_SIZE 3
volatile uint16_t adc4_dma_buf_raw[ADC4_DMA_BUF_SIZE];

float current_temp = 0.0f;   // Nhiệt độ thực tế sau khi tính toán
volatile float pwm_duty_cycle = 0.0f;

/* Sets the duty cycle of timer controlling the heater */
void set_heater_duty(uint32_t dutycycle){
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, dutycycle);
}


void InitDisplay() {
  // 1. Hardware Start
  HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL);

  // 2. TFT, LVGL Core Init
  ST7789_Init();
  lv_init();

  // 3. Display Buffer & Driver
  static lv_disp_draw_buf_t draw_buf;
  //lv_disp_draw_buf_init(&draw_buf, buf1, buf2, 320 * 40);
  lv_disp_draw_buf_init(&draw_buf, buf1, NULL, 320 * 40);

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = 320;
  disp_drv.ver_res = 240;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  // 4. Input Driver (Encoder)
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_ENCODER;
  indev_drv.read_cb = encoder_read;
  indev_drv.long_press_time = 1200;
  indev_drv.long_press_repeat_time = 200;
  lv_indev_drv_register(&indev_drv);

  // 5. UI Init
  ui_init();
}

//===========================================================================


/* Timing constants */
uint32_t previous_millis_display = 0;
uint32_t interval_display = 15;

uint32_t previous_millis_debug = 0;
uint32_t interval_debug = 50;

uint32_t previous_millis_heating_halted_update = 0;
uint32_t interval_heating_halted_update = 500;

uint32_t previous_millis_left_stand = 0;

uint32_t previous_millis_standby = 0;

uint32_t previous_measure_current_update = 0;
uint32_t interval_measure_current = 250;

uint32_t previous_sensor_update_high_update = 0;
uint32_t interval_sensor_update_high_update = 10;

uint32_t previous_sensor_update_low_update = 0;
uint32_t interval_sensor_update_low_update = 250;

uint32_t previous_millis_popup = 0;
uint32_t interval_popup = 2000;

uint32_t previous_stand_debounce = 0;
uint32_t interval_stand_debounce = 100;

cartridge_state_t cartridge_state = ATTACHED;
cartridge_state_t previous_cartridge_state = ATTACHED;

/* power sources */
power_source_t power_source = POWER_DC;

/* Custom tuning parameters */
float Kp_tuning = 0;
float Ki_tuning = 0;
float Kd_tuning = 0;
float temperature_tuning = 100;
float PID_MAX_I_LIMIT_tuning = 0;

/* Allow use of custom temperatue, used for tuning */
uint8_t custom_temperature_on = 0;

/* ADC Buffer */
#define ADC1_BUF_LEN 57 //3*19
uint16_t ADC1_BUF[ADC1_BUF_LEN];

volatile uint16_t ntc_adc_raw[1]; // Buffer cho DMA
float current_cold_junction_temp = 25.0f; // Giá trị nhiệt độ cuối cùng


/* RAW ADC from current measurement */
uint16_t adc2_dma_buffer[2]; // [0] = channel 2, [1] = channel 10
uint16_t current_raw = 0;
uint16_t current_leak = 0;

/* Stand toggle variables */
uint8_t previous_stand_status = 0;
uint8_t stand_debounced_flag = 0;

/* Thermocouple temperature */
float TC_temp = 0;


/* Flag to indicate that startup sequence is done */
uint8_t startup_done = 0;

/* Flag to indicate that settings menu is active */
uint8_t settings_menu_active = 0;

/* Flag to indicate if beep has been done at set temperature */
uint8_t beeped_at_set_temp = 0;

/* Flag to indicate if a popup is shown */
uint8_t popup_shown = 0;

/* Variables for thermocouple outlier detection */
float NTC_temp_from_ADC = 0;
float TC_temp_from_ADC = 0;
float TC_temp_from_ADC_previous = 0;
float TC_temp_from_ADC_diff = 0;
uint16_t TC_outliers_detected = 0;
#define TC_OUTLIERS_THRESHOLD 300

/* Max allowed tip temperature */
#define EMERGENCY_SHUTDOWN_TEMPERATURE 450

/* Min allowed bus voltage and power */
#define MIN_BUSVOLTAGE 8.0
#define MIN_BUSPOWER 8.0
#define USB_PD_POWER_REDUCTION_FACTOR 1.0

/* Max allowed power per handle type */
#define NT115_MAX_POWER   22
#define T210_MAX_POWER    65
#define T245_MAX_POWER    130
#define No_name_MAX_POWER   150

/* TC Compensation constants */
#define TC_COMPENSATION_X2_NT115 (5.1026665462522864e-05)
#define TC_COMPENSATION_X1_NT115 0.42050803230712813
#define TC_COMPENSATION_X0_NT115 20.14538589052425

#define TC_COMPENSATION_X2_T210 (4.223931712905644e-06)
#define TC_COMPENSATION_X1_T210 0.31863796444354214
#define TC_COMPENSATION_X0_T210 20.968033870812942

#define TC_COMPENSATION_X2_T245 (-4.735112838956741e-07)
#define TC_COMPENSATION_X1_T245 0.11936452029674384
#define TC_COMPENSATION_X0_T245 23.777399955382318

/* Constants for internal MCU temperture */
#define V30     0.76      // from datasheet
#define VSENSE    (3.3/4096.0)  // VSENSE value
#define Avg_Slope   0.0025      // 2.5mV from datasheet

/* Largest delta temperature before detecting a faulty or missing cartridge */
#define MAX_TC_DELTA_FAULTDETECTION 50

/* Define time for long button press */
#define BTN_LONG_PRESS 15 //*50ms (htim16 interval) --> 15 = 750ms

/* Button flags */
volatile uint8_t SW_ready = 1;
volatile uint8_t SW_1_pressed = 0;
volatile uint8_t SW_2_pressed = 0;
volatile uint8_t SW_3_pressed = 0;
volatile uint8_t SW_1_pressed_long = 0;
volatile uint8_t SW_2_pressed_long = 0;
volatile uint8_t SW_3_pressed_long = 0;


/* UART send buffers */
#define MAX_BUFFER_LEN 250
uint8_t UART_transmit_buffer[MAX_BUFFER_LEN];
uint8_t UART_packet_index = 0;
uint8_t UART_packet_length = 0;

enum handles attached_handle;

/* Struct to hold sensor values */
struct sensor_values_struct {
  float set_temperature;
  float thermocouple_temperature;
  float thermocouple_temperature_previous;
  float thermocouple_temperature_filtered;
  float requested_power;
  float requested_power_filtered;
  float bus_voltage;
  float heater_current;
  uint16_t leak_current;
  float mcu_temperature;
  float in_stand;
  float handle1_sense;
  float handle2_sense;
  mainstates current_state;
  mainstates previous_state;
  float max_power_watt;
  float USB_PD_power_limit;
};

/* Struct to hold sensor values */
volatile sensor_values_struct sensor_values = {
    .set_temperature = 0.0f,
    .thermocouple_temperature = 0.0f,
    .thermocouple_temperature_previous = 0.0f,
    .thermocouple_temperature_filtered = 0.0f,
    .requested_power = 0.0f,
    .requested_power_filtered = 0.0f,
    .bus_voltage = 0.0f,
    .heater_current = 0.0f,
    .leak_current = 0,
    .mcu_temperature = 0.0f,
    .in_stand = 0.0f,
    .handle1_sense = 0.0f,
    .handle2_sense = 0.0f,
    .current_state = SLEEP,
    .previous_state = SLEEP,
    .max_power_watt = 0.0f,
    .USB_PD_power_limit = DBL_MAX
};

/* Struct to hold flash values */
Flash_values flash_values;
Flash_values default_flash_values = {
  .startup_temperature = 330,
  .temperature_offset = 0,
  .standby_temp = 150,
  .standby_time = 10,
  .emergency_time = 3,
  .buzzer_enabled = 1,
  .preset_temp_1 = 330,
  .preset_temp_2 = 430,
  .GPIO4_ON_at_run = 0,
  .screen_rotation = 0,
  .momentary_stand = 0,
  .current_measurement = 1,
  .startup_beep = 1,
  .deg_celsius = 1,
  .temp_cal_100 = 100,
  .temp_cal_200 = 200,
  .temp_cal_300 = 300,
  .temp_cal_350 = 350,
  .temp_cal_400 = 400,
  .temp_cal_450 = 450,
  .serial_debug_print = 0,
  .displayed_temp_filter = 5,
  .startup_temp_is_previous_temp = 0,
  .three_button_mode = 0,
  .beep_at_set_temp = 1,
  .beep_tone = 0,
  .power_unit = 0,
  .detect_nt115 = 1,
    .power_limit_T245 = 0,
    .power_limit_T210 = 0,
    .power_limit_NT115 = 0,
  .power_limit_No_name = 0,
  .display_graph = 0,
  .delta_t_detection = 1};

/* PID data */
float PID_setpoint = 0.0f;

/* Flags for temp and current measurements */
volatile uint8_t current_measurement_requested = 0;
uint8_t current_measurement_done = 1;
uint8_t thermocouple_measurement_requested = 0;
uint8_t thermocouple_measurement_done = 1;

/* Moving average filters for sensor data */
FilterTypeDef thermocouple_temperature_filter_struct;
FilterTypeDef thermocouple_temperature_filtered_filter_struct;
FilterTypeDef requested_power_filtered_filter_struct;
FilterTypeDef mcu_temperature_filter_struct;
FilterTypeDef input_voltage_filterStruct;
FilterTypeDef current_filterStruct;
FilterTypeDef stand_sense_filterStruct;
FilterTypeDef handle1_sense_filterStruct;
FilterTypeDef handle2_sense_filterStruct;
FilterTypeDef ntc_temperature_filter_struct;

/* Hysteresis filters for sensor data */
Hysteresis_FilterTypeDef thermocouple_temperature_filtered_hysteresis;


PID_TypeDef TPID;

/* Function to clamp d between the limits min and max */
float clamp(float d, float min, float max) {
    if (d < min) return min;
    if (d > max) return max;
    return d;
}

/* Function to get min value of a and b */
float min(float a, float b) {
  return a < b ? a : b;
}

/* Function to get min value of a,b and c */
float min3(float a, float b, float c) {
  return (a < b ? (a < c ? a : c) : (b < c ? b : c));
}

//void GetLastNSamples(float *s, uint16_t N)
//{
//    uint16_t pos = ADC2_DMA_BUF_SIZE - __HAL_DMA_GET_COUNTER(hadc2.DMA_Handle);
//
//    int idx = pos - 1;
//    if (idx < 0)
//        idx = ADC2_DMA_BUF_SIZE - 1;
//
//    for (int i = 0; i < N; i++) {
//        int index = idx - (N - 1 - i);
//
//        if (index < 0)
//            index += ADC2_DMA_BUF_SIZE;
//
//        s[i] = adc2_dma_buf_raw[index];
//    }
//}

//float Get_Temperature_For_PID(void)
//{
//    static float filtered = 0.0f;
//
//    float s[ADC2_DMA_BUF_SIZE];  // hoặc define max size riêng
//    GetLastNSamples(s, ADC2_DMA_BUF_SIZE);
//
//    float alpha = 0.2f;
//
//    for (int i = 0; i < ADC2_DMA_BUF_SIZE; i++) {
//        filtered = filtered + alpha * (s[i] - filtered);
//    }
//
//    return filtered;
//}

//float Get_Temperature_For_PID(){
//  float ADC_filter_mean = 0.0f;
//  int count = 0;
//
//  for (int n = 0; n < ADC2_DMA_BUF_SIZE; n += 1) {
//      ADC_filter_mean += adc2_dma_buf_raw[n];
//      count++;
//  }
//
//  return (count > 0) ? (ADC_filter_mean / (float)count) : 0.0f;
//}

/**
 * @brief Lấy giá trị ADC trung bình/trung vị từ 5 mẫu DMA
 * @return Giá trị ADC đã được lọc sạch nhiễu spike và làm mịn
 */
float Get_Temperature_For_PID()
{
    // 1. Copy dữ liệu từ buffer DMA
    float n[5];
    for (int i = 0; i < 5; i++) {
        n[i] = (float)adc2_dma_buf_raw[i];
    }

    // 2. Sắp xếp nổi bọt (Bubble Sort) để tìm trung vị
    // Chỉ cần 5 phần tử nên chạy cực nhanh
    for (int i = 0; i < 4; i++) {
        for (int j = i + 1; j < 5; j++) {
            if (n[i] > n[j]) {
                float temp = n[i];
                n[i] = n[j];
                n[j] = temp;
            }
        }
    }

    // 3. Lấy giá trị trung vị (Median)
    // Loại bỏ hoàn toàn 2 mẫu nhiễu thấp nhất và 2 mẫu nhiễu cao nhất
    float median_adc = n[2];

    // 4. Bộ lọc Alpha (Low-pass filter)
    // Giúp giá trị ADC không bị nhảy bậc thang, làm mượt khâu Vi phân (D) của PID
    static float last_adc = 0.0f;
    if (last_adc == 0.0f) last_adc = median_adc; // Khởi tạo lần đầu

    // Hệ số 0.4 giúp cân bằng giữa độ mượt và tốc độ đáp ứng
    last_adc = (0.3f * median_adc) + (0.7f * last_adc);

    return last_adc;
}

float get_mean_ADC_Vbus(){
  float ADC_filter_mean = 0.0f;
  int count = 0;

  for (int n = 0; n < ADC4_DMA_BUF_SIZE; n += 1) {
      ADC_filter_mean += adc4_dma_buf_raw[n];
      count++;
  }

  return (count > 0) ? (ADC_filter_mean / (float)count) : 0.0f;
}

/**
 * @brief Returns the average value of 5 readings in the ADC_buffer vector.
 * @param index Channel index: only 0 now
 * @return Average value for that channel
 */
float get_mean_ADC_reading_indexed(uint8_t index){
  if (index > 0) return 0.0f;  // Incorrect Index Protection

  float ADC_filter_mean = 0.0f;
  int count = 0;

  for (int n = 0; n < ADC2_DMA_BUF_SIZE; n += 1) {
      ADC_filter_mean += adc2_dma_buf_raw[n];
      count++;
  }

  return (count > 0) ? (ADC_filter_mean / (float)count) : 0.0f;
}

/* Function to change the main state */
void change_state(mainstates new_state){

  // Save the previous state
  sensor_values.previous_state = sensor_values.current_state;
  // Set the new current state
  sensor_values.current_state = new_state;
  //debug_log("[s]=%u\r\n",sensor_values.current_state);
  // If transitioning TO RUN STATE and the current temperature should be saved as the startup temperature
  if((sensor_values.previous_state != RUN) && (sensor_values.current_state == RUN) && (flash_values.startup_temp_is_previous_temp == 1)){
    flash_values.startup_temperature = sensor_values.set_temperature;
    FlashWrite(&flash_values);
  }

  // Reset the beep flag upon reaching the temperature — only when entering RUN
  if((sensor_values.previous_state != RUN) && (sensor_values.current_state == RUN)){
          beeped_at_set_temp = 0;
  }
  if (sensor_values.previous_state != sensor_values.current_state) {
    beep(flash_values.buzzer_enabled);
    if (sensor_values.current_state == RUN){
      debug_log("Start PID\r\n");
    }
    else {
      __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);
      debug_log("Stop PID, s=%u\r\n",sensor_values.current_state);
    }
  }
}

/* Function to get the filtered MCU temperature */
void get_mcu_temp(){
  //sensor_values.mcu_temperature = Moving_Average_Compute((((get_mean_ADC_reading_indexed(2) * VSENSE) - V30) / Avg_Slope + 25), &mcu_temperature_filter_struct); //Index 2 is MCU temp
}

/* Function to get the filtered bus voltage */
void get_bus_voltage(){
  // 1. Hệ số chia áp: (101k + 10k) / 10k = 11.1
  // 2. Độ phân giải: 12-bit (4095) vì đã dùng Right Shift 4 trong Oversampling
  // 3. Vref: 3.3V (Kiểm tra lại nếu board dùng nguồn khác)
  sensor_values.bus_voltage = (get_mean_ADC_Vbus() / 4095.0f) * 3.3f * 11.1f;
}

/* Function to get the filtered heater current */
void get_heater_current(){
  sensor_values.heater_current = Moving_Average_Compute(sensor_values.requested_power_filtered/(sensor_values.bus_voltage), &current_filterStruct);
}

void get_thermocouple_temperature(){
  /* --- Step 1: Outlier filter on raw ADC data --- */
  TC_temp_from_ADC_previous = TC_temp_from_ADC;

  //TC_temp_from_ADC = get_mean_ADC_reading_indexed(0);
  TC_temp_from_ADC = Get_Temperature_For_PID();
  //debug_log("[a] %0.3f\r\n", TC_temp_from_ADC);
  TC_temp_from_ADC = (TC_temp_from_ADC - AD627_OFFSET - AD627_IN_0V)*AXXSOLDER_GAIN/AD627_GAIN;
  TC_temp_from_ADC_diff = fabs(TC_temp_from_ADC_previous - TC_temp_from_ADC);

  if((TC_temp_from_ADC_diff > TC_OUTLIERS_THRESHOLD) && (TC_outliers_detected < 2)){
    TC_outliers_detected++;
    if(TC_outliers_detected < 2){
      TC_temp_from_ADC = TC_temp_from_ADC_previous;
    }
  }
  else{
    TC_outliers_detected = 0;
  }

  /* --- Step 2: Moving average filter --- */
  //TC_temp = Moving_Average_Compute(TC_temp_from_ADC, &thermocouple_temperature_filter_struct); /* Moving average */
  TC_temp = TC_temp_from_ADC;

  /* --- Step 3: Apply handle-specific compensation polynomial --- */
  if(attached_handle == T210){
    sensor_values.thermocouple_temperature = TC_temp*TC_temp*TC_COMPENSATION_X2_T210 + TC_temp*TC_COMPENSATION_X1_T210 + TC_COMPENSATION_X0_T210;
  }
  else if(attached_handle == T245){
    sensor_values.thermocouple_temperature = TC_temp*TC_temp*TC_COMPENSATION_X2_T245 + TC_temp*TC_COMPENSATION_X1_T245 + TC_COMPENSATION_X0_T245;
  }
  else if(attached_handle == NT115){
    sensor_values.thermocouple_temperature = TC_temp*TC_temp*TC_COMPENSATION_X2_NT115 + TC_temp*TC_COMPENSATION_X1_NT115 + TC_COMPENSATION_X0_NT115;
  }

  /* --- Step 4: Adjust measured temperature to fit calibrated values */
  if(sensor_values.thermocouple_temperature < 100){
    sensor_values.thermocouple_temperature = sensor_values.thermocouple_temperature*(flash_values.temp_cal_100)/100.0f;
  }
  else if(sensor_values.thermocouple_temperature < 200){
    sensor_values.thermocouple_temperature = (sensor_values.thermocouple_temperature - 100.0f)*(flash_values.temp_cal_200-flash_values.temp_cal_100)/100.0f + flash_values.temp_cal_100;
  }
  else if(sensor_values.thermocouple_temperature < 300){
    sensor_values.thermocouple_temperature = (sensor_values.thermocouple_temperature - 200.0f)*(flash_values.temp_cal_300-flash_values.temp_cal_200)/100.0f + flash_values.temp_cal_200;
  }
  else if(sensor_values.thermocouple_temperature < 350){
    sensor_values.thermocouple_temperature = (sensor_values.thermocouple_temperature - 300.0f)*(flash_values.temp_cal_350-flash_values.temp_cal_300)/50.0f + flash_values.temp_cal_300;
  }
  else if(sensor_values.thermocouple_temperature < 400){
    sensor_values.thermocouple_temperature = (sensor_values.thermocouple_temperature - 350.0f)*(flash_values.temp_cal_400-flash_values.temp_cal_350)/50.0f + flash_values.temp_cal_350;
  }
  else{
    sensor_values.thermocouple_temperature = (sensor_values.thermocouple_temperature - 400.0f)*(flash_values.temp_cal_450-flash_values.temp_cal_400)/50.0f + flash_values.temp_cal_400;
  }
  // Add temperature offset value
  sensor_values.thermocouple_temperature += flash_values.temperature_offset;
  NTC_temp_from_ADC = Moving_Average_Compute(Calculate_NTC_Temp(ntc_adc_raw[0]),&ntc_temperature_filter_struct);
  //debug_log("[t] %0.3f, %0.3f, %u\r\n", TC_temp_from_ADC, NTC_temp_from_ADC, ntc_adc_raw[0]);
  // Adjust CJC - NTC
  sensor_values.thermocouple_temperature += NTC_temp_from_ADC - 25;

  // Clamp
  sensor_values.thermocouple_temperature = clamp(sensor_values.thermocouple_temperature ,0 ,500);
  sensor_values.thermocouple_temperature_filtered = Moving_Average_Compute(sensor_values.thermocouple_temperature, &thermocouple_temperature_filtered_filter_struct); // Moving average filter
  sensor_values.thermocouple_temperature_filtered = Hysteresis_Add(sensor_values.thermocouple_temperature_filtered, &thermocouple_temperature_filtered_hysteresis); // Hysteresis filter
}


/* Update the duty cycle of timer controlling the heater PWM */
void update_heater_PWM(){
  if (sensor_values.bus_voltage <= 0.0f) return;

    float duty_cycle = sensor_values.requested_power;

    pwm_duty_cycle = clamp(duty_cycle, 0.0f, PWM_HARDWARE_MAX);

    if (sensor_values.current_state != RUN){
      pwm_duty_cycle = 0;
    }
    set_heater_duty((uint32_t)pwm_duty_cycle);
}

/* Return the temperature in the correct unit */
float convert_temperature(float temperature){
  if (flash_values.deg_celsius == 1){
    return temperature;
  }
  else{
    return ((temperature * 9) + 3) / 5 + 32;
  }
}


/* Formatting float into a string as right-aligned, by adding spaces on the left */
void format_number_right(float input, char* buffer) {
    int rounded = (int)roundf(input);
    clamp(rounded, 0, 999);

    char number_str[4]; // Enough for "100" + '\0'
    sprintf(number_str, "%d", rounded);

    int num_digits = strlen(number_str);
    int padding_spaces = (3 - num_digits) * 2;

    // Fill padding spaces first
    memset(buffer, ' ', padding_spaces);
    // Copy the number after the padding
    strcpy(buffer + padding_spaces, number_str);

    // Null terminate the full string
    buffer[padding_spaces + num_digits] = '\0';
}

/* Formatting float into a string as left-aligned, by adding spaces on the right */
void format_number_left(float input, char* buffer) {
    int rounded = (int)roundf(input);
    clamp(rounded, 0, 999);

    char number_str[4]; // "100" + '\0'
    sprintf(number_str, "%d", rounded);

    int num_digits = strlen(number_str);
    int padding_spaces = (3 - num_digits) * 2;

    // Copy number into buffer
    strcpy(buffer, number_str);

    // Add spaces to the right
    memset(buffer + num_digits, ' ', padding_spaces);
    buffer[num_digits + padding_spaces] = '\0';
}

/* Get encoder value (Set temp.) and limit */
void get_set_temperature(){
  sensor_values.set_temperature = 330; // init
}

void show_popup(){}
/* Function to set state to EMERGENCY_SLEEP */
void handle_emergency_shutdown(){
  /* Get time when iron turns on */
  if(sensor_values.previous_state != RUN && sensor_values.current_state == RUN){
    previous_millis_left_stand = HAL_GetTick();
  }
  /* Set state to EMERGENCY_SLEEP if iron ON for longer time than emergency_time */
  if ((sensor_values.in_stand == 0) && (HAL_GetTick() - previous_millis_left_stand >= flash_values.emergency_time*60000) && sensor_values.current_state == RUN){
    change_state(EMERGENCY_SLEEP);
    debug_log("Standby timeout");
    //ui_solder_trigger_overheat();
  }
  /* Set state to EMERGENCY_SLEEP if input voltage is too low */
  if((sensor_values.bus_voltage <= MIN_BUSVOLTAGE) && (sensor_values.current_state == RUN)){
    change_state(EMERGENCY_SLEEP);
    debug_log("Inp. Voltage too low");
    //ui_solder_trigger_overheat();
  }
  /* Set state to EMERGENCY_SLEEP if input power is too low */
  if((sensor_values.max_power_watt <= MIN_BUSPOWER) && (sensor_values.current_state == RUN)){
    change_state(EMERGENCY_SLEEP);
    debug_log("Inp. Power too low");
    //ui_solder_trigger_overheat();
  }
  /* Set state to EMERGENCY_SLEEP if curent is to high */
  else if(sensor_values.heater_current > 6.5f){
    change_state(EMERGENCY_SLEEP);
    debug_log("High current, stop");
    //ui_solder_trigger_overheat();
  }
  /* Set state to EMERGENCY_SLEEP if no tip detected (no current draw) */
//  else if((sensor_values.heater_current < 0.05f) && (sensor_values.current_state == RUN)){ //NT115 at 9V draws 2.3
//    change_state(EMERGENCY_SLEEP);
//    debug_log("No tip detected");
//    //ui_solder_trigger_overheat();
//  }
  /* Set state to EMERGENCY_SLEEP if iron is over max allowed temp */
  else if((sensor_values.thermocouple_temperature_filtered > EMERGENCY_SHUTDOWN_TEMPERATURE) && (sensor_values.current_state != EMERGENCY_SLEEP)){
    change_state(EMERGENCY_SLEEP);
    debug_log("OVERTEMP");
    ui_solder_trigger_overheat(EMERGENCY_SHUTDOWN_TEMPERATURE);
  }

  // 2. ĐIỀU KIỆN THOÁT CẢNH BÁO (Nguội xuống dưới 200 độ)
  // Chỉ thoát khi đang ở EMERGENCY_SLEEP
  if (sensor_values.current_state == EMERGENCY_SLEEP && sensor_values.thermocouple_temperature_filtered < (EMERGENCY_SHUTDOWN_TEMPERATURE-5)) {
      change_state(SLEEP); // Chuyển về trạng thái nghỉ bình thường
      // UI sẽ tự động nhận biết state thoát này qua hàm update_values
      debug_log("exit EMERGENCY_SLEEP");
  }
}

/* Function to handle the cartridge presence */
void handle_cartridge_presence(){
  if((sensor_values.heater_current < 1) || (TC_temp > 4096-10)) { //NT115 at 9V draws 2.3
    cartridge_state = DETACHED;
    change_state(EMERGENCY_SLEEP);
  }
  else{
    cartridge_state = ATTACHED;
  }
  /* When a inserted cartridge is detected - fill the moving average filter with TC measurements */
  if ((previous_cartridge_state == DETACHED) && (cartridge_state == ATTACHED)){
    HAL_Delay(100);
    Moving_Average_Set_Value(sensor_values.thermocouple_temperature, &thermocouple_temperature_filtered_filter_struct);
  }
  previous_cartridge_state = cartridge_state;
}


/* Function to toggle between RUN and HALTED at each press of the encoder button */
void handle_button_status(){
  //debug_log("[!] %s\r\n", __FUNCTION__);
}

/* Get the status of handle in/on stand to trigger SLEEP */
void get_stand_status(){
// Read stand input: low level = handle is in the stand
  uint8_t stand_status = (HAL_GPIO_ReadPin(STAND_INP_GPIO_Port, STAND_INP_Pin) == GPIO_PIN_RESET) ? 1 : 0;
  sensor_values.in_stand = Moving_Average_Compute(stand_status, &stand_sense_filterStruct); // Moving average filter

  if (sensor_values.current_state == RUN){
    if(sensor_values.in_stand > 0.9f) {
      change_state(STANDBY);
      previous_millis_standby = HAL_GetTick();
      debug_log("in_stand > 0.99f, STANDBY");
    }
  } else {
    if(sensor_values.in_stand <= 0.6f) {
      if ((sensor_values.current_state == STANDBY) || (sensor_values.current_state == RUN)){
        change_state(RUN);
        debug_log("in_stand < 0.45f, STANDBY/RUN => RUN");
      }
    }
    else {
      if((HAL_GetTick()-previous_millis_standby >= flash_values.standby_time*60000.0) && (sensor_values.current_state == STANDBY)){
        change_state(SLEEP);
        debug_log("in_stand: STANDBY => SLEEP");
      }
    }
  }
}


/* Automatically detect handle type, NT115, T210 or T245 based on HANDLE_INP_1_Pin and HANDLE_INP_2_Pin.*/
void get_handle_type(){
  attached_handle = T245;
}

/* Function to set heating values depending on detected handle */
void set_handle_values(){
  uint16_t usb_limit = sensor_values.USB_PD_power_limit;
  uint16_t WATT;
  uint16_t flash_limit;
  switch (attached_handle) {
    case NT115:
      flash_limit = (uint16_t)flash_values.power_limit_NT115;
      WATT = (flash_limit != 0) ? flash_limit : NT115_MAX_POWER;
      sensor_values.max_power_watt = min3(usb_limit, NT115_MAX_POWER, WATT);
      PID_SetTunings(&TPID, KP_NT115, KI_NT115 * PID_TIME_RATIO, KD_NT115 / PID_TIME_RATIO);
      PID_SetILimits(&TPID, -MAX_I_NT115, MAX_I_NT115);
      break;

    case T210:
      flash_limit = (uint16_t)flash_values.power_limit_T210;
      WATT = (flash_limit != 0) ? flash_limit : T210_MAX_POWER;
      sensor_values.max_power_watt = min3(usb_limit, T210_MAX_POWER, WATT);
      PID_SetTunings(&TPID, KP_T210, KI_T210 * PID_TIME_RATIO, KD_T210 / PID_TIME_RATIO);
      PID_SetILimits(&TPID, -MAX_I_T210, MAX_I_T210);
      break;

    case T245:
      flash_limit = (uint16_t)flash_values.power_limit_T245;
      WATT = (flash_limit != 0) ? flash_limit : T245_MAX_POWER;
      sensor_values.max_power_watt = min3(usb_limit, T245_MAX_POWER, WATT);
      PID_SetTunings(&TPID, KP_T245, KI_T245 * PID_TIME_RATIO, KD_T245 / PID_TIME_RATIO);
      PID_SetILimits(&TPID, -MAX_I_T245, MAX_I_T245);
      break;

    default:
      flash_limit = (uint16_t)flash_values.power_limit_No_name;
      WATT = (flash_limit != 0) ? flash_limit : No_name_MAX_POWER;
      sensor_values.max_power_watt = min3(usb_limit, No_name_MAX_POWER, WATT);
      PID_SetTunings(&TPID, KP_No_name, KI_No_name * PID_TIME_RATIO, KD_No_name / PID_TIME_RATIO);
      PID_SetILimits(&TPID, -MAX_I_No_name, MAX_I_No_name);
      break;
  }
}

/* Signal that the set temperature has been reached */
void beep_at_set_temp(){
    if (!flash_values.beep_at_set_temp) return;
    if (beeped_at_set_temp) return;

    float temp = sensor_values.thermocouple_temperature_filtered;
    float target = sensor_values.set_temperature;
    float delta = fabsf(temp - target);

    if (delta <= 5.0f){
    beeped_at_set_temp = 1;
    beep_double(flash_values.buzzer_enabled);
  }
}

/* Interrupts at button press */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
}

/* Interrupts at every encoder increment */
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim){
  if ((htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1) || (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2) ) {
    beep(flash_values.buzzer_enabled);
  }
}

void do_pid(){
  get_thermocouple_temperature();
  get_bus_voltage();
  if (sensor_values.current_state == RUN){
    PID_Compute_Advanced(&TPID);
  }
  else {
    sensor_values.requested_power = 0;
  }
  update_heater_PWM();
  pid_flag = 0;
}


void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim){
  //debug_log(".");
  // PID hander - 10 ms
  if (htim->Instance == TIM6) {
    pid_flag = 1;
    do_pid();
  }
  if (htim->Instance == TIM17) {
    ui_flag = 1;
  }
}

void solder_init(){
  QuickLog_Init();
  // 1. Khởi động ADC với DMA (đọc 5 mẫu rồi quay vòng)
  HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);
  HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED);
  HAL_ADCEx_Calibration_Start(&hadc3, ADC_SINGLE_ENDED);
  HAL_ADCEx_Calibration_Start(&hadc4, ADC_SINGLE_ENDED);
  __HAL_DMA_CLEAR_FLAG(&hdma_adc3, DMA_FLAG_TC1 | DMA_FLAG_HT1 | DMA_FLAG_TE1);
  HAL_ADC_Start_DMA(&hadc2, (uint32_t*)adc2_dma_buf_raw, (uint32_t)ADC2_DMA_BUF_SIZE);
  HAL_ADC_Start_DMA(&hadc4, (uint32_t*)adc4_dma_buf_raw, (uint32_t)ADC4_DMA_BUF_SIZE);
  //HAL_ADC_Start(&hadc3);
  if (HAL_ADC_Start_DMA(&hadc3, (uint32_t*)ntc_adc_raw, 1) != HAL_OK) {
      Error_Handler();
  }

  // 2. Khởi động Channel 2 của TIM1 (Đây là mốc 1850 để chích ADC). Bật PWM ở Channel 1 để cấp nguồn Heater (PA8)
  HAL_TIM_OC_Start(&htim1, TIM_CHANNEL_2);
  //HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);

  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);

  /* 1. Calibrate & start DAC2 Channel 1 */
  HAL_DAC_Start(&hdac2, DAC_CHANNEL_1);
  /* 2. Ghi giá trị 248 để có mức áp ~200mV */
  HAL_DAC_SetValue(&hdac2, DAC_CHANNEL_1, DAC_ALIGN_12B_R, AD627_OFFSET);

  // 3. Waiting for ADC data available first
  HAL_Delay(100);

  // 4. Check if user data in flash is valid, if not - write default parameters
#ifdef _USE_FLASH_
  // Check if user data in flash is valid, if not - write default parameters
  if(!FlashCheckCRC()){
    FlashWrite(&default_flash_values);
    beep(1); //Beep once to indicate default parameters written to flash
    HAL_Delay(100);
  }

  /* Read flash data */
  FlashRead(&flash_values);
#else
  flash_values = default_flash_values;
#endif

  // 5. Set initial encoder timer value
  //TIM2->CNT = flash_values.startup_temperature;

  // 7. Initialize moving average functions
  Moving_Average_Init(&ntc_temperature_filter_struct,(uint32_t)100);
  Moving_Average_Init(&thermocouple_temperature_filter_struct,(uint32_t)2);
  Moving_Average_Init(&thermocouple_temperature_filtered_filter_struct,(uint32_t)flash_values.displayed_temp_filter*30);
  Moving_Average_Init(&requested_power_filtered_filter_struct,(uint32_t)20);
  Moving_Average_Init(&mcu_temperature_filter_struct,(uint32_t)100);
  Moving_Average_Init(&input_voltage_filterStruct,(uint32_t)25);
  Moving_Average_Init(&current_filterStruct,(uint32_t)2);
  Moving_Average_Init(&stand_sense_filterStruct,(uint32_t)30);
  Moving_Average_Init(&handle1_sense_filterStruct,(uint32_t)20);
  Moving_Average_Init(&handle2_sense_filterStruct,(uint32_t)20);

  // 8. initialize hysteresis functions
  Hysteresis_Init(&thermocouple_temperature_filtered_hysteresis, 0.1);

  // 9. Init and fill filter structures with initial values
  for (int i = 0; i<200;i++){
    get_bus_voltage(); // Function to retrieve the filtered bus voltage
    get_heater_current(); // Function to retrieve the filtered heater current
    get_mcu_temp(); // Function to retrieve the MCU temperature
    get_thermocouple_temperature(); // Get thermocouple temperature
    get_handle_type(); // Get the handle type attached
    set_handle_values(); // Function to set heating values depending on the detected handle
    get_stand_status(); // Get handle stand status to activate SLEEP
    handle_button_status(); // Function to get button status
    handle_cartridge_presence(); // Function to monitor cartridge presence
  }

  /* Initiate PID controller */
  PID(&TPID, &sensor_values.thermocouple_temperature, &sensor_values.requested_power, &PID_setpoint, 0, 0, 0, _PID_CD_DIRECT); //PID parameters are set depending on detected handle by set_handle_values()
  PID_SetMode(&TPID, _PID_MODE_AUTOMATIC);
  PID_SetSampleTime(&TPID, PID_UPDATE_INTERVAL, 0);     // Set PID update time to "PID_UPDATE_INTERVAL"
  PID_SetOutputLimits(&TPID, 0, PID_MAX_OUTPUT);      // Set max and min output limit
  PID_SetILimits(&TPID, 0, 0);                  // Set max and min I limit
  PID_SetIminError(&TPID,PID_ADD_I_MIN_ERROR);      // Set I min Error
  PID_SetNegativeErrorIgainMult(&TPID, PID_NEG_ERROR_I_MULT, PID_NEG_ERROR_I_BIAS); // Set un-symmetric I gain parameters

  /* Set beep tone */
  switch((int)flash_values.beep_tone){
    case 0: {
      set_tone(2700, 10);
      break;
    }
    case 1: {
      set_tone(2700, 50);
      break;
    }
    case 2: {
      set_tone(1000, 10);
      break;
    }
    case 3: {
      set_tone(1000, 50);
      break;
    }
  }

  // Start-up beep
  beep_double(flash_values.startup_beep);

  //Flag to indicate that the startup sequence is done
  startup_done = 1;
  sensor_values.thermocouple_temperature_previous = sensor_values.thermocouple_temperature;
  get_set_temperature();
  // 6. Initiate display
  InitDisplay();

  HAL_TIM_Base_Start_IT(&htim6);
  HAL_TIM_Base_Start_IT(&htim17);
}


void do_ui(){
  float ppercent = pwm_duty_cycle/PWM_HARDWARE_FULL;
  float pWatt = sensor_values.max_power_watt*ppercent;

  sensor_values.requested_power_filtered = clamp(Moving_Average_Compute(pWatt, &requested_power_filtered_filter_struct), 0.0f, sensor_values.max_power_watt);
  ui_solder_update_chart(sensor_values.thermocouple_temperature_filtered, sensor_values.requested_power_filtered);
  ui_solder_set_power(100*ppercent);
  ui_solder_update_values(
          sensor_values.bus_voltage,
          sensor_values.heater_current,
          sensor_values.requested_power_filtered,
          sensor_values.thermocouple_temperature_filtered,
          PID_setpoint);
  ui_flag = 0;
}

void solder_while1(){
  //if (pid_flag) do_pid();

  lv_timer_handler();
  //=========================================
  if(HAL_GetTick() - previous_sensor_update_high_update >= interval_sensor_update_high_update){
    get_stand_status();
    get_handle_type();
    set_handle_values();
//    handle_button_status(); XXX
    handle_emergency_shutdown();
//    handle_cartridge_presence();
    previous_sensor_update_high_update = HAL_GetTick();
  }

  if(HAL_GetTick() - previous_sensor_update_low_update >= interval_sensor_update_low_update){
    get_heater_current();
    get_mcu_temp();
    beep_at_set_temp();
    previous_sensor_update_low_update = HAL_GetTick();
  }

  /* switch */
  switch(sensor_values.current_state) {
    case RUN: {
      PID_setpoint = sensor_values.set_temperature;
      break;
    }
    case STANDBY: {
      if(flash_values.standby_temp > sensor_values.set_temperature){
      PID_setpoint = sensor_values.set_temperature;
      }
      else{
      PID_setpoint = flash_values.standby_temp;
      }
      break;
    }
    case SLEEP:
    case EMERGENCY_SLEEP:
    case HALTED: {
      PID_setpoint = 0;
      //is_running = false;
      break;
    }
  }

  buzzer_update();
  QuickLog_Process();
  if (ui_flag) do_ui();
}
