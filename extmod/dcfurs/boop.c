#include <stdio.h>
#include <string.h>

#include "py/runtime.h"
#include "py/mphal.h"
#include "ports/stm32/timer.h"
#include "ports/stm32/irq.h"

#include "dcfurs.h"

#define DCF_PIN_TSC_CAP     (1 << 14) /* TSC_G1_IO3 */
#define DCF_PIN_TSC_BOOP    (1 << 15) /* TSC_G1_IO4 */

typedef struct _dcfurs_obj_boop_t {
    mp_obj_base_t base;
    int newdata;
    int boopstate;
    TSC_HandleTypeDef handle;
    /* Short-term average data */
    int sta_index;
    uint32_t sta_sum;
    uint32_t sta_samples[4];
    /* Long-term average data */
    int lta_index;
    uint32_t lta_sum;
    uint16_t lta_samples[256];
} dcfurs_obj_boop_t;

static dcfurs_obj_boop_t dcfurs_boop_data;

void HAL_TSC_ConvCpltCallback(TSC_HandleTypeDef *htsc)
{
    dcfurs_obj_boop_t *self = &dcfurs_boop_data;
    if (self) {
        uint16_t count = 255 - HAL_TSC_GroupGetValue(&self->handle, 0);

        /* Update the short-term average. */
        self->sta_sum -= self->sta_samples[self->sta_index];
        self->sta_sum += count;
        self->sta_samples[self->sta_index++] = count;
        if (self->sta_index >= MP_ARRAY_SIZE(self->sta_samples)) self->sta_index = 0;

        /* Update the long-term average. */
        self->lta_sum -= self->lta_samples[self->lta_index];
        self->lta_sum += count;
        self->lta_samples[self->lta_index++] = count;
        if (self->lta_index >= MP_ARRAY_SIZE(self->lta_samples)) self->lta_index = 0;

        /* Flag that there is new data to be read. */
        self->newdata = 0;
    }
}

void TSC_IRQHandler(void) {
    IRQ_ENTER(TSC_IRQn);
    HAL_TSC_IRQHandler(&dcfurs_boop_data.handle);
    IRQ_EXIT(TSC_IRQn);
}

static mp_obj_t boop_start(mp_obj_t self_in)
{
    dcfurs_obj_boop_t *self = MP_OBJ_TO_PTR(self_in);
    HAL_TSC_Start_IT(&self->handle);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(boop_start_obj, boop_start);

static mp_obj_t boop_value(mp_obj_t self_in)
{
    dcfurs_obj_boop_t *self = MP_OBJ_TO_PTR(self_in);

    /* If recently updated, return the latest value. */
    if (!self->newdata) {
        if (HAL_TSC_GetState(&self->handle) != HAL_TSC_STATE_BUSY) {
            HAL_TSC_Start_IT(&self->handle);
        }
        HAL_TSC_PollForAcquisition(&self->handle);
    }

    int index = self->lta_index ? self->lta_index : MP_ARRAY_SIZE(self->lta_samples);
    self->newdata = 0;
    return mp_obj_new_int(self->lta_samples[index-1]);
}
static MP_DEFINE_CONST_FUN_OBJ_1(boop_value_obj, boop_value);

static mp_obj_t boop_average(mp_obj_t self_in)
{
    dcfurs_obj_boop_t *self = MP_OBJ_TO_PTR(self_in);
    return mp_obj_new_int(self->lta_sum / MP_ARRAY_SIZE(self->lta_samples));
}
static MP_DEFINE_CONST_FUN_OBJ_1(boop_average_obj, boop_average);

static mp_obj_t boop_event(mp_obj_t self_in)
{
    dcfurs_obj_boop_t *self = MP_OBJ_TO_PTR(self_in);

    /* Define 'touched' if short-term average is greater that twice the long-term average. */
    uint32_t threshold = (self->lta_sum * 2) / MP_ARRAY_SIZE(self->lta_samples);
    uint32_t measured = self->sta_sum / MP_ARRAY_SIZE(self->sta_samples);
    int delta = measured - threshold;

    /* If we have been booped, return false until not touched. */
    if (self->boopstate) {
        self->boopstate = delta > 0;
        return mp_const_false;
    }
    else {
        self->boopstate = delta > 0;
        return self->boopstate ? mp_const_true : mp_const_false;
    }
}
static MP_DEFINE_CONST_FUN_OBJ_1(boop_event_obj, boop_event);

static const mp_rom_map_elem_t boop_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_start), MP_ROM_PTR(&boop_start_obj) },
    { MP_ROM_QSTR(MP_QSTR_value), MP_ROM_PTR(&boop_value_obj) },
    { MP_ROM_QSTR(MP_QSTR_average), MP_ROM_PTR(&boop_average_obj) },
    { MP_ROM_QSTR(MP_QSTR_event), MP_ROM_PTR(&boop_event_obj) },
};
static MP_DEFINE_CONST_DICT(boop_locals_dict, boop_locals_dict_table);

static mp_obj_t boop_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args)
{
    dcfurs_obj_boop_t *self = &dcfurs_boop_data;
    uint32_t hclk_freq = HAL_RCC_GetHCLKFreq();

    memset(&self->base, 0, sizeof(mp_obj_base_t));
    self->base.type = &dcfurs_boop_type;
    self->newdata = 0;
    self->boopstate = 0;

    /* Configure the TSC GPIO pins */
    GPIO_InitTypeDef gpio;
    gpio.Speed = GPIO_SPEED_FAST;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Alternate = GPIO_AF9_TSC;
    gpio.Pin = DCF_PIN_TSC_CAP | DCF_PIN_TSC_BOOP;
    HAL_GPIO_Init(GPIOB, &gpio);

    /* Configure the TSC controller */
    __HAL_RCC_TSC_CLK_ENABLE();

    self->handle.Instance                      = TSC;
    self->handle.Init.CTPulseHighLength        = TSC_CTPH_16CYCLES;
    self->handle.Init.CTPulseLowLength         = TSC_CTPL_16CYCLES;
    self->handle.Init.SpreadSpectrum           = ENABLE;
    self->handle.Init.SpreadSpectrumDeviation  = hclk_freq / 25000000; /* Aim for 400ns deviation. */
    self->handle.Init.SpreadSpectrumPrescaler  = TSC_SS_PRESC_DIV1;
    self->handle.Init.PulseGeneratorPrescaler  = TSC_PG_PRESC_DIV1;
    self->handle.Init.MaxCountValue            = TSC_MCV_255;
    self->handle.Init.IODefaultMode            = TSC_IODEF_OUT_PP_LOW;
    self->handle.Init.SynchroPinPolarity       = TSC_SYNC_POLARITY_FALLING;
    self->handle.Init.AcquisitionMode          = TSC_ACQ_MODE_NORMAL;
    self->handle.Init.MaxCountInterrupt        = DISABLE;
    self->handle.Init.ChannelIOs = (1 << 3);
    self->handle.Init.ShieldIOs  = 0;
    self->handle.Init.SamplingIOs = (1 << 2);

    /* Find the prescaler that results in a clock less than 16MHz  */
    while (self->handle.Init.PulseGeneratorPrescaler < TSC_PG_PRESC_DIV128) {
        if (hclk_freq < 24000000) break;
        self->handle.Init.PulseGeneratorPrescaler += (1 << 12);
        hclk_freq >>= 1;
    }

    HAL_TSC_Init(&self->handle);

    NVIC_SetPriority(TSC_IRQn, IRQ_PRI_TIMX);
    HAL_NVIC_EnableIRQ(TSC_IRQn);

    return MP_OBJ_FROM_PTR(self);
}

const mp_obj_type_t dcfurs_boop_type = {
    { &mp_type_type },
    .name = MP_QSTR_boop,
    //.print = boop_print,
    .make_new = boop_make_new,
    .locals_dict = (mp_obj_dict_t*)&boop_locals_dict,
};
