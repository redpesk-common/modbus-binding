message(STATUS "Custom options: 00-opensuse-config.cmake --")

# LibModbus is not part of standard Linux Distro (https://libmodbus.org/)
set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:/usr/local/lib64/pkgconfig")

set(CMAKE_BUILD_TYPE "DEBUG")