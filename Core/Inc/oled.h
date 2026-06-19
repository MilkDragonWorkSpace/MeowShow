/**
 ******************************************************************************
 * @file    oled.h
 * @brief   CH1116 OLED display driver API (128x64, I2C, SSD1306-compatible)
 ******************************************************************************
 */

#ifndef __OLED_H
#define __OLED_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <string.h>

/* Exported constants --------------------------------------------------------*/

/** @defgroup OLED_Dimensions OLED Display Dimensions
  * @{
  */
#define OLED_WIDTH             128U
#define OLED_HEIGHT            64U
#define OLED_PAGES             8U      /* 64 / 8 = 8 pages */
#define OLED_BUFFER_SIZE       1024U   /* 8 pages x 128 columns */
/**
  * @}
  */

/** @defgroup OLED_Colors Pixel Colors
  * @{
  */
#define OLED_BLACK             0U
#define OLED_WHITE             1U
/**
  * @}
  */

/**
 * @brief Font descriptor structure.
 *
 * Each font is stored in column-major order: for a W×H glyph,
 * the first W bytes cover glyph column 0 (top-to-bottom within the column),
 * the next W bytes cover column 1, etc.
 */
typedef struct {
    const uint8_t *data;        /**< Pointer to font bitmap data */
    uint8_t        width;       /**< Character width in pixels */
    uint8_t        height;      /**< Character height in pixels */
    uint8_t        first_char;  /**< First character code in the font */
    uint8_t        last_char;   /**< Last character code in the font */
    uint8_t        bytes_per_col; /**< Bytes per column = ceil(height/8) */
} FontDef;

/**
 * @brief Monochrome image descriptor.
 */
typedef struct {
    const uint8_t *data;        /**< Pointer to image bitmap (column-major) */
    uint8_t        width;       /**< Image width in pixels */
    uint8_t        height;      /**< Image height in pixels */
} OLED_Image;

/* Exported variables --------------------------------------------------------*/

/** Global framebuffer: 8 pages x 128 columns, column-major layout.
 *  buffer[page][col] — each byte = 8 vertical pixels, bit 0 = top. */
extern uint8_t OLED_Buffer[OLED_PAGES][OLED_WIDTH];

/* Exported font declarations ------------------------------------------------*/
extern const FontDef font8x6;
extern const FontDef font12x6;
extern const FontDef font16x8;
extern const FontDef font16x16;

/* Exported functions prototypes ---------------------------------------------*/

/** @defgroup OLED_Init Initialization
  * @{
  */
void OLED_Init(void);
/**
  * @}
  */

/** @defgroup OLED_Framebuffer Framebuffer Management
  * @{
  */
void OLED_NewFrame(void);
void OLED_ShowFrame(void);
void OLED_DrawFullBitmap(const uint8_t data[OLED_BUFFER_SIZE]);
/**
  * @}
  */

/** @defgroup OLED_Drawing Drawing Primitives
  * @{
  */
void OLED_SetPixel(uint8_t x, uint8_t y, uint8_t color);
void OLED_DrawLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t color);
void OLED_DrawRectangle(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color);
void OLED_DrawFilledRectangle(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color);
void OLED_DrawCircle(uint8_t cx, uint8_t cy, uint8_t r, uint8_t color);
void OLED_DrawFilledCircle(uint8_t cx, uint8_t cy, uint8_t r, uint8_t color);
void OLED_DrawTriangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2,
                       uint8_t x3, uint8_t y3, uint8_t color);
void OLED_DrawFilledTriangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2,
                             uint8_t x3, uint8_t y3, uint8_t color);
/**
  * @}
  */

/** @defgroup OLED_Text Text Rendering
  * @{
  */
void OLED_PrintString(uint8_t x, uint8_t y, const char *str,
                      const FontDef *font, uint8_t color);
/**
  * @}
  */

/** @defgroup OLED_Image Image Rendering
  * @{
  */
void OLED_DrawImage(uint8_t x, uint8_t y, const OLED_Image *img, uint8_t color);
/**
  * @}
  */

#ifdef __cplusplus
}
#endif

#endif /* __OLED_H */
