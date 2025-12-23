#include "app/matrix.h"
#include "driver/st7565.h"
#include "driver/keyboard.h"
#include "driver/system.h"
#include "ui/helper.h"

// ตัด stdlib.h ออก เพื่อแก้ปัญหา Linker Error
// #include <stdlib.h> 

// --- Config ---
#define SCREEN_W 128
#define SCREEN_H 52
#define COL_WIDTH 8
#define NUM_COLS (SCREEN_W / COL_WIDTH)

static int drops[NUM_COLS];

// --- Custom Random Generator ---
// สูตร LCG (Linear Congruential Generator) แบบง่าย
// ใช้แทน rand() ของระบบ เพื่อประหยัดที่และแก้ Error
static unsigned long int next = 123456789;

static int SimpleRand(void) {
    next = next * 1103515245 + 12345;
    return (unsigned int)(next / 65536) % 32768;
}

// ฟังก์ชันสุ่มตัวเลข 0 หรือ 1
static char GetRandomChar() {
    return (SimpleRand() % 2) ? '1' : '0';
}

void APP_RunMatrix(void) {
    // 1. Init
    for(int i=0; i<NUM_COLS; i++) {
        drops[i] = (SimpleRand() % 64) - 64; 
    }

    bool running = true;
    
    // กันเด้ง
    while(KEYBOARD_Poll() != KEY_INVALID) SYSTEM_DelayMs(10);

    while(running) {
        if(KEYBOARD_Poll() != KEY_INVALID) {
            running = false;
        }

        UI_DisplayClear();

        // 2. Loop วาด
        for(int i=0; i<NUM_COLS; i++) {
            int x = i * COL_WIDTH;
            int headY = drops[i];

            // วาดหัว (ใช้ GUI_DisplaySmallest แบบ Bold เพื่อความชัวร์เรื่องตำแหน่ง)
            if(headY >= -8 && headY < SCREEN_H) {
                char c[2] = { GetRandomChar(), '\0' };
                GUI_DisplaySmallest(c, x, headY, false, true); 
            }

            // วาดหาง
            for(int j=1; j<=4; j++) {
                int tailY = headY - (j * 8); 
                if(tailY >= -8 && tailY < SCREEN_H) {
                     char c[2] = { GetRandomChar(), '\0' };
                     // ตัวบาง
                     GUI_DisplaySmallest(c, x, tailY, false, false); 
                }
            }

            // 3. Update Physics
            drops[i] += 3;

            if(drops[i] > SCREEN_H) {
                // ใช้ SimpleRand แทน rand()
                drops[i] = -10 - (SimpleRand() % 40); 
            }
        }

        ST7565_BlitFullScreen();
        SYSTEM_DelayMs(60); 
    }
    
    while(KEYBOARD_Poll() != KEY_INVALID) SYSTEM_DelayMs(10);
}