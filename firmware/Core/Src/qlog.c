/*
 * qlog.c
 *
 *  Created on: Apr 22, 2026
 *      Author: tuant
 */


#include "qlog.h"
#include "usbd_cdc_if.h" // Thư viện USB CDC tạo bởi CubeMX

static uint8_t ring_buf[LOG_BUF_SIZE];
static uint16_t head = 0;
static uint16_t tail = 0;

void QuickLog_Init(void) {
    head = 0;
    tail = 0;
}

void QuickLog_Write(const char* fmt, ...) {
    char temp_buf[128]; // Chứa tối đa 128 ký tự cho 1 dòng log
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(temp_buf, sizeof(temp_buf), fmt, args);
    va_end(args);

    if (len > 0) {
        for (int i = 0; i < len; i++) {
            uint16_t next_head = (head + 1) % LOG_BUF_SIZE;
            if (next_head != tail) { // Kiểm tra đầy buffer
                ring_buf[head] = temp_buf[i];
                head = next_head;
            }
        }
    }
}

void QuickLog_Process(void) {
    if (head == tail) return; // Buffer trống

    uint16_t current_head = head;
    uint16_t data_len;

    if (current_head > tail) {
        data_len = current_head - tail;
    } else {
        data_len = LOG_BUF_SIZE - tail;
    }

    // Gửi qua USB CDC
    uint8_t result = CDC_Transmit_FS(&ring_buf[tail], data_len);

    if (result == USBD_OK) {
        tail = (tail + data_len) % LOG_BUF_SIZE;
    }
}
