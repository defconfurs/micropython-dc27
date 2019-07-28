
#ifndef DCFURS_H_INCLUDED
#define DCFURS_H_INCLUDED

#define DCF_TOTAL_COLS      18
#define DCF_TOTAL_ROWS      7

#define DCF_SETUP_STEPS     3
#define DCF_PWM_RED_STEPS   42
#define DCF_PWM_GREEN_STEPS 51
#define DCF_PWM_BLUE_STEPS  63
#define DCF_PWM_TOTAL_STEPS (DCF_PWM_RED_STEPS + DCF_PWM_GREEN_STEPS + DCF_PWM_BLUE_STEPS)
#define DCF_TOTAL_STEPS     (DCF_SETUP_STEPS + DCF_PWM_TOTAL_STEPS * DCF_TOTAL_ROWS)

#define DCF_PWM_RED_DIVISOR     (255 / DCF_PWM_RED_STEPS)
#define DCF_PWM_GREEN_DIVISOR   (255 / DCF_PWM_GREEN_STEPS)
#define DCF_PWM_BLUE_DIVISOR    (255 / DCF_PWM_BLUE_STEPS)

mp_obj_t dcfurs_init(mp_obj_t timer);
mp_obj_t dcfurs_set_pixel(mp_obj_t xobj, mp_obj_t yobj, mp_obj_t vobj);
mp_obj_t dcfurs_set_pix_rgb(mp_obj_t xobj, mp_obj_t yobj, mp_obj_t rgb);
mp_obj_t dcfurs_set_pix_hue(size_t n_args, const mp_obj_t *args);
mp_obj_t dcfurs_set_row(size_t n_args, const mp_obj_t *args);
mp_obj_t dcfurs_set_frame(size_t n_args, const mp_obj_t *args);
mp_obj_t dcfurs_has_pixel(mp_obj_t xobj, mp_obj_t yobj);
mp_obj_t dcfurs_clear(void);

mp_obj_t dcfurs_credits(void);
mp_obj_t dcfurs_eula(void);

extern const mp_obj_type_t dcfurs_boop_type;

#endif /* DCFURS_H_INCLUDED */
