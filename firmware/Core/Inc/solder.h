/*
 * solder.h
 *
 *  Created on: Mar 20, 2026
 *      Author: tuant
 */

#ifndef INC_SOLDER_H_
#define INC_SOLDER_H_
#include <stdbool.h>
#include <stdint.h>
/* states for runtime switch */
typedef enum {
    RUN,
  STANDBY,
  SLEEP,
  EMERGENCY_SLEEP,
  HALTED
} mainstates;

/* Handles */
enum handles {
  NT115,
  T210,
  T245,
  No_name
};
// extern declaration (only a declaration)
extern enum handles attached_handle;
// Definition of enum for power sources
typedef enum {
  POWER_DC,
  POWER_USB,
  POWER_BAT // Future feature
} power_source_t;
// Declaration of external variable
extern power_source_t power_source;


/* Global variables defined in main.c */
//extern Flash_values flash_values;
//extern Flash_values default_flash_values;

// Sensor values structure
typedef struct {
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
} sensor_values_struct;
/* Global variable, defined in main.c */

extern volatile sensor_values_struct sensor_values;
// Enum definition
typedef enum {
  ATTACHED,
  DETACHED
} cartridge_state_t;
// Declaration of external variables
extern cartridge_state_t cartridge_state;
extern cartridge_state_t previous_cartridge_state;

extern uint8_t sleep_state_written_to_LCD;
extern uint8_t standby_state_written_to_LCD;

extern uint8_t fw_version_major;
extern uint8_t fw_version_minor;
extern uint8_t fw_version_patch;

/* Flag to indicate that settings menu is active */
extern uint8_t settings_menu_active;

#define MAX_POWER     150

/* Min and Max selectable values */
#define MIN_SELECTABLE_TEMPERATURE 20
#define MAX_SELECTABLE_TEMPERATURE 450

#define AD627_OFFSET     248
#define AD627_GAIN       148.88489f
#define AXXSOLDER_GAIN   240
#define AD627_IN_0V     8.6162f



/* Max PWM */
//#define PWM_HARDWARE_MAX 45000.0f  // Counter Max 60000. PWM max 56000, 59700 start ADC
#define PWM_HARDWARE_MAX 1100.0f  // Counter Max 60000. PWM max 56000, 59700 start ADC
#define PWM_HARDWARE_FULL 2000.0f

/* General PID parameters */
#define PID_MAX_OUTPUT PWM_HARDWARE_MAX

/* Constants for scaling power in W to correct duty cycle */
#define POWER_CONVERSION_FACTOR 0.123
#define K_AXX_TO_ESOLDER 4.0f // (esolder 60000/ AxxSolder 500)

#define FIXED_MEASURE_DUTY (PID_MAX_OUTPUT / 2)  // i.e. 250 / 2 = 250
#define PID_UPDATE_INTERVAL 10  // 100hz PID ~ 10ms
#define PID_UPDATE_INTERVAL_AXX 25.0f // Tần số gốc của AxxSolder (25ms)
#define PID_TIME_RATIO    (PID_UPDATE_INTERVAL / PID_UPDATE_INTERVAL_AXX) // = 0.4

#define PID_ADD_I_MIN_ERROR 5.0f  // Axxsolder: 75, cân nhắc 50
extern float PID_NEG_ERROR_I_MULT;
extern float PID_NEG_ERROR_I_BIAS;

extern char DISPLAY_buffer[40];

/* Flag to indicate if a popup is shown */
extern uint8_t popup_shown;

extern  bool flag_show_popup;

/* Timing constants */
extern uint32_t previous_millis_display;
extern uint32_t interval_display;

extern uint32_t previous_millis_debug;
extern uint32_t interval_debug;

extern uint32_t previous_millis_heating_halted_update;
extern uint32_t interval_heating_halted_update;

extern uint32_t previous_millis_left_stand;

extern uint32_t previous_millis_standby;

extern uint32_t previous_measure_current_update;
extern uint32_t interval_measure_current;

extern uint32_t previous_sensor_update_high_update;
extern uint32_t interval_sensor_update_high_update;

extern uint32_t previous_sensor_update_low_update;
extern uint32_t interval_sensor_update_low_update;

extern uint32_t previous_millis_popup;
extern uint32_t interval_popup;

/* Thermocouple temperature */
extern float TC_temp;

/* RAW ADC from current measurement */
extern uint16_t current_raw;

extern uint16_t current_leak;

void solder_init();
void solder_while1();
void Set_Soldering_Power(uint8_t percent);
void InitPWM();
void testPWM();
void change_state(mainstates new_state);
#endif /* INC_SOLDER_H_ */
