# Modbus Binding

Modbus binding support TCP Modbus with format conversion for multi-register type as int32, Float, ...

Checkout the documentation from sources on docs folder or on [the redpesk documentation](https://docs.redpesk.bzh/docs/en/master/redpesk-core/modbus/1-architecture_presentation.html).

## Simulation

A simulation has been implemented and will be generated with the command `make`

```bash
$ ./simulation/modbus-simulation -h
This is the usage for the modbus-simulation binary:
    -a : TCP address of the emulated modbus device
    -p : TCP port of the emulated modbus device
    -h : Helper (print this)
    
example: modbus-simulation -a 127.0.0.1 -p 2000
```
