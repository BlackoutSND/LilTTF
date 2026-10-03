/*
 * @file main_sample.c
 * @brief Sample integration of LilTTF on STM32 with SSD1327 OLED.
 */

#include "main.h"
#include "LilTTF/Rasterizer.h"
#include <stdio.h>
#include <string.h>

/* Display Dimensions (SSD1327 Grayscale OLED) */
#define OLED_WIDTH   128
#define OLED_HEIGHT  96

/* Grayscale: 4-bits per pixel => 2 pixels per byte */
#define OLED_BUFFER_SIZE ((OLED_WIDTH * OLED_HEIGHT) / 2)

/* Pin Definitions - Adjust according to your hardware target */
#define OLED_CS_PIN    GPIO_PIN_6
#define OLED_CS_PORT   GPIOB
#define OLED_DC_PIN    GPIO_PIN_8
#define OLED_DC_PORT   GPIOA
#define OLED_RST_PIN   GPIO_PIN_9
#define OLED_RST_PORT  GPIOA

#define OLED_CS_LOW()    HAL_GPIO_WritePin(OLED_CS_PORT, OLED_CS_PIN, GPIO_PIN_RESET)
#define OLED_CS_HIGH()   HAL_GPIO_WritePin(OLED_CS_PORT, OLED_CS_PIN, GPIO_PIN_SET)
#define OLED_DC_CMD()    HAL_GPIO_WritePin(OLED_DC_PORT, OLED_DC_PIN, GPIO_PIN_RESET)
#define OLED_DC_DATA()   HAL_GPIO_WritePin(OLED_DC_PORT, OLED_DC_PIN, GPIO_PIN_SET)
#define OLED_RST_LOW()   HAL_GPIO_WritePin(OLED_RST_PORT, OLED_RST_PIN, GPIO_PIN_RESET)
#define OLED_RST_HIGH()  HAL_GPIO_WritePin(OLED_RST_PORT, OLED_RST_PIN, GPIO_PIN_SET)

extern SPI_HandleTypeDef hspi1;

/* Global 4-bit Framebuffer (6144 bytes) */
static uint8_t s_frameBuffer[OLED_BUFFER_SIZE];

/* Example external font binary array generated via tools/font_to_c.py */
extern const uint8_t FontData[];
extern const uint32_t FontData_SIZE;

/* ========================================================================= */
/*                          OLED LOW-LEVEL DRIVER                            */
/* ========================================================================= */

static void OLED_WriteCmd(uint8_t cmd) {
  OLED_DC_CMD();
  OLED_CS_LOW();
  HAL_SPI_Transmit(&hspi1, &cmd, 1, 10);
  OLED_CS_HIGH();
}

static void OLED_Init_SSD1327(void) {
  OLED_RST_HIGH();
  HAL_Delay(10);
  OLED_RST_LOW();
  HAL_Delay(100);
  OLED_RST_HIGH();
  HAL_Delay(100);

  OLED_WriteCmd(0xAE); /* Display OFF */
  OLED_WriteCmd(0x15); /* Set Column Address */
  OLED_WriteCmd(0x00);
  OLED_WriteCmd(0x3F);
  OLED_WriteCmd(0x75); /* Set Row Address */
  OLED_WriteCmd(0x00);
  OLED_WriteCmd(0x5F);
  OLED_WriteCmd(0x81); /* Contrast */
  OLED_WriteCmd(0x80);
  OLED_WriteCmd(0xA0); /* Segment Re-map */
  OLED_WriteCmd(0x51);
  OLED_WriteCmd(0xA1); /* Start Line */
  OLED_WriteCmd(0x00);
  OLED_WriteCmd(0xA2); /* Display Offset */
  OLED_WriteCmd(0x20);
  OLED_WriteCmd(0xA4); /* Normal Display */
  OLED_WriteCmd(0xA8); /* Multiplex Ratio */
  OLED_WriteCmd(0x5F);
  OLED_WriteCmd(0xB1); /* Phase Length */
  OLED_WriteCmd(0xF1);
  OLED_WriteCmd(0xB3); /* Clock Divider */
  OLED_WriteCmd(0xF1);
  OLED_WriteCmd(0xAB); /* Function Selection A */
  OLED_WriteCmd(0x01);
  OLED_WriteCmd(0xB6); /* Pre-charge Period */
  OLED_WriteCmd(0x32);
  OLED_WriteCmd(0xBE); /* VCOMH Deselect */
  OLED_WriteCmd(0x0F);
  OLED_WriteCmd(0xBC); /* Function Selection B */
  OLED_WriteCmd(0x08);
  OLED_WriteCmd(0xD5); /* Display Enhancement */
  OLED_WriteCmd(0x62);
  OLED_WriteCmd(0xFD); /* Command Lock */
  OLED_WriteCmd(0x12);

  HAL_Delay(50);
  OLED_WriteCmd(0xAF); /* Display ON */
}

static void OLED_UpdateScreen(void) {
  OLED_WriteCmd(0x15);
  OLED_WriteCmd(0x00);
  OLED_WriteCmd(0x3F);
  OLED_WriteCmd(0x75);
  OLED_WriteCmd(0x00);
  OLED_WriteCmd(0x5F);

  OLED_DC_DATA();
  OLED_CS_LOW();
  HAL_SPI_Transmit(&hspi1, s_frameBuffer, OLED_BUFFER_SIZE, HAL_MAX_DELAY);
  OLED_CS_HIGH();
}

static void OLED_Clear(uint8_t pattern) {
  memset(s_frameBuffer, pattern, sizeof(s_frameBuffer));
}

/* ========================================================================= */
/*                         UTF-8 STRING DRAWING                              */
/* ========================================================================= */

/**
 * @brief Render a UTF-8 string into the display buffer at (x, y) with dynamic sizing.
 */
void OLED_DrawString(uint16_t x, uint16_t y, const char *str, int font_size) {
  int canvasHeight = GetFontVMetrics(font_size, NULL, NULL);
  if (canvasHeight < font_size)
    canvasHeight = font_size;

  int currentX = (int)x;
  if (currentX % 2 != 0)
    currentX++; /* Align to 4-bit pixel boundaries */

  while (*str) {
    uint32_t codepoint;
    int bytes_read = 0;
    uint8_t byte = (uint8_t)*str;

    /* Decode UTF-8 sequence */
    if (byte < 0x80) {
      codepoint = byte;
      bytes_read = 1;
    } else if ((byte & 0xE0) == 0xC0) {
      codepoint = ((byte & 0x1F) << 6) | ((uint8_t)str[1] & 0x3F);
      bytes_read = 2;
    } else if ((byte & 0xF0) == 0xE0) {
      codepoint = ((byte & 0x0F) << 12) | (((uint8_t)str[1] & 0x3F) << 6) |
                  ((uint8_t)str[2] & 0x3F);
      bytes_read = 3;
    } else if ((byte & 0xF8) == 0xF0) {
      codepoint = ((byte & 0x07) << 18) | (((uint8_t)str[1] & 0x3F) << 12) |
                  (((uint8_t)str[2] & 0x3F) << 6) | ((uint8_t)str[3] & 0x3F);
      bytes_read = 4;
    } else {
      codepoint = '?';
      bytes_read = 1;
    }

    int width = 0;
    int advance = 0;

    /* Rasterize directly into 4-bit packed grayscale framebuffer with blending */
    if (GetBitmapForChar(codepoint, s_frameBuffer, currentX, y,
                         OLED_WIDTH / 2, OLED_HEIGHT, font_size,
                         &width, &advance, true, true)) {
      int renderWidth = width;
      if (renderWidth % 2 != 0)
        renderWidth++;
      if (currentX + renderWidth > OLED_WIDTH)
        break; /* Screen clip limit */

      if (advance % 2 != 0)
        advance++;
      currentX += advance;
    }

    str += bytes_read;
  }
}

/* ========================================================================= */
/*                             APPLICATION ENTRY                             */
/* ========================================================================= */

void App_Run(void) {
  /* 1. Initialize display hardware */
  OLED_Init_SSD1327();
  OLED_Clear(0x00);

  /* 2. Initialize font rasterizer with font in Flash */
  if (!FontRasterizer_Init(FontData, FontData_SIZE)) {
    /* Handle font header parse error */
    while (1);
  }

  /* 3. Render strings at different font sizes */
  OLED_DrawString(0,  5, "Embedded TTF", 18);
  OLED_DrawString(0, 32, "Vector Font",  24);
  OLED_DrawString(0, 68, "Speed: 1.2ms", 14);

  /* 4. Flush framebuffer to display */
  OLED_UpdateScreen();
}
