#include "st7735.h"

static SPI_HandleTypeDef *st7735_hspi;

// GPIO Helper Macros
#define CS_LOW()   HAL_GPIO_WritePin(ST7735_CS_PORT, ST7735_CS_PIN, GPIO_PIN_RESET)
#define CS_HIGH()  HAL_GPIO_WritePin(ST7735_CS_PORT, ST7735_CS_PIN, GPIO_PIN_SET)
#define DC_CMD()   HAL_GPIO_WritePin(ST7735_DC_PORT, ST7735_DC_PIN, GPIO_PIN_RESET)
#define DC_DATA()  HAL_GPIO_WritePin(ST7735_DC_PORT, ST7735_DC_PIN, GPIO_PIN_SET)
#define RES_LOW()  HAL_GPIO_WritePin(ST7735_RES_PORT, ST7735_RES_PIN, GPIO_PIN_RESET)
#define RES_HIGH() HAL_GPIO_WritePin(ST7735_RES_PORT, ST7735_RES_PIN, GPIO_PIN_SET)

static void ST7735_WriteCommand(uint8_t cmd) {
    DC_CMD();
    CS_LOW();
    HAL_SPI_Transmit(st7735_hspi, &cmd, 1, HAL_MAX_DELAY);
    CS_HIGH();
}

static void ST7735_WriteData(uint8_t *buff, size_t buff_size) {
    DC_DATA();
    CS_LOW();
    HAL_SPI_Transmit(st7735_hspi, buff, buff_size, HAL_MAX_DELAY);
    CS_HIGH();
}

static void ST7735_SetAddressWindow(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) {
    // Apply display offset coordinates for KMR-1.8 SPI TFT module
    x0 += ST7735_XSTART;
    x1 += ST7735_XSTART;
    y0 += ST7735_YSTART;
    y1 += ST7735_YSTART;

    // Column Address Set
    ST7735_WriteCommand(ST7735_CASET);
    uint8_t data_x[] = { 0x00, x0, 0x00, x1 };
    ST7735_WriteData(data_x, sizeof(data_x));

    // Row Address Set
    ST7735_WriteCommand(ST7735_RASET);
    uint8_t data_y[] = { 0x00, y0, 0x00, y1 };
    ST7735_WriteData(data_y, sizeof(data_y));

    // Write to RAM
    ST7735_WriteCommand(ST7735_RAMWR);
}

void ST7735_Init(SPI_HandleTypeDef *hspi) {
    st7735_hspi = hspi;

    // Hardware Reset
    RES_LOW();
    vTaskDelay(pdMS_TO_TICKS(10));
    RES_HIGH();
    vTaskDelay(pdMS_TO_TICKS(105));

    // Software Reset
    ST7735_WriteCommand(ST7735_SWRESET);
    vTaskDelay(pdMS_TO_TICKS(120));

    // Exit Sleep
    ST7735_WriteCommand(ST7735_SLPOUT);
    vTaskDelay(pdMS_TO_TICKS(120));

    // Set Color Format: 16-bit / pixel (RGB565)
    ST7735_WriteCommand(ST7735_COLMOD);
    uint8_t color_mode = 0x05;
    ST7735_WriteData(&color_mode, 1);

    // Memory Access Control
    ST7735_WriteCommand(ST7735_MADCTL);
    uint8_t madctl = 0xC8; // BGR order for KMR-1.8
    ST7735_WriteData(&madctl, 1);

    // Display On
    ST7735_WriteCommand(ST7735_DISPON);
    vTaskDelay(pdMS_TO_TICKS(100));
}

void ST7735_DrawPixel(uint16_t x, uint16_t y, uint16_t color) {
    if ((x >= ST7735_WIDTH) || (y >= ST7735_HEIGHT)) return;

    ST7735_SetAddressWindow(x, y, x, y);

    // Explicitly order High Byte then Low Byte for SPI
    uint8_t data[2] = { (color >> 8) & 0xFF, color & 0xFF };
    ST7735_WriteData(data, 2);
}

void ST7735_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    if ((x >= ST7735_WIDTH) || (y >= ST7735_HEIGHT)) return;
    if ((x + w - 1) >= ST7735_WIDTH)  w = ST7735_WIDTH - x;
    if ((y + h - 1) >= ST7735_HEIGHT) h = ST7735_HEIGHT - y;

    ST7735_SetAddressWindow(x, y, x + w - 1, y + h - 1);

    uint8_t data[2] = { color >> 8, color & 0xFF };

    DC_DATA();
    CS_LOW();
    for (uint32_t i = 0; i < (uint32_t)w * h; i++) {
        HAL_SPI_Transmit(st7735_hspi, data, 2, HAL_MAX_DELAY);
    }
    CS_HIGH();
}

void ST7735_FillScreen(uint16_t color) {
    ST7735_FillRect(0, 0, ST7735_WIDTH, ST7735_HEIGHT, color);
}

void ST7735_WriteChar(uint16_t x, uint16_t y, char ch, FontDef font, uint16_t color, uint16_t bgcolor) {
    uint32_t i, j;

    // Convert lowercase to uppercase or default to space
    if (ch < 32 || ch > 90) {
        if (ch >= 'a' && ch <= 'z') {
            ch -= 32;
        } else {
            ch = ' ';
        }
    }

    if (font.width == 8) {
        // Safe 8-bit pointer access for Font_8x12
        const uint8_t *font_bytes = (const uint8_t *)font.data;

        for (i = 0; i < font.height; i++) {
            uint8_t b = font_bytes[(ch - 32) * font.height + i];

            for (j = 0; j < font.width; j++) {
                if ((b & (0x80 >> j))) {
                    ST7735_DrawPixel(x + j, y + i, color);
                } else if (bgcolor != color) {
                    ST7735_DrawPixel(x + j, y + i, bgcolor);
                }
            }
        }
    } else {
        // 16-bit access for Font_7x10 and Font_11x18
        for (i = 0; i < font.height; i++) {
            uint16_t b = font.data[(ch - 32) * font.height + i];

            for (j = 0; j < font.width; j++) {
                if ((b & (0x0800 >> j))) {
                    ST7735_DrawPixel(x + j, y + i, color);
                } else if (bgcolor != color) {
                    ST7735_DrawPixel(x + j, y + i, bgcolor);
                }
            }
        }
    }
}

void ST7735_WriteString(uint16_t x, uint16_t y, const char *str, FontDef font, uint16_t color, uint16_t bgcolor) {
    while (*str) {
        if (x + font.width >= ST7735_WIDTH) {
            x = 0;
            y += font.height;
            if (y + font.height >= ST7735_HEIGHT) {
                break;
            }
            if (*str == ' ') {
                str++;
                continue;
            }
        }

        ST7735_WriteChar(x, y, *str, font, color, bgcolor);
        x += font.width;
        str++;
    }
}
