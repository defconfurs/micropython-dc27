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
    uint32_t red[DCF_TOTAL_ROWS * DCF_PWM_RED_STEPS];
    uint32_t green[DCF_TOTAL_ROWS * DCF_PWM_GREEN_STEPS];
    uint32_t blue[DCF_TOTAL_ROWS * DCF_PWM_BLUE_STEPS];
};

struct dcf_framebuf {
    /* Pixel data buffers. */
    struct dcf_pwm_program pxdataB;
    struct dcf_pwm_program pxdataC;

    /* Pixel DMA channels. */
    DMA_HandleTypeDef dma_gpioB;
    DMA_HandleTypeDef dma_gpioC;
};

static struct dcf_framebuf dcf_fb;

static const DMA_InitTypeDef dma_init_blit_gpio = {
    .Request             = DMA_REQUEST_7,
    .Direction           = DMA_MEMORY_TO_PERIPH,
    .PeriphInc           = DMA_PINC_DISABLE,
    .MemInc              = DMA_MINC_ENABLE,
    .PeriphDataAlignment = DMA_PDATAALIGN_WORD,
    .MemDataAlignment    = DMA_MDATAALIGN_WORD,
    .Mode                = DMA_CIRCULAR,
    .Priority            = DMA_PRIORITY_HIGH,
};

mp_obj_t dcfurs_init(mp_obj_t timer)
{
    TIM_HandleTypeDef *htim = pyb_timer_get_handle(timer);
    GPIO_InitTypeDef gpio;
    int i;

    mp_obj_t timer_channel_args[5] = {
        0,  /* Function object */
        0,  /* Timer object */
        0,  /* Timer channel number */
        MP_ROM_QSTR(MP_QSTR_mode), mp_load_attr(timer, MP_QSTR_OC_TIMING)
    };

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
    for (i = 0; i < DCF_TOTAL_ROWS; i++) {
        dcf_fb.pxdataC.red[(i * DCF_PWM_RED_STEPS) + 0] = DCF_PIN_COL_BANKC << 16;
        dcf_fb.pxdataB.red[(i * DCF_PWM_RED_STEPS) + 0] = DCF_PIN_CLK | (DCF_PIN_COL_BANKB << 16);
        dcf_fb.pxdataB.red[(i * DCF_PWM_RED_STEPS) + 1] = (DCF_PIN_CLK << 16);
        dcf_fb.pxdataC.green[(i * DCF_PWM_GREEN_STEPS) + 0] = DCF_PIN_COL_BANKC << 16;
        dcf_fb.pxdataB.green[(i * DCF_PWM_GREEN_STEPS) + 0] = DCF_PIN_CLK | (DCF_PIN_COL_BANKB << 16);
        dcf_fb.pxdataB.green[(i * DCF_PWM_GREEN_STEPS) + 1] |= (DCF_PIN_CLK << 16);
        dcf_fb.pxdataC.blue[(i * DCF_PWM_BLUE_STEPS) + 0] = DCF_PIN_COL_BANKC << 16;
        dcf_fb.pxdataB.blue[(i * DCF_PWM_BLUE_STEPS) + 0] |= DCF_PIN_CLK | (DCF_PIN_COL_BANKB << 16);
        dcf_fb.pxdataB.blue[(i * DCF_PWM_BLUE_STEPS) + 1] |= (DCF_PIN_CLK << 16);
    }
    /* The last clock for the R and G channels should setup a pulse for the next color plane. */
    dcf_fb.pxdataB.red[(DCF_TOTAL_ROWS - 1) * DCF_PWM_RED_STEPS - 1] |= DCF_PIN_GREEN;
    dcf_fb.pxdataB.red[(DCF_TOTAL_ROWS - 1) * DCF_PWM_RED_STEPS + 1] |= (DCF_PIN_GREEN << 16);
    dcf_fb.pxdataB.green[(DCF_TOTAL_ROWS - 1) * DCF_PWM_GREEN_STEPS - 1] |= DCF_PIN_BLUE;
    dcf_fb.pxdataB.green[(DCF_TOTAL_ROWS - 1) * DCF_PWM_GREEN_STEPS + 1] |= (DCF_PIN_BLUE << 16);

    /* Initialize the DMA channels  */
    /* TODO: Sanity-check that the caller gave us the right timer (TIM8)? */
    __HAL_RCC_DMA2_CLK_ENABLE();

    dcf_fb.dma_gpioB.Instance = DMA2_Channel6;
    memcpy(&dcf_fb.dma_gpioB.Init, &dma_init_blit_gpio, sizeof(DMA_InitTypeDef));
    HAL_DMA_DeInit(&dcf_fb.dma_gpioB);
    HAL_DMA_Init(&dcf_fb.dma_gpioB);

    dcf_fb.dma_gpioC.Instance = DMA2_Channel7;
    memcpy(&dcf_fb.dma_gpioC.Init, &dma_init_blit_gpio, sizeof(DMA_InitTypeDef));
    HAL_DMA_DeInit(&dcf_fb.dma_gpioC);
    HAL_DMA_Init(&dcf_fb.dma_gpioC);

    /* Configure the timer channels. */
    timer_channel_args[2] = MP_OBJ_NEW_SMALL_INT(1);
    mp_load_method(timer, MP_QSTR_channel, timer_channel_args);
    mp_call_method_n_kw(1, 1, timer_channel_args);

    timer_channel_args[2] = MP_OBJ_NEW_SMALL_INT(2);
    mp_load_method(timer, MP_QSTR_channel, timer_channel_args);
    mp_call_method_n_kw(1, 1, timer_channel_args);

    /* Start the DMA */
    htim->Instance->DIER &= ~(TIM_DMA_CC1 | TIM_DMA_CC2);
    HAL_DMA_Start(&dcf_fb.dma_gpioB, (uint32_t)&dcf_fb.pxdataB, (uint32_t)&GPIOB->BSRR, DCF_TOTAL_STEPS);
    HAL_DMA_Start(&dcf_fb.dma_gpioC, (uint32_t)&dcf_fb.pxdataC, (uint32_t)&GPIOC->BSRR, DCF_TOTAL_STEPS);
    htim->Instance->DIER |= (TIM_DMA_CC1 | TIM_DMA_CC2);

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
    int b = (pix & 0x03) * 3;
    int i;

    /* Some bit fiddling... */
    if (col >= 13) {
        /* COL13-17 are mapped to middle bits of port B. */
        bit = col - 8;
    } else if (col >= 3) {
        /* COL3-12 are mapped onto port C instead. */
        prog = &dcf_fb.pxdataC;
    }

    /* Write the red channel */
    for (i = 0; i < r; i++) {
        prog->red[row * DCF_PWM_RED_STEPS + i] |= (1 << bit);
        prog->red[row * DCF_PWM_RED_STEPS + i] &= ~(0x10000 << bit);
    }
    while (i < DCF_PWM_RED_STEPS) {
        prog->red[row * DCF_PWM_RED_STEPS + i] &= ~(1 << bit);
        prog->red[row * DCF_PWM_RED_STEPS + i] |= (0x10000 << bit);
        i++;
    }

    /* Write the green channel */
    for (i = 0; i < g; i++) {
        prog->green[row * DCF_PWM_GREEN_STEPS + i] |= (1 << bit);
        prog->green[row * DCF_PWM_GREEN_STEPS + i] &= ~(0x10000 << bit);
    }
    while (i < DCF_PWM_GREEN_STEPS) {
        prog->green[row * DCF_PWM_GREEN_STEPS + i] &= ~(1 << bit);
        prog->green[row * DCF_PWM_GREEN_STEPS + i] |= (0x10000 << bit);
        i++;
    }

    /* Write the blue channel */
    for (i = 0; i < b; i++) {
        prog->blue[row * DCF_PWM_BLUE_STEPS + i] |= (1 << bit);
        prog->blue[row * DCF_PWM_BLUE_STEPS + i] &= ~(0x10000 << bit);
    }
    while (i < DCF_PWM_BLUE_STEPS) {
        prog->blue[row * DCF_PWM_BLUE_STEPS + i] &= ~(1 << bit);
        prog->blue[row * DCF_PWM_BLUE_STEPS + i] |= (0x10000 << bit);
        i++;
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
    int i;

    for (i = 0; i < (DCF_PWM_RED_STEPS * DCF_TOTAL_ROWS); i++) {
        dcf_fb.pxdataB.red[i] |= DCF_PIN_COL_BANKB << 16;
        dcf_fb.pxdataB.red[i] &= ~DCF_PIN_COL_BANKB;
        dcf_fb.pxdataC.red[i] |= DCF_PIN_COL_BANKC << 16;
        dcf_fb.pxdataC.red[i] &= ~DCF_PIN_COL_BANKC;
    }
    for (i = 0; i < (DCF_PWM_GREEN_STEPS * DCF_TOTAL_ROWS); i++) {
        dcf_fb.pxdataB.green[i] |= DCF_PIN_COL_BANKB << 16;
        dcf_fb.pxdataB.green[i] &= ~DCF_PIN_COL_BANKB;
        dcf_fb.pxdataC.green[i] |= DCF_PIN_COL_BANKC << 16;
        dcf_fb.pxdataC.green[i] &= ~DCF_PIN_COL_BANKC;
    }
    for (i = 0; i < (DCF_PWM_BLUE_STEPS * DCF_TOTAL_ROWS); i++) {
        dcf_fb.pxdataB.blue[i] |= DCF_PIN_COL_BANKB << 16;
        dcf_fb.pxdataB.blue[i] &= ~DCF_PIN_COL_BANKB;
        dcf_fb.pxdataC.blue[i] |= DCF_PIN_COL_BANKC << 16;
        dcf_fb.pxdataC.blue[i] &= ~DCF_PIN_COL_BANKC;
    }

    return mp_const_none;
}
