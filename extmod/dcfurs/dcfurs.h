
#ifndef DCFURS_H_INCLUDED
#define DCFURS_H_INCLUDED

#define DCF_TOTAL_COLS      18
#define DCF_TOTAL_ROWS      7

#define DCF_SETUP_STEPS     3
#define DCF_PWM_RED_STEPS   7
#define DCF_PWM_GREEN_STEPS 9
#define DCF_PWM_BLUE_STEPS  12
#define DCF_PWM_TOTAL_STEPS (DCF_PWM_RED_STEPS + DCF_PWM_GREEN_STEPS + DCF_PWM_BLUE_STEPS)
#define DCF_TOTAL_STEPS     (DCF_SETUP_STEPS + DCF_PWM_TOTAL_STEPS * DCF_TOTAL_ROWS)

mp_obj_t dcfurs_init(mp_obj_t timer);
mp_obj_t dcfurs_set_pixel(mp_obj_t xobj, mp_obj_t yobj, mp_obj_t vobj);
mp_obj_t dcfurs_set_row(mp_obj_t yobj, mp_obj_t rowdata);
mp_obj_t dcfurs_set_frame(mp_obj_t fbuf);
mp_obj_t dcfurs_has_pixel(mp_obj_t xobj, mp_obj_t yobj);
mp_obj_t dcfurs_clear(void);

#endif /* DCFURS_H_INCLUDED */
