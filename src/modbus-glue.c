/*
 * Copyright (C) 2015-2024 IoT.bzh Company
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

#include "modbus-binding.h"
#include <afb-req-utils.h>
#include <arpa/inet.h>
#include <errno.h>
#include <modbus.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

static int ModbusFormatResponse(ModbusSensorT *sensor,
                                json_object **responseJ) {
  ModbusFormatCbT *format = sensor->format;
  ModbusSourceT source;
  json_object *elemJ;
  int err;

  if (!format->decodeCB) {
    AFB_API_NOTICE(sensor->api, "ModbusFormatResponse: No decodeCB uid=%s",
                   sensor->uid);
    goto OnErrorExit;
  }

  // create source info
  source.sensor = sensor->uid;
  source.api = sensor->api;
  source.context = sensor->context;

  if (sensor->count == 1) {
    err = format->decodeCB(&source, format, (uint16_t *)sensor->buffer, 0,
                           responseJ);
    if (err)
      goto OnErrorExit;
  } else {
    *responseJ = json_object_new_array();
    for (int idx = 0; idx < sensor->count; idx++) {
      err = sensor->format->decodeCB(&source, format,
                                     (uint16_t *)sensor->buffer, idx, &elemJ);
      if (err)
        goto OnErrorExit;
      json_object_array_add(*responseJ, elemJ);
    }
  }
  return 0;

OnErrorExit:
  return 1;
}

// try to reconnect when RTU close connection
static void ModbusReconnect(ModbusSensorT *sensor) {
  modbus_t *ctx = (modbus_t *)sensor->rtu->connection->context;
  AFB_API_NOTICE(sensor->api, "ModbusReconnect: Try reconnecting rtu=%s",
                 sensor->rtu->uid);

  // force deconnection/reconnection
  modbus_close(ctx);
  int err = modbus_connect(ctx);
  if (err) {
    AFB_API_ERROR(sensor->api,
                  "ModbusReconnect: Socket disconnected rtu=%s error=%s",
                  sensor->rtu->uid, strerror(err));
  }
}

static int ModbusRtuSemWait(afb_api_t api, ModbusRtuT *rtu) {
  if (rtu->connection->semaphore) sem_wait (rtu->connection->semaphore);
  return ModbusRtuSetSlave(api, rtu);
}

/**
 * Discards received data.
 *
 * This should be called before sending a command to a Modbus device. If
 * the device has answered the previous command after the timeout, the
 * response will be read when sending a new command and the client will
 * receive inconsistent data. Flushing circumvents that.
*/
static int ModbusFlush(afb_api_t api, ModbusConnectionT *conn) {
  int rc;

  rc = modbus_flush(conn->context);

  if (rc < 0) {
    AFB_API_ERROR(api, "ModbusFlush failed for %s with error %s", conn->uri, modbus_strerror(errno));
    return 1;
  }

  return 0;
}

static int ModbusReadBits(ModbusSensorT *sensor, json_object **responseJ) {
  ModbusFunctionCbT *function = sensor->function;
  ModbusRtuT *rtu = sensor->rtu;
  modbus_t *ctx = (modbus_t *)rtu->connection->context;
  int err;

  ModbusRtuSemWait(sensor->api, rtu);
  err = ModbusFlush(sensor->api, rtu->connection);
  if(err)
    goto OnErrorExit;

  // allocate input buffer on 1st read (all buffer are in 16bit for event diff
  // processing)
  if (!sensor->buffer) {
    sensor->buffer = (uint16_t *)calloc(sensor->count, sizeof(uint16_t));
    if (!sensor->buffer) {
      AFB_API_ERROR(sensor->api, "ModbusReadBits: out of memory");
      goto OnErrorExit;
    }
  }
  uint8_t *data8 = (uint8_t *)sensor->buffer;

  switch (function->type) {
  case MB_COIL_STATUS:
    err = modbus_read_bits(ctx, sensor->registry, sensor->count, data8);
    if (err != sensor->count)
      goto OnErrorExit;
    break;

  case MB_COIL_INPUT:
    err = modbus_read_input_bits(ctx, sensor->registry, sensor->count, data8);
    if (err != sensor->count)
      goto OnErrorExit;
    break;

  default:
    err = 0;
    goto OnErrorExit;
  }

  // if responseJ is provided build JSON response
  if (responseJ) {
    err = ModbusFormatResponse(sensor, responseJ);
    if (err)
      goto OnErrorExit;
  }

  if (rtu->connection->semaphore) sem_post (rtu->connection->semaphore);
  return 0;

OnErrorExit:
  AFB_API_ERROR(sensor->api,
                "ModbusReadBit: fail to read rtu=%s sensor=%s error=%s",
                rtu->uid, sensor->uid, modbus_strerror(errno));
  if (err == -1)
    ModbusReconnect(sensor);

  if (rtu->connection->semaphore) sem_post (rtu->connection->semaphore);
  return 1;
}

static int ModbusReadRegisters(ModbusSensorT *sensor, json_object **responseJ) {
  ModbusFunctionCbT *function = sensor->function;
  ModbusFormatCbT *format = sensor->format;
  ModbusRtuT *rtu = sensor->rtu;
  modbus_t *ctx = (modbus_t *)rtu->connection->context;
  int err, regcount;

  ModbusRtuSemWait(sensor->api, rtu);
  err = ModbusFlush(sensor->api, rtu->connection);
  if(err)
    goto OnErrorExit;

  regcount =
      sensor->count * format->nbreg; // number of register to read on device

  // allocate input buffer on 1st read
  if (!sensor->buffer) {
    sensor->buffer = (uint16_t *)calloc(regcount, sizeof(uint16_t));
    if (!sensor->buffer) {
      AFB_API_ERROR(sensor->api, "ModbusReadRegisters: out of memory");
      goto OnErrorExit;
    }
  }

  switch (function->type) {
  case MB_REGISTER_INPUT:
    err = modbus_read_input_registers(ctx, sensor->registry, regcount,
                                      (uint16_t *)sensor->buffer);
    if (err != regcount)
      goto OnErrorExit;
    break;

  case MB_REGISTER_HOLDING:
    err = modbus_read_registers(ctx, sensor->registry, regcount,
                                (uint16_t *)sensor->buffer);
    if (err != regcount)
      goto OnErrorExit;
    break;

  default:
    err = 0;
    goto OnErrorExit;
  }

  // if responseJ is provided build JSON response
  if (responseJ) {
    err = ModbusFormatResponse(sensor, responseJ);
    if (err)
      goto OnErrorExit;
  }

  if (rtu->connection->semaphore) sem_post (rtu->connection->semaphore);
  return 0;

OnErrorExit:
  AFB_API_ERROR(sensor->api,
                "ModbusReadRegisters: fail to read rtu=%s sensor=%s error=%s",
                rtu->uid, sensor->uid, modbus_strerror(errno));
  if (err == -1)
    ModbusReconnect(sensor);

  if (rtu->connection->semaphore) sem_post (rtu->connection->semaphore);
  return 1;
}

static int ModbusWriteBits(ModbusSensorT *sensor, json_object *queryJ) {
  ModbusFormatCbT *format = sensor->format;
  ModbusRtuT *rtu = sensor->rtu;
  modbus_t *ctx = (modbus_t *)rtu->connection->context;
  json_object *elemJ;
  int err, idx;

  ModbusRtuSemWait(sensor->api, rtu);
  err = ModbusFlush(sensor->api, rtu->connection);
  if(err)
    goto OnErrorExit;

  uint8_t *data8 =
      (uint8_t *)alloca(sizeof(uint8_t) * sensor->count);

  if (!json_object_is_type(queryJ, json_type_array)) {
    data8[0] = (uint8_t)json_object_get_boolean(queryJ);

    err = modbus_write_bit(
        ctx, sensor->registry,
        data8[0]); // Modbus function code 0x05 (force single coil).
    if (err != 1)
      goto OnErrorExit;

  } else {
    for (idx = 0; idx < sensor->count; idx++) {
      elemJ = json_object_array_get_idx(queryJ, idx);
      data8[idx] = (uint8_t)json_object_get_boolean(elemJ);
    }
    err = modbus_write_bits(
        ctx, sensor->registry, sensor->count,
        data8); // Modbus function code 0x0F (force multiple coils).
    if (err != sensor->count)
      goto OnErrorExit;
  }

  if (rtu->connection->semaphore) sem_post (rtu->connection->semaphore);
  return 0;

OnErrorExit:
  AFB_API_ERROR(
      sensor->api,
      "ModbusWriteBits: fail to write rtu=%s sensor=%s error=%s data=%s",
      rtu->uid, sensor->uid, modbus_strerror(errno),
      json_object_get_string(queryJ));
  if (err == -1)
    ModbusReconnect(sensor);

  if (rtu->connection->semaphore) sem_post (rtu->connection->semaphore);
  return 1;
}

static int ModbusWriteRegisters(ModbusSensorT *sensor, json_object *queryJ) {
  ModbusFormatCbT *format = sensor->format;
  ModbusRtuT *rtu = sensor->rtu;
  modbus_t *ctx = (modbus_t *)rtu->connection->context;
  json_object *elemJ;
  int err = 0;
  int idx = 0;
  ModbusSourceT source;

  uint16_t *data16 =
      (uint16_t *)alloca(sizeof(uint16_t) * format->nbreg * sensor->count);

  // create source info
  source.sensor = sensor->uid;
  source.api = sensor->api;
  source.context = sensor->context;

  ModbusRtuSemWait(sensor->api, rtu);
  err = ModbusFlush(sensor->api, rtu->connection);
  if(err)
    goto OnErrorExit;

  if (!format->encodeCB) {
    AFB_API_NOTICE(sensor->api, "ModbusFormatResponse: No encodeCB uid=%s",
                   sensor->uid);
    goto OnErrorExit;
  }

  if (!json_object_is_type(queryJ, json_type_array)) {

    err = format->encodeCB(&source, format, queryJ, &data16, 0);
    if (err)
      goto OnErrorExit;
    if (format->nbreg == 1) {
      err = modbus_write_register(
          ctx, sensor->registry,
          data16[0]); // Modbus function code 0x06 (preset single register).
    } else {
      err = modbus_write_registers(
          ctx, sensor->registry, format->nbreg,
          data16); // Modbus function code 0x06 (preset single register).
    }
    if (err != format->nbreg)
      goto OnErrorExit;

  } else {
    for (idx = 0; idx < sensor->format->nbreg; idx++) {
      elemJ = json_object_array_get_idx(queryJ, idx);
      err = format->encodeCB(&source, format, elemJ, &data16, idx);
      if (err)
        goto OnErrorExit;
    }
    err = modbus_write_registers(
        ctx, sensor->registry, format->nbreg,
        data16); // Modbus function code 0x10 (preset multiple registers).
    if (err != format->nbreg)
      goto OnErrorExit;
  }
  if (rtu->connection->semaphore) sem_post (rtu->connection->semaphore);
  return 0;

OnErrorExit:
  AFB_API_ERROR(
      sensor->api,
      "ModbusWriteBits: fail to write rtu=%s sensor=%s error=%s data=%s",
      rtu->uid, sensor->uid, modbus_strerror(errno),
      json_object_get_string(queryJ));
  if (err == -1)
    ModbusReconnect(sensor);
  if (rtu->connection->semaphore) sem_post (rtu->connection->semaphore);
  return 1;
}

// Modbus Read/Write per register type/function Callback
static ModbusFunctionCbT ModbusFunctionsCB[] = {
    {.uid = "COIL_INPUT",
     .type = MB_COIL_INPUT,
     .info = "Boolean ReadOnly register",
     .readCB = ModbusReadBits,
     .writeCB = NULL},
    {.uid = "COIL_HOLDING",
     .type = MB_COIL_STATUS,
     .info = "Boolean ReadWrite register",
     .readCB = ModbusReadBits,
     .writeCB = ModbusWriteBits},
    {.uid = "REGISTER_INPUT",
     .type = MB_REGISTER_INPUT,
     .info = "INT16 ReadOnly register",
     .readCB = ModbusReadRegisters,
     .writeCB = NULL},
    {.uid = "REGISTER_HOLDING",
     .type = MB_REGISTER_HOLDING,
     .info = "INT16 ReadWrite register",
     .readCB = ModbusReadRegisters,
     .writeCB = ModbusWriteRegisters},

    {.uid = NULL} // should be NULL terminated
};

// return type/function callbacks from sensor type name
ModbusFunctionCbT *mbFunctionFind(afb_api_t api, const char *uid) {
  int idx;
  assert(uid);

  for (idx = 0; ModbusFunctionsCB[idx].uid; idx++) {
    if (!strcasecmp(ModbusFunctionsCB[idx].uid, uid))
      break;
  }

  return (&ModbusFunctionsCB[idx]);
}

// Timer base sensor polling tic send event if sensor value changed
static void ModbusTimerCallback(afb_timer_t timer, void *userdata,
                               uint decount) {

  ModbusEvtT *context = (ModbusEvtT *)userdata;
  ModbusSensorT *sensor = context->sensor;
  json_object *responseJ;
  int err, count;

  // update sensor buffer with current value without building JSON
  err = (sensor->function->readCB)(sensor, NULL);

  if (err) {
    AFB_API_ERROR(sensor->api,
                  "ModbusTimerCallback: fail read sensor rtu=%s sensor=%s",
                  sensor->rtu->uid, sensor->uid);
    goto OnErrorExit;
  }

  // if buffer change then update JSON and send event
  if (memcmp(context->buffer, sensor->buffer,
             sizeof(uint16_t) * sensor->format->nbreg * sensor->count) ||
      !--context->idle) {

    // if responseJ is provided build JSON response
    err = ModbusFormatResponse(sensor, &responseJ);
    if (err)
      goto OnErrorExit;

    // send event and it no more client remove event and timer
    afb_data_t data = afb_data_json_c_hold(responseJ);
    count = afb_event_push(sensor->event, 1, &data);
    if (count == 0) {
      afb_event_unref(sensor->event);
      sensor->event = NULL;
      afb_timer_unref(timer);
      sensor->timer=NULL;
      free(context->buffer);
    } else {
      // save current sensor buffer for next comparison
      memcpy(context->buffer, sensor->buffer,
             sizeof(uint16_t) * sensor->format->nbreg * sensor->count);
      context->idle = sensor->idle; // reset idle counter
    }
  }
  return;

OnErrorExit:
  return;
}

static int ModbusSensorEventCreate(ModbusSensorT *sensor,
                                   json_object **responseJ) {
  ModbusRtuT *rtu = sensor->rtu;
  int err;
  ModbusEvtT *mbEvtHandle;

  if (!sensor->function->readCB)
    goto OnErrorExit;

  err = (sensor->function->readCB)(sensor, responseJ);
  if (err) {
    AFB_API_WARNING(sensor->api,
                  "ModbusSensorEventCreate: fail read sensor rtu=%s sensor=%s",
                  rtu->uid, sensor->uid);
  }

  // if no even attach to sensor create one
  if (!sensor->event) {
    err = afb_api_new_event(sensor->api, sensor->uid, &sensor->event);
    if (err) {
      AFB_API_ERROR(
          sensor->api,
          "ModbusSensorEventCreate: fail to create event rtu=%s sensor=%s",
          rtu->uid, sensor->uid);
      goto OnErrorExit;
    }

    mbEvtHandle = (ModbusEvtT *)calloc(1, sizeof(ModbusEvtT));
    if (!mbEvtHandle) {
      AFB_API_ERROR(sensor->api, "ModbusSensorEventCreate: out of memory");
      goto OnErrorExit;
    }
    mbEvtHandle->sensor = sensor;
    mbEvtHandle->idle = sensor->idle;
    mbEvtHandle->buffer =
        (uint16_t *)calloc(sensor->format->nbreg * sensor->count,
                           sizeof(uint16_t)); // keep track of old value
    if (!mbEvtHandle->buffer) {
      AFB_API_ERROR(sensor->api, "ModbusSensorEventCreate: out of memory");
      goto OnErrorExit;
    }
    err = afb_timer_create(&sensor->timer, 0, 0, 0,
                           0, // run forever,
                           (uint)1000 / rtu->hertz, 0, ModbusTimerCallback,
                           mbEvtHandle, 1);

    if (err != 0) {
      AFB_API_ERROR(
          sensor->api,
          "ModbusSensorTimerCreate: fail to create timer rtu=%s sensor=%s",
          rtu->uid, sensor->uid);
      goto OnErrorExit;
    }
  }
  return 0;

OnErrorExit:
  return 1;
}

void ModbusSensorRequest(afb_req_t request, ModbusSensorT *sensor,
                         json_object *queryJ) {
  assert(sensor);
  assert(sensor->rtu);
  ModbusRtuT *rtu = sensor->rtu;
  const char *action;
  json_object *dataJ, *responseJ = NULL;
  int err;

  if (!rtu->connection->context) {
    afb_req_reply_string_f(request, AFB_ERRNO_INTERNAL_ERROR,
        "not-connected, ModbusSensorRequest: RTU not connected rtu=%s sensor=%s query=%s",
        rtu->uid, sensor->uid, json_object_get_string(queryJ));
    goto OnErrorExit;
  };

  err =
      rp_jsonc_unpack(queryJ, "{ss s?o !}", "action", &action, "data", &dataJ);
  if (err) {
    afb_req_reply_string_f(request, AFB_ERRNO_INTERNAL_ERROR,
        "querry-error, ModbusSensorRequest: invalid 'json' rtu=%s sensor=%s query=%s",
        rtu->uid, sensor->uid, json_object_get_string(queryJ));
    goto OnErrorExit;
  }

  if (!strcasecmp(action, "WRITE")) {
    if (!sensor->function->writeCB)
      goto OnWriteError;

    err = (sensor->function->writeCB)(sensor, dataJ);
    if (err)
      goto OnWriteError;

  } else if (!strcasecmp(action, "READ")) {
    if (!sensor->function->readCB)
      goto OnReadError;

    err = (sensor->function->readCB)(sensor, &responseJ);
    if (err)
      goto OnReadError;

  } else if (!strcasecmp(action, "SUBSCRIBE")) {
    err = ModbusSensorEventCreate(sensor, &responseJ);
    if (err)
      goto OnSubscribeError;
    err = afb_req_subscribe(request, sensor->event);
    if (err)
      goto OnSubscribeError;

  } else if (!strcasecmp(action, "UNSUBSCRIBE")) { // Fulup ***** Virer l'event
                                                   // quand le count est à zero
    if (sensor->event) {
      err = afb_req_unsubscribe(request, sensor->event);
      if (err)
        goto OnSubscribeError;
    }
  } else {
    afb_req_reply_string_f(request, AFB_ERRNO_INTERNAL_ERROR,
        "syntax-error, ModbusSensorRequest: action='%s' UNKNOWN rtu=%s sensor=%s query=%s",
        action, rtu->uid, sensor->uid, json_object_get_string(queryJ));
    goto OnErrorExit;
  }

  afb_data_t repldata = afb_data_json_c_hold(responseJ);
  afb_req_reply(request, 0, 1, &repldata);
  return;

OnWriteError:
  afb_req_reply_string_f(request, AFB_ERRNO_INTERNAL_ERROR,
      "write-error, ModbusSensorRequest: fail to write data=%s rtu=%s sensor=%s error=%s",
      json_object_get_string(dataJ), rtu->uid, sensor->uid, modbus_strerror(errno));
  goto OnErrorExit;

OnReadError:
  afb_req_reply_string_f(request, AFB_ERRNO_INTERNAL_ERROR,
      "read-error, ModbusSensorRequest: fail to read rtu=%s sensor=%s error=%s",
      rtu->uid, sensor->uid, modbus_strerror(errno));
  goto OnErrorExit;

OnSubscribeError:
  afb_req_reply_string_f(request, AFB_ERRNO_INTERNAL_ERROR,
      "subscribe-error, ModbusSensorRequest: fail to subscribe rtu=%s sensor=%s error=%s",
      rtu->uid, sensor->uid, modbus_strerror(errno));
  goto OnErrorExit;

OnErrorExit:
  return;
}

static int ModbusParseTTY(const char *uri, char **ttydev, int *baud) {
#define TTY_PREFIX "tty://"
  char *devpath, *speed, *uri_tmp;
  int idx;
  static int prefixlen = sizeof(TTY_PREFIX) - 1;

  if (strncasecmp(uri, TTY_PREFIX, prefixlen))
    goto OnErrorExit;
  uri_tmp = strdup(uri);
  // break URI in substring ignoring leading tcp:
  for (idx = prefixlen; idx < strlen(uri_tmp); idx++) {
    if (uri_tmp[idx] == 0)
      break;
    if (uri_tmp[idx] == ':')
      uri_tmp[idx] = 0;
  }

  devpath = &uri_tmp[prefixlen - 1];
  speed = &uri_tmp[idx];
  sscanf(speed, "%d", baud);

  *ttydev = strdup(devpath);
  free(uri_tmp);
  return 0;

OnErrorExit:
  return 1;
}

static int ModbusParseURI(const char *uri, char **addr, int *port) {
#define TCP_PREFIX "tcp://"
  char *hostaddr, *tcpport, *uri_tmp;
  int idx;
  static int prefixlen = sizeof(TCP_PREFIX) - 1;
  char ip[100];
  struct hostent *he;
  struct in_addr **addrlist;

  if (strncasecmp(uri, TCP_PREFIX, prefixlen))
    goto OnErrorExit;
  uri_tmp = strdup(uri);
  // break URI in substring ignoring leading tcp:
  for (idx = prefixlen; idx < strlen(uri_tmp); idx++) {
    if (uri_tmp[idx] == 0)
      break;
    if (uri_tmp[idx] == ':')
      uri_tmp[idx] = 0;
  }

  // extract IP addr and port
  hostaddr = &uri_tmp[prefixlen];
  tcpport = &uri_tmp[idx];
  sscanf(tcpport, "%d", port);

  if ((he = gethostbyname(hostaddr)) == NULL)
    goto OnErrorExit;

  addrlist = (struct in_addr **)he->h_addr_list;
  for (idx = 0; addrlist[idx] != NULL; idx++) {
    // Return the first one;
    strcpy(ip, inet_ntoa(*addrlist[idx]));
    break;
  }

  *addr = strdup(ip);
  free(uri_tmp);
  return 0;

OnErrorExit:
  return 1;
}

int ModbusRtuConnect(afb_api_t api, ModbusConnectionT *connection, const char *rtu_uid) {
  modbus_t *ctx;

  if (!strncmp(connection->uri, "tty:", 4)) {
    char *ttydev = NULL;
    int speed = 19200;
    if (ModbusParseTTY(connection->uri, &ttydev, &speed)) {
      AFB_API_ERROR(api, "ModbusRtuConnect: fail to parse uid=%s uri=%s",
                    rtu_uid, connection->uri);
      goto OnErrorExit;
    }

    ctx = modbus_new_rtu(ttydev, speed, 'N', 8, 1);
    if (modbus_connect(ctx) == -1) {
      AFB_API_ERROR(
          api,
          "ModbusRtuConnect: fail to connect tty uid=%s ttydev=%s speed=%d",
          rtu_uid, ttydev, speed);
      modbus_free(ctx);
      goto OnErrorExit;
    }

    // serial link do not support simultaneous read
    connection->semaphore = malloc (sizeof(sem_t));
    if (!connection->semaphore) {
      AFB_API_ERROR(api, "ModbusRtuConnect: out of memory");
      goto OnErrorExit;
    }
    int err = sem_init(connection->semaphore, 0, 1);
    if (err < 0) {
        AFB_API_ERROR(api, "ModbusRtuConnect: fail to init tty semaphore uid=%s uri=%s",
                    rtu_uid, connection->uri);
        goto OnErrorExit;
    }

  } else {
    char *addr;
    int port;
    if (ModbusParseURI(connection->uri, &addr, &port)) {
      AFB_API_ERROR(api, "ModbusRtuConnect: fail to parse uid=%s uri=%s",
                    rtu_uid, connection->uri);
      goto OnErrorExit;
    }
    ctx = modbus_new_tcp(addr, port);
    if (modbus_connect(ctx) == -1) {
      AFB_API_ERROR(
          api, "ModbusRtuConnect: fail to connect TCP uid=%s addr=%s port=%d",
          rtu_uid, addr, port);
      modbus_free(ctx);
      goto OnErrorExit;
    }
  }

  // store current libmodbus ctx with rtu handle
  connection->context = (void *)ctx;
  return 0;

OnErrorExit:
  return 1;
}

int ModbusRtuSetSlave(afb_api_t api, ModbusRtuT *rtu) {
  modbus_t *ctx = (modbus_t *)rtu->connection->context;

  if (rtu->slaveid) {
    if (modbus_set_slave(ctx, rtu->slaveid) == -1) {
      AFB_API_ERROR(api, "ModbusRtuSetSlave: fail to set slaveid=%d uid=%s",
                    rtu->slaveid, rtu->uid);
      modbus_free(ctx);
      goto OnErrorExit;
    }
  }

  if (rtu->timeout) {
    if (modbus_set_response_timeout(ctx, 0, rtu->timeout * 1000) == -1) {
      AFB_API_ERROR(api, "ModbusRtuSetSlave: fail to set timeout=%d uid=%s",
                    rtu->timeout, rtu->uid);
      modbus_free(ctx);
      goto OnErrorExit;
    }
  }

  if (rtu->debug) {
    if (modbus_set_debug(ctx, rtu->debug) == -1) {
      AFB_API_ERROR(api, "ModbusRtuSetSlave: fail to set debug=%d uid=%s",
                    rtu->debug, rtu->uid);
      modbus_free(ctx);
      goto OnErrorExit;
    }
  }

  return 0;

OnErrorExit:
  return 1;
}

void ModbusRtuSensorsId(ModbusRtuT *rtu, int verbose, json_object *responseJ) {
  json_object *elemJ, *dataJ, *actionsJ;
  ModbusSensorT *sensor;
  int err = 0;

  // loop on every sensors
  for (int idx = 0; rtu->sensors[idx].uid; idx++) {
    sensor = &rtu->sensors[idx];
    switch (verbose) {
    default:
    case 1:
      err += rp_jsonc_pack(&elemJ, "{ss ss ss si si}", "uid", sensor->uid,
                           "type", sensor->function->uid, "format",
                           sensor->format->uid, "count", sensor->count, "nbreg",
                           sensor->format->nbreg * sensor->count);
      break;
    case 2:
      err += (sensor->function->readCB)(sensor, &dataJ);

      if (err)
        dataJ = NULL;
      err = rp_jsonc_pack(&elemJ, "{ss ss ss si si so}", "uid", sensor->uid,
                          "type", sensor->function->uid, "format",
                          sensor->format->uid, "count", sensor->count, "nbreg",
                          sensor->format->nbreg * sensor->count, "data", dataJ);
      break;
    case 3:
      // if not usage try to build one
      if (!sensor->usage) {
        actionsJ = json_object_new_array();

        if (sensor->function->writeCB) {
          json_object_array_add(actionsJ, json_object_new_string("write"));
        }
        if (sensor->function->readCB) {
          json_object_array_add(actionsJ, json_object_new_string("read"));
          json_object_array_add(actionsJ, json_object_new_string("subscribe"));
          json_object_array_add(actionsJ,
                                json_object_new_string("unsubscribe"));
        }
        rp_jsonc_pack(&sensor->usage, "{so ss*}", "action", actionsJ, "data",
                      sensor->format->info);
      }

      // make sure it does not get deleted config json object after 1st usage
      if (sensor->sample)
        json_object_get(sensor->sample);
      json_object_get(sensor->usage);

      err += rp_jsonc_pack(
          &elemJ, "{ss ss ss* ss* ss* so* so* si*}", "uid", sensor->uid, "verb",
          sensor->apiverb, "info", sensor->info, "type", sensor->function->info,
          "format", sensor->format->uid, "usage", sensor->usage, "sample",
          sensor->sample, "count", sensor->count);
      break;
    }

    if (err) {
      AFB_DEBUG("ModbusRtuSensorsId: Fail to wrap json RTU sensor info rtu=%s "
                "sensor=%s",
                rtu->uid, sensor->uid);
    } else {
      json_object_array_add(responseJ, elemJ);
    }
  }
}

int ModbusRtuIsConnected(afb_api_t api, ModbusRtuT *rtu) {
  uint8_t response[MODBUS_MAX_PDU_LENGTH];
  modbus_t *ctx = (modbus_t *)rtu->connection->context;
  int run;

  if (ModbusRtuSetSlave(api, rtu)) {
    AFB_API_ERROR(api, "ModbusLoadOne: failed to set slave ID uid=%s uri=%s",
                  rtu->uid, rtu->connection->uri);
    goto OnErrorExit;
  }

  ModbusRtuSemWait(api, rtu);
  run = modbus_report_slave_id(ctx, sizeof(response), response);
  if (rtu->connection->semaphore) sem_post (rtu->connection->semaphore);

  if (run < 0) {
    // handle case where RTU does not support "Report Server ID"
    if (strcmp(modbus_strerror(errno), "Illegal function") == 0)
      return 1;
    else
      goto OnErrorExit;
  }

  if (run > 0)
    return 1;
  else
    return 0;

OnErrorExit:
  AFB_API_ERROR(
      api, "ModbusRtuIsConnected: fail to get RTU=%s connection status err=%s",
      rtu->uid, modbus_strerror(errno));
  return -1;
}

void ModbusRtuRequest(afb_req_t request, ModbusRtuT *rtu, json_object *queryJ) {
  modbus_t *ctx = (modbus_t *)rtu->connection->context;
  const char *action;
  const char *uri = NULL;
  int verbose = 0;
  json_object *responseJ = NULL;
  int err;

  err = rp_jsonc_unpack(queryJ, "{ss s?s s?i !}",
                        "action", &action, "uri", &uri, "verbose", &verbose);
  if (err) {
    afb_req_reply_string_f(request, AFB_ERRNO_INTERNAL_ERROR,
        "ModbusRtuAdmin, invalid query rtu=%s query=%s",
        rtu->uid, json_object_get_string(queryJ));
    goto OnErrorExit;
  }

  if (!strcasecmp(action, "connect")) {

    if (rtu->connection->context) {
      afb_req_reply_string_f(request, AFB_ERRNO_INTERNAL_ERROR,
          "ModbusRtuAdmin, cannot connect twice rtu=%s query=%s",
          rtu->uid, json_object_get_string(queryJ));
      goto OnErrorExit;
    }

    if (!uri) {
      afb_req_reply_string_f(request, AFB_ERRNO_INTERNAL_ERROR,
          "ModbusRtuAdmin, cannot connect URI missing rtu=%s query=%s",
          rtu->uid, json_object_get_string(queryJ));
      goto OnErrorExit;
    }

    rtu->connection->uri = uri;
    err = ModbusRtuConnect(afb_req_get_api(request), rtu->connection, rtu->uid);
    if (err) {
      afb_req_reply_string_f(request, AFB_ERRNO_INTERNAL_ERROR,
          "ModbusRtuAdmin, fail to connect: uri=%s query=%s",
          uri, json_object_get_string(queryJ));
      goto OnErrorExit;
    } else {
      err = ModbusRtuSetSlave(afb_req_get_api(request), rtu);
      if (err) {
        afb_req_reply_string_f(request, AFB_ERRNO_INTERNAL_ERROR,
            "ModbusRtuAdmin, failed to set slave ID uri=%s query=%s",
            uri, json_object_get_string(queryJ));
      }
    }

  } else if (!strcasecmp(action, "disconnect")) {
    modbus_close(ctx);
    modbus_free(ctx);
    rtu->connection->context = NULL;

  } else if (!strcasecmp(action, "info")) {

    responseJ = json_object_new_array();
    ModbusRtuSensorsId(rtu, verbose, responseJ);
  }

  afb_data_t repldata = afb_data_json_c_hold(responseJ);
  afb_req_reply(request, 0, 1, &repldata);
  return;

OnErrorExit:
  return;
}
