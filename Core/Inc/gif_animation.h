/**
 ******************************************************************************
 * @file    gif_animation.h
 * @brief   GIF Animation playback engine for OLED display.
 *
 * Supports two modes:
 *   1. Procedural — frames drawn via callback (no Flash overhead)
 *   2. Bitmap — pre-rendered 1024-byte frames stored in Flash
 *
 * Both modes share the same frame playback logic (clear → blit/draw →
 * label bar → show → delay).
 ******************************************************************************
 */

#ifndef __GIF_ANIMATION_H
#define __GIF_ANIMATION_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>

/* Exported types ------------------------------------------------------------*/

/**
 * @brief A single pre-rendered animation frame.
 */
typedef struct {
    const uint8_t *bitmap;   /**< 1024-byte CH1116-format bitmap */
    uint16_t       delay_ms; /**< Display duration in milliseconds */
} GifFrame;

/**
 * @brief A complete pre-rendered bitmap animation.
 *
 * @note loop_count = 0 means infinite loop.
 */
typedef struct {
    const GifFrame *frames;
    uint16_t        frame_count;
    uint16_t        loop_count;  /**< 0 = loop forever */
    const char     *name;
} GifAnimation;

/**
 * @brief Procedural frame drawing callback.
 *
 * Called each frame to draw the animation content. The framebuffer
 * starts clear; the callback should use OLED drawing APIs to render
 * the current frame.
 *
 * @param frame_index  Current frame number (0-based)
 * @param frame_count  Total frame count
 */
typedef void (*GifFrameDrawCallback)(uint16_t frame_index, uint16_t frame_count);

/* Exported functions prototypes ---------------------------------------------*/

/** @defgroup GifAnim_Bitmap Bitmap Animation API
  * @{
  */
void GifAnim_Play(const GifAnimation *anim, uint8_t loop_forever);
void GifAnim_Start(const GifAnimation *anim);
void GifAnim_Tick(void);
/**
  * @}
  */

/** @defgroup GifAnim_Procedural Procedural Animation API
  * @{
  */
void GifAnim_PlayProc(GifFrameDrawCallback draw_cb,
                      uint16_t frame_count,
                      const char *name,
                      uint16_t loop_count);
void GifAnim_ShowDemo(void);
/**
  * @}
  */

#ifdef __cplusplus
}
#endif

#endif /* __GIF_ANIMATION_H */
