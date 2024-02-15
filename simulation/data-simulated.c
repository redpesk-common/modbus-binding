/*
 * Copyright (C) 2021 - 2024 "IoT.bzh"
 *
 * Author: Valentin Lefebvre <valentin.lefebvre@iot.bzh
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdint.h>
uint16_t modbus_simu_data_static[115] = {
0b0010000000000000,
0b0001000000000000,
0b0000100000000000,
0b0000010000000000,
0b0000001000000000,
(uint16_t) 3,   // Consommation Cumulée MP1 (L)
(uint16_t) 2,   // Consommation Cumulée MP2 (L)
(uint16_t) 5,   // Consommation Cumulée MP1 + MP2 (L)
(uint16_t) 2,   // Consommation Instant MP1 (L/H)
(uint16_t) 1,   // Consommation Instant MP2 (L/H)
(uint16_t) 3,   // Consommation Instant MP1 + MP2 (L/H)
(uint16_t) 32,  // Temperature Carburant MP1 IN (°C)
(uint16_t) 41,  // Temperature Carburant MP1 OUT (°C)
(uint16_t) 30,  // Temperature Carburant MP2 IN (°C)
(uint16_t) 53,  // Temperature Carburant MP2 OUT (°C)
(uint16_t) 1025,// Pression air machine (Bar)
(uint16_t) 45,  // Temperature machine (°C)
(uint16_t) 20,  // Humidity machine (%)
(uint16_t) 69,  // Emission de NOx MP1 (PPM)
(uint16_t) 21,  // Emission de o2 MP1 (%)
(uint16_t) 74,  // Emission de NOx MP2 (PPM)
(uint16_t) 24,  // Emission de o2 MP2 (%)
(uint16_t) 0,
(uint16_t) (86400 >> 16)&0xffff,   // Oil pump operation time (TIME hours, minutes, secondes)
(uint16_t) (86400 & 0xffff),// Oil pump operation time (TIME hours, minutes, secondes)
(uint16_t) 0,   // fuel centrifuge operation time (TIME hours, minutes, secondes)
(uint16_t) 0,   // fuel centrifuge operation time (TIME hours, minutes, secondes)
(uint16_t) 0,   // dewatering pump N1 operation time (TIME hours, minutes, secondes)
(uint16_t) 0,   // dewatering pump N1 operation time (TIME hours, minutes, secondes)
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
(uint16_t) 0,
};
