/* Micropython module definitions */
#include <stdio.h>
#include <string.h>

#include "py/runtime.h"
#include "dcfurs.h"

/* LED Matrix operations */
STATIC MP_DEFINE_CONST_FUN_OBJ_0(dcfurs_init_obj, dcfurs_init);
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR(dcfurs_loop_obj, 0, dcfurs_loop);
STATIC MP_DEFINE_CONST_FUN_OBJ_1(dcfurs_columns_obj, dcfurs_columns);
STATIC MP_DEFINE_CONST_FUN_OBJ_3(dcfurs_set_pixel_obj, dcfurs_set_pixel);
STATIC MP_DEFINE_CONST_FUN_OBJ_2(dcfurs_set_row_obj, dcfurs_set_row);
STATIC MP_DEFINE_CONST_FUN_OBJ_1(dcfurs_set_frame_obj, dcfurs_set_frame);
STATIC MP_DEFINE_CONST_FUN_OBJ_2(dcfurs_has_pixel_obj, dcfurs_has_pixel);
STATIC MP_DEFINE_CONST_FUN_OBJ_0(dcfurs_clear_obj, dcfurs_clear);

STATIC const mp_rom_map_elem_t mp_module_dcfurs_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_dcfurs) },

    /* LED Matrix API */
    { MP_ROM_QSTR(MP_QSTR_init), MP_ROM_PTR(&dcfurs_init_obj) },
    { MP_ROM_QSTR(MP_QSTR_loop), MP_ROM_PTR(&dcfurs_loop_obj) },
    { MP_ROM_QSTR(MP_QSTR_columns), MP_ROM_PTR(&dcfurs_columns_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_pixel), MP_ROM_PTR(&dcfurs_set_pixel_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_row), MP_ROM_PTR(&dcfurs_set_row_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_frame), MP_ROM_PTR(&dcfurs_set_frame_obj) },
    { MP_ROM_QSTR(MP_QSTR_clear), MP_ROM_PTR(&dcfurs_clear_obj) },
    { MP_ROM_QSTR(MP_QSTR_has_pixel), MP_ROM_PTR(&dcfurs_has_pixel_obj) },
    { MP_ROM_QSTR(MP_QSTR_ncols), MP_ROM_INT(DCF_TOTAL_COLS) },
    { MP_ROM_QSTR(MP_QSTR_nrows), MP_ROM_INT(DCF_TOTAL_ROWS) },
};

STATIC MP_DEFINE_CONST_DICT(mp_module_dcfurs_globals, mp_module_dcfurs_globals_table);

const mp_obj_module_t mp_module_dcfurs = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&mp_module_dcfurs_globals,
};
