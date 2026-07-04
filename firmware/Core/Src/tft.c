#include "tft.h"
#include "lvgl/lvgl.h"

extern SPI_HandleTypeDef hspi2;
volatile bool is_flushing = false;
//extern uint32_t flush_count;
/* MACRO */

/* Biến này dùng để báo cho LVGL biết khi nào DMA truyền xong */
lv_disp_drv_t *disp_drv_p_flush = NULL;

/* WRITE COMMAND */

void ST7789_WriteCommand(uint8_t cmd)
{
    CS_LOW();
    DC_LOW();

    HAL_SPI_Transmit(&hspi2,&cmd,1,HAL_MAX_DELAY);

    CS_HIGH();
}

/* WRITE DATA */

void ST7789_WriteData(uint8_t *data,uint16_t size)
{
    CS_LOW();
    DC_HIGH();

    HAL_SPI_Transmit(&hspi2,data,size,HAL_MAX_DELAY);

    CS_HIGH();
}

/* RESET */

void ST7789_Reset()
{
    HAL_GPIO_WritePin(TFT_RST_GPIO_Port,TFT_RST_Pin,GPIO_PIN_RESET);
    HAL_Delay(20);

    HAL_GPIO_WritePin(TFT_RST_GPIO_Port,TFT_RST_Pin,GPIO_PIN_SET);
    HAL_Delay(150);
}

/* SET ADDRESS */

//void ST7789_SetAddr(uint16_t x1,uint16_t y1,uint16_t x2,uint16_t y2)
//{
//    uint8_t data[4];
//
//    ST7789_WriteCommand(0x2A);
//
//    data[0]=x1>>8;
//    data[1]=x1;
//    data[2]=x2>>8;
//    data[3]=x2;
//
//    ST7789_WriteData(data,4);
//
//    ST7789_WriteCommand(0x2B);
//
//    data[0]=y1>>8;
//    data[1]=y1;
//    data[2]=y2>>8;
//    data[3]=y2;
//
//    ST7789_WriteData(data,4);
//
//    ST7789_WriteCommand(0x2C);
//}

void ST7789_SetAddr(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
    static uint8_t cmd_2a = 0x2A;
    static uint8_t cmd_2b = 0x2B;
    static uint8_t cmd_2c = 0x2C;
    uint8_t data_x[4] = { (uint8_t)(x1 >> 8), (uint8_t)(x1 & 0xFF), (uint8_t)(x2 >> 8), (uint8_t)(x2 & 0xFF) };
    uint8_t data_y[4] = { (uint8_t)(y1 >> 8), (uint8_t)(y1 & 0xFF), (uint8_t)(y2 >> 8), (uint8_t)(y2 & 0xFF) };

    // Gửi Column Address
    DC_LOW(); CS_LOW();
    HAL_SPI_Transmit(&hspi2, &cmd_2a, 1, 10);
    DC_HIGH();
    HAL_SPI_Transmit(&hspi2, data_x, 4, 10); // Gửi cả mảng 4 byte nhanh hơn gọi 4 lần 1 byte

    // Gửi Row Address
    DC_LOW();
    HAL_SPI_Transmit(&hspi2, &cmd_2b, 1, 10);
    DC_HIGH();
    HAL_SPI_Transmit(&hspi2, data_y, 4, 10);

    // Chuẩn bị ghi RAM
    DC_LOW();
    HAL_SPI_Transmit(&hspi2, &cmd_2c, 1, 10);

    // Lưu ý: Không nâng CS ở đây, để hàm Flush lo nốt phần data sau đó
    DC_HIGH();
}

/* INIT */

void ST7789_Init()
{
    ST7789_Reset();

    uint8_t data;

    ST7789_WriteCommand(0x11);
    HAL_Delay(120);

    ST7789_WriteCommand(0x36);
    //data=0x00;
    //ST7789_WriteData(&data,1);
    data = 0xA0; // Thử 0x70 hoặc 0xA0 cho chế độ Landscape (Ngang)
    ST7789_WriteData(&data, 1);

    ST7789_WriteCommand(0x3A);
    data=0x55;
    ST7789_WriteData(&data,1);

    ST7789_WriteCommand(0x21);

    ST7789_WriteCommand(0x29);

    HAL_GPIO_WritePin(TFT_BL_GPIO_Port,TFT_BL_Pin,GPIO_PIN_SET);
}

/* FILL SCREEN */

void ST7789_Fill_Color(uint16_t color)
{
    uint8_t data[2];

    data[0]=color>>8;
    data[1]=color;

    ST7789_SetAddr(0,0,239,319);

    CS_LOW();
    DC_HIGH();

    for(uint32_t i=0;i<240*320;i++)
    {
        HAL_SPI_Transmit(&hspi2,data,2,HAL_MAX_DELAY);
    }

    CS_HIGH();
}

// Khai báo buffer chứa 1 dòng (240 pixel * 2 byte = 480 byte)
// Đặt là 'static' để bộ nhớ không bị giải phóng khi thoát hàm
//static uint16_t line_buf[240];

//void ST7789_Fill_Color_DMA(uint16_t color) {
//    // 1. Chuẩn bị dữ liệu cho 1 dòng (Đảo byte vì ST7789 nhận High byte trước)
//    uint16_t color_swapped = (color << 8) | (color >> 8);
//    for (int i = 0; i < 240; i++) {
//        line_buf[i] = color_swapped;
//    }
//
//    // 2. Thiết lập vùng vẽ toàn màn hình (0,0 đến 239,319)
//    ST7789_SetAddr(0, 0, 239, 319);
//
//    // 3. Bắt đầu phiên truyền dữ liệu
//    HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_RESET);
//    HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin, GPIO_PIN_SET);
//
//    // 4. Gửi 320 dòng
//    for (int j = 0; j < 320; j++) {
//        // Gửi 1 dòng qua DMA
//        HAL_SPI_Transmit_DMA(&hspi2, (uint8_t*)line_buf, 240 * 2);
//
//        // Đợi DMA gửi xong dòng này mới cho phép gửi dòng tiếp theo
//        // hspi2.State sẽ chuyển về READY khi Callback được gọi
//        while (hspi2.State != HAL_SPI_STATE_READY);
//    }
//
//    // Lưu ý: Chân CS sẽ được kéo lên cao tự động trong Callback bạn đã viết ở main.c
//}

void my_disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p) {
    // 1. CHỐT CHẶN: Nếu đang flushing thì thoát ra ngay, không làm gì cả
    // LVGL sẽ thử lại ở chu kỳ timer tiếp theo
//    if(is_flushing) {
//        lv_disp_flush_ready(disp_drv); // Hoặc chỉ đơn giản là return;
//        return;
//    }

    disp_drv_p_flush = disp_drv;
    is_flushing = true; // Đánh dấu bắt đầu bận

    // 2. Thiết lập vùng vẽ
    ST7789_SetAddr(area->x1, area->y1, area->x2, area->y2);

    uint32_t size = (lv_area_get_width(area) * lv_area_get_height(area)) * 2;

    // 3. Chuẩn bị chân Pin cho DMA
    HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin, GPIO_PIN_SET);

    // 4. Bắn DMA
    if(HAL_SPI_Transmit_DMA(&hspi2, (uint8_t*)color_p, (uint16_t)size) != HAL_OK) {
        is_flushing = false;
        HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);
        lv_disp_flush_ready(disp_drv);
    }
}

void ui_scope_update(int16_t adc_val) {}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi) {
    if(hspi == &hspi2) {
        // 1. Nâng CS lên báo kết thúc truyền dữ liệu
        HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);

        // 2. Giải phóng cờ bận
        is_flushing = false;

        // 3. Báo cho LVGL biết đã sẵn sàng cho lượt tiếp theo
        if(disp_drv_p_flush != NULL) {
            lv_disp_flush_ready(disp_drv_p_flush);
        }
    }
}
