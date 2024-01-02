message(STATUS "Custom options: 00-debian-config.cmake --")

# LibModbus is not part of standard Linux Distro (https://libmodbus.org/)
set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:/usr/local/lib64/pkgconfig")
