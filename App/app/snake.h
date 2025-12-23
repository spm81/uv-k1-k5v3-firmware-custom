/* Copyright 2025 Armel F4HWN
 * Modified for Snake Game
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

#define MAX_SNAKE_LENGTH 100

// โครงสร้างตำแหน่งพิกัดสำหรับงูและอาหาร
typedef struct {
    int8_t x;
    int8_t y;
} Point;

// ฟังก์ชันหลักของเกม
void initSnake(void);
void spawnFood(void);
void moveSnake(void);
void drawSnake(void);

// ต้องใช้ชื่อนี้เพื่อให้ main.c เรียกใช้งานได้ถูกต้อง
void APP_RunSnake(void);