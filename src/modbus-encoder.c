/*
 * Copyright (C) 2015-2020 IoT.bzh Company
 * Author "Fulup Ar Foll"
 *
 * $RP_BEGIN_LICENSE$
 * Commercial License Usage
 *  Licensees holding valid commercial IoT.bzh licenses may use this file in
 *  accordance with the commercial license agreement provided with the
 *  Software or, alternatively, in accordance with the terms contained in
 *  a written agreement between you and The IoT.bzh Company. For licensing terms
 *  and conditions see https://www.iot.bzh/terms-conditions. For further
 *  information use the contact form at https://www.iot.bzh/contact.
 *
 * GNU General Public License Usage
 *  Alternatively, this file may be used under the terms of the GNU General
 *  Public license version 3. This license is as published by the Free Software
 *  Foundation and appearing in the file LICENSE.GPLv3 included in the packaging
 *  of this file. Please review the following information to ensure the GNU
 *  General Public License requirements will be met
 *  https://www.gnu.org/licenses/gpl-3.0.html.
 * $RP_END_LICENSE$
*/

#define _GNU_SOURCE

#include <modbus.h>
#include "modbus-binding.h"

typedef struct modbusRegistryS {
   char *uid;
   struct modbusRegistryS *next;
   ModbusFormatCbT *formats;
} modbusRegistryT;


// registry holds a linked list of core+pugins encoders
static modbusRegistryT *registryHead = NULL;

// add a new plugin encoder to the registry
void mbEncoderRegister (char *uid, ModbusFormatCbT *encoderCB) {
    modbusRegistryT *registryIdx, *registryEntry;

    // create holding hat for encoder/decoder CB
    registryEntry= (modbusRegistryT*) calloc (1, sizeof(modbusRegistryT));
    registryEntry->uid = uid;
    registryEntry->formats = encoderCB;


    // if not 1st encoder insert at the end of the chain
    if (!registryHead) {
        registryHead = registryEntry;
    } else {
        for (registryIdx= registryHead; registryIdx->next; registryIdx=registryIdx->next);
        registryIdx->next = registryEntry;
    }
}

// find on format encoder/decoder within one plugin
ModbusFormatCbT *mvOneFormatFind (ModbusFormatCbT *format, const char *uid) {
    int idx;
    assert (uid);
    assert(format);

    for (idx=0; format[idx].uid; idx++) {
        if (!strcasecmp (format[idx].uid, uid)) break;
    }

    return (&format[idx]);
}

static void PluginParseURI (const char* uri, char **plugin, char **format) {
    char *chaine;
    int idx;
    static int prefixlen = sizeof(PLUGIN_ACTION_PREFIX)-1;

    if (strncasecmp(uri, PLUGIN_ACTION_PREFIX, prefixlen)) goto NoPluginExit;

    // break URI in substring ignoring leading tcp:
    chaine= strdup(uri);
    for (idx=prefixlen; idx < strlen(chaine); idx ++) {
        if (chaine[idx] == 0) break;
        if (chaine[idx] == '#') chaine[idx] = 0;
    }

    // extract IP addr and port
    *plugin = &chaine[prefixlen];
    *format= &chaine[idx];
    return;

NoPluginExit:
    return;
}


// search for a plugin encoders/decoders CB list
ModbusFormatCbT *mbEncoderFind (afb_api_t api, const char *uri) {
    char *pluginuid = NULL, *formatuid = NULL;
    modbusRegistryT *registryIdx;
    ModbusFormatCbT *format;

    PluginParseURI (uri, &pluginuid, &formatuid);

    // if no plugin search for core use directly format uri
    if (!pluginuid) {
        for (registryIdx= registryHead; registryIdx->uid && registryIdx->next; registryIdx=registryIdx->next);
        if (registryIdx->uid) {
            AFB_API_ERROR(api, "mbEncoderFind: (Internal error) fail find core encoders");
            goto OnErrorExit;
        }

        format= mvOneFormatFind (registryIdx->formats, uri);
        if (!format || !format->uid) {
            AFB_API_ERROR(api, "mbEncoderFind: Fail find format='%s' within default core encoders", uri);
            goto OnErrorExit;
        }

    }  else {
        for (registryIdx= registryHead; registryIdx->next; registryIdx=registryIdx->next) {
            if (registryIdx->uid && !strcasecmp (registryIdx->uid, uri)) break;
        }
        if (!registryIdx) {
            AFB_API_ERROR(api, "mbEncoderFind: Fail to find plugin='%s' format encoder ", formatuid);
            goto OnErrorExit;
        }

        format= mvOneFormatFind (registryIdx->formats, formatuid);
        if (!format || !format->uid) {
            AFB_API_ERROR(api, "mbEncoderFind: Fail to find plugin='%s' format='%s' encoder", pluginuid, formatuid);
            goto OnErrorExit;
        }
    }

    return (format);

OnErrorExit:
    return NULL;
}

// float64 subtype
enum {
    MB_FLOAT_ABCD,
    MB_FLOAT_BADC,
    MB_FLOAT_DCBA,
    MB_FLOAT_CDAB,
} MbFloatSubType;

static int mbDecodeFloat64 (ModbusSourceT *source, ModbusFormatCbT *format, uint16_t *data, uint index, json_object **responseJ) {
    float value;

    switch (format->subtype) {
        case MB_FLOAT_ABCD:
            value= modbus_get_float_abcd (&data [index*format->nbreg]);
            break;
        case MB_FLOAT_BADC:
            value= modbus_get_float_badc (&data [index*format->nbreg]);
            break;
        case MB_FLOAT_DCBA:
            value= modbus_get_float_dcba (&data [index*format->nbreg]);
            break;
        case MB_FLOAT_CDAB:
            value= modbus_get_float_cdab (&data [index*format->nbreg]);
            break;
        default:
            goto OnErrorExit;
    }
    *responseJ = json_object_new_double (value);
    return 0;

OnErrorExit:
    AFB_API_ERROR(source->api, "mbDecodeFloat64: invalid subtype format='%s' subtype=%d", format->uid, format->subtype);
    return 1;
}

static int mbEncodeFloat64(ModbusSourceT *source, ModbusFormatCbT *format, json_object *sourceJ, uint16_t **response, uint index) {
    float value;

    if (!json_object_is_type (sourceJ, json_type_double)) goto OnErrorExit;
    value= (float)json_object_get_double (sourceJ);

    switch (format->subtype) {
        case MB_FLOAT_ABCD:
            modbus_set_float_abcd (value, *&response[index*format->nbreg]);
            break;
        case MB_FLOAT_BADC:
            modbus_set_float_badc (value, *&response[index*format->nbreg]);
            break;
        case MB_FLOAT_DCBA:
            modbus_set_float_dcba (value, *&response[index*format->nbreg]);
            break;
        case MB_FLOAT_CDAB:
            modbus_set_float_cdab (value, *&response[index*format->nbreg]);
            break;
        default:
            goto OnErrorExit;
    }
    modbus_set_float_abcd (value, *&response[index*format->nbreg]);
    return 0;

OnErrorExit:
    AFB_API_ERROR(source->api, "mbDecodeFloatABCD: format='%s' value='%s' not a double/float ", format->uid, json_object_get_string (sourceJ));
    return 1;
}

static int mbDecodeInt64 (ModbusSourceT *source, ModbusFormatCbT *format, uint16_t *data, uint index, json_object **responseJ) {
    uint64_t value= MODBUS_GET_INT64_FROM_INT16(data, index*format->nbreg);
    *responseJ = json_object_new_int64 (value);
    return 0;
}

static int mbEncodeInt64(ModbusSourceT *source, ModbusFormatCbT *format, json_object *sourceJ, uint16_t **response, uint index) {

   if (!json_object_is_type (sourceJ, json_type_int)) goto OnErrorExit;
   uint64_t val= (int64_t)json_object_get_int (sourceJ);
   MODBUS_SET_INT64_TO_INT16 (*response, index*format->nbreg, val);
   return 0;

OnErrorExit:
    AFB_API_ERROR(source->api, "mbDecodeInt64: [%s] not an interger", json_object_get_string (sourceJ));
    return 1;
}

static int mbDecodeUInt32 (ModbusSourceT *source, ModbusFormatCbT *format, uint16_t *data, uint index, json_object **responseJ) {
    uint32_t value= (uint32_t) MODBUS_GET_INT32_FROM_INT16(data, index*format->nbreg);
    *responseJ = json_object_new_int64 ((int64_t)value);
    return 0;
}

static int mbEncodeUInt32(ModbusSourceT *source, ModbusFormatCbT *format, json_object *sourceJ, uint16_t **response, uint index) {

   if (!json_object_is_type (sourceJ, json_type_int)) goto OnErrorExit;
   uint32_t value= (uint32_t)json_object_get_int (sourceJ);
   MODBUS_SET_INT32_TO_INT16 (*response, index*format->nbreg, (int32_t)value);
   return 0;

OnErrorExit:
    AFB_API_ERROR(source->api, "mbDecodeInt16: [%s] not an interger", json_object_get_string (sourceJ));
    return 1;
}


static int mbDecodeInt32 (ModbusSourceT *source, ModbusFormatCbT *format, uint16_t *data, uint index, json_object **responseJ) {
    int32_t value= MODBUS_GET_INT32_FROM_INT16(data, index*format->nbreg);
    *responseJ = json_object_new_int (value);
    return 0;
}

static int mbEncodeInt32(ModbusSourceT *source, ModbusFormatCbT *format, json_object *sourceJ, uint16_t **response, uint index) {

   if (!json_object_is_type (sourceJ, json_type_int)) goto OnErrorExit;
   int32_t value= (int32_t)json_object_get_int (sourceJ);
   MODBUS_SET_INT32_TO_INT16 (*response, index*format->nbreg, value);
   return 0;

OnErrorExit:
    AFB_API_ERROR(source->api, "mbDecodeInt16: [%s] not an interger", json_object_get_string (sourceJ));
    return 1;
}


int mbDecodeInt16 (ModbusSourceT *source, ModbusFormatCbT *format, uint16_t *data, uint index, json_object **responseJ) {

    *responseJ = json_object_new_int (data[index*format->nbreg]);
    return 0;
}

static int mbEncodeInt16(ModbusSourceT *source, ModbusFormatCbT *format, json_object *sourceJ, uint16_t **response, uint index) {

   if (!json_object_is_type (sourceJ, json_type_int))  goto OnErrorExit;
   int16_t value = (int16_t)json_object_get_int (sourceJ);

   *response[index*format->nbreg] = value;
   return 0;

OnErrorExit:
    AFB_API_ERROR(source->api, "mbDecodeInt16: [%s] not an interger", json_object_get_string (sourceJ));
    return 1;
}

int mbDecodeBoolean (ModbusSourceT *source, ModbusFormatCbT *format, uint16_t *data, uint index, json_object **responseJ) {

    *responseJ = json_object_new_boolean (data[index*format->nbreg]);
    return 0;
}

static int mbEncodeBoolean(ModbusSourceT *source, ModbusFormatCbT *format, json_object *sourceJ, uint16_t **response, uint index) {

   if (!json_object_is_type (sourceJ, json_type_boolean))  goto OnErrorExit;
   int16_t value = (int16_t)json_object_get_boolean (sourceJ);

   *response[index*format->nbreg] = value;
   return 0;

OnErrorExit:
    AFB_API_ERROR(source->api, "mbEncodeBoolean: [%s] not a boolean", json_object_get_string (sourceJ));
    return 1;
}

static ModbusFormatCbT coreEncodersCB[] = {
  {.uid="BOOL"      , .info="json_boolean", .nbreg=1, .decodeCB=mbDecodeBoolean, .encodeCB=mbEncodeBoolean},
  {.uid="INT16"     , .info="json_integer", .nbreg=1, .decodeCB=mbDecodeInt16  , .encodeCB=mbEncodeInt16},
  {.uid="INT32"     , .info="json_integer", .nbreg=2, .decodeCB=mbDecodeInt32  , .encodeCB=mbEncodeInt32},
  {.uid="UINT32"    , .info="json_integer", .nbreg=2, .decodeCB=mbDecodeUInt32 , .encodeCB=mbEncodeUInt32},
  {.uid="INT64"     , .info="json_integer", .nbreg=2, .decodeCB=mbDecodeInt64  , .encodeCB=mbEncodeInt64},
  {.uid="FLOAT_ABCD", .info="json_float",   .nbreg=4, .decodeCB=mbDecodeFloat64, .encodeCB=mbEncodeFloat64, .subtype=MB_FLOAT_ABCD},
  {.uid="FLOAT_BADC", .info="json_float",   .nbreg=4, .decodeCB=mbDecodeFloat64, .encodeCB=mbEncodeFloat64, .subtype=MB_FLOAT_BADC},
  {.uid="FLOAT_DCBA", .info="json_float",   .nbreg=4, .decodeCB=mbDecodeFloat64, .encodeCB=mbEncodeFloat64, .subtype=MB_FLOAT_DCBA},
  {.uid="FLOAT_CDAB", .info="json_float",   .nbreg=4, .decodeCB=mbDecodeFloat64, .encodeCB=mbEncodeFloat64, .subtype=MB_FLOAT_CDAB},

  {.uid= NULL} // must be null terminated
};

// register callback and use it to register core encoders
void mbRegisterCoreEncoders (void) {

  // Builtin Encoder don't have UID
  mbEncoderRegister (NULL, coreEncodersCB);

}
