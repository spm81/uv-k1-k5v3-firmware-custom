/* Copyright 2025 Armel F4HWN
 * Modified for Snake Game
 */

#include "app/snake.h"     
#include "app/breakout.h" 

#ifdef ENABLE_FEAT_F4HWN_SCREENSHOT
#include "screenshot.h"
#endif

// --- Global Variables ---
static uint32_t randSeed = 1;

static bool isInitialized = false;
static bool isPaused = false;
static bool isBeep = false;
static bool isGameOver = false;

// ตัวแปรใหม่! เอาไว้กันกดรัว
static bool allowChangeDir = true; 

static uint16_t tone = 0;
static uint16_t score = 0;

static char str[12];

// --- Snake Specific Variables ---
Point snake[MAX_SNAKE_LENGTH];
uint8_t snakeLength;
Point food;
int8_t dx, dy; 

// --- Helper Functions ---
static void srand_custom(uint32_t seed) {
    randSeed = seed;
}

static int rand_custom(void) {
    randSeed = randSeed * 1103515245 + 12345;
    return (randSeed >> 16) & 0x7FFF;
}

static int randInt(int min, int max) {
    return min + (rand_custom() % (max - min + 1));
}

static void playBeep(uint16_t tone_freq)
{
    BK4819_PlayTone(tone_freq, true);
    AUDIO_AudioPathOn();
    BK4819_ExitTxMute();
    SYSTEM_DelayMs(100);
    BK4819_EnterTxMute();
    AUDIO_AudioPathOff();
}

static void drawScore()
{
    memset(gStatusLine,  0, sizeof(gStatusLine));
    GUI_DisplaySmallest("SNAKE", 0, 1, true, true);
    sprintf(str, "Score %04u", score);
    GUI_DisplaySmallest(str, 88, 1, true, true);
}

// --- Input Handling ---

static KEY_Code_t GetKey()
{
    KEY_Code_t btn = KEYBOARD_Poll();
    if (btn == KEY_INVALID && GPIO_IsPttPressed())
    {
        btn = KEY_PTT;
    }
    return btn;
}

void handleSnakeInput(uint8_t key) {
    // 1. ปุ่มเมนูและออก ให้ทำงานได้ตลอด ไม่ต้องรอรอบ
    if (key == KEY_MENU) {
        isPaused = !isPaused; 
        if(isPaused) UI_PrintStringSmallBold("PAUSE", 0, 128, 4);
        return;
    }
    if (key == KEY_EXIT) {
        isInitialized = false;
        return;
    }

    // 2. ถ้ากดเปลี่ยนทิศทางไปแล้วในรอบนี้ ให้ "ล็อก" ห้ามเปลี่ยนอีกจนกว่างูจะขยับ
    if (!allowChangeDir) return;

    int8_t old_dx = dx;
    int8_t old_dy = dy;
    bool dirChanged = false; // ตัวเช็คว่ามีการเปลี่ยนทิศจริงไหม

    switch (key) {
        // --- การควบคุมแบบสัมพัทธ์ (Relative) ---
        case KEY_UP: // เลี้ยวซ้ายของงู
            dx = old_dy;
            dy = -old_dx;
            dirChanged = true;
            break;

        case KEY_DOWN: // เลี้ยวขวาของงู
            dx = -old_dy;
            dy = old_dx;
            dirChanged = true;
            break;

        // --- การควบคุมแบบสัมบูรณ์ (Absolute) ---
        case KEY_2: if (dy == 0) { dx = 0; dy = -2; dirChanged = true; } break;
        case KEY_8: if (dy == 0) { dx = 0; dy = 2; dirChanged = true; } break;
        case KEY_4: if (dx == 0) { dx = -2; dy = 0; dirChanged = true; } break;
        case KEY_6: if (dx == 0) { dx = 2; dy = 0; dirChanged = true; } break;
    }

    // 3. ถ้าทิศทางเปลี่ยนจริง ให้ล็อกปุ่มทันที
    if (dirChanged) {
        allowChangeDir = false;
    }
}

// --- Game Logic ---

void spawnFood() {
    food.x = randInt(0, 120); 
    if(food.x % 2 != 0) food.x += 1; 
    food.y = randInt(10, 50);
    if(food.y % 2 != 0) food.y += 1;
}

void initSnake() {
    snakeLength = 5;
    snake[0] = (Point){64, 32};
    snake[1] = (Point){62, 32};
    snake[2] = (Point){60, 32};
    snake[3] = (Point){58, 32};
    snake[4] = (Point){56, 32};
    dx = 2; dy = 0;
    spawnFood();
    isGameOver = false;
    score = 0;
    allowChangeDir = true; // เริ่มเกมมาอนุญาตให้กดได้
}

void moveSnake() {
    // Move body
    for (int i = snakeLength - 1; i > 0; i--) {
        snake[i] = snake[i - 1];
    }
    
    // Move head
    snake[0].x += dx;
    snake[0].y += dy;

    // Check Wall Collision
    if (snake[0].x < 0 || snake[0].x > 126 || snake[0].y < 0 || snake[0].y > 62) {
        isGameOver = true;
        isBeep = true;
        tone = 800; 
    }

    // Check Self Collision
    for(int i = 1; i < snakeLength; i++) {
        if(snake[0].x == snake[i].x && snake[0].y == snake[i].y) {
            isGameOver = true;
            isBeep = true;
            tone = 800;
        }
    }

    // Check Food
    if (abs(snake[0].x - food.x) < 3 && abs(snake[0].y - food.y) < 3) {
        BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, true); // เปิดไฟ
        if (snakeLength < MAX_SNAKE_LENGTH) snakeLength++;
        score += 1;
        isBeep = true;
        tone = 400; 
        spawnFood();
    }
    
    // 4. สำคัญมาก! พองูขยับเสร็จแล้ว ปลดล็อกให้กดปุ่มได้ใหม่
    allowChangeDir = true;
}

void drawSnake() {
    UI_DisplayClear();
    UI_DrawRectangleBuffer(gFrameBuffer, food.x, food.y, food.x + 2, food.y + 2, true);
    for (int i = 0; i < snakeLength; i++) {
        UI_DrawRectangleBuffer(gFrameBuffer, snake[i].x, snake[i].y, snake[i].x + 1, snake[i].y + 1, true);
    }
}

// --- Main Loop ---
void APP_RunSnake(void) {
    static uint8_t frameSkip = 0;
    
    // ตัวแปรใหม่! เอาไว้จำว่าปุ่มที่แล้วคือกดอะไรค้างไว้
    KEY_Code_t lastKey = KEY_INVALID; 
    
    srand_custom(BK4819_ReadRegister(BK4819_REG_67) & 0x01FF * gBatteryVoltageAverage * gEeprom.VfoInfo[0].pRX->Frequency);
    BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, false);
    
    initSnake();
    isInitialized = true;
    isPaused = false;

    while(isInitialized)
    {
        // 1. อ่านค่าปุ่มปัจจุบัน
        KEY_Code_t currentKey = GetKey();

        // 2. เช็คเงื่อนไข: 
        //    - ต้องมีการกดปุ่ม (ไม่ใช่ KEY_INVALID)
        //    - และ **ต้องไม่ซ้ำกับปุ่มที่กดค้างไว้** (currentKey != lastKey)
        if(currentKey != KEY_INVALID && currentKey != lastKey) {
             handleSnakeInput(currentKey);
             
             // จำค่าไว้ ว่าปุ่มนี้ถูกใช้งานแล้ว (กันกดเบิ้ลจากการกดค้าง)
             lastKey = currentKey; 
        }
        else if (currentKey == KEY_INVALID) {
             // ถ้าปล่อยมือแล้ว ให้เคลียร์ค่าจำ เพื่อให้กดปุ่มเดิมซ้ำได้ในครั้งต่อไป
             lastKey = KEY_INVALID;
        }

        if(!isGameOver && !isPaused)
        {
            if(frameSkip == 0)
            {
                moveSnake();
            }
            int speed = 4 - (score / 10); 
            if(speed < 1) speed = 1;
            
            frameSkip = (frameSkip + 1) % speed;

            drawScore();
            drawSnake();
            
            if(isBeep)
            {
                playBeep(tone);
                BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, false);
                isBeep = false;
            }
            else
            {
                SYSTEM_DelayMs(30); 
            }
        }
        else if (isGameOver) 
        {
             UI_PrintStringSmallBold("GAME", 32, 25, 4);
             UI_PrintStringSmallBold("OVER", 66, 25, 4);
             drawScore();
             
             // ตรงนี้ต้องแก้ให้ใช้ currentKey แทน key เดิมที่ไม่มีใน scope นี้
             if(currentKey == KEY_MENU || currentKey == KEY_EXIT) {
                 initSnake();
                 if(currentKey == KEY_EXIT) isInitialized = false;
                 lastKey = KEY_INVALID; // รีเซ็ตปุ่มด้วยเมื่อเริ่มใหม่
             }
        }

        ST7565_BlitStatusLine();
        ST7565_BlitFullScreen();
    }
}