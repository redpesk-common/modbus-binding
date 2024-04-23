# Configuration

## API usage

Modbus binding creates one verb per sensor. By default each sensor verb
is prefixed by the RTU uid. There can be multiple RTUs per configuration
file, the JSON `modbus` value has to be an array of objects instead of
just one object. See [this config](https://github.com/redpesk-industrial/modbus-binding/tree/master/config-samples/example-multiple-rtus-same-link.json)
for an example.

The [config-samples](https://github.com/redpesk-industrial/modbus-binding/tree/master/config-samples) directory contains multiple examples.
[eastron-sdm72d.json](https://github.com/redpesk-industrial/modbus-binding/blob/master/config-samples/eastron-sdm72d.json) uses serial Modbus, the others use Ethernet
Modbus.

A proper working config is actually a binder config. It should have some
metadata as show in the "config schema" section below. You can then add
the Modbus-specific bits in the `binding/modbus` array of the config (if
there's only one, it can be directly an object instead of an array of
one object). There are samples of this object in the sections after
"config schema".

If you want to communicate with multiple RTUs over the same serial
link/device, you have to specify a global URI at the same level as
`metadata` and `modbus` in the JSON config. The URI must not be present
in the configuration section of the RTUs which should use this URI (in
other words, the global URI is used only when no URI is specified at the
RTU level). An example of this use case is available in
[config-samples/example-multiple-rtus-same-link.json](https://github.com/redpesk-industrial/modbus-binding/blob/master/config-samples/example-multiple-rtus-same-link.json).

If you want to communicate over multiple serial links, each having
multiple RTUs, you must run a binding instance per serial link. If other
serial links than the global one are connected to only one RTU, you can
keep specifying the URI in the according RTU configuration.

### modbus-binding config schema

```json
{
  "binding": ["/usr/redpesk/modbus-binding/lib/modbus-binding.so"],
  "set": {
    "modbus-binding.so": {
      "metadata": {
        "uid": "modbus",
        "version": "2.0",
        "api": "modbus",
        "info": "My config name"
      },
      "modbus": {
        SEE SAMPLES BELOW
      }
    }
  }
}
```

### Sample TCP modbus-binding config

```json
"modbus": {
  "uid": "King-Pigeon-myrtu",
  "info": "King Pigeon TCP I/O Module",
  "uri" : "tcp://192.168.1.110:502",
  "privilege": "global RTU required privilege",
  "autostart" : 1, // connect to RTU at binder start
  "prefix": "myrtu", // api verb prefix
  "timeout": xxxx, // optional response timeout in ms
  "debug": 0-3, // option libmodbus debug level
  "period": 100, // default polling for event subscription
  "idle": 0, // force event every <idle> poll even when value does not change
  "sensors": [
    {
      "uid": "PRODUCT_INFO",
      "info" : "Array with Product Number, Lot, Serial, OnlineTime, Hardware, Firmware",
      "function": "Input_Register",
      "format" : "plugin://king-pigeon/devinfo",
      "register" : 26,
      "privilege": "optional sensor required privilege"
    },
    {
      "uid": "DIN01_switch",
      "function": "Coil_input",
      "format" : "BOOL",
      "register" : 1,
      "privilege": "optional sensor required privilege",
      "period": xxx, // special polling period (ms) for this sensor
      "idle": xxx, // force event every <idle> poll when value does not change
    },
    {
      "uid": "DIN01_counter",
      "function": "Register_Holding",
      "format" : "UINT32",
      "register" : 6,
      "privilege": "optional sensor required privilege",
      "period": xxx, // special polling period (ms) for this sensor
    },
...
```

### Sample serial modbus-binding config

```json
"modbus": {
  "uid": "Eastron-SDM72D",
  "info": "Three Phase Four Wire Energy Meter ",
  "uri": "tty://dev/ttyUSB_RS485:19200",
  "prefix": "SDM72D",
  "slaveid": 1,
  "timeout": 250,
  "autostart": 1,
  "privilege": "Eastron:Modbus",
  "period": 100,
  "sensors": [
    {
      "uid": "Volts-L1",
      "register": 0,
      "type": "Register_input",
      "format": "FLOAT_DCBA",
      "sample": [
        { "action": "read" },
        { "action": "subscribe" }
      ]
    },
    {
      "uid": "Volts-L2",
      "register": 2,
      "type": "Register_input",
      "format": "FLOAT_DCBA"
    },
...
```

## Subscription

`modbus-binding` allows subscribing to a sensor. By default, it means
that the binding will read the sensor every 100 milliseconds and send an
event to the subscribed client when the value changes.

This behavior can be tweaked to one's needs in the configuration:

- a sensor can have `period`, `period_s` and/or `period_m` values to
  configure the delay (respectively in milliseconds, seconds and
  minutes) between each read. If multiple of these values are used, they
  are added (so a `period_s` of 2 and a `period` of 1 will result in a
  delay of 2001 milliseconds). `period_s` and `period_m` support integer
  and floating point values.
- a sensor can have an `idle` value to send an event even if the data
  read on the sensor hasn't changed. An `idle` of 1 means "always send
  an event even if the data hasn't changed"; an `idle` of 5 means "send
  an event every 5 reads even if the data hasn't changed". The default
  value is 0 (an even is sent only when the data changes).
- to avoid setting the same values for every single sensor in your
  configuration, you can set default values (for `period`, `period_s`,
  `period_m` and `idle`) at the RTU level.

### Examples

```json
"modbus": {
  "uid": "Eastron-SDM72D",
  "period_m": 1,
  "period_s": 30,
  "idle": 5,
  ...
  "sensors": [
    {
      "uid": "Volts-L1",
      "period": 500,
      ...
    }
  ]
}
```

In this example, we set a default polling period of 1 minute and 30
seconds, and we ask to get an event every 5 polls when the data doesn't
change (we will still get an event every time the data changes).

Then we declare a sensor `Volts-L1` which overrides the default polling
period and uses a period of 500 milliseconds instead. Since it does not
declare an `idle` value, the default `idle` value of 5 set at the RTU
level of the configuration will be used.

## Modbus controller exposed

### Two builtin verb

* `modbus ping`: check if binder is alive
* `modbus info`: return registered MTU

### One introspection verb per declared RTU

* `modbus myrtu/info`

### One action verb per declared Sensor

* `modbus myrtu/din01_switch`
* `modbus myrtu/din01_counter`
* etc…

### For each sensor the API accepts 4 actions

* read (return register(s) value after format decoding)
* write (push value on register(s) after format encoding)
* subscribe (subscribe to sensors value changes, frequency is defined by
  sensor or globally at RTU level)
* unsubscribe (unsubscribe to sensors value changes)

Example: `modbus myrtu/din01_counter {"action": "read"}`

## Encoders

The Modbus binding supports both builtin format converters and optional
custom converters provided by user through plugins.

Standard converters include the traditional INT16, UINT16, INT32,
UINT32, FLOATABCD… Depending on the format one or more register is
read. See [src/modbus-encoder.c](https://github.com/redpesk-industrial/modbus-binding/blob/master/src/modbus-encoder.c)
for the whole detailed list.

### Custom encoders

The user may also add its own encoding/decoding format to
handle device specific representation (ex: device info string), or
custom application encoding (ex: float to uint16 for an analog output).
Custom encoders/decoders are stored within user plugins (see sample at
[src/plugins/kingpigeon-encoder.c](https://github.com/redpesk-industrial/modbus-binding/blob/master/src/plugins/kingpigeon-encoder.c).

Custom converters should export an array of structures named
`modbusFormats`, which describes the provided formats, and which is
terminated with a `NULL` named format.

* uid is the formatter name as declared inside JSON config file.
* decode/encode callbacks are respectively called for read/write actions.
* init callback is called at format registration time and might be
  used to process a special value for a given sensor (e.g. deviation
  for a wind sensor). Each sensor attaches a `void*` context. The
  developer may declare a private context for each sensor (e.g. to
  store a previous value, a min/max…). The init callback receives the
  sensor source to store context and optionally the `args` JSON object
  when present within the sensor's JSON config.

**WARNING:** do not confuse format count and `nbreg`. `nbreg` is the
number of 16 bits registers used for a given formatter (e.g. 4 for a
64 bits float). Count is the number of values you want to read in one
operation (e.g. you may want to read all of your digital inputs in one
operation and receive them as an array of booleans).

```c
// Custom formatter sample (src/plugins/kingpigeon/kingpigeon-encoder.c)
// ---------------------------------------------------------------------
...
#include "modbus-binding.h"
#include <ctl-lib-plugin.h>

CTL_PLUGIN_DECLARE("king_pigeon", "MODBUS plugin for king pigeon");
...
static int decodePigeonInfo(ModbusSourceT *source, ModbusFormatCbT *format, uint16_t *data, uint index, json_object **responseJ) {
...
static int encodePigeonInfo(ModbusSourceT *source, ModbusFormatCbT *format, json_object *sourceJ, uint16_t **response, uint index) {
...
ModbusFormatCbT modbusFormats[] = {
  {
    .uid = "devinfo",
    .info = "return KingPigeon Device Info as an array",
    .nbreg = 6,
    .decodeCB = decodePigeonInfo,
    .encodeCB = encodePigeonInfo
  },
...
  { .uid = NULL } // must be NULL terminated
};
```
