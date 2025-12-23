/* Copyright 2025 Andrej A (Tunas1337)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 */

#pragma once

#include "keyboard_state.h"

#include "../bitmaps.h"
#include "../board.h"
#include "py32f0xx.h"
#include "../driver/bk4819-regs.h"
#include "../driver/bk4819.h"
#include "../driver/gpio.h"
#include "../driver/keyboard.h"
#include "../driver/st7565.h"
#include "../driver/system.h"
#include "../driver/systick.h"
#include "../external/printf/printf.h"
#include "../font.h"
#include "../helper/battery.h"
#include "../misc.h"
#include "../radio.h"
#include "../settings.h"
#include "../ui/helper.h"
#include "../audio.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

// Screen dimensions
#define MOTO_SCREEN_WIDTH   128
#define MOTO_SCREEN_HEIGHT  56   // Main screen area (excluding status line)

// Ground constants
#define GROUND_Y            24   // Y position of the ground line
#define ROAD_SCROLL_SPEED   2    // Pixels per frame to scroll

// Motorcycle dimensions  
#define MOTO_WIDTH          16
#define MOTO_HEIGHT         12

// Tilt limits (in degrees)
#define TILT_MAX            180  // Maximum tilt angle (can flip fully)

typedef struct {
    int16_t x;          // X position (center of motorcycle)
    int16_t y;          // Y position (bottom of motorcycle)
    int16_t tilt;       // Tilt angle (degrees, negative = left, positive = right)
    int8_t  velocity_y; // Vertical velocity (for future jumping/hills)
    uint8_t wheel_anim; // Wheel animation frame
} Motorcycle;

typedef struct {
    int16_t scroll_offset;  // Current scroll position for road animation
    uint16_t distance;      // Distance traveled (score)
} GameState;

// Main game function
void APP_RunMotorcycle(void);
