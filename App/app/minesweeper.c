#include "app/minesweeper.h"
#include "app/breakout.h" // ยืมฟังก์ชันวาดภาพ/เสียง

#ifdef ENABLE_FEAT_F4HWN_SCREENSHOT
#include "screenshot.h"
#endif

// --- Bitmaps (8x8 pixel) ---
const uint8_t BMP_FLAG[] = { 0x00, 0x7E, 0x7E, 0x0A, 0x0A, 0x4, 0x00, 0x00 };
//const uint8_t BMP_MINE[] = { 0x24, 0x5A, 0xFF, 0xFF, 0xFF, 0x5A, 0x24, 0x00 };
const uint8_t BMP_MINE[] = { 0xa5, 0x7e, 0x3c, 0xff, 0x3c, 0x7e, 0xa5, 0x00 };
const uint8_t BMP_HIDDEN[] = { 0x3C, 0x42, 0x81, 0x81, 0x81, 0x81, 0x42, 0x3C };

// --- Variables ---
static bool isInitialized = false;
static bool isGameOver = false;
static bool isWin = false;
static uint32_t randSeed = 1234;

static char str[12];

static Cell board[MINE_COLS][MINE_ROWS];
static int cursorX = 0;
static int cursorY = 0;
static int minesLeft = TOTAL_MINES;

// --- Helpers ---
static void srand_custom(uint32_t seed) { randSeed = seed; }
static int rand_custom(void) {
    randSeed = randSeed * 1103515245 + 12345;
    return (randSeed >> 16) & 0x7FFF;
}
static int randInt(int min, int max) {
    return min + (rand_custom() % (max - min + 1));
}

// --- Game Logic ---
static void Reveal(int x, int y) {
    if (x < 0 || x >= MINE_COLS || y < 0 || y >= MINE_ROWS) return;
    if (board[x][y].isVisible || board[x][y].isFlagged) return;

    board[x][y].isVisible = true;

    if (board[x][y].neighborMines == 0 && !board[x][y].isMine) {
        for (int dx = -1; dx <= 1; dx++) {
            for (int dy = -1; dy <= 1; dy++) {
                if(dx != 0 || dy != 0) Reveal(x + dx, y + dy);
            }
        }
    }
}

static void InitGame() {
    isGameOver = false;
    isWin = false;
    minesLeft = TOTAL_MINES;
    cursorX = MINE_COLS / 2;
    cursorY = MINE_ROWS / 2;

    // Clear Board
    for(int x=0; x<MINE_COLS; x++) {
        for(int y=0; y<MINE_ROWS; y++) {
            board[x][y].isMine = false;
            board[x][y].isVisible = false;
            board[x][y].isFlagged = false;
            board[x][y].neighborMines = 0;
        }
    }

    // Place Mines
    int placed = 0;
    while(placed < TOTAL_MINES) {
        int rx = randInt(0, MINE_COLS - 1);
        int ry = randInt(0, MINE_ROWS - 1);
        if(!board[rx][ry].isMine) {
            board[rx][ry].isMine = true;
            placed++;
        }
    }

    // Calc Numbers
    for(int x=0; x<MINE_COLS; x++) {
        for(int y=0; y<MINE_ROWS; y++) {
            if(!board[x][y].isMine) {
                int count = 0;
                for (int dx = -1; dx <= 1; dx++) {
                    for (int dy = -1; dy <= 1; dy++) {
                        int nx = x + dx;
                        int ny = y + dy;
                        if(nx >= 0 && nx < MINE_COLS && ny >= 0 && ny < MINE_ROWS) {
                            if(board[nx][ny].isMine) count++;
                        }
                    }
                }
                board[x][y].neighborMines = count;
            }
        }
    }
}

static void CheckWin() {
    int covered = 0;
    for(int x=0; x<MINE_COLS; x++) {
        for(int y=0; y<MINE_ROWS; y++) {
            if(!board[x][y].isVisible) covered++;
        }
    }
    if(covered == TOTAL_MINES) {
        isWin = true;
        isGameOver = true;
        BK4819_PlayTone(1000, true);
        SYSTEM_DelayMs(500);
        BK4819_EnterTxMute();
    }
}

static void DrawBitmap(int x, int y, const uint8_t *bitmap) {
    for(int i=0; i<8; i++) {
        for(int j=0; j<8; j++) {
            if(bitmap[i] & (1 << j)) {
                UI_DrawPixelBuffer(gFrameBuffer, x+i, y+j, true);
            }
        }
    }
}

static void drawScore()
{
    memset(gStatusLine,  0, sizeof(gStatusLine));
    GUI_DisplaySmallest("MINE SWEEPER", 0, 1, true, true);
    sprintf(str, "Mines: %d", minesLeft);
    GUI_DisplaySmallest(str, 88, 1, true, true);
}

static void Draw() {
    UI_DisplayClear();

    // วาดตาราง
    for(int x=0; x<MINE_COLS; x++) {
        for(int y=0; y<MINE_ROWS; y++) {
            int px = x * 8;
            int py = y * 8; // (ใช้สูตรที่คุณแก้มา ถูกต้องแล้วครับ)

            // วาด Cursor
            if(x == cursorX && y == cursorY) {
                UI_DrawRectangleBuffer(gFrameBuffer, px, py, px+8, py+8, true);
            }

            if(board[x][y].isFlagged) {
                DrawBitmap(px, py, BMP_FLAG);
            } 
            else if(!board[x][y].isVisible) {
                // ยังไม่เปิด
                UI_DrawPixelBuffer(gFrameBuffer, px+3, py+3, true); 
                UI_DrawPixelBuffer(gFrameBuffer, px+4, py+3, true); 
                UI_DrawPixelBuffer(gFrameBuffer, px+3, py+4, true); 
                UI_DrawPixelBuffer(gFrameBuffer, px+4, py+4, true); 
            } 
            else {
                // เปิดแล้ว
                if(board[x][y].isMine) {
                    DrawBitmap(px, py, BMP_MINE);
                } else if(board[x][y].neighborMines > 0) {
                    char s[2];
                    sprintf(s, "%d", board[x][y].neighborMines);
                    GUI_DisplaySmallest(s, px+2, py+1, false, true); 
                }
            }
        }
    }

    // วาด Header (ย้ายมาด้านล่างสุด y=57)
    //char str[20];
    if(isGameOver) {
        //if(isWin) sprintf(str, "YOU WIN!");
		if(isWin) UI_PrintStringSmallBold("YOU WIN!", 32, 25, 3);
        //else sprintf(str, "GAME OVER");
		else UI_PrintStringSmallBold("GAME OVER", 32, 25, 3);
    } else {
        //sprintf(str, "Mines: %d", minesLeft);
		drawScore();
    }
    // แสดงผลที่บรรทัดล่างสุด (Pixel ที่ 57-63)
    //GUI_DisplaySmallest(str, 40, 0, true, true);

    ST7565_BlitFullScreen();
}

// --- Main Loop ---
void APP_RunMinesweeper(void) {
    srand_custom(BK4819_ReadRegister(BK4819_REG_67));
    
    // --- จุดสำคัญ: ล้างขยะหน้าจอและ Status Line ---
    UI_DisplayClear();
    memset(gStatusLine, 0, sizeof(gStatusLine)); // ล้างค่า Text เก่าจาก Breakout
    ST7565_BlitStatusLine(); // สั่งวาด Status Line ที่ว่างเปล่าลงจอทันที
    ST7565_BlitFullScreen();
    // ------------------------------------------

    InitGame();
    isInitialized = true;

    KEY_Code_t lastKey = KEY_INVALID; 

    while(isInitialized) {
        KEY_Code_t key = KEYBOARD_Poll();
        
        if(key != KEY_INVALID && key != lastKey) {
            
            if(key == KEY_EXIT) isInitialized = false;

            if(!isGameOver) {
                if(key == KEY_UP || key == KEY_2)    if(cursorY > 0) cursorY--;
                if(key == KEY_DOWN || key == KEY_8)  if(cursorY < MINE_ROWS-1) cursorY++;
                if(key == KEY_4)                     if(cursorX > 0) cursorX--;
                if(key == KEY_6)                     if(cursorX < MINE_COLS-1) cursorX++;

                if(key == KEY_5 || key == KEY_MENU) {
                    if(!board[cursorX][cursorY].isFlagged) {
                        if(board[cursorX][cursorY].isMine) {
                            board[cursorX][cursorY].isVisible = true;
                            isGameOver = true;
                            BK4819_PlayTone(200, true);
                            for(int x=0; x<MINE_COLS; x++)
                                for(int y=0; y<MINE_ROWS; y++)
                                    if(board[x][y].isMine) board[x][y].isVisible = true;
                        } else {
                            Reveal(cursorX, cursorY);
                            CheckWin();
                            BK4819_PlayTone(800, true);
                            BK4819_EnterTxMute();
                        }
                    }
                }

                if(key == KEY_1 || GPIO_IsPttPressed()) {
                    if(!board[cursorX][cursorY].isVisible) {
                        board[cursorX][cursorY].isFlagged = !board[cursorX][cursorY].isFlagged;
                        if(board[cursorX][cursorY].isFlagged) minesLeft--;
                        else minesLeft++;
                    }
                }
            } else {
                if(key == KEY_5 || key == KEY_MENU) InitGame();
            }

            lastKey = key;
        } 
        else if (key == KEY_INVALID) {
            lastKey = KEY_INVALID;
        }

        Draw();
        SYSTEM_DelayMs(50); 
		
        ST7565_BlitStatusLine();
        ST7565_BlitFullScreen();
    }
}