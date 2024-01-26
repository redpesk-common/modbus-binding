# Installation

## Installing from your distribution repositories

* Declare redpesk repository [(see doc)]({% chapter_link host-configuration-doc.setup-your-build-host %}):

* redpesk: `sudo dnf install modbus-binding afb-ui-devtools`
* Fedora: `sudo dnf install modbus-binding afb-ui-devtools`
* OpenSuse: `sudo zipper install modbus-binding afb-ui-devtools`
* Ubuntu: `sudo apt-get install modbus-binding afb-ui-devtools`

## Rebuilding from source

### Modbus binding dependencies

* Declare redpesk repository [(see doc)]({% chapter_link host-configuration-doc.setup-your-build-host %}):

* From redpesk repos
  * application framework 'afb-binder' & 'afb-binding-devel'
  * binding helpers 'librp-utils-static'
  * binding helpers 'afb-helpers4-static'
  * cmake template 'afb-cmake-modules'
  * ui-devel html5 'afb-ui-devtools'

* From your preferred Linux distribution repos
  * Libmodbus 3.1.6

#### Install LibModbus manually

WARNING: Fedora 33 and many distros ship the old 3.0. This binding uses the 3.1 !!!

```bash
# download from https://libmodbus.org/download/
wget https://libmodbus.org/releases/libmodbus-3.1.6.tar.gz
tar -xzf libmodbus-3.1.6.tar.gz && cd libmodbus-3.1.6/
./configure --libdir=/usr/local/lib64
make && sudo make install-strip
```

Update `conf.d/cmake/00-????-config.cmake` with chosen installation
directory, ex: `set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:/opt/libmodbus-3.1.6/lib64 pkgconfig")`

### Modbus Binding build

```bash
git clone https://github.com/redpesk-industrial/modbus-binding.git
mkdir build && cd build
cmake ..
make
```
