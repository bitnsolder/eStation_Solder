#include "debug_sys.h"
#include "usbd_cdc_if.h"
#include <string.h>
#include "debug.h"

//extern USBD_HandleTypeDef hUsbDeviceFS;

//uint32_t heartbeat = 0;
//
//void Heartbeat() {
//  // Heartbeat LED (Nháy mỗi 500ms)
//  if(HAL_GetTick() - heartbeat > 500) {
//      heartbeat = HAL_GetTick();
//      HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_6);
//  }
//}

void My_Log(const char* fmt, ...) {
#if USE_SYS_MONITOR
    static char buf[256];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    // Chống nghẽn USB CDC
//    USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef*)hUsbDeviceFS.pClassData;
//    uint32_t startTick = HAL_GetTick();
//    while(hcdc->TxState != 0) {
//        if(HAL_GetTick() - startTick > 10) return; // Timeout ngắn để tránh lag FPS
//    }

    CDC_Transmit_FS((uint8_t*)buf, len);
#endif
}

void USB_Log(char* msg) {
#if USE_SYS_MONITOR
  CDC_Transmit_FS((uint8_t*)msg, strlen(msg));
#endif
}

void USB_Printf(const char* fmt, ...) {
#if USE_SYS_MONITOR
    static char buf[256]; // Tăng lên 256 để thoải mái hơn
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (len > 0) {
        // Chờ cho đến khi cổng USB sẵn sàng hoặc bỏ qua nếu bận
        uint8_t result = CDC_Transmit_FS((uint8_t*)buf, len);
        if(result == USBD_BUSY) {
            // Có thể xử lý đợi hoặc bỏ qua tùy bạn
        }
    }
#endif
}

void my_monitor_cb(lv_disp_drv_t * disp_drv, uint32_t time, uint32_t px) {
#if USE_SYS_MONITOR
    // 1. Lấy FPS theo cách LVGL tính (giống hệt con số trên màn hình)
    // time là thời gian render 1 frame (ms).
    // Nếu render mất 20ms -> 1000/20 = 50 FPS.
    uint32_t fps_to_log = 0;
    if(time > 0) {
        fps_to_log = 1000 / time;
    } else {
        // Nếu chip quá nhanh (<1ms), ta coi như đạt max refresh rate (vd: 60)
        fps_to_log = 60;
    }

    // 2. Chỉ in Log mỗi 1 giây để tránh lag
    static uint32_t last_log = 0;
    if(HAL_GetTick() - last_log > 1000) {
        last_log = HAL_GetTick();

        uint8_t cpu_busy = 100 - lv_timer_get_idle();

        lv_mem_monitor_t mon;
        lv_mem_monitor(&mon);

        // IN RA USB: Đây chính là con số "Flush Rate" mày thấy lúc nãy
        debug_log("\r\n[LVGL STATS] FPS: %d | CPU: %d%% | RAM: %ld B\r\n",
               fps_to_log, cpu_busy, mon.total_size - mon.free_size);
    }
#endif
}



void Debug_Init(lv_disp_drv_t * disp_drv) {
#if USE_SYS_MONITOR
    disp_drv->monitor_cb = my_monitor_cb;
    My_Log("--- Debug System Initialized ---\r\n");
#endif
}
