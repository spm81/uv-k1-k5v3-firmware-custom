/* Copyright 2023 Dual Tachyon
 * https://github.com/DualTachyon
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

#ifndef DRIVER_GPIO_H
#define DRIVER_GPIO_H

#include <stdint.h>
#include "py32f071_ll_gpio.h"


enum GPIOA_PINS {
    GPIOA_PIN_SPI2_SCK   = 0,
    GPIOA_PIN_SPI2_MOSI  = 1,
    GPIOA_PIN_SPI2_MISO  = 2,
    GPIOA_PIN_SPI2_CS    = 3,

    GPIOA_PIN_ST7565_A0  = 6,

    GPIOA_PIN_AUDIO_PATH = 8,

    GPIOA_PIN_SWD_IO     = 13,
    GPIOA_PIN_SWD_CLK    = 14,
};

enum GPIOB_PINS {
    GPIOB_PIN_KEYBOARD_0 = 15,
    GPIOB_PIN_KEYBOARD_1 = 14,
    GPIOB_PIN_KEYBOARD_2 = 13,
    GPIOB_PIN_KEYBOARD_3 = 12,
    GPIOB_PIN_KEYBOARD_4 = 6,
    GPIOB_PIN_KEYBOARD_5 = 5,
    GPIOB_PIN_KEYBOARD_6 = 4,
    GPIOB_PIN_KEYBOARD_7 = 3,

    GPIOB_PIN_PTT        = 10,

    GPIOB_PIN_BK1080     = 15,

    GPIOB_PIN_BK4819_SCL = 8,
    GPIOB_PIN_BK4819_SDA = 9,
};

enum GPIOC_PINS {
    GPIOC_PIN_FLASHLIGHT = 13,
};

enum GPIOF_PINS {
    GPIOF_PIN_I2C_SCL    = 5,
    GPIOF_PIN_I2C_SDA    = 6,

    GPIOF_PIN_BACKLIGHT  = 8,

    GPIOF_PIN_BK4819_SCN = 9,
};

// Convert pin ID to pin mask
#define GPIO_PIN_MASK(id)    (1u << (id))

static inline void GPIO_SetAudioPath() {
    LL_GPIO_SetOutputPin(GPIOA, GPIO_PIN_MASK(GPIOA_PIN_AUDIO_PATH));
}

static inline void GPIO_ResetAudioPath() {
    LL_GPIO_ResetOutputPin(GPIOA, GPIO_PIN_MASK(GPIOA_PIN_AUDIO_PATH));
}

static inline void GPIO_SetBacklight() {
    LL_GPIO_SetOutputPin(GPIOF, GPIO_PIN_MASK(GPIOF_PIN_BACKLIGHT));
}

static inline void GPIO_ResetBacklight() {
    LL_GPIO_ResetOutputPin(GPIOF, GPIO_PIN_MASK(GPIOF_PIN_BACKLIGHT));
}

#endif

