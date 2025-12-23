#pragma once

#include <stdbool.h>
#include <stdint.h>

// กำหนดขนาดตาราง (16 คอลัมน์ x 7 แถว)
// ขนาดช่องละ 8x8 พิกเซล
#define MINE_COLS 16
#define MINE_ROWS 7
#define TOTAL_MINES 15

typedef struct {
    bool isMine;
    bool isVisible;
    bool isFlagged;
    uint8_t neighborMines;
} Cell;

void APP_RunMinesweeper(void);