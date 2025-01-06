###########################################################################
# Copyright (C) 2015-2025 IoT.bzh Company
#
# Author: Fulup Ar Foll <fulup@iot.bzh>
#          Louis-Baptiste Sobolewski <lb.sobolewski@iot.bzh>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
###########################################################################

CMAKE_MINIMUM_REQUIRED(VERSION 3.6)

# Project info
project(modbus-binding
    VERSION 2.1.1
    DESCRIPTION "Provide a Modbus binding support TCP Modbus with format conversion for multi-register"
    HOMEPAGE_URL "https://github.com/redpesk-industrial/modbus-binding"
    LANGUAGES C
)
set(PROJECT_ICON "icon.png")
set(PROJECT_AUTHOR "Iot-Team")
set(PROJECT_AUTHOR_MAIL "secretaria@iot.bzh")
set(PROJECT_LICENSE "APL2.0")

include(GNUInstallDirs)
include(FindPkgConfig)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Declare options
set(AFM_APP_DIR ${CMAKE_INSTALL_PREFIX}/redpesk CACHE PATH "Applications directory")
set(APP_DIR ${AFM_APP_DIR}/${PROJECT_NAME})

# Check dependencies
pkg_check_modules(deps REQUIRED
	json-c
	afb-binding
	librp-utils-static
	afb-helpers4-static>=10.0.5
	libmodbus>=3.1.6
)
include_directories(AFTER ${deps_INCLUDE_DIRS})

# Build modbus-binding
add_library(modbus-binding SHARED src/modbus-binding.c src/modbus-encoder.c src/modbus-glue.c)
set_target_properties(modbus-binding PROPERTIES PREFIX "")
target_link_libraries(modbus-binding PRIVATE ${deps_LIBRARIES} Threads::Threads)
pkg_get_variable(vscript afb-binding version_script)
if(vscript)
    target_link_options(modbus-binding PRIVATE -Wl,--version-script=${vscript})
endif(vscript)
configure_file(modbus.pc.in ${CMAKE_BINARY_DIR}/modbus.pc @ONLY)
configure_file(manifest.yml.in ${CMAKE_BINARY_DIR}/manifest.yml @ONLY)
# Install modbus-binding
install(TARGETS modbus-binding DESTINATION ${APP_DIR}/lib)
install(FILES ${CMAKE_BINARY_DIR}/manifest.yml DESTINATION ${APP_DIR}/.rpconfig)
install(FILES ${CMAKE_BINARY_DIR}/modbus.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
install(FILES src/modbus-binding.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY config-samples/ DESTINATION ${APP_DIR}/etc)

# Build & install plugins
macro(add_plugin name)
	add_library(${name} SHARED ${ARGN})
	set_target_properties(${name} PROPERTIES PREFIX "")
	target_include_directories(${name} PRIVATE ${deps_INCLUDE_DIRS} src)
	target_link_libraries(${name} PRIVATE ${deps_LIBRARIES} Threads::Threads)
	install(TARGETS ${name} DESTINATION ${APP_DIR}/lib/plugins)
endmacro()

add_plugin(kingpigeon           src/plugins/kingpigeon-encoder.c)
add_plugin(raymarine-anenometer src/plugins/raymarine-anenometer.c)
add_plugin(r4dcb08-temperature  src/plugins/r4dcb08-temperature.c)
target_link_libraries(raymarine-anenometer PRIVATE m)

# Build & install simulation
add_executable(modbus-simulation simulation/simulation.c simulation/data-simulated.c)
target_link_libraries(modbus-simulation PUBLIC pthread)
install(TARGETS modbus-simulation DESTINATION ${CMAKE_INSTALL_BINDIR})
