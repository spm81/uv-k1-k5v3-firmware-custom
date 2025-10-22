/* Copyright 2025 muzkr https://github.com/muzkr
 * Copyright 2023 Manuel Jinger
 * Copyright 2023 Dual Tachyon
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

#include "driver/gpio.h"
#include "driver/keyboard.h"
#include "driver/systick.h"
#include "driver/i2c.h"
#include "misc.h"

KEY_Code_t gKeyReading0     = KEY_INVALID;
KEY_Code_t gKeyReading1     = KEY_INVALID;
uint16_t   gDebounceCounter = 0;
bool       gWasFKeyPressed  = false;

#define PIN_MASK(n) GPIO_PIN_MASK(GPIOB_PIN_KEYBOARD_##n)

static const uint16_t pin_masks[] = {
    PIN_MASK(0),
    PIN_MASK(1),
    PIN_MASK(2),
    PIN_MASK(3),
    PIN_MASK(4),
    PIN_MASK(5),
    PIN_MASK(6),
    PIN_MASK(7),
};

static const KEY_Code_t keyboard[5][4] = {
    {   // Zero col
        // Set to zero to handle special case of nothing pulled down
        KEY_SIDE1, 
        KEY_SIDE2, 

        // Duplicate to fill the array with valid values
        KEY_INVALID, 
        KEY_INVALID, 
    },
    {   // First col
        KEY_MENU, 
        KEY_1, 
        KEY_4, 
        KEY_7, 
    },
    {   // Second col
        KEY_UP, 
        KEY_2 , 
        KEY_5 , 
        KEY_8 , 
    },
    {   // Third col
        KEY_DOWN, 
        KEY_3   , 
        KEY_6   , 
        KEY_9   , 
    },
    {   // Fourth col
        KEY_EXIT, 
        KEY_STAR, 
        KEY_0   , 
        KEY_F   , 
    }
};

static inline void set_cols()
{
    LL_GPIO_SetOutputPin(GPIOB, pin_masks[4] | pin_masks[5] | pin_masks[6] | pin_masks[7]);
}

static inline void reset_col(uint32_t index)
{
    LL_GPIO_ResetOutputPin(GPIOB, pin_masks[index + 4]);
}

static inline uint32_t read_rows()
{
    return LL_GPIO_ReadInputPort(GPIOB);
}

KEY_Code_t KEYBOARD_Poll(void)
{
    KEY_Code_t Key = KEY_INVALID;

    //  if (!GPIO_CheckBit(&GPIOC->DATA, GPIOC_PIN_PTT))
    //      return KEY_PTT;

    // *****************

    for (unsigned int j = 0; j < 5; j++)
    {
        uint16_t reg;
        unsigned int i;
        unsigned int k;

        // Set all high
        set_cols();

        // Clear the pin we are selecting
        if (j > 0)
            reset_col(j - 1);

        // Read all 4 GPIO pins at once .. with de-noise, max of 8 sample loops
        for (i = 0, k = 0, reg = 0; i < 3 && k < 8; i++, k++)
        {
            SYSTICK_DelayUs(1);
            uint16_t reg2 = (uint16_t) read_rows();
            i *= reg == reg2;
            reg = reg2;
        }

        if (i < 3)
            break; // noise is too bad

        for (unsigned int i = 0; i < 4; i++)
        {
            const uint16_t mask = pin_masks[i];
            if (!(reg & mask))
            {
                Key = keyboard[j][i];
                break;
            }
        }

        if (Key != KEY_INVALID)
            break;
    }

    return Key;
}
