###########################################################################
# Copyright 2022 IoT.bzh
#
# Author: Valentin Lefebvre <valentin.lefebvre@iot.bzh>
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

# Add target to project dependency list
PROJECT_TARGET_ADD(modbus-simulation)

    # Define project Sources
    AUX_SOURCE_DIRECTORY(${CMAKE_CURRENT_SOURCE_DIR} SRC_SIMULATION)

    # Add the executable to the project
    ADD_EXECUTABLE(${TARGET_NAME} ${SRC_SIMULATION})

    # Link libraries
    TARGET_LINK_LIBRARIES(${TARGET_NAME} PUBLIC pthread)

    # Set the executable properties
    INSTALL(TARGETS ${TARGET_NAME})
