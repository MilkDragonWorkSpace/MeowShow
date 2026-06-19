/**
 ******************************************************************************
 * @file    gif_animation.c
 * @brief   GIF Animation playback engine implementation.
 *
 * Two modes:
 *   1. Procedural — frames drawn via callback (3 built-in demos)
 *   2. Bitmap — blit pre-rendered 1024-byte frames
 *
 * Each frame rendering sequence:
 *   OLED_NewFrame() → content → divider + name label → OLED_ShowFrame() → delay
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "gif_animation.h"
#include "oled.h"
#include "main.h"            /* for HAL_Delay, HAL_GetTick */
#include <string.h>

/* Private defines -----------------------------------------------------------*/

/** Y coordinate of the horizontal divider between animation and label bar. */
#define LABEL_DIVIDER_Y   52U
/** Y coordinate of the label text below the divider. */
#define LABEL_TEXT_Y      54U
/** Default frame delay when none specified (ms). */
#define DEFAULT_DELAY_MS  100U

/* ---------------------------------------------------------------------------*/
/*                      Non-blocking Playback State                           */
/* ---------------------------------------------------------------------------*/

static const GifAnimation *nb_anim      = NULL;
static uint16_t            nb_frame_idx = 0;
static uint16_t            nb_loop      = 0;
static uint32_t            nb_next_tick = 0;
static uint8_t             nb_active    = 0;

/* ---------------------------------------------------------------------------*/
/*                      Internal Helpers                                      */
/* ---------------------------------------------------------------------------*/

/**
 * @brief  Draw the bottom status bar: horizontal divider + animation name.
 * @param  name  Animation name string
 */
static void DrawLabelBar(const char *name)
{
    /* Horizontal divider line at y=52 */
    OLED_DrawLine(0, LABEL_DIVIDER_Y, OLED_WIDTH - 1U, LABEL_DIVIDER_Y,
                  OLED_WHITE);
    /* Animation name below the divider (use 8x6 small font) */
    if (name != NULL && name[0] != '\0') {
        OLED_PrintString(2, LABEL_TEXT_Y, name, &font8x6, OLED_WHITE);
    }
}

/**
 * @brief  Render a single bitmap frame to the OLED (blocking wait for delay).
 * @param  frame  Pointer to the GifFrame to display
 * @param  name   Animation name for the label bar
 */
static void RenderBitmapFrame(const GifFrame *frame, const char *name)
{
    OLED_NewFrame();
    if (frame->bitmap != NULL) {
        OLED_DrawFullBitmap(frame->bitmap);
    }
    DrawLabelBar(name);
    OLED_ShowFrame();
    HAL_Delay(frame->delay_ms > 0 ? frame->delay_ms : DEFAULT_DELAY_MS);
}

/* ---------------------------------------------------------------------------*/
/*                  Bitmap Animation — Blocking API                           */
/* ---------------------------------------------------------------------------*/

/**
 * @brief  Play a pre-rendered bitmap animation (blocking).
 *
 * @param  anim          Pointer to the animation descriptor
 * @param  loop_forever  If non-zero, loop indefinitely; otherwise use
 *                       anim->loop_count (0 also means loop forever)
 */
void GifAnim_Play(const GifAnimation *anim, uint8_t loop_forever)
{
    uint16_t loop;
    uint16_t max_loops;

    if (anim == NULL || anim->frames == NULL || anim->frame_count == 0) {
        return;
    }

    max_loops = loop_forever ? 0U : anim->loop_count;

    loop = 0;
    while (1) {
        for (uint16_t i = 0; i < anim->frame_count; i++) {
            RenderBitmapFrame(&anim->frames[i], anim->name);
        }

        /* Check loop termination */
        if (max_loops > 0U) {
            loop++;
            if (loop >= max_loops) {
                break;
            }
        }
        /* max_loops == 0 means infinite */
    }
}

/* ---------------------------------------------------------------------------*/
/*                  Bitmap Animation — Non-blocking API                       */
/* ---------------------------------------------------------------------------*/

/**
 * @brief  Start a non-blocking bitmap animation.
 *
 * The animation advances each time GifAnim_Tick() is called and the
 * frame delay has elapsed.
 *
 * @param  anim  Pointer to the animation descriptor
 */
void GifAnim_Start(const GifAnimation *anim)
{
    nb_anim      = anim;
    nb_frame_idx = 0;
    nb_loop      = 0;
    nb_next_tick = 0;
    nb_active    = 1;
}

/**
 * @brief  Non-blocking tick: advance to the next frame when delay expires.
 *
 * Call this from the main loop or a timer ISR. When the current frame's
 * delay has elapsed since the last advance, it renders the next frame.
 *
 * If the animation has finished its loop count, nb_active is cleared.
 */
void GifAnim_Tick(void)
{
    uint32_t now;

    if (!nb_active || nb_anim == NULL || nb_anim->frames == NULL) {
        return;
    }

    now = HAL_GetTick();

    /* First frame or delay elapsed? */
    if (nb_next_tick == 0 || (now - nb_next_tick) < 0x80000000U) {
        /* Time to advance */
        const GifFrame *frame = &nb_anim->frames[nb_frame_idx];

        OLED_NewFrame();
        if (frame->bitmap != NULL) {
            OLED_DrawFullBitmap(frame->bitmap);
        }
        DrawLabelBar(nb_anim->name);
        OLED_ShowFrame();

        /* Schedule next tick */
        nb_next_tick = now + (uint32_t)frame->delay_ms;

        /* Advance frame index */
        nb_frame_idx++;
        if (nb_frame_idx >= nb_anim->frame_count) {
            nb_frame_idx = 0;

            /* Check loop count */
            if (nb_anim->loop_count > 0U) {
                nb_loop++;
                if (nb_loop >= nb_anim->loop_count) {
                    nb_active = 0;  /* Done */
                }
            }
        }
    }
}

/* ---------------------------------------------------------------------------*/
/*              Procedural Animation — Blocking API                           */
/* ---------------------------------------------------------------------------*/

/**
 * @brief  Play a procedural animation (blocking).
 *
 * Each frame: clear → call draw_cb → label bar → show → delay.
 *
 * @param  draw_cb      Frame drawing callback
 * @param  frame_count  Number of frames
 * @param  name         Animation name for the label bar
 * @param  loop_count   Number of loops (0 = infinite)
 */
void GifAnim_PlayProc(GifFrameDrawCallback draw_cb,
                      uint16_t frame_count,
                      const char *name,
                      uint16_t loop_count)
{
    uint16_t loop;
    uint16_t max_loops;

    if (draw_cb == NULL || frame_count == 0) {
        return;
    }

    max_loops = loop_count;

    loop = 0;
    while (1) {
        for (uint16_t i = 0; i < frame_count; i++) {
            OLED_NewFrame();
            draw_cb(i, frame_count);
            DrawLabelBar(name);
            OLED_ShowFrame();
            HAL_Delay(DEFAULT_DELAY_MS);
        }

        if (max_loops > 0U) {
            loop++;
            if (loop >= max_loops) {
                break;
            }
        }
    }
}

/* ---------------------------------------------------------------------------*/
/*                   Built-in Demo Animations                                 */
/* ---------------------------------------------------------------------------*/

/**
 * @brief  Blink Face — frame 0 callback.
 *         Draws a smiley face with eyes open.
 */
static void DemoBlink_DrawOpen(uint16_t fi, uint16_t fc)
{
    (void)fi; (void)fc;

    /* Face circle (center 64,28 radius 18) */
    OLED_DrawCircle(64, 28, 18, OLED_WHITE);

    /* Left eye — open (filled circle) */
    OLED_DrawFilledCircle(56, 22, 3, OLED_WHITE);
    /* Right eye — open */
    OLED_DrawFilledCircle(72, 22, 3, OLED_WHITE);

    /* Smile mouth: arc from (52,34) to (76,34) */
    for (uint8_t sx = 52; sx <= 76; sx++) {
        /* Quadratic smile: y = 34 + 0.03*(x-64)^2 - 4 */
        int16_t dx = (int16_t)sx - 64;
        int16_t dy = 34 + (dx * dx) / 35 - 4;
        if (dy >= 0 && dy < (int16_t)OLED_HEIGHT) {
            OLED_SetPixel(sx, (uint8_t)dy, OLED_WHITE);
        }
    }
}

/**
 * @brief  Blink Face — frame with eyes closed (blink).
 */
static void DemoBlink_DrawClosed(uint16_t fi, uint16_t fc)
{
    (void)fi; (void)fc;

    /* Face circle */
    OLED_DrawCircle(64, 28, 18, OLED_WHITE);

    /* Eyes closed — horizontal lines */
    OLED_DrawLine(53, 22, 59, 22, OLED_WHITE);
    OLED_DrawLine(69, 22, 75, 22, OLED_WHITE);

    /* Smile */
    for (uint8_t sx = 52; sx <= 76; sx++) {
        int16_t dx = (int16_t)sx - 64;
        int16_t dy = 34 + (dx * dx) / 35 - 4;
        if (dy >= 0 && dy < (int16_t)OLED_HEIGHT) {
            OLED_SetPixel(sx, (uint8_t)dy, OLED_WHITE);
        }
    }
}

/**
 * @brief  Heart Beat Draw — large heart.
 */
static void DemoHeart_DrawLarge(uint16_t fi, uint16_t fc)
{
    (void)fi; (void)fc;

    /* Large heart centered at (64,28) */
    for (int16_t y = 8; y < 48; y++) {
        for (int16_t x = 24; x < 104; x++) {
            int16_t rx = x - 64;
            int16_t ry = y - 28;
            /* Heart equation: (x^2 + y^2 - r^2)^3 - x^2 * y^3 < 0 */
            int32_t t = rx * rx + ry * ry - 300;
            int32_t val = t * t * t - rx * rx * ry * ry * ry;
            if (val < 0) {
                OLED_SetPixel((uint8_t)x, (uint8_t)y, OLED_WHITE);
            }
        }
    }
}

/**
 * @brief  Heart Beat Draw — small heart.
 */
static void DemoHeart_DrawSmall(uint16_t fi, uint16_t fc)
{
    (void)fi; (void)fc;

    for (int16_t y = 12; y < 44; y++) {
        for (int16_t x = 30; x < 98; x++) {
            int16_t rx = x - 64;
            int16_t ry = y - 28;
            int32_t t = rx * rx + ry * ry - 180;
            int32_t val = t * t * t - rx * rx * ry * ry * ry;
            if (val < 0) {
                OLED_SetPixel((uint8_t)x, (uint8_t)y, OLED_WHITE);
            }
        }
    }
}

/**
 * @brief  Loading Spinner Draw — rotating arc at given angle.
 * @param  angle  Angle index (0–7)
 */
static void DemoSpinner_DrawAngle(uint16_t angle)
{
    int16_t cx = 64, cy = 28;
    int16_t r  = 14;

    /* Draw center dot */
    OLED_DrawFilledCircle((uint8_t)cx, (uint8_t)cy, 2, OLED_WHITE);

    /* Draw 8 arms, highlight the one matching `angle` */
    for (uint8_t a = 0; a < 8; a++) {
        int16_t dx, dy;
        /* Precomputed sin/cos approximations for 8 angles */
        switch (a) {
            case 0: dx =  0;  dy = -1;  break;  /* 12 o'clock */
            case 1: dx =  1;  dy = -1;  break;  /* 1:30 */
            case 2: dx =  1;  dy =  0;  break;  /* 3 o'clock */
            case 3: dx =  1;  dy =  1;  break;  /* 4:30 */
            case 4: dx =  0;  dy =  1;  break;  /* 6 o'clock */
            case 5: dx = -1;  dy =  1;  break;  /* 7:30 */
            case 6: dx = -1;  dy =  0;  break;  /* 9 o'clock */
            case 7: dx = -1;  dy = -1;  break;  /* 10:30 */
            default: dx = 0; dy = 0; break;
        }

        int16_t ex = cx + dx * r;
        int16_t ey = cy + dy * r;

        if (a == angle) {
            /* Highlighted arm — draw as a line */
            if (ex >= 0 && ey >= 0 && ex < (int16_t)OLED_WIDTH && ey < (int16_t)OLED_HEIGHT) {
                OLED_DrawLine((uint8_t)cx, (uint8_t)cy, (uint8_t)ex, (uint8_t)ey, OLED_WHITE);
            }
        } else {
            /* Dim arm — just a dot at the end */
            if (ex >= 0 && ey >= 0 && ex < (int16_t)OLED_WIDTH && ey < (int16_t)OLED_HEIGHT) {
                OLED_SetPixel((uint8_t)ex, (uint8_t)ey, OLED_WHITE);
            }
        }
    }
}

/* ---------------------------------------------------------------------------*/
/*                       Demo Orchestrator                                    */
/* ---------------------------------------------------------------------------*/

/**
 * @brief  Frame callbacks for Blink Face (5 frames):
 *         open, open, closed, closed, open
 */
static void DemoBlink_CB(uint16_t frame_index, uint16_t frame_count)
{
    (void)frame_count;
    if (frame_index == 2 || frame_index == 3) {
        DemoBlink_DrawClosed(frame_index, frame_count);
    } else {
        DemoBlink_DrawOpen(frame_index, frame_count);
    }
}

/**
 * @brief  Frame callbacks for Heart Beat (2 frames):
 *         large, small
 */
static void DemoHeart_CB(uint16_t frame_index, uint16_t frame_count)
{
    (void)frame_count;
    if (frame_index == 0) {
        DemoHeart_DrawLarge(frame_index, frame_count);
    } else {
        DemoHeart_DrawSmall(frame_index, frame_count);
    }
}

/**
 * @brief  Frame callbacks for Loading Spinner (8 frames):
 *         each frame highlights a different arm
 */
static void DemoSpinner_CB(uint16_t frame_index, uint16_t frame_count)
{
    (void)frame_count;
    DemoSpinner_DrawAngle(frame_index);
}

/**
 * @brief  Play all three built-in demo animations in sequence.
 *
 * Shows: Blink Face → Heart Beat → Loading Spinner.
 * Each demo plays for a fixed number of loops.
 */
void GifAnim_ShowDemo(void)
{
    /* 1. Blink Face — 5 frames, 3 loops (~1.5s) */
    GifAnim_PlayProc(DemoBlink_CB, 5, "Blink", 3);

    /* 2. Heart Beat — 2 frames, 4 loops (~0.8s) */
    GifAnim_PlayProc(DemoHeart_CB, 2, "Heart", 4);

    /* 3. Loading Spinner — 8 frames, 3 loops (~2.4s) */
    GifAnim_PlayProc(DemoSpinner_CB, 8, "Loader", 3);
}
