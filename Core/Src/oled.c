/**
 ******************************************************************************
 * @file    oled.c
 * @brief   CH1116 OLED display driver (128x64, I2C, SSD1306-compatible)
 *
 * Framebuffer layout: column-major (page-first).
 * OLED_Buffer[page][col] — each byte = 8 vertical pixels within that page.
 * Bit 0 = top pixel, Bit 7 = bottom pixel. Matches CH1116 horizontal
 * addressing mode hardware auto-increment order.
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "oled.h"
#include "main.h"

/* External variables --------------------------------------------------------*/
extern I2C_HandleTypeDef hi2c1;

/* Private defines -----------------------------------------------------------*/

/**
 * I2C address of the CH1116 OLED.
 *
 * The module's 7-bit address is 0x3D (SA0 pin = HIGH).
 * Common datasheet notation writes this as 8-bit write address 0x7A
 * (i.e. 0x3D << 1). HAL expects the address left-shifted by 1 bit,
 * so we pass (0x3D << 1) = 0x7A.
 *
 * If your module uses the other common address, try 0x3C (0x78 in 8-bit).
 */
#define OLED_I2C_ADDR           (0x3DU << 1)

/** Control byte: following bytes are commands. */
#define OLED_CTRL_CMD           0x00U
/** Control byte: following bytes are data (GDDRAM write). */
#define OLED_CTRL_DATA          0x40U

/** I2C timeout in milliseconds. Using a finite timeout prevents the MCU
 *  from hanging forever if the OLED doesn't ACK its address. */
#define OLED_I2C_TIMEOUT        100U

/* CH1116 / SSD1306 command set ----------------------------------------------*/

#define CMD_DISPLAY_OFF         0xAEU
#define CMD_DISPLAY_ON          0xAFU
#define CMD_SET_MUX_RATIO       0xA8U
#define CMD_SET_DISPLAY_OFFSET  0xD3U
#define CMD_SET_START_LINE      0x40U
#define CMD_SET_SEG_REMAP       0xA1U   /* column 127 = SEG0 (A1=remap, A0=normal) */
#define CMD_SET_COM_SCAN_DIR    0xC8U   /* C8=remap (COM[N-1]→COM0), C0=normal */
#define CMD_SET_COM_PINS        0xDAU
#define CMD_SET_CONTRAST        0x81U
#define CMD_SET_PRECHARGE       0xD9U
#define CMD_SET_VCOMH           0xDBU
#define CMD_CHARGE_PUMP         0x8DU
#define CMD_ENTIRE_DISPLAY_ON   0xA4U   /* A4=normal, A5=all on */
#define CMD_SET_INVERSE         0xA6U   /* A6=normal, A7=inverse */
#define CMD_DEACTIVATE_SCROLL   0x2EU
#define CMD_SET_MEM_ADDR_MODE   0x20U
#define CMD_SET_COL_ADDR        0x21U
#define CMD_SET_PAGE_ADDR       0x22U

/* Global variables ----------------------------------------------------------*/

/**
 * @brief OLED framebuffer: 8 pages x 128 columns.
 *
 * Layout: buffer[page][col]. Each byte holds 8 vertical pixels for
 * the given page (page 0 = rows 0–7, page 1 = rows 8–15, …).
 * Bit 0 = top pixel of that page's 8-row band.
 */
uint8_t OLED_Buffer[OLED_PAGES][OLED_WIDTH];

/* Private function prototypes -----------------------------------------------*/

static void OLED_WriteCmd(uint8_t cmd);
static void OLED_WriteData(const uint8_t *data, uint16_t len);
static void OLED_DrawGlyph(uint8_t x, uint8_t y, char ch,
                           const FontDef *font, uint8_t color);
static void OLED_SwapInt(int16_t *a, int16_t *b);

/* ---------------------------------------------------------------------------*/
/*                          I2C Communication                                 */
/* ---------------------------------------------------------------------------*/

/**
 * @brief  Send a single command byte to the OLED.
 * @param  cmd  Command byte
 */
static void OLED_WriteCmd(uint8_t cmd)
{
    uint8_t buf[2];
    buf[0] = OLED_CTRL_CMD;
    buf[1] = cmd;
    HAL_I2C_Master_Transmit(&hi2c1, OLED_I2C_ADDR, buf, 2, OLED_I2C_TIMEOUT);
}

/**
 * @brief  Send data bytes to the OLED GDDRAM.
 *
 * Builds a buffer with the 0x40 control byte prepended, then sends
 * everything in a single HAL_I2C_Master_Transmit call. This approach
 * avoids HAL_I2C_Mem_Write which has unreliable size limits on F1.
 *
 * Callers must ensure len does not cause the stack buffer to overflow.
 * Our maximum is 128 bytes per page, so a 129-byte buffer is safe.
 *
 * @param  data  Pointer to data buffer (max 128 bytes per call)
 * @param  len   Number of bytes to send (max 128)
 */
static void OLED_WriteData(const uint8_t *data, uint16_t len)
{
    /* Stack buffer: 1 control byte + up to 128 data bytes = 129 bytes max.
     * This is well within the 20 KB RAM budget of STM32F103C8T6. */
    uint8_t buf[129];
    uint16_t i;

    if (len > 128U) {
        len = 128U;  /* Safety clamp */
    }

    buf[0] = OLED_CTRL_DATA;          /* Control byte: next bytes are GDDRAM data */
    for (i = 0; i < len; i++) {
        buf[1 + i] = data[i];
    }

    HAL_I2C_Master_Transmit(&hi2c1, OLED_I2C_ADDR, buf, (uint16_t)(len + 1U),
                            OLED_I2C_TIMEOUT);
}

/* ---------------------------------------------------------------------------*/
/*                          Initialization                                    */
/* ---------------------------------------------------------------------------*/

/**
 * @brief  Initialize the CH1116 OLED display.
 *
 * Sequence based on CH1116 datasheet (command-set compatible with SSD1306):
 *  1. Display OFF
 *  2. Configure oscillator, mux ratio, offset
 *  3. Enable charge pump
 *  4. Set horizontal addressing mode
 *  5. Configure segment remap and COM scan direction
 *  6. COM pins hardware config, contrast, precharge, VCOMH
 *  7. Normal display, no inverse, no scroll
 *  8. Clear framebuffer, Display ON
 */
void OLED_Init(void)
{
    /* Step 1: Wait for OLED power-up (slower than STM32 boot) */
    HAL_Delay(50);

    /* Step 2: Display OFF during configuration */
    OLED_WriteCmd(CMD_DISPLAY_OFF);

    /* Step 3: Fundamental configuration */
    OLED_WriteCmd(CMD_SET_MUX_RATIO);
    OLED_WriteCmd(0x3FU);               /* 64 rows (0x3F = 63d) */

    OLED_WriteCmd(CMD_SET_DISPLAY_OFFSET);
    OLED_WriteCmd(0x00U);               /* No offset */

    OLED_WriteCmd(CMD_SET_START_LINE | 0x00U); /* Start line 0 */

    /* Step 4: Charge pump (required for CH1116 / SSD1306 when using internal DC-DC) */
    OLED_WriteCmd(CMD_CHARGE_PUMP);
    OLED_WriteCmd(0x14U);               /* Enable charge pump (0x14 = enable, 0x10 = disable) */

    /* Step 5: Memory addressing mode — page addressing.
     * Page mode is most reliable for page-by-page buffer writes:
     * after each page's 128 bytes, the column auto-resets to the
     * set start column (0x00/0x10). */
    OLED_WriteCmd(CMD_SET_MEM_ADDR_MODE);
    OLED_WriteCmd(0x02U);               /* Page addressing mode */

    /* Step 6: Segment remap and COM scan */
    OLED_WriteCmd(CMD_SET_SEG_REMAP);   /* Remap column address: SEG0 = COL127 */
    OLED_WriteCmd(CMD_SET_COM_SCAN_DIR);/* Remap COM: COM[N-1] → COM0 */

    /* Step 7: COM pins hardware configuration */
    OLED_WriteCmd(CMD_SET_COM_PINS);
    OLED_WriteCmd(0x12U);               /* Alternative COM pin config, disable COM L/R remap */

    /* Step 8: Display parameters */
    OLED_WriteCmd(CMD_SET_CONTRAST);
    OLED_WriteCmd(0x7FU);               /* Mid contrast (0–255) */

    OLED_WriteCmd(CMD_SET_PRECHARGE);
    OLED_WriteCmd(0x22U);               /* Precharge period: phase1=2 DCLK, phase2=2 DCLK */

    OLED_WriteCmd(CMD_SET_VCOMH);
    OLED_WriteCmd(0x20U);               /* VCOMH deselect level ~0.77 x VCC */

    /* Step 9: Display mode */
    OLED_WriteCmd(CMD_ENTIRE_DISPLAY_ON);  /* Normal (follows RAM content) */
    OLED_WriteCmd(CMD_SET_INVERSE);        /* Normal (not inverted) */
    OLED_WriteCmd(CMD_DEACTIVATE_SCROLL);  /* Disable scrolling */

    /* Step 10: Wait, then turn display ON */
    HAL_Delay(100);
    OLED_WriteCmd(CMD_DISPLAY_ON);

    /* Step 11: Clear framebuffer and push to display */
    OLED_NewFrame();
    OLED_ShowFrame();
}

/* ---------------------------------------------------------------------------*/
/*                        Framebuffer Management                              */
/* ---------------------------------------------------------------------------*/

/**
 * @brief  Clear the entire framebuffer (set all pixels to black).
 */
void OLED_NewFrame(void)
{
    memset(OLED_Buffer, 0x00, sizeof(OLED_Buffer));
}

/**
 * @brief  Push the entire framebuffer to the OLED via I2C.
 *
 * Sends the framebuffer page-by-page (128 bytes each, 8 pages total).
 * This avoids I2C transfer-size limitations on STM32F1 (where a single
 * HAL_I2C_Mem_Write call may be capped at 255 bytes) and works reliably
 * across CH1116 / SSD1306 variants.
 *
 * Uses page addressing mode (0x02): for each page we set the page
 * address (0xB0+n), reset column to 0 (0x00, 0x10), then send 128 bytes.
 */
void OLED_ShowFrame(void)
{
    uint8_t page;

    for (page = 0; page < OLED_PAGES; page++) {
        /* Set page address (0xB0–0xB7 for pages 0–7) */
        OLED_WriteCmd(0xB0U | page);

        /* Set column address to 0 (lower nibble, upper nibble) */
        OLED_WriteCmd(0x00U);               /* Column low  = 0 */
        OLED_WriteCmd(0x10U);               /* Column high = 0 */

        /* Send this page's 128 bytes.
         * At <= 128 bytes per transfer, we avoid any HAL I2C size limit.
         * Each byte is sent with a 0x40 control prefix (GDDRAM write). */
        OLED_WriteData(OLED_Buffer[page], OLED_WIDTH);
    }
}

/**
 * @brief  Blit a pre-computed 1024-byte bitmap directly into the framebuffer.
 * @param  data  Pointer to 1024-byte CH1116-format bitmap
 */
void OLED_DrawFullBitmap(const uint8_t data[OLED_BUFFER_SIZE])
{
    memcpy(OLED_Buffer, data, OLED_BUFFER_SIZE);
}

/* ---------------------------------------------------------------------------*/
/*                          Drawing Primitives                                */
/* ---------------------------------------------------------------------------*/

/**
 * @brief  Set a single pixel in the framebuffer.
 * @param  x      X coordinate (0–127)
 * @param  y      Y coordinate (0–63)
 * @param  color  OLED_BLACK or OLED_WHITE
 */
void OLED_SetPixel(uint8_t x, uint8_t y, uint8_t color)
{
    if (x >= OLED_WIDTH || y >= OLED_HEIGHT) {
        return;
    }

    uint8_t page = y / 8U;
    uint8_t bit  = y % 8U;

    if (color) {
        OLED_Buffer[page][x] |= (1U << bit);
    } else {
        OLED_Buffer[page][x] &= ~(1U << bit);
    }
}

/**
 * @brief  Draw a line using Bresenham's algorithm.
 */
void OLED_DrawLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t color)
{
    int16_t dx, dy, sx, sy, err, e2;
    int16_t cx = (int16_t)x1;
    int16_t cy = (int16_t)y1;
    int16_t ex = (int16_t)x2;
    int16_t ey = (int16_t)y2;

    dx = (ex > cx) ? (int16_t)(ex - cx) : (int16_t)(cx - ex);
    dy = (ey > cy) ? (int16_t)(ey - cy) : (int16_t)(cy - ey);
    sx = (cx < ex) ? 1 : -1;
    sy = (cy < ey) ? 1 : -1;
    err = dx - dy;

    while (1) {
        OLED_SetPixel((uint8_t)cx, (uint8_t)cy, color);

        if (cx == ex && cy == ey) {
            break;
        }

        e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            cx += sx;
        }
        if (e2 < dx) {
            err += dx;
            cy += sy;
        }
    }
}

/**
 * @brief  Draw a hollow rectangle outline.
 */
void OLED_DrawRectangle(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color)
{
    OLED_DrawLine(x, y, (uint8_t)(x + w - 1U), y, color);              /* Top */
    OLED_DrawLine(x, (uint8_t)(y + h - 1U), (uint8_t)(x + w - 1U),
                  (uint8_t)(y + h - 1U), color);                       /* Bottom */
    OLED_DrawLine(x, y, x, (uint8_t)(y + h - 1U), color);              /* Left */
    OLED_DrawLine((uint8_t)(x + w - 1U), y, (uint8_t)(x + w - 1U),
                  (uint8_t)(y + h - 1U), color);                       /* Right */
}

/**
 * @brief  Draw a filled rectangle.
 */
void OLED_DrawFilledRectangle(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color)
{
    uint8_t x_end = (uint8_t)(x + w);
    uint8_t y_end = (uint8_t)(y + h);
    uint8_t xi, yi;

    for (yi = y; yi < y_end && yi < OLED_HEIGHT; yi++) {
        for (xi = x; xi < x_end && xi < OLED_WIDTH; xi++) {
            OLED_SetPixel(xi, yi, color);
        }
    }
}

/**
 * @brief  Draw a hollow circle using the Midpoint algorithm.
 */
void OLED_DrawCircle(uint8_t cx, uint8_t cy, uint8_t r, uint8_t color)
{
    int16_t f = 1 - (int16_t)r;
    int16_t dx = 0;
    int16_t dy = -2 * (int16_t)r;
    int16_t x = 0;
    int16_t y = (int16_t)r;

    OLED_SetPixel(cx, (uint8_t)(cy + r), color);
    OLED_SetPixel(cx, (uint8_t)(cy - r), color);
    OLED_SetPixel((uint8_t)(cx + r), cy, color);
    OLED_SetPixel((uint8_t)(cx - r), cy, color);

    while (x < y) {
        if (f >= 0) {
            y--;
            dy += 2;
            f += dy;
        }
        x++;
        dx += 2;
        f += dx + 1;

        OLED_SetPixel((uint8_t)(cx + x), (uint8_t)(cy + y), color);
        OLED_SetPixel((uint8_t)(cx - x), (uint8_t)(cy + y), color);
        OLED_SetPixel((uint8_t)(cx + x), (uint8_t)(cy - y), color);
        OLED_SetPixel((uint8_t)(cx - x), (uint8_t)(cy - y), color);
        OLED_SetPixel((uint8_t)(cx + y), (uint8_t)(cy + x), color);
        OLED_SetPixel((uint8_t)(cx - y), (uint8_t)(cy + x), color);
        OLED_SetPixel((uint8_t)(cx + y), (uint8_t)(cy - x), color);
        OLED_SetPixel((uint8_t)(cx - y), (uint8_t)(cy - x), color);
    }
}

/**
 * @brief  Draw a filled circle using the Midpoint algorithm.
 */
void OLED_DrawFilledCircle(uint8_t cx, uint8_t cy, uint8_t r, uint8_t color)
{
    int16_t f = 1 - (int16_t)r;
    int16_t dx = 0;
    int16_t dy = -2 * (int16_t)r;
    int16_t x = 0;
    int16_t y = (int16_t)r;
    int16_t i;

    /* Draw horizontal lines from vertical extremes inward */
    OLED_DrawLine((uint8_t)(cx - r), cy, (uint8_t)(cx + r), cy, color);

    while (x < y) {
        if (f >= 0) {
            y--;
            dy += 2;
            f += dy;
        }
        x++;
        dx += 2;
        f += dx + 1;

        /* For each pair of symmetric Y coordinates, draw a horizontal span */
        for (i = cx - x; i <= cx + x; i++) {
            OLED_SetPixel((uint8_t)i, (uint8_t)(cy + y), color);
            OLED_SetPixel((uint8_t)i, (uint8_t)(cy - y), color);
        }
        for (i = cx - y; i <= cx + y; i++) {
            OLED_SetPixel((uint8_t)i, (uint8_t)(cy + x), color);
            OLED_SetPixel((uint8_t)i, (uint8_t)(cy - x), color);
        }
    }
}

/**
 * @brief  Draw a hollow triangle outline.
 */
void OLED_DrawTriangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2,
                       uint8_t x3, uint8_t y3, uint8_t color)
{
    OLED_DrawLine(x1, y1, x2, y2, color);
    OLED_DrawLine(x2, y2, x3, y3, color);
    OLED_DrawLine(x3, y3, x1, y1, color);
}

/**
 * @brief  Swap two int16_t values.
 */
static void OLED_SwapInt(int16_t *a, int16_t *b)
{
    int16_t t = *a;
    *a = *b;
    *b = t;
}

/**
 * @brief  Draw a filled triangle using scanline rasterization.
 */
void OLED_DrawFilledTriangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2,
                             uint8_t x3, uint8_t y3, uint8_t color)
{
    int16_t ax = (int16_t)x1, ay = (int16_t)y1;
    int16_t bx = (int16_t)x2, by = (int16_t)y2;
    int16_t cx = (int16_t)x3, cy = (int16_t)y3;
    int16_t dx1, dx2, dx3;
    int16_t y, line_y;
    float   sx, ex;

    /* Sort vertices by Y (ay <= by <= cy) */
    if (ay > by) { OLED_SwapInt(&ay, &by); OLED_SwapInt(&ax, &bx); }
    if (ay > cy) { OLED_SwapInt(&ay, &cy); OLED_SwapInt(&ax, &cx); }
    if (by > cy) { OLED_SwapInt(&by, &cy); OLED_SwapInt(&bx, &cx); }

    /* Compute edge X increments */
    float inv_slope1 = (by > ay) ? (float)(bx - ax) / (float)(by - ay) : 0.0f;
    float inv_slope2 = (cy > ay) ? (float)(cx - ax) / (float)(cy - ay) : 0.0f;
    float inv_slope3 = (cy > by) ? (float)(cx - bx) / (float)(cy - by) : 0.0f;

    /* Top half: ay → by */
    for (y = ay; y <= by && y < (int16_t)OLED_HEIGHT; y++) {
        if (y < 0) continue;
        line_y = y - ay;
        sx = (float)ax + inv_slope1 * (float)line_y;
        ex = (float)ax + inv_slope2 * (float)line_y;
        if (sx > ex) { float t = sx; sx = ex; ex = t; }
        for (dx1 = (int16_t)sx; dx1 <= (int16_t)ex; dx1++) {
            if (dx1 >= 0 && dx1 < (int16_t)OLED_WIDTH) {
                OLED_SetPixel((uint8_t)dx1, (uint8_t)y, color);
            }
        }
    }

    /* Bottom half: by → cy */
    for (y = by; y <= cy && y < (int16_t)OLED_HEIGHT; y++) {
        if (y < 0) continue;
        line_y = y - by;
        sx = (float)bx + inv_slope3 * (float)line_y;
        ex = (float)ax + inv_slope2 * (float)(y - ay);
        if (sx > ex) { float t = sx; sx = ex; ex = t; }
        for (dx1 = (int16_t)sx; dx1 <= (int16_t)ex; dx1++) {
            if (dx1 >= 0 && dx1 < (int16_t)OLED_WIDTH) {
                OLED_SetPixel((uint8_t)dx1, (uint8_t)y, color);
            }
        }
    }
}

/* ---------------------------------------------------------------------------*/
/*                          Text Rendering                                    */
/* ---------------------------------------------------------------------------*/

/**
 * @brief  Draw a single character glyph into the framebuffer.
 *
 * Font data is column-major: for a glyph of width W and height H with
 * bytes_per_col = (H+7)/8, the data layout is:
 *   [col0_byte0, col0_byte1, ...] [col1_byte0, col1_byte1, ...] ...
 *
 * @param  x      Top-left X coordinate
 * @param  y      Top-left Y coordinate
 * @param  ch     ASCII character to draw
 * @param  font   Font descriptor
 * @param  color  OLED_BLACK or OLED_WHITE
 */
static void OLED_DrawGlyph(uint8_t x, uint8_t y, char ch,
                           const FontDef *font, uint8_t color)
{
    uint8_t  col, row_byte, bit;
    uint16_t glyph_offset;
    uint8_t  byte_data;

    if (ch < font->first_char || ch > font->last_char) {
        return;
    }

    glyph_offset = (uint16_t)(ch - font->first_char)
                   * (uint16_t)font->width
                   * (uint16_t)font->bytes_per_col;

    for (col = 0; col < font->width; col++) {
        if ((uint16_t)(x + col) >= OLED_WIDTH) break;

        for (row_byte = 0; row_byte < font->bytes_per_col; row_byte++) {
            byte_data = font->data[glyph_offset
                                   + (uint16_t)col * (uint16_t)font->bytes_per_col
                                   + row_byte];

            for (bit = 0; bit < 8U; bit++) {
                uint16_t pixel_y = (uint16_t)y
                                   + ((uint16_t)row_byte * 8U)
                                   + bit;
                if (pixel_y >= OLED_HEIGHT) break;

                if (byte_data & (1U << bit)) {
                    OLED_SetPixel((uint8_t)(x + col), (uint8_t)pixel_y, color);
                }
            }
        }
    }
}

/**
 * @brief  Render a null-terminated string onto the OLED.
 *
 * Characters are drawn left-to-right. Characters that would extend beyond
 * the right edge are clipped. No automatic wrapping is performed.
 *
 * @param  x      Starting X coordinate
 * @param  y      Starting Y coordinate (top of characters)
 * @param  str    Null-terminated ASCII string
 * @param  font   Font to use
 * @param  color  OLED_BLACK or OLED_WHITE
 */
void OLED_PrintString(uint8_t x, uint8_t y, const char *str,
                      const FontDef *font, uint8_t color)
{
    while (*str) {
        OLED_DrawGlyph(x, y, *str, font, color);
        x += font->width;
        if (x >= OLED_WIDTH) break;
        str++;
    }
}

/* ---------------------------------------------------------------------------*/
/*                          Image Rendering                                   */
/* ---------------------------------------------------------------------------*/

/**
 * @brief  Draw a monochrome image at the specified position.
 *
 * Image data is column-major with the same page-aligned layout as the
 * OLED framebuffer. Each column's bytes cover 8-pixel vertical bands.
 *
 * @param  x      Left X coordinate (should be page-aligned for best results)
 * @param  y      Top Y coordinate (should be page-aligned, i.e. multiple of 8)
 * @param  img    Image descriptor (column-major bitmap)
 * @param  color  OLED_BLACK or OLED_WHITE
 */
void OLED_DrawImage(uint8_t x, uint8_t y, const OLED_Image *img, uint8_t color)
{
    uint8_t  col, row_byte, bit;
    uint16_t img_idx;
    uint8_t  byte_data;
    uint8_t  img_pages = (img->height + 7U) / 8U;

    for (col = 0; col < img->width; col++) {
        uint8_t screen_x = x + col;
        if (screen_x >= OLED_WIDTH) break;

        for (row_byte = 0; row_byte < img_pages; row_byte++) {
            img_idx = (uint16_t)col * (uint16_t)img_pages + row_byte;
            byte_data = img->data[img_idx];

            for (bit = 0; bit < 8U; bit++) {
                uint8_t screen_y = y + (row_byte * 8U) + bit;
                if (screen_y >= OLED_HEIGHT) break;

                if (byte_data & (1U << bit)) {
                    OLED_SetPixel(screen_x, screen_y, color);
                }
            }
        }
    }
}
