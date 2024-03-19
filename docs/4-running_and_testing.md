# Running/Testing

By default Kingpigeon devices uses a fixed IP address (192.168.1.110).
You may need to add this network to your own desktop config before
starting your test. Check hardware documentation on
[Device Store / King Pigeon]({% chapter_link devices-store-doc.king-pigeon %})

```bash
sudo ip a add 192.168.1.1/24 dev eth0 # eth0, enp0s20f0u4u1 or whatever is your ethernet card name
ping 192.168.1.110 # check you can ping your TCP modbus device
# check default config with browser at http://192.168.1.110
```

## Start sample Binder

```bash
afb-binder --name=afb-kingpigeon --port=1234  --ldpaths=src --workdir=. --verbose
```

Open the binder devtool with your browser at <http://localhost:1234/devtools/index.html>

![afb-ui-devtool modbus Screenshot](assets/afb-ui-devtool_modbus_Screenshot.png)

## Test binding in CLI

```bash
afb-client --human ws://localhost:1234/api
# you can now send requests with the following syntax : <api> <verb> [eventual data in json format]
# here are some available examples for modbus binding :
modbus ping
modbus info
modbus RTU0/D01_SWITCH {"action":"write","data":1}
modbus RTU0/D01_SWITCH {"action":"read"}
modbus RTU0/D01_SWITCH {"action":"write","data":0}
modbus RTU0/D01_SWITCH {"action":"read"}
```

## About timeouts

A request which has timed out will have a status code of `-1001`.

A time out can be a symptom of multiple causes:

- the RTU you're using is slower than the default timeout value we use
  in configurations (250ms). Try to increase this value to something
  which fits your RTU better.
- you are sending a Modbus command which is not supported by the RTU,
  and it simply does not reply (instead of sending an "Illegal function"
  error for instance). You can use `interceptty` to debug your problem,
  as described in [Configuration](./3-configuration.html).

## Adding your own config

The JSON config file is selected with the
[binder options]({% chapter_link afb_binder.options-of-afb-binder %})
(see `--config=configpath` or `--binding=bindingpath:configpath`).

```bash
afb-binder --binding=/usr/redpesk/modbus-binding/lib/afb-modbus.so:/usr/redpesk/modbus-binding/etc/myconfig.json
# or if you specify the binding to use in the config file
afb-binder --config=/usr/redpesk/modbus-binding/etc/myconfig.json
# or a mix of both
afb-binder --binding=/usr/redpesk/modbus-binding/lib/afb-modbus.so --config=/usr/redpesk/modbus-binding/etc/myconfig.json
```

**Warning:** some TCP Modbus devices, as KingPigeon's, check SlaveID
even for building I/O. Generic config make the assumption that your
slaveID is set to `1`.
