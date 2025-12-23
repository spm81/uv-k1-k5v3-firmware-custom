#include "app/rtttl.h"
#include "driver/st7565.h"
#include "driver/keyboard.h"
#include "driver/system.h"
#include "driver/bk4819.h"
#include "ui/helper.h"
#include "settings.h"
#include "../audio.h" 
#include <string.h>
// [แก้ไข] เอา stdio.h ออก เพื่อแก้ปัญหา undefined reference to _sbrk

// --- Config ---
#define ITEMS_PER_PAGE 4

// ฟังก์ชันตรวจสอบตัวเลข
static bool IsDigit(char c) {
    return (c >= '0' && c <= '9');
}

// รายชื่อเพลง RTTTL (10 เพลง)
static const char* song_list[] = {
    // 2. Super Mario (Full Loop)
    "Super Mario:d=4,o=5,b=100:16e6,16e6,32p,8e6,16c6,8e6,8g6,8p,8g,8p,8c6,16p,8g,16p,8e,16p,8a,8b,16a#,8a,16g.,16e6,16g6,8a6,16f6,8g6,8e6,16c6,16d6,8b,16p,8c6,16p,8g,16p,8e,16p,8a,8b,16a#,8a,16g.,16e6,16g6,8a6,16f6,8g6,8e6,16c6,16d6,8b,8p,16g6,16f#6,16f6,16d#6,16e6,8p,16g#,16a,16c6,16p,16a,16c6,16d6,8p,16g6,16f#6,16f6,16d#6,16e6,8p,16c7,16p,16c7,16c7,p,16g6,16f#6,16f6,16d#6,16e6,8p,16g#,16a,16c6,16p,16a,16c6,16d6,8p,16d#6,8p,16d6,8p,16c6",
    
    // 3. Imperial March (Darth Vader)
    "Imperial March:d=4,o=5,b=104:4g,4g,4g,8d#.,16a#,4g,8d#.,16a#,2g,4d6,4d6,4d6,8d#6.,16a#,4f#,8d#.,16a#,2g,4g6,8g.,16g,4g6,8f#6,16f6,16e6,16d#6,8e6,8p,8g#,4c#6,8c6,16b,16a#,16a,8a#,8p,8d#,4f#,8d#,4g,8a#,16g,2d6",
    
    // 4. Star Wars Main Title
    "SW Main Theme:d=4,o=5,b=108:8g4,8g4,8g4,2c6,2g6,8f6,8e6,8d6,2c7,4g6,8f6,8e6,8d6,2c7,4g6,8f6,8e6,8f6,2d6,8g4,8g4,8g4,2c6,2g6,8f6,8e6,8d6,2c7,4g6,8f6,8e6,8d6,2c7,4g6,8f6,8e6,8f6,2d6",

    // 5. Nokia Tune
    "Nokia Tune:d=4,o=5,b=160:8e6,8d6,4f#,4g#,8c#6,8b,4d,4e,8b,8a,4c#,4e,2a",
	
    // 9. Tetris (Korobeiniki)
    "Tetris:d=4,o=5,b=160:e6,8b,8c6,8d6,16e6,16d6,8c6,8b,a,8a,8c6,e6,8d6,8c6,b,8b,8c6,d6,e6,c6,a,2a,8p,d6,8f6,a6,8g6,8f6,e6,8c6,e6,8d6,8c6,b,8b,8c6,d6,e6,c6,a,2a",
	
	"The Simpsons:d=4,o=5,b=160:c.6,e6,f#6,8a6,g.6,e6,c6,8a,8f#,8f#,8f#,2g,8p,8p,8f#,8f#,8f#,8g,a#.,8c6,8c6,8c6,c6",
	
	"Mission Impossible:d=16,o=6,b=95:32d,32d#,32d,32d#,32d,32d#,32d,32d#,32d,32d,32d#,32e,32f,32f#,32g,g,8p,g,8p,a#,p,c7,p,g,8p,g,8p,f,p,f#,p,g,8p,g,8p,a#,p,c7,p,g,8p,g,8p,f,p,f#,p,a#,g,2d,32p,a#,g,2c#,32p,a#,g,2c,a#5,8c,2p,32p,a#5,g5,2f#,32p,a#5,g5,2f,32p,a#5,g5,2e,d#,8d",
	"Flinstones:d=4,o=5,b=40:32p,16f6,16a#,16a#6,32g6,16f6,16a#.,16f6,32d#6,32d6,32d6,32d#6,32f6,16a#,16c6,d6,16f6,16a#.,16a#6,32g6,16f6,16a#.,32f6,32f6,32d#6,32d6,32d6,32d#6,32f6,16a#,16c6,a#,16a6,16d.6,16a#6,32a6,32a6,32g6,32f#6,32a6,8g6,16g6,16c.6,32a6,32a6,32g6,32g6,32f6,32e6,32g6,8f6,16f6,16a#.,16a#6,32g6,16f6,16a#.,16f6,32d#6,32d6,32d6,32d#6,32f6,16a#,16c.6,32d6,32d#6,32f6,16a#,16c.6,32d6,32d#6,32f6,16a#6,16c7,8a#.6",
	
	"Indiana Jones:d=4,o=5,b=250:e,8p,8f,8g,8p,1c6,8p.,d,8p,8e,1f,p.,g,8p,8a,8b,8p,1f6,p,a,8p,8b,2c6,2d6,2e6,e,8p,8f,8g,8p,1c6,p,d6,8p,8e6,1f.6,g,8p,8g,e.6,8p,d6,8p,8g,e.6,8p,d6,8p,8g,f.6,8p,e6,8p,8d6,2c6",
	"Popcorn:o=5,d=16,b=160,b=160:a,p,g,p,a,p,e,p,c,p,e,p,8a4,8p,a,p,g,p,a,p,e,p,c,p,e,p,8a4,8p,a,p,b,p,c6,p,b,p,c6,p,a,p,b,p,a,p,b,p,g,p,a,p,g,p,a,p,f,8a,8p,a,p,g,p,a,p,e,p,c,p,e,p,8a4,8p,a,p,g,p,a,p,e,p,c,p,e,p,8a4,8p,a,p,b,p,c6,p,b,p,c6,p,a,p,b,p,a,p,b,p,g,p,a,p,g,p,a,p,b,4c6",
	"Mozart:o=5,d=16,b=125,b=125:16d#,c#,c,c#,8e,8p,f#,e,d#,e,8g#,8p,a,g#,g,g#,d#6,c#6,c6,c#6,d#6,c#6,c6,c#6,4e6,8c#6,8e6,32b,32c#6,d#6,8c#6,8b,8c#6,32b,32c#6,d#6,8c#6,8b,8c#6,32b,32c#6,d#6,8c#6,8b,8a#,4g#,d#,32c#,c,c#,8e,8p,f#,e,d#,e,8g#,8p,a,g#,g,g#,d#6,c#6,c6,c#6,d#6,c#6,c6,c#6,4e6,8c#6,8e6,32b,32c#6,d#6,8c#6,8b,8c#6,32b,32c#6,d#6,8c#6,8b,8c#6,32b,32c#6,d#6,8c#6,8b,8a#,4g#",
	"Jackson:o=5,d=8,b=120,b=120:b,e6,g#6,a6,g#6,e6,b,a,g#,a,4b.,4p,16b,16b,e6,g#6,a6,g#6,e6,b,a,g#,a,4b.,p,16b,16b,c#6,16c#6,e6,16e6,f#6.,16e6,e6,p,16e6,16e6,c#6,c#6,e6,e6,f#6,e6,f#6,4g#6.,1p,4b,4b,b,16f#6,16f#6,16f#6,f#6.,g#6,f#6,e6,e6,e6,c#6,e6,e6,c#6,f#6,4e6,4e6.,1p,b,e6,g#6,a6,g#6,e6,b,a,g#,a,4b.,4p,16b,16b,e6,g#6,a6,g#6,e6,b,a,g#,a,4b.,p,16b,16b,c#6,16c#6,e6,16e6,f#6.,16e6,e6,p,16e6,16e6,c#6,c#6,e6,e6,f#6,e6,f#6,4g#6.,1p,4b,4b,b,16f#6,16f#6,16f#6,f#6.,g#6,f#6,e6,e6,e6,c#6,e6,e6,c#6,f#6,4e6,4e6.."
};

static const char* song_names[] = {
    "Super Mario",
    "Imperial March",
    "Star Wars Main",
    "Nokia Tune",
    "Tetris",
	"The Simpsons",
	"Mission Impossible",
	"Flinstones",
	"Indiana Jones",
	"Popcorn",
	"Mozart",
	"Jackson"
};

#define NUM_SONGS (sizeof(song_list) / sizeof(song_list[0]))

const uint16_t notes[] = {
    262, 277, 294, 311, 330, 349, 370, 392, 415, 440, 466, 494
};

static int menuScrollIndex = 0; 

// --- Helper Functions ---

static void PlayTone(uint16_t freq, uint16_t duration_ms) {
    if (freq > 0) {
        BK4819_PlayTone(freq, true); 
        BK4819_ExitTxMute();
    } else {
        BK4819_EnterTxMute();
    }
    SYSTEM_DelayMs(duration_ms);
}

static void PlayRTTTL(const char *p) {
    // 1. เตรียม Header
    while (*p != ':' && *p != 0) p++;
    if (!*p) return; 
    p++; 

    int default_dur = 4;
    int default_oct = 5;
    int bpm = 63;
    int num;

    while (*p != ':') {
        if (!*p) return;
        if (*p == 'd') {
            p += 2; num = 0;
            while (IsDigit(*p)) { num = (num * 10) + (*p++ - '0'); }
            if (num > 0) default_dur = num;
        } else if (*p == 'o') {
            p += 2; num = 0;
            while (IsDigit(*p)) { num = (num * 10) + (*p++ - '0'); }
            if (num >= 3 && num <= 7) default_oct = num;
        } else if (*p == 'b') {
            p += 2; num = 0;
            while (IsDigit(*p)) { num = (num * 10) + (*p++ - '0'); }
            if (num > 0) bpm = num;
        }
        while (*p != ',' && *p != ':') {
            if (!*p) return;
            p++;
        }
        if (*p == ',') p++;
    }
    p++; 

    long wholenote = (60 * 1000L / bpm) * 4;

    AUDIO_AudioPathOn();      
    SYSTEM_DelayMs(50);       

    // 2. Loop เล่นโน้ต
    while (*p) {
        if (KEYBOARD_Poll() != KEY_INVALID) break; 

        num = 0;
        while (IsDigit(*p)) {
            num = (num * 10) + (*p++ - '0');
        }
        int duration = (num > 0) ? num : default_dur;
        long note_len = wholenote / duration;

        int note_index = 0; 
        bool isPause = false;

        switch (*p) {
            case 'c': note_index = 0; break;
            case 'd': note_index = 2; break;
            case 'e': note_index = 4; break;
            case 'f': note_index = 5; break;
            case 'g': note_index = 7; break;
            case 'a': note_index = 9; break;
            case 'b': note_index = 11; break;
            case 'p': isPause = true; break;
        }
        p++;

        if (*p == '#') { note_index++; p++; }
        if (*p == '.') { 
            note_len = (note_len * 3) / 2; 
            p++; 
        }

        num = 0;
        while (IsDigit(*p)) {
            num = (num * 10) + (*p++ - '0');
        }
        int scale = (num > 0) ? num : default_oct;

        if (isPause) {
            PlayTone(0, note_len);
        } else {
            uint16_t freq = notes[note_index];
            if (scale > 4) { for (int i = 0; i < (scale - 4); i++) freq *= 2; } 
            else if (scale < 4) { for (int i = 0; i < (4 - scale); i++) freq /= 2; }

            PlayTone(freq, (note_len * 9) / 10); 
            PlayTone(0, note_len / 10);    
        }

        while (*p != ',' && *p != 0) p++;
        if (*p == ',') p++;
    }

    BK4819_EnterTxMute();      
    BK4819_PlayTone(0, false); 
    SYSTEM_DelayMs(20);        
    AUDIO_AudioPathOff();      
    BK4819_EnterTxMute();      
}

// --- Main Menu ---
void APP_RunRTTTL(void) {
    bool playing = true;
    int selected = 0;
    
    // Reset scroll index
    menuScrollIndex = 0;

    memset(gStatusLine, 0, sizeof(gStatusLine));
    ST7565_BlitStatusLine();

    while(KEYBOARD_Poll() != KEY_INVALID) SYSTEM_DelayMs(10);

    while (playing) {
        KEY_Code_t key = KEYBOARD_Poll();

        if (key == KEY_UP || key == KEY_2) {
            if (selected > 0) {
                selected--;
                if (selected < menuScrollIndex) {
                    menuScrollIndex = selected;
                }
            } else {
                selected = NUM_SONGS - 1;
                menuScrollIndex = NUM_SONGS - ITEMS_PER_PAGE;
                if (menuScrollIndex < 0) menuScrollIndex = 0;
            }
            SYSTEM_DelayMs(150);
        }
        else if (key == KEY_DOWN || key == KEY_8) {
            if (selected < NUM_SONGS - 1) {
                selected++;
                if (selected >= menuScrollIndex + ITEMS_PER_PAGE) {
                    menuScrollIndex = selected - ITEMS_PER_PAGE + 1;
                }
            } else {
                selected = 0;
                menuScrollIndex = 0;
            }
            SYSTEM_DelayMs(150);
        }
        else if (key == KEY_MENU || key == KEY_5) {
            UI_DisplayClear();
            UI_PrintStringSmallBold("NOW PLAYING...", 20, 20, 0);
            
            // [แก้ไข] เปลี่ยน snprintf เป็น strcpy เพื่อเลี่ยง Error _sbrk
            char playingBuf[32];
            strcpy(playingBuf, song_names[selected]); // ก๊อปปี้ชื่อเพลงมาใส่ buffer
            GUI_DisplaySmallest(playingBuf, 10, 40, false, true); 
            
            ST7565_BlitFullScreen();

            while(KEYBOARD_Poll() != KEY_INVALID) SYSTEM_DelayMs(10);

            PlayRTTTL(song_list[selected]);
            
            while(KEYBOARD_Poll() != KEY_INVALID) SYSTEM_DelayMs(10);
        }
        else if (key == KEY_EXIT) {
            playing = false;
        }

        UI_DisplayClear();
        UI_PrintStringSmallBold("MUSIC PLAYER", 25, 0, 0);
        UI_DrawRectangleBuffer(gFrameBuffer, 0, 10, 127, 11, true);

        // --- ส่วนวาดรายการ (พร้อม Scroll) ---
        for (int i = 0; i < ITEMS_PER_PAGE; i++) {
            int itemIndex = menuScrollIndex + i;
            if (itemIndex >= NUM_SONGS) break;
            
            int y = 14 + (i * 10);
            char buf[32];
            
            // [แก้ไข] ใช้ strcpy + strcat แทน sprintf
            if (itemIndex == selected) {
                strcpy(buf, "> ");
                strcat(buf, song_names[itemIndex]);
                GUI_DisplaySmallest(buf, 0, y, false, true);
            } else {
                strcpy(buf, "  ");
                strcat(buf, song_names[itemIndex]);
                GUI_DisplaySmallest(buf, 0, y, false, true); 
            }
        }
        
        if (NUM_SONGS > ITEMS_PER_PAGE) {
            int barHeight = 40; 
            int cursorH = barHeight / NUM_SONGS; 
            if (cursorH < 2) cursorH = 2;
            
            int cursorY = 14 + (selected * (barHeight - cursorH) / (NUM_SONGS - 1));
            
            UI_DrawRectangleBuffer(gFrameBuffer, 125, 14, 126, 14+barHeight, true);
            UI_DrawRectangleBuffer(gFrameBuffer, 124, cursorY, 127, cursorY+cursorH, true);
        }
        
        ST7565_BlitFullScreen();
        SYSTEM_DelayMs(20);
    }
}