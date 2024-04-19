/*
 * Copyright (C) 2018-2024 "IoT.bzh"
 * Author "Louis-Baptiste Sobolewski" <lb.sobolewski@iot.bzh>
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at https://opensource.org/licenses/MIT.
 */

#include "modbus-binding.h"
#include <afb-helpers4/ctl-lib-plugin.h>

CTL_PLUGIN_DECLARE("r4dcb08_temperature", "Modbus plugin for R4DCB08 temperature reader");

static int decodeTemps(ModbusSourceT *source, ModbusFormatCbT *format, uint16_t *data, unsigned int index, json_object **responseJ) {
    json_object *res;
    *responseJ = json_object_new_array();
    
    for (int idx = 0; idx < format->nbreg; idx++) {
        if (*data > 32768) {
            res = json_object_new_double(*data - 65536.0 / 10.0);
        } else if (*data < 32768) {
            res = json_object_new_double(*data / 10.0);
        } else {
            res = NULL;
        }

        json_object_array_add(*responseJ, res);
    }
    
    return 0;
}

ModbusFormatCbT modbusFormats[] = {
    { .uid = "temps", .info = "json_array", .nbreg = 8, .decodeCB = decodeTemps, .encodeCB = NULL },
    { .uid = NULL }
};
