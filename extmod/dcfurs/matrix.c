#include <stdio.h>
#include <string.h>

#include "py/runtime.h"
#include "ports/stm32/timer.h"

#include "dcfurs.h"

#define DCF_PIN_CLK         (1 << 4)
#define DCF_PIN_RED         (1 << 12)
#define DCF_PIN_GREEN       (1 << 13)
#define DCF_PIN_BLUE        (1 << 3)
#define DCF_PIN_ROW_BANKB   (DCF_PIN_RED | DCF_PIN_GREEN | DCF_PIN_BLUE)
#define DCF_PIN_COL_BANKB   0x03E7
#define DCF_PIN_ALL_BANKB   (DCF_PIN_ROW_BANKB | DCF_PIN_CLK | DCF_PIN_COL_BANKB)
#define DCF_PIN_COL_BANKC   0x1FF8
#define DCF_PIN_ROW_ENABLE  (1 << 15)
#define DCF_PIN_ALL_BANKC   (DCF_PIN_COL_BANKC | DCF_PIN_ROW_ENABLE)

#define DCF_BITBAND_SRAM(_addr_, _bit_) \
    ((uint32_t *)SRAM1_BB_BASE)[(((unsigned long)_addr_) - SRAM1_BASE) * 8 + (_bit_)]

/* PWM Program data, per GPIO bank. */
struct dcf_pwm_program {
    uint32_t setup[DCF_SETUP_STEPS];
    uint32_t red[DCF_TOTAL_ROWS * DCF_DIMMING_STEPS];
    uint32_t green[DCF_TOTAL_ROWS * DCF_DIMMING_STEPS];
    uint32_t blue[DCF_TOTAL_ROWS * DCF_DIMMING_STEPS];
};

struct dcf_framebuf {
    int count;

    /* Pixel data buffers. */
    struct dcf_pwm_program pxdataB;
    struct dcf_pwm_program pxdataC;

    /* Pixel DMA channels. */
    DMA_HandleTypeDef dma_gpioB;
    DMA_HandleTypeDef dma_gpioC;
};

static struct dcf_framebuf dcf_fb = {0};

/* DMA Descriptors for blitting row data directly to the GPIO registers. */
static const DMA_InitTypeDef dma_init_blit_gpio = {
    .Request             = DMA_REQUEST_6,
    .Direction           = DMA_MEMORY_TO_PERIPH,
    .PeriphInc           = DMA_PINC_DISABLE,
    .MemInc              = DMA_MINC_ENABLE,
    .PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD,
    .MemDataAlignment    = DMA_MDATAALIGN_HALFWORD,
    .Mode                = DMA_NORMAL,
    .Priority            = DMA_PRIORITY_HIGH
};

/* Callback function to drive the matrix display. */
mp_obj_t dcfurs_loop(size_t n_args, const mp_obj_t *args)
{
    if (dcf_fb.count >= (DCF_SETUP_STEPS + (DCF_TOTAL_ROWS * DCF_DIMMING_STEPS * 3))) {
        dcf_fb.count = 0;
    }
    /* Do the next step of the program. */
    GPIOB->BSRR = dcf_fb.pxdataB.setup[dcf_fb.count];
    GPIOC->BSRR = dcf_fb.pxdataC.setup[dcf_fb.count];
    dcf_fb.count++;

    return mp_const_none;
}

/* Drive all column banks in parallel. */
mp_obj_t dcfurs_columns(mp_obj_t obj)
{
    int x = mp_obj_get_int(obj);
    int y = (x & 0x7) | ((x & 0x3e000) >> 8);
    uint32_t bankB = (y & DCF_PIN_COL_BANKB) | ((~y & DCF_PIN_COL_BANKC) << 16);
    uint32_t bankC = (x & DCF_PIN_COL_BANKC) | ((~x & DCF_PIN_COL_BANKC) << 16);
    GPIOB->BSRR = bankB;
    GPIOC->BSRR = bankC;

    return mp_const_none;
}

mp_obj_t dcfurs_init(void)
{
    GPIO_InitTypeDef gpio;
    int i;

    /* Prepare row and column driver outputs (group B) */
    gpio.Speed = GPIO_SPEED_FAST;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Alternate = 0;
    gpio.Pin = DCF_PIN_ALL_BANKB;
    HAL_GPIO_Init(GPIOB, &gpio);
    GPIOB->BSRR = DCF_PIN_ROW_BANKB;

    /* Prepare column driver outputs (group C) */
    gpio.Speed = GPIO_SPEED_FAST;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Alternate = 0;
    gpio.Pin = DCF_PIN_ALL_BANKC;
    HAL_GPIO_Init(GPIOC, &gpio);
    GPIOC->BSRR = DCF_PIN_COL_BANKC;

    /* Row setup requires 3 clocks to shift in the first bit. */
    /* Kindof a bug due to the shift and latch clocks being tied together. */
    memset(&dcf_fb, 0, sizeof(dcf_fb));
    dcf_fb.pxdataC.setup[0] = (DCF_PIN_COL_BANKC << 16);
    dcf_fb.pxdataB.setup[0] = DCF_PIN_RED | (DCF_PIN_ALL_BANKB << 16);
    dcf_fb.pxdataB.setup[1] = DCF_PIN_CLK;               /* First Rising CLK - latch RED in */
    dcf_fb.pxdataB.setup[2] = (DCF_PIN_ALL_BANKB << 16); /* First Falling CLK */

    /* A rising edge shifts the pulse to the next row at each step. */
    /* TODO: This will get more complex to add color. */
    for (i = 0; i < DCF_TOTAL_ROWS; i++) {
        dcf_fb.pxdataC.red[(i * DCF_DIMMING_STEPS) + 0] = DCF_PIN_COL_BANKC << 16;
        dcf_fb.pxdataB.red[(i * DCF_DIMMING_STEPS) + 0] = DCF_PIN_CLK | (DCF_PIN_COL_BANKB << 16);
        dcf_fb.pxdataB.red[(i * DCF_DIMMING_STEPS) + 1] = (DCF_PIN_CLK << 16);
        dcf_fb.pxdataC.green[(i * DCF_DIMMING_STEPS) + 0] = DCF_PIN_COL_BANKC << 16;
        dcf_fb.pxdataB.green[(i * DCF_DIMMING_STEPS) + 0] = DCF_PIN_CLK | (DCF_PIN_COL_BANKB << 16);
        dcf_fb.pxdataB.green[(i * DCF_DIMMING_STEPS) + 1] |= (DCF_PIN_CLK << 16);
        dcf_fb.pxdataC.blue[(i * DCF_DIMMING_STEPS) + 0] = DCF_PIN_COL_BANKC << 16;
        dcf_fb.pxdataB.blue[(i * DCF_DIMMING_STEPS) + 0] |= DCF_PIN_CLK | (DCF_PIN_COL_BANKB << 16);
        dcf_fb.pxdataB.blue[(i * DCF_DIMMING_STEPS) + 1] |= (DCF_PIN_CLK << 16);
    }
    /* The last clock for the R and G channels should setup a pulse for the next color plane. */
    dcf_fb.pxdataB.red[(DCF_TOTAL_ROWS - 1) * DCF_DIMMING_STEPS - 1] |= DCF_PIN_GREEN;
    dcf_fb.pxdataB.red[(DCF_TOTAL_ROWS - 1) * DCF_DIMMING_STEPS + 1] |= (DCF_PIN_GREEN << 16);
    dcf_fb.pxdataB.green[(DCF_TOTAL_ROWS - 1) * DCF_DIMMING_STEPS - 1] |= DCF_PIN_BLUE;
    dcf_fb.pxdataB.green[(DCF_TOTAL_ROWS - 1) * DCF_DIMMING_STEPS + 1] |= (DCF_PIN_BLUE << 16);

    /* Enable the matrix driver output. */
    GPIOC->BSRR = DCF_PIN_ROW_ENABLE << 16;

    return mp_const_none;
}

static void dcfurs_setpix(int row, int col, int pix)
{
    struct dcf_pwm_program *prog = &dcf_fb.pxdataB;
    int bit = col;
    int r = (pix & 0xE0) >> 5;
    int g = (pix & 0x1C) >> 2;
    int b = (pix & 0x03) << 1;
    int i;

    /* Some bit fiddling... */
    if (col >= 13) {
        /* COL13-17 are mapped to middle bits of port B. */
        bit = col - 8;
    } else if (col >= 3) {
        /* COL3-12 are mapped onto port C instead. */
        prog = &dcf_fb.pxdataC;
    }

    for (i = 0; i < DCF_DIMMING_STEPS; i++) {
        DCF_BITBAND_SRAM(&prog->red[row * DCF_DIMMING_STEPS + i], bit) = (r > i);
        DCF_BITBAND_SRAM(&prog->red[row * DCF_DIMMING_STEPS + i], bit + 16) = (r <= i);
        DCF_BITBAND_SRAM(&prog->green[row * DCF_DIMMING_STEPS + i], bit) = (g > i);
        DCF_BITBAND_SRAM(&prog->green[row * DCF_DIMMING_STEPS + i], bit + 16) = (g <= i);
        DCF_BITBAND_SRAM(&prog->blue[row * DCF_DIMMING_STEPS + i], bit) = (b > i);
        DCF_BITBAND_SRAM(&prog->blue[row * DCF_DIMMING_STEPS + i], bit + 16) = (b <= i);
    }
}

mp_obj_t dcfurs_set_pixel(mp_obj_t xobj, mp_obj_t yobj, mp_obj_t vobj)
{
    int col = mp_obj_get_int(xobj);
    int row = mp_obj_get_int(yobj);

    if ((row < 0) || (row >= DCF_TOTAL_ROWS)) {
        return mp_const_none; /* TODO: Throw a range error or something? */
    }
    if ((col < 0) || (col >= DCF_TOTAL_COLS)) {
        return mp_const_none; /* TODO: Throw a range error or something? */
    }

    dcfurs_setpix(row, col, mp_obj_get_int(vobj));
    return mp_const_none;
}

mp_obj_t dcfurs_set_row(mp_obj_t rowobj, mp_obj_t data)
{
    int rownum = mp_obj_get_int(rowobj);
    mp_buffer_info_t bufinfo;
    if ((rownum < 0) || (rownum >= DCF_TOTAL_ROWS)) {
        return mp_const_none; /* TODO: Throw a range error or something? */
    }
    
    /* If an integer was provided, use it as a bitmap of pixels to switch on. */
    if (mp_obj_is_integer(data)) {
        int col = 0;
        int pixels = mp_obj_get_int(data);
        for (col = 0; col < DCF_TOTAL_COLS; col++) {
            dcfurs_setpix(rownum, col, (pixels & (1 << col)) ? 0xff : 0);
        }
    }
    /* If a bytearray was provided, use it as an array of 8-bit RGB pixels. */
    else if (mp_get_buffer(data, &bufinfo, MP_BUFFER_READ)) {
        int col;
        const uint8_t *px = bufinfo.buf;
        if (bufinfo.len > DCF_TOTAL_COLS) {
            bufinfo.len = DCF_TOTAL_COLS;
        }
        for (col = 0; col < bufinfo.len; col++) {
            dcfurs_setpix(rownum, col, px[col]);
        }
    }

    return mp_const_none;
}

mp_obj_t dcfurs_set_frame(mp_obj_t fbobj)
{
    mp_obj_t *rowdata;
    mp_obj_get_array_fixed_n(fbobj, DCF_TOTAL_ROWS, &rowdata);
    int y;
    for (y = 0; y < DCF_TOTAL_ROWS; y++) {
        dcfurs_set_row(MP_OBJ_NEW_SMALL_INT(y), rowdata[y]);
    }
    return mp_const_none;
}

mp_obj_t dcfurs_has_pixel(mp_obj_t xobj, mp_obj_t yobj)
{
    int x = mp_obj_get_int(xobj);
    int y = mp_obj_get_int(yobj);
    if ((x < 0) || (x >= DCF_TOTAL_COLS)) {
        return mp_const_false;
    }
    if ((y < 0) || (y >= DCF_TOTAL_ROWS)) {
        return mp_const_false;
    }

    /* Corners */
    if ((x == 0) || (x == (DCF_TOTAL_COLS-1))) {
        if ((y == 0) || (y==(DCF_TOTAL_ROWS-1))) {
            return mp_const_false;
        }
    }

    /* Bridge of the nose */
    if (y == (DCF_TOTAL_ROWS-2)) {
        if ((x > 6) && (x < 11)) {
            return mp_const_false;
        }
    }
    if (y == (DCF_TOTAL_ROWS-1)) {
        if ((x > 5) && (x < 12)) {
            return mp_const_false;
        }
    }

    /* Otherwise, the pixel exists. */
    return mp_const_true;
}

mp_obj_t dcfurs_clear(void)
{
    return mp_const_none;
}
