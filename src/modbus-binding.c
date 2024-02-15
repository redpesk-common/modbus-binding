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

// Le contexte de sensor loader au moment de l'API n'est retrouvé avec le
// request ****

#define _GNU_SOURCE

#include "modbus-binding.h"
#include <afb-helpers4/afb-req-utils.h>

#ifndef MB_DEFAULT_POLLING_FEQ
#define MB_DEFAULT_POLLING_FEQ 10
#endif

// static binding plugin store
static plugin_store_t plugins = PLUGIN_STORE_INITIAL;

static void PingTest(afb_req_t request, unsigned argc,
                     afb_data_t const args[]) {
  static int count = 0;
  char response[32];
  afb_data_t arg, repl;
  int curcount = ++count;
  afb_req_param_convert(request, 0, AFB_PREDEFINED_TYPE_STRINGZ, &arg);
  AFB_REQ_NOTICE(request, "ping count=%d query=%s", curcount,
                 (const char *)afb_data_ro_pointer(arg));
  snprintf(response, sizeof(response), "\"pong=%d\"", curcount);
  repl = afb_data_json_copy(response, 0);
  afb_req_reply(request, 0, 1, &repl);
}

static void InfoRtu(afb_req_t request, unsigned argc, afb_data_t const args[]) {
  json_object *elemJ;
  int err, idx, status;
  afb_data_t arg;
  int verbose = 0;

  CtlHandleT *controller= afb_req_get_vcbdata(request);
  ModbusRtuT *rtus = (ModbusRtuT *)controller->modbus;


  afb_req_param_convert(request, 0, AFB_PREDEFINED_TYPE_JSON_C, &arg);
  json_object *queryJ = (json_object *)afb_data_ro_pointer(arg);
  json_object *responseJ = json_object_new_array();

  if (json_object_is_type(queryJ, json_type_object)) {
    err = rp_jsonc_unpack(queryJ, "{s?i !}", "verbose", &verbose);
    if (err) {
      afb_req_reply_string_f(request, AFB_ERRNO_INTERNAL_ERROR,
          "ModbusRtuAdmin, ListRtu: invalid 'json query' query=%s",
          json_object_get_string(queryJ));
      goto OnErrorExit;
    }
  }

  // loop on every defined RTU
  if (verbose) {
    for (idx = 0; rtus[idx].uid; idx++) {
      switch (verbose) {
      case 1:
      default:
        rp_jsonc_pack(&elemJ, "{ss ss ss}", "uid", rtus[idx].uid, "uri",
                      rtus[idx].context->uri, "info", rtus[idx].info);
        break;
      case 2:
        status = ModbusRtuIsConnected(afb_req_get_api(request), &rtus[idx]);
        if (status < 0) {
          rp_jsonc_pack(&elemJ, "{ss ss ss}", "uid", rtus[idx].uid, "uri",
                        rtus[idx].context->uri, "info", rtus[idx].info);
        } else {
          rp_jsonc_pack(&elemJ, "{ss ss ss sb}", "uid", rtus[idx].uid, "uri",
                        rtus[idx].context->uri, "info", rtus[idx].info, "status", status);
        }
        break;
      }
      json_object_array_add(responseJ, elemJ);
    }
  } else {
    // build global info page for developer dynamic HTML5 page
    json_object *rtuJ, *rtusJ, *statusJ, *sensorsJ, *admincmdJ, *usageJ,
        *actionsJ;

    rtusJ = json_object_new_array();
    for (idx = 0; rtus[idx].uid; idx++) {
      status = ModbusRtuIsConnected(afb_req_get_api(request), &rtus[idx]);
      err = rp_jsonc_pack(&statusJ, "{ss si sb}", "uri", rtus[idx].context->uri,
                          "slaveid", rtus[idx].slaveid, "status", status >= 0);

      // prepare array to hold every sensor verbs
      sensorsJ = json_object_new_array();
      err +=
          rp_jsonc_pack(&actionsJ, "[s s s]", "info", "connect", "disconnect");
      err +=
          rp_jsonc_pack(&usageJ, "{so, si}", "action", actionsJ, "verbose", 3);
      err += rp_jsonc_pack(&admincmdJ, "{ss ss ss so}", "uid", "admin", "info",
                           "RTU admin cmd", "verb", rtus[idx].adminapi, "usage",
                           usageJ);
      json_object_array_add(sensorsJ, admincmdJ);

      // create group object with rtu_info and rtu-sensors
      ModbusRtuSensorsId(&rtus[idx], 3, sensorsJ);
      err +=
          rp_jsonc_pack(&rtuJ, "{ss ss* so* so}", "uid", rtus[idx].uid, "info",
                        rtus[idx].info, "status", statusJ, "verbs", sensorsJ);
      if (err) {
        AFB_DEBUG("InfoRtu: Fail to wrap json sensors info rtu=%s",
                  rtus[idx].uid);
        goto OnErrorExit;
      }
      json_object_array_add(rtusJ, rtuJ);
    }

    err= rp_jsonc_pack(&responseJ, "{so s{ss ss* ss* ss*}}", "groups", rtusJ, "metadata", "uid",
		      controller->metadata.uid, "info", controller->metadata.info, "version", controller->metadata.version, "author",
		      controller->metadata.author);
    if (err) {
      AFB_DEBUG("InfoRtu: Fail to wrap json binding global response");
      goto OnErrorExit;
    }
  }
  afb_data_t repldata = afb_data_json_c_hold(responseJ);
  afb_req_reply(request, 0, 1, &repldata);
  return;

OnErrorExit:
  return;
}

// Static verb not depending on Modbus json config file
static afb_verb_t CtrlApiVerbs[] = {
    /* VERB'S NAME         FUNCTION TO CALL         SHORT DESCRIPTION */
    {.verb = "ping", .callback = PingTest, .info = "Modbus API ping test"},
    {.verb = "info", .callback = InfoRtu, .info = "Modbus List RTUs"},
    {.verb = NULL} /* marker for end of the array */
};

static int CtrlLoadStaticVerbs(afb_api_t api, afb_verb_t *verbs,
                               void *vcbdata) {
  int errcount = 0;

  for (int idx = 0; verbs[idx].verb; idx++) {
    errcount +=
        afb_api_add_verb(api, CtrlApiVerbs[idx].verb, CtrlApiVerbs[idx].info,
                         CtrlApiVerbs[idx].callback, vcbdata, 0, 0, 0);
  }

  return errcount;
};
static void RtuDynRequest(afb_req_t request, unsigned argc,
                          afb_data_t const args[]) {
  afb_data_t arg;

  afb_req_param_convert(request, 0, AFB_PREDEFINED_TYPE_JSON_C, &arg);
  json_object *queryJ = (json_object *)afb_data_ro_pointer(arg);

  // retrieve action handle from request and execute the request
  ModbusRtuT *rtu = (ModbusRtuT *)afb_req_get_vcbdata(request);
  ModbusRtuRequest(request, rtu, queryJ);
}

static void SensorDynRequest(afb_req_t request, unsigned argc,
                             afb_data_t const args[]) {
  afb_data_t arg;

  afb_req_param_convert(request, 0, AFB_PREDEFINED_TYPE_JSON_C, &arg);
  json_object *queryJ = (json_object *)afb_data_ro_pointer(arg);

  // retrieve action handle from request and execute the request
  ModbusSensorT *sensor = (ModbusSensorT *)afb_req_get_vcbdata(request);
  ModbusSensorRequest(request, sensor, queryJ);
}

static int SensorLoadOne(afb_api_t api, ModbusRtuT *rtu, ModbusSensorT *sensor,
                         json_object *sensorJ) {
  int err = 0;
  const char *type = NULL;
  const char *format = NULL;
  const char *privilege = NULL;
  afb_auth_t *authent = NULL;
  json_object *argsJ = NULL;
  ModbusSourceT source;

  // should already be allocated
  assert(sensorJ);

  // set default values
  memset(sensor, 0, sizeof(ModbusSensorT));
  sensor->rtu = rtu;
  sensor->hertz = rtu->hertz;
  sensor->idle = rtu->idle;
  sensor->count = 1;

  err = rp_jsonc_unpack(
      sensorJ, "{ss,ss,si,s?s,s?s,s?s,s?i,s?i,s?i,s?o,s?o,s?o !}", "uid",
      &sensor->uid, "type", &type, "register", &sensor->registry, "info",
      &sensor->info, "privilege", &privilege, "format", &format, "hertz",
      &sensor->hertz, "idle", &sensor->idle, "count", &sensor->count, "usage",
      &sensor->usage, "sample", &sensor->sample, "args", &argsJ);
  if (err)
    goto ParsingErrorExit;

  // keep sample and usage as object when defined
  if (sensor->usage)
    json_object_get(sensor->usage);
  if (sensor->sample)
    json_object_get(sensor->sample);

  // find modbus register type/function callback
  sensor->function = mbFunctionFind(api, type);
  if (!sensor->function)
    goto FunctionErrorExit;

  // find encode/decode callback
  sensor->format = mbEncoderFind(api, format);
  if (!sensor->format)
    goto TypeErrorExit;

  // Fulup should insert global auth here
  sensor->api = api;
  if (privilege) {
    authent = (afb_auth_t *)calloc(1, sizeof(afb_auth_t));
    authent->type = afb_auth_Permission;
    authent->text = privilege;
  }

  err = asprintf((char **)&sensor->apiverb, "%s/%s", rtu->prefix, sensor->uid);

  // if defined call format init callback
  if (sensor->format->initCB) {
    source.sensor = sensor->uid;
    source.api = api;
    source.context = NULL;
    err = sensor->format->initCB(&source, argsJ);
    if (err) {
      AFB_API_ERROR(api, "SensorLoadOne: fail to init format verb=%s",
                    sensor->apiverb);
      goto OnErrorExit;
    }
    // remember context for further encode/decode callback
    sensor->context = source.context;
  }

  err = afb_api_add_verb(api, sensor->apiverb, sensor->info, SensorDynRequest,
                         sensor, authent, 0, 0);
  if (err) {
    AFB_API_ERROR(api, "SensorLoadOne: fail to register API verb=%s",
                  sensor->apiverb);
    goto OnErrorExit;
  }

  return 0;

ParsingErrorExit:
  AFB_API_ERROR(api, "SensorLoadOne: Fail to parse sensor: %s",
                json_object_to_json_string(sensorJ));
  return -1;

TypeErrorExit:
  AFB_API_ERROR(api, "SensorLoadOne: missing/invalid Format JSON=%s",
                json_object_to_json_string(sensorJ));
  return -1;
FunctionErrorExit:
  AFB_API_ERROR(api, "SensorLoadOne: missing/invalid Modbus Type=%s JSON=%s",
                type, json_object_to_json_string(sensorJ));
  return -1;
OnErrorExit:
  return -1;
}

static int ModbusLoadOne(afb_api_t api, CtlHandleT *controller, int rtu_idx, json_object *rtuJ) {
  int err = 0;
  json_object *sensorsJ;
  afb_auth_t *authent = NULL;
  ModbusRtuT *rtu = &controller->modbus[rtu_idx];

  // should already be allocated
  assert(rtuJ);
  assert(api);

  memset(rtu, 0, sizeof(ModbusRtuT)); // default is empty
  rtu->context = (ModbusContextT *)calloc(1, sizeof(ModbusContextT));
  err = rp_jsonc_unpack(
      rtuJ, "{ss,s?s,s?s,s?s,s?i,s?s,s?i,s?i,s?i,s?i,s?i,s?i,so}", "uid",
      &rtu->uid, "info", &rtu->info, "uri", &rtu->context->uri, "privileges",
      &rtu->privileges, "autostart", &rtu->autostart, "prefix", &rtu->prefix,
      "slaveid", &rtu->slaveid, "debug", &rtu->debug, "timeout", &rtu->timeout,
      "idlen", &rtu->idlen, "hertz", &rtu->hertz, "idle", &rtu->idle, "sensors",
      &sensorsJ);
  if (err) {
    AFB_API_ERROR(api, "Fail to parse rtu JSON : (%s)",
                  json_object_to_json_string(rtuJ));
    goto OnErrorExit;
  }

  // create an admin command for RTU
  if (rtu->privileges) {
    authent = (afb_auth_t *)calloc(1, sizeof(afb_auth_t));
    authent->type = afb_auth_Permission;
    authent->text = rtu->privileges;
  }

  // if not API prefix let's use RTU uid
  if (!rtu->prefix)
    rtu->prefix = rtu->uid;

  // set default pooling frequency
  if (!rtu->hertz)
    rtu->hertz = MB_DEFAULT_POLLING_FEQ;

  err = asprintf((char **)&rtu->adminapi, "%s/%s", rtu->prefix, "admin");
  err = afb_api_add_verb(api, rtu->adminapi, rtu->info, RtuDynRequest, rtu,
                         authent, 0, 0);
  if (err) {
    AFB_API_ERROR(
        api, "ModbusLoadOne: fail to register API uid=%s verb=%s/%s info=%s",
        rtu->uid, rtu->adminapi, rtu->adminapi, rtu->info);
    goto OnErrorExit;
  }

  // if uri is provided let's try to connect now
  if (rtu->context->uri && rtu->autostart) {
    err = ModbusRtuConnect(api, rtu->context, rtu->uid);
    if (err) {
      AFB_API_ERROR(api, "ModbusLoadOne: fail to connect TTY/RTU uid=%s uri=%s",
                    rtu->uid, rtu->uid);
      if (rtu->autostart > 1)
        goto OnErrorExit;
    }
  } else {
    // else use global context/uri, which is already connected
    if (!controller->context) {
      AFB_API_ERROR(api, "ModbusLoadOne: RTU %s has no URI and there is no global fallback URI",
                    rtu->uid);
      goto OnErrorExit;
    }
    free(rtu->context);
    rtu->context = controller->context;
  }
  err = ModbusRtuSetSlave(api, rtu);
  if (err) {
    AFB_API_ERROR(api, "ModbusLoadOne: failed to set slave ID uid=%s uri=%s",
                  rtu->uid, rtu->uid);
    if (rtu->autostart > 1)
      goto OnErrorExit;
  }

  // loop on sensors
  if (json_object_is_type(sensorsJ, json_type_array)) {
    int count = (int)json_object_array_length(sensorsJ);
    rtu->sensors = (ModbusSensorT *)calloc(count + 1, sizeof(ModbusSensorT));

    for (int idx = 0; idx < count; idx++) {
      json_object *sensorJ = json_object_array_get_idx(sensorsJ, idx);
      err = SensorLoadOne(api, rtu, &rtu->sensors[idx], sensorJ);
      if (err)
        goto OnErrorExit;
    }

  } else {
    rtu->sensors = (ModbusSensorT *)calloc(2, sizeof(ModbusSensorT));
    err = SensorLoadOne(api, rtu, &rtu->sensors[0], sensorsJ);
    if (err)
      goto OnErrorExit;
  }

  return 0;

OnErrorExit:
  return -1;
}

static int ReadModbusSection(afb_api_t api, CtlHandleT *controller,
                             json_object *configJ, char *key) {
  int err;

  // everything is done during initial config call
  json_object *rtusJ = json_object_object_get(configJ, key);
  if (rtusJ == NULL)
    goto OnErrorExit;

  // modbus array is close with a nullvalue;
  if (json_object_is_type(rtusJ, json_type_array)) {
    int count = (int)json_object_array_length(rtusJ);
    controller->modbus = (ModbusRtuT *)calloc(count + 1, sizeof(ModbusRtuT));

    for (int idx = 0; idx < count; idx++) {
      json_object *rtuJ = json_object_array_get_idx(rtusJ, idx);
      err = ModbusLoadOne(api, controller, idx, rtuJ);
      if (err)
        goto OnErrorExit;
    }

  } else {
    controller->modbus = (ModbusRtuT *)calloc(2, sizeof(ModbusRtuT));
    err = ModbusLoadOne(api, controller, 0, rtusJ);
    if (err)
      goto OnErrorExit;
  }

  // add static controls verbs
  err = CtrlLoadStaticVerbs(api, CtrlApiVerbs, (void *)controller);
  if (err) {
    AFB_API_ERROR(api, "CtrlLoadOneApi fail to Registry static API verbs");
    goto OnErrorExit;
  }

  return 0;

OnErrorExit:
  AFB_API_ERROR(api, "Fail to initialise Modbus section check Json Config");
  return -1;
}

static int ReadGlobalUri(afb_api_t api, CtlHandleT *controller,
                             json_object *configJ, char *key) {
  int err;
  char *global_uri;
  bool key_not_found;
  ModbusContextT *context;

  err = rp_jsonc_unpack(configJ, "{s:s}", "uri", &global_uri);
  key_not_found = !strcmp(rp_jsonc_get_error_string(err), "key not found");
  if (err && !key_not_found) {
    AFB_API_ERROR(api, "ReadGlobalUri: failed to parse global URI in config");
    goto OnErrorExit;
  }

  if (key_not_found) {
    controller->context = NULL;
  } else {
    context = (ModbusContextT *)calloc(1, sizeof(ModbusContextT));
    context->uri = global_uri;

    context->semaphore = malloc(sizeof(sem_t));
    err = sem_init(context->semaphore, 0, 1);
    if (err < 0) {
      AFB_API_ERROR(api, "ReadGlobalUri: failed to init semaphore");
      goto OnErrorExit;
    }

    err = ModbusRtuConnect(api, context, "");
    if (err) {
      AFB_API_ERROR(api, "ReadGlobalUri: fail to connect uri=%s", context->uri);
      goto OnErrorExit;
    }

    controller->context = context;
  }

  return 0;

OnErrorExit:
  return -1;
}

// main binding entry
int AfbApiCtrlCb(afb_api_t rootapi, afb_ctlid_t ctlid, afb_ctlarg_t ctlarg,
                 void *userdata) {
  CtlHandleT *controller = userdata;

  int status = 0;
  switch (ctlid) {
  case afb_ctlid_Root_Entry:
    // should be never happen
    AFB_API_ERROR(rootapi, "Modbus root_entry call after api creation\n");
    goto OnErrorExit;

  // let process modbus config as part of binding config
  case afb_ctlid_Pre_Init:

    // load api (dependencies+verb+event creation)
    status = ctl_set_requires(&controller->metadata, rootapi);
    if (status < 0) {
      AFB_API_ERROR(rootapi,
                    "Modbus mandatory api dependencies not satisfied\n");
      goto OnErrorExit;
    }

    status =
        ctl_actionset_exec(&controller->onstart, rootapi, plugins, controller);
    if (status < 0) {
      AFB_API_ERROR(rootapi, "Modbus fail register sensors actions\n");
      goto OnErrorExit;
    }

    status = ReadGlobalUri(rootapi, controller, controller->config, "uri");
    if (status < 0) {
      AFB_API_ERROR(rootapi, "Modbus failed to setup global URI context");
      goto OnErrorExit;
    }

    // load api (dependencies+verb+event creation)
    status = ReadModbusSection(rootapi, controller, controller->config,
                               "modbus");
    if (status < 0) {
      AFB_API_ERROR(rootapi,
                    "Modbus fail api controller config initialization\n");
      goto OnErrorExit;
    }
    break;

  /** called for init */
  case afb_ctlid_Init:
    break;

  /** called when required classes are ready */
  case afb_ctlid_Class_Ready:
    break;

  /** called when an event is not handled */
  case afb_ctlid_Orphan_Event:
    AFB_API_NOTICE(rootapi, "Modbus received unexpected event:%s\n",
                   ctlarg->orphan_event.name);
    break;

  /** called when shuting down */
  case afb_ctlid_Exiting:
    break;
  }
  return 0;

OnErrorExit:
  AFB_API_ERROR(rootapi, "Modbus fail register sensors actions\n");
  return -1;
}

static int initialize_codecs_of_plugins(void *closure, const plugin_t *plugin)
{
	ModbusFormatCbT *codecs = plugin_get_object(plugin, "modbusFormats");
	if (codecs != NULL)
		mbEncoderRegister(plugin_name(plugin), codecs);
	return 0;
}

/* create one API per config file object */
static CtlHandleT *ReadConfig(afb_api_t rootapi, json_object *configJ) {
  afb_api_t api;

  /* allocates */
  CtlHandleT *controller = calloc(1, sizeof *controller);
  if (controller == NULL) {
    AFB_ERROR("out of memory");
    goto OnErrorExit;
  } else {
    controller->api = rootapi;
    controller->onstart = CTL_ACTIONSET_INITIALIZER;
    controller->onevent = CTL_ACTIONSET_INITIALIZER;
    controller->config = configJ;

    // read controller sections configs (modbus section is read from
    // apicontrolcb after api creation)
    if (ctl_subread_metadata(&controller->metadata, configJ, false) < 0)
      goto OnErrorExit;

    if (ctl_subread_plugins(&plugins, configJ, NULL, "plugins") < 0)
      goto OnErrorExit;

    if (plugin_store_iter(plugins, initialize_codecs_of_plugins, NULL) < 0)
      goto OnErrorExit;

    if (ctl_subread_actionset(&controller->onstart, configJ, "onstart") < 0)
      goto OnErrorExit;

    if (ctl_subread_actionset(&controller->onevent, configJ, "events") < 0)
      goto OnErrorExit;

    // controller api should be created before processing modbus config
    if (afb_create_api(&api, controller->metadata.api,
                       controller->metadata.info, 1, AfbApiCtrlCb,
                       controller) < 0)
      goto OnErrorExit;
  }

  // lock json config in ram
  json_object_get(configJ);
  return controller;

OnErrorExit:
  if (controller != NULL) {
    ctl_actionset_free(&controller->onstart);
    ctl_actionset_free(&controller->onevent);
    free(controller);
  }
  return NULL;
}

// main entry is called right after binding is loaded with dlopen
int afbBindingV4entry(afb_api_t rootapi, afb_ctlid_t ctlid, afb_ctlarg_t ctlarg,
                      void *api_data) {

  if (ctlid != afb_ctlid_Root_Entry)
    goto OnErrorExit;

  // get some relevant config
  json_object *config = ctlarg->root_entry.config;
  if (NULL == json_object_object_get(config, "metadata")) {
    config = afb_api_settings(rootapi);
    if (NULL == json_object_object_get(config, "metadata")) {
      AFB_API_ERROR(rootapi, "No metadata found in configurations.\n");
      goto OnErrorExit;
    }
  }

  // register core encoders
  mbRegisterCoreEncoders();

  // process modbus controller config
  CtlHandleT *controller = ReadConfig(rootapi, config);
  if (!controller) {
    AFB_API_ERROR(rootapi, "Modbus fail to read binding controller config\n");
    goto OnErrorExit;
  }

  return 0;

OnErrorExit:
  return -1;
}
