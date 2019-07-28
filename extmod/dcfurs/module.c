/* Micropython module definitions */
#include <stdio.h>
#include <string.h>

#include "py/runtime.h"
#include "dcfurs.h"

/* LED Matrix operations */
STATIC MP_DEFINE_CONST_FUN_OBJ_1(dcfurs_init_obj, dcfurs_init);
STATIC MP_DEFINE_CONST_FUN_OBJ_3(dcfurs_set_pixel_obj, dcfurs_set_pixel);
STATIC MP_DEFINE_CONST_FUN_OBJ_3(dcfurs_set_pix_rgb_obj, dcfurs_set_pix_rgb);
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(dcfurs_set_pix_hue_obj, 3, 4, dcfurs_set_pix_hue);
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(dcfurs_set_row_obj, 2, 3, dcfurs_set_row);
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(dcfurs_set_frame_obj, 1, 2, dcfurs_set_frame);
STATIC MP_DEFINE_CONST_FUN_OBJ_2(dcfurs_has_pixel_obj, dcfurs_has_pixel);
STATIC MP_DEFINE_CONST_FUN_OBJ_0(dcfurs_clear_obj, dcfurs_clear);

/* Interactive text game functions */
STATIC MP_DEFINE_CONST_FUN_OBJ_0(dcfurs_credits_obj, dcfurs_credits);
STATIC MP_DEFINE_CONST_FUN_OBJ_0(dcfurs_eula_obj, dcfurs_eula);

STATIC const mp_rom_map_elem_t mp_module_dcfurs_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_dcfurs) },

    /* LED Matrix API */
    { MP_ROM_QSTR(MP_QSTR_init), MP_ROM_PTR(&dcfurs_init_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_pixel), MP_ROM_PTR(&dcfurs_set_pixel_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_pix_rgb), MP_ROM_PTR(&dcfurs_set_pix_rgb_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_pix_hue), MP_ROM_PTR(&dcfurs_set_pix_hue_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_row), MP_ROM_PTR(&dcfurs_set_row_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_frame), MP_ROM_PTR(&dcfurs_set_frame_obj) },
    { MP_ROM_QSTR(MP_QSTR_clear), MP_ROM_PTR(&dcfurs_clear_obj) },
    { MP_ROM_QSTR(MP_QSTR_has_pixel), MP_ROM_PTR(&dcfurs_has_pixel_obj) },
    { MP_ROM_QSTR(MP_QSTR_ncols), MP_ROM_INT(DCF_TOTAL_COLS) },
    { MP_ROM_QSTR(MP_QSTR_nrows), MP_ROM_INT(DCF_TOTAL_ROWS) },

    /* Hidden Text Functions */
    { MP_ROM_QSTR(MP_QSTR_credits), MP_ROM_PTR(&dcfurs_credits_obj) },
    { MP_ROM_QSTR(MP_QSTR_eula), MP_ROM_PTR(&dcfurs_eula_obj) },

    /* Touch Sense/Booping */
    { MP_ROM_QSTR(MP_QSTR_boop), MP_ROM_PTR(&dcfurs_boop_type) },
};

STATIC MP_DEFINE_CONST_DICT(mp_module_dcfurs_globals, mp_module_dcfurs_globals_table);

const mp_obj_module_t mp_module_dcfurs = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&mp_module_dcfurs_globals,
};
