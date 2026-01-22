#include "app/cw.h"
#include "driver/bk4819.h"
#include "driver/st7565.h"
#include "driver/system.h"
#include "driver/keyboard.h"
#include "driver/gpio.h" 
#include "settings.h"
#include "../audio.h" 
#include "../radio.h" 
#include "ui/helper.h"
#include <string.h>
#include <stdio.h> 

#ifdef ENABLE_FEAT_F4HWN_SCREENSHOT
#include "screenshot.h"
#endif

#ifndef BK4819_AF_OPEN
#define BK4819_AF_OPEN 1
#endif

#ifndef UART1
#define UART1 USART1
#endif

// --- Macros for PTT Check (UV-K5 Standard is GPIOC Pin 5 Active Low) ---
//#define PTT_IS_PRESSED ((GPIOC->IDR & GPIO_IDR_ID5) == 0)

extern void RADIO_ConfigureSquelchAndOutputPower(VFO_Info_t *pInfo);

// --- Config ---
#define RSSI_THRESHOLD_OFFSET 10 
#define MAX_COLS 18      
#define VISIBLE_ROWS 3
#define LOG_PAGES 10
#define TOTAL_ROWS (VISIBLE_ROWS * LOG_PAGES) 

#define MIN_PULSE_MS 30   
#define MIN_DOT_MS 40     
#define MAX_DOT_MS 240    
#define CW_SIDETONE_FREQ 700

// --- ADC Config ---
#define ADC_THRESHOLD_DOT_MAX  1200 
#define ADC_THRESHOLD_DASH_MAX 2500 

// --- Variables ---
static bool isInitialized = false;
static char historyLog[TOTAL_ROWS][MAX_COLS + 1]; 
static int curCol = 0;   
static int viewPage = LOG_PAGES - 1; 

static uint16_t noiseFloor = 0;
static uint32_t stateDuration = 0; 
static bool isSignalOn = false;    

// RX Variables
static uint16_t dotLen = 80; 

// TX Variables
static bool txMode = false;
static uint16_t txDotLen = 100; 
static uint32_t lastTxTime = 0; 

static char symbolBuf[8]; 
static uint8_t symbolCount = 0;

static uint16_t currentRSSI = 0;

static KEY_Code_t lastKey = KEY_INVALID;
static int keyHoldCounter = 0;

// Button Debounce & State Flags
static bool key1WasPressed = false;
static bool key2WasPressed = false; 
static bool key8WasPressed = false; // [NEW] สำหรับปุ่ม 8
static bool keyHashWasPressed = false; // [NEW] สำหรับปุ่ม #

// *** CW Keyer Mode ***
static bool isThaiMode = false; // false=EN, true=TH
static bool isManualMode = false; // [NEW] false=Auto(Repeat), true=Manual(Single)
static bool manualDotProcessed = false;  // Flag กันเบิ้ลสำหรับ Manual Dot
static bool manualDashProcessed = false; // Flag กันเบิ้ลสำหรับ Manual Dash

// *** [NEW] Bandwidth Control ***
static uint8_t cwBwIndex = 0; // Default 0 = 1.7kHz
const char* cwBwNames[] = {"1.7k", "2.0k", "2.5k", "3.0k", "3.75k", "4.0k", "4.25k", "4.5k"};

// --- Tables & Helpers ---

static int GetTotalActivePages() {
    // วนลูปเช็คตั้งแต่หน้าแรก (บนสุดของ Array)
    for (int p = 0; p < LOG_PAGES; p++) {
        bool pageHasData = false;
        // เช็คทุกบรรทัดในหน้านั้น
        for(int r = 0; r < VISIBLE_ROWS; r++) {
            if(strlen(historyLog[(p * VISIBLE_ROWS) + r]) > 0) { 
                pageHasData = true; 
                break; 
            }
        }
        // ถ้าหน้านี้มีข้อมูล แปลว่าหน้าถัดๆ ไปข้างล่างก็ต้อง Active ทั้งหมด
        if (pageHasData) return LOG_PAGES - p; 
    }
    return 1; // อย่างน้อยต้องมี 1 หน้าเสมอ
}

// [NEW] ฟังก์ชันตั้งค่า Bandwidth โดยเขียนลง REG_43 โดยตรง
/* static void SetCwBandwidth(uint8_t index) {
    if (index > 7) index = 0;
    
    // อ่านค่าเดิมจาก REG_43
    uint16_t reg43 = BK4819_ReadRegister(0x43);
    
    // เคลียร์บิต 14, 13, 12 (Mask 0x7000)
    // 0x7000 = 0111 0000 0000 0000
    reg43 &= ~(0x7000); 
    
    // 2. ใส่ค่า Bandwidth ใหม่ที่เราต้องการ
    // 000=1.7k, 001=2.0k, ...
    reg43 |= (index << 12);
    
    // 3. [สำคัญมาก] เคลียร์ Bit 5 (Bandwidth Multiplier)
    // ถ้า Bit นี้เป็น 1 ค่า Bandwidth จะถูกคูณ 2 (เช่น 1.7k จะกลายเป็น 3.4k)
    // เราต้องปรับให้เป็น 0 เพื่อให้ได้ 1.7k จริงๆ
    reg43 &= ~(1 << 5); 
    
    BK4819_WriteRegister(0x43, reg43);
	// 1. Bypass Audio Filters (เอาเสียงดิบ)
	// ตัวอย่างการแก้ REG_2B ให้ปลอดภัยขึ้น
	uint16_t reg2b = BK4819_ReadRegister(0x2B);
	// 1 = Disable Filter (Bypass)
	reg2b |= (1 << 10) | (1 << 9) | (1 << 8); 
	BK4819_WriteRegister(0x2B, reg2b);
} */
static void SetCwBandwidth(uint8_t index)
{
    index &= 7;

    uint16_t reg43 = BK4819_ReadRegister(0x43);

    // ตั้ง BW ปกติ (14:12) และ BW ตอนสัญญาณอ่อน (11:9) ให้เหมือนกัน
    reg43 &= (uint16_t)~(0x7000u | 0x0E00u);
    reg43 |= (uint16_t)((index & 7u) << 12);
    reg43 |= (uint16_t)((index & 7u) << 9);

    // ไม่แตะ reg43 bit5 เพื่อไม่เสี่ยงกระทบ BW Mode Selection (5:4)

    BK4819_WriteRegister(0x43, reg43);
	
	// 1. Bypass Audio Filters (เอาเสียงดิบ)
	// ตัวอย่างการแก้ REG_2B ให้ปลอดภัยขึ้น
	uint16_t reg2b = BK4819_ReadRegister(0x2B);
	// 1 = Disable Filter (Bypass)
	reg2b |= (1 << 10) | (1 << 9) | (1 << 8); 
	BK4819_WriteRegister(0x2B, reg2b);
}


/* static char DecodeMorse(const char* code) {
    if (strcmp(code, ".-") == 0) return 'A';
    if (strcmp(code, "-...") == 0) return 'B';
    if (strcmp(code, "-.-.") == 0) return 'C';
    if (strcmp(code, "-..") == 0) return 'D';
    if (strcmp(code, ".") == 0) return 'E';
    if (strcmp(code, "..-.") == 0) return 'F';
    if (strcmp(code, "--.") == 0) return 'G';
    if (strcmp(code, "....") == 0) return 'H';
    if (strcmp(code, "..") == 0) return 'I';
    if (strcmp(code, ".---") == 0) return 'J';
    if (strcmp(code, "-.-") == 0) return 'K';
    if (strcmp(code, ".-..") == 0) return 'L';
    if (strcmp(code, "--") == 0) return 'M';
    if (strcmp(code, "-.") == 0) return 'N';
    if (strcmp(code, "---") == 0) return 'O';
    if (strcmp(code, ".--.") == 0) return 'P';
    if (strcmp(code, "--.-") == 0) return 'Q';
    if (strcmp(code, ".-.") == 0) return 'R';
    if (strcmp(code, "...") == 0) return 'S';
    if (strcmp(code, "-") == 0) return 'T';
    if (strcmp(code, "..-") == 0) return 'U';
    if (strcmp(code, "...-") == 0) return 'V';
    if (strcmp(code, ".--") == 0) return 'W';
    if (strcmp(code, "-..-") == 0) return 'X';
    if (strcmp(code, "-.--") == 0) return 'Y';
    if (strcmp(code, "--..") == 0) return 'Z';
    
    if (strcmp(code, ".----") == 0) return '1';
    if (strcmp(code, "..---") == 0) return '2';
    if (strcmp(code, "...--") == 0) return '3';
    if (strcmp(code, "....-") == 0) return '4';
    if (strcmp(code, ".....") == 0) return '5';
    if (strcmp(code, "-....") == 0) return '6';
    if (strcmp(code, "--...") == 0) return '7';
    if (strcmp(code, "---..") == 0) return '8';
    if (strcmp(code, "----.") == 0) return '9';
    if (strcmp(code, "-----") == 0) return '0';
	
	if (strcmp(code, "----") == 0) return '_';
	if (strcmp(code, ".-.-.-") == 0) return '.';
	if (strcmp(code, "-...-") == 0) return '=';
	if (strcmp(code, "--..--") == 0) return ',';
	if (strcmp(code, "-..-.") == 0) return '/';
	if (strcmp(code, "..--..") == 0) return '?';
	
    return '?'; 
} */

static const struct { const char* code; char ch; } morse_table_en[] = {
    {".-", 'A'}, {"-...", 'B'}, {"-.-.", 'C'}, {"-..", 'D'}, {".", 'E'}, {"..-.", 'F'}, {"--.", 'G'}, {"....", 'H'},
    {"..", 'I'}, {".---", 'J'}, {"-.-", 'K'}, {".-..", 'L'}, {"--", 'M'}, {"-.", 'N'}, {"---", 'O'}, {".--.", 'P'},
    {"--.-", 'Q'}, {".-.", 'R'}, {"...", 'S'}, {"-", 'T'}, {"..-", 'U'}, {"...-", 'V'}, {".--", 'W'}, {"-..-", 'X'},
    {"-.--", 'Y'}, {"--..", 'Z'},
    {".----", '1'}, {"..---", '2'}, {"...--", '3'}, {"....-", '4'}, {".....", '5'}, {"-....", '6'}, {"--...", '7'}, {"---..", '8'}, {"----.", '9'}, {"-----", '0'},
    {"----", '_'}, {".-.-.-", '.'}, {"-...-", '='}, {"--..--", ','}, {"-..-.", '/'}, {"..--..", '?'},
    {NULL, '\0'}
};

// ตาราง Morse ภาษาไทย (TH) - Mapping มาตรฐาน
// หมายเหตุ: บางรหัสอาจซ้ำกันตามธรรมชาติของ Morse ไทย (เช่น ก/น) 
// ตารางนี้เลือกตัวอักษรที่พบบ่อยเป็นหลัก
static const struct {
    const char* code;
    unsigned char ch; // ใช้ unsigned char รองรับ 0xA1-0xFB
} morse_table_th[] = {
    // พยัญชนะ
    {"--.", 0xA1},    // ก (G)
    {"-.-.", 0xA2},  // ข (C)
    {"-.-", 0xA4},   // ค (K)
    //{"-...-", 0xA6}, // ฆ (X ในบางตำรา หรือเครื่องหมาย = )
    {"-.--.", 0xA7}, // ง
    {"-..-.", 0xA8},  // จ (P)
    {"----", 0xA9},  // ฉ
    {"-..-", 0xAA},  // ช ฌ
    {"--..", 0xAB},  // ซ (Z)
    //{"-.--", 0xAC},  // ฌ (Y)
    {".---", 0xAD},   // ญ (W)
    {"-..", 0xB4},   // ด (D)
    {"-", 0xB5},     // ต (T)
	{"-.-..", 0xB6},  // ถ, ฐ
    {"-..--", 0xB7}, // ท, ธ
    {"-.", 0xB9},    // น 
    {"-...", 0xBA},  // บ 
    {".--.", 0xBB},  // ป 
	{"--.-", 0xBC},  // ผ
	{"-.-.-", 0xBD},  // ฝ
	{".--..", 0xBE},  // พ ภ
	{"..-.", 0xBF},  // ฟ
	{"--", 0xC1},  // ม
	{"-.--", 0xC2},  // ย
    {".-.", 0xC3},   // ร (R)
	{".-.--", 0xC4},  // ฤ 
    {".-..", 0xC5},  // ล (L)
    {".--", 0xC7},  // ว 
    {"...", 0xCA},   // ศ ษ ส  (S)
    {"....", 0xCB},  // ห (H)
    {"-...-", 0xCD},    // อ 
    {"--.--", 0xCE},  // ฮ 
	
	{"--.-.", 0xCF},  // ฯ
    
    // สระและวรรณยุกต์ (Mapping ยอดนิยม)
    {".-...", 0xD0},    // อะ
	{".--.-", 0xD1},  // ั
	{".-", 0xD2},  // อา
	{"...-.", 0xD3},  // อำ
	{"..-..", 0xD4},  // อิ
	{"..", 0xD5},  // อี
	{"..--.", 0xD6},  // อึ
	{"..--", 0xD7},  // อื
	{"..-.-", 0xD8},  // อุ 
	{"---.", 0xD9},  // อู
	{".", 0xE0},  // เอ
	{".-.-", 0xE1},  // แอ
	{"---", 0xE2},  // โอ
	{".-..-", 0xE4},  // ไ ใ
	
	
	{"-.---", 0xE6},  // ๆ
	
	{"..-", 0xE8},  // ่
	{"...-", 0xE9},  // ้
	//{"--...", 0xCA},  // ๊
	{".-.-.", 0xEB},  // ๋
	
	//{".-..-.", 0xCA},  // " "
	//{"-.--.-", 0xCA},  // ()
	
	//{"---.-", 0xCA},  // ฯลฯ
	
	
    
    // ตัวเลข (ใช้เหมือนสากล)
    {".----", '1'}, {"..---", '2'}, {"...--", '3'}, {"....-", '4'},
    {".....", '5'}, {"-....", '6'}, {"--...", '7'}, {"---..", '8'},
    {"----.", '9'}, {"-----", '0'},
    
    // เครื่องหมาย
    {".-.-.-", '.'}, {"--..--", ','}, {"-..-.", '/'}, {"..--..", '?'},
    
    {NULL, 0}
};

static char DecodeMorse(const char* code) {
    if (isThaiMode) {
        for (int i = 0; morse_table_th[i].code != NULL; i++) {
            if (strcmp(code, morse_table_th[i].code) == 0) return (char)morse_table_th[i].ch;
        }
    }
    for (int i = 0; morse_table_en[i].code != NULL; i++) {
        if (strcmp(code, morse_table_en[i].code) == 0) return morse_table_en[i].ch;
    }
    return '?'; 
}

static void AppendChar(char c) {
    if (curCol >= MAX_COLS) {
        for (int i = 0; i < TOTAL_ROWS - 1; i++) {
            strcpy(historyLog[i], historyLog[i+1]);
        }
        memset(historyLog[TOTAL_ROWS - 1], 0, sizeof(historyLog[TOTAL_ROWS - 1]));
        curCol = 0;
    }
    historyLog[TOTAL_ROWS - 1][curCol++] = c;
    historyLog[TOTAL_ROWS - 1][curCol] = '\0'; 
    viewPage = LOG_PAGES - 1; 
}

static void ProcessSymbolBuffer() {
    if (symbolCount == 0) return;
    symbolBuf[symbolCount] = '\0'; 
    char decoded = DecodeMorse(symbolBuf);
    AppendChar(decoded);
    symbolCount = 0;
    memset(symbolBuf, 0, sizeof(symbolBuf));
}

static void Draw() {
    UI_DisplayClear();

    uint32_t freq = gEeprom.VfoInfo[gEeprom.TX_VFO].pRX->Frequency;
    uint8_t mod = gEeprom.VfoInfo[gEeprom.TX_VFO].Modulation;
    //uint8_t bw = gEeprom.VfoInfo[gEeprom.TX_VFO].CHANNEL_BANDWIDTH;
	
    // Show Language Mode
    char languageStr[5];
    sprintf(languageStr, "%s", isThaiMode ? "[TH]" : "[EN]");
    GUI_DisplaySmallest(languageStr, 0, 13, false, true);
    
    char modStr[8];
    switch(mod) {
        case MODULATION_FM:  strcpy(modStr, "FM"); break;
        case MODULATION_AM:  strcpy(modStr, "AM"); break;
        case MODULATION_USB: strcpy(modStr, "USB"); break;
        default:      strcpy(modStr, "(\?\?)"); break;
    }
/*     char bwStr[6];
    switch(bw) {
        case BK4819_FILTER_BW_WIDE:     strcpy(bwStr, "25k"); break;
        case BK4819_FILTER_BW_NARROW:   strcpy(bwStr, "12k"); break;
        case BK4819_FILTER_BW_NARROWER: strcpy(bwStr, "6k"); break;
        default:                        strcpy(bwStr, "?k"); break;
    } */
	
    // [MODIFIED] Show Custom CW Bandwidth instead of Standard BW
    char bwStr[10];
    sprintf(bwStr, "%s", cwBwNames[cwBwIndex]);

    char freqStr[32];
    sprintf(freqStr, "%lu.%05lu %s %s", freq / 100000, freq % 100000, modStr, bwStr);
    UI_PrintStringSmallBold(freqStr, 2, 0, 0);

    UI_DrawRectangleBuffer(gFrameBuffer, 0, 10, 127, 11, true);

    int totalPages = GetTotalActivePages();
    int minValidPageIndex = LOG_PAGES - totalPages; 
    if (viewPage < minValidPageIndex) viewPage = minValidPageIndex;

    int startRow = viewPage * VISIBLE_ROWS;
    GUI_DisplaySmallest(historyLog[startRow], 25, 13, false, true);
    GUI_DisplaySmallest(historyLog[startRow + 1], 25, 21, false, true);
    if (viewPage == LOG_PAGES - 1) {
        UI_PrintStringSmallBold(historyLog[startRow + 2], 1, 0, 4);
    } else {
        GUI_DisplaySmallest(historyLog[startRow + 2], 25, 29, false, true);
    }

    int displayNum = viewPage - (LOG_PAGES - totalPages) + 1;
    char pageStr[32];
    sprintf(pageStr, "%d/%d", displayNum, totalPages);
    GUI_DisplaySmallest(pageStr, 108, 13, false, true);

    UI_DrawRectangleBuffer(gFrameBuffer, 0, 41, 127, 42, true);

    // Status Bar
    char symDisp[12];
    memset(symDisp, 0, sizeof(symDisp));
    for(int i=0; i<symbolCount; i++) symDisp[i] = symbolBuf[i];
    
    if(txMode) GUI_DisplaySmallest("OUT:", 2, 44, false, true);
    else GUI_DisplaySmallest("IN:", 2, 44, false, true); 
    
    GUI_DisplaySmallest(symDisp, 20, 44, false, true);

    // Speed & Key Mode Display
    char spd[32];
	int txWpm = 1200 / txDotLen;
    int rxWpm = 1200 / dotLen;
    // เพิ่มการแสดงผลโหมด Keyer: A=Auto, M=Manual
    //sprintf(spd, "%s T:%d", isManualMode ? "MAN" : "AUTO", 1200 / txDotLen);
	//sprintf(spd, "TX:%d RX:%d", txWpm, rxWpm);
	sprintf(spd, "TX:%d RX:%d %s", txWpm, rxWpm, isManualMode ? "Mn" : "Au");
    GUI_DisplaySmallest(spd, 52, 44, false, true);
	
    uint8_t pwr = gEeprom.VfoInfo[gEeprom.TX_VFO].OUTPUT_POWER;
    char pwrStr[16];
    if (pwr >= 1 && pwr <= 5) sprintf(pwrStr, "P:L%d", pwr);
    else if (pwr == 6) strcpy(pwrStr, "P:M");
    else if (pwr == 7) strcpy(pwrStr, "P:H");
    else strcpy(pwrStr, "P:L");
	GUI_DisplaySmallest(pwrStr, 112, 44, false, true);

    UI_DrawRectangleBuffer(gFrameBuffer, 1, 55, 25 + (currentRSSI/4), 60, true);
    int thValue = noiseFloor + RSSI_THRESHOLD_OFFSET;
    int thLine = 25 + (thValue / 4);
    UI_DrawRectangleBuffer(gFrameBuffer, thLine, 52, thLine+1, 63, true);
	
    char debugStr[32];
    sprintf(debugStr, "R:%d T:%d", currentRSSI, thValue);
    GUI_DisplaySmallest(debugStr, 85, 50, false, true);
    
    ST7565_BlitFullScreen();
}

static void UpdateDotLen(uint32_t newLen) {
    if (newLen < MIN_DOT_MS) newLen = MIN_DOT_MS;
    if (newLen > MAX_DOT_MS) newLen = MAX_DOT_MS;
    dotLen = ((dotLen * 3) + newLen) / 4;
}

static void TX_Start(void) {
    uint32_t txFreq = gEeprom.VfoInfo[gEeprom.TX_VFO].pRX->Frequency;
    BK4819_SetFrequency(txFreq);
    AUDIO_AudioPathOff();
    BK4819_ToggleGpioOut(BK4819_GPIO0_PIN28_RX_ENABLE, false); 
    BK4819_WriteRegister(BK4819_REG_70, 0); 
    BK4819_WriteRegister(BK4819_REG_51, 0);
    BK4819_WriteRegister(BK4819_REG_7D, 0);
    BK4819_EnterTxMute();
    BK4819_EnableTXLink();
    BK4819_SetupPowerAmplifier(gEeprom.VfoInfo[gEeprom.TX_VFO].TXP_CalculatedSetting, txFreq);
}

static void TX_Key(bool on) {
    if (on) {
        BK4819_ToggleGpioOut(BK4819_GPIO1_PIN29_PA_ENABLE, true);
        BK4819_ToggleGpioOut(BK4819_GPIO5_PIN1_RED, true);
    } else {
        BK4819_ToggleGpioOut(BK4819_GPIO1_PIN29_PA_ENABLE, false);
        BK4819_ToggleGpioOut(BK4819_GPIO5_PIN1_RED, false);
    }
}

static void TX_Stop(void)
{
    // --- SAVE state BEFORE any reset that may force FM ---
    VFO_Info_t *pVfo = &gEeprom.VfoInfo[gEeprom.TX_VFO];
    const uint8_t  savedMod  = pVfo->Modulation;
    const uint32_t savedFreq = pVfo->pRX->Frequency;

    TX_Key(false);
    BK4819_EnterTxMute();
    BK4819_TurnsOffTones_TurnsOnRX();
    BK4819_ToggleGpioOut(BK4819_GPIO0_PIN28_RX_ENABLE, true);

    // รีเซ็ต (ถ้าจำเป็นต้องใช้จริง ๆ)
    RADIO_SetupRegisters(true);

    // --- RESTORE USB/AM/FM mode explicitly (don’t trust struct after reset) ---
    pVfo->Modulation = savedMod;
    RADIO_SetModulation(savedMod);

    BK4819_SetFrequency(0);
    BK4819_SetFrequency(savedFreq);
    BK4819_PickRXFilterPathBasedOnFrequency(savedFreq);

    RADIO_ConfigureSquelchAndOutputPower(pVfo);

    // เผื่อบางกรณีไม่ได้ RX on จริงจาก TurnsOffTones...
    BK4819_RX_TurnOn();

    SetCwBandwidth(cwBwIndex);

    AUDIO_AudioPathOn();
    BK4819_SetAF(BK4819_AF_OPEN);
}


void APP_RunCW(void) {
    isInitialized = true;
    txMode = false;
    dotLen = 80; 
    txDotLen = 100;
    
    for(int i=0; i<TOTAL_ROWS; i++) memset(historyLog[i], 0, sizeof(historyLog[i]));
    curCol = 0;
    viewPage = LOG_PAGES - 1;
    
    AUDIO_AudioPathOn();           
    BK4819_SetAF(BK4819_AF_OPEN); 

    uint32_t startFreq = gEeprom.VfoInfo[gEeprom.TX_VFO].pRX->Frequency;
    BK4819_SetFrequency(startFreq);
    BK4819_PickRXFilterPathBasedOnFrequency(startFreq);
    BK4819_ToggleGpioOut(BK4819_GPIO0_PIN28_RX_ENABLE, true);
    BK4819_RX_TurnOn();
	
    // [NEW] บังคับใช้ Bandwidth เริ่มต้น (1.7k) ทันทีที่เปิดแอป
    cwBwIndex = 0;
    SetCwBandwidth(cwBwIndex);
    
    memset(gStatusLine, 0, sizeof(gStatusLine));
    GUI_DisplaySmallest("CW TX/RX", 0, 1, true, true);
    ST7565_BlitStatusLine();
    
    symbolCount = 0;
    stateDuration = 0;
    isSignalOn = false;
    currentRSSI = 0;
    
    uint32_t sum = 0;
    for(int i=0; i<10; i++) {
        sum += BK4819_GetRSSI();
        SYSTEM_DelayMs(10);
    }
    noiseFloor = sum / 10;
    
    uint32_t lastDrawTime = 0;
	lastTxTime = 0;

    keyHoldCounter = 0;
    lastKey = KEY_INVALID;
    key1WasPressed = false;
    key2WasPressed = false;
    key8WasPressed = false;
    keyHashWasPressed = false;
	
    // Reset Manual Flags
    manualDotProcessed = false;
    manualDashProcessed = false;

	Draw(); 

    while(isInitialized) {
        KEY_Code_t key = KEYBOARD_Poll();
		
        // --- KEY 1: Toggle Language ---
        if (key == KEY_1) {
            if (!key1WasPressed) {
                isThaiMode = !isThaiMode; 
                Draw(); 
                key1WasPressed = true;
            }
        } else {
            key1WasPressed = false;
        }
        
        // --- KEY 2: Page Cycle ---
        if (key == KEY_2) {
            if (!key2WasPressed) {
                int total = GetTotalActivePages();
                int minIdx = LOG_PAGES - total;
				if (viewPage == LOG_PAGES - 1) viewPage = minIdx; else viewPage++;
                Draw();
                key2WasPressed = true;
            }
        } else {
            key2WasPressed = false;
        }

        // --- KEY 6: TX Power ---
        if (key == KEY_6) {
             uint8_t *pwr = &gEeprom.VfoInfo[gEeprom.TX_VFO].OUTPUT_POWER;
             (*pwr)++;
             if (*pwr > 7) *pwr = 1; 
             RADIO_ConfigureSquelchAndOutputPower(&gEeprom.VfoInfo[gEeprom.TX_VFO]);
             Draw();
             SYSTEM_DelayMs(200); 
        }

        // --- [NEW] KEY 8: Toggle Auto/Manual Mode ---
        if (key == KEY_8) {
            if (!key8WasPressed) {
                isManualMode = !isManualMode;
                Draw(); // Update status on screen
                key8WasPressed = true;
            }
        } else {
            key8WasPressed = false;
        }
		
        // --- [NEW] KEY # (HASH): Toggle Bandwidth ---
        if (key == KEY_F) { // ใช้ KEY_F แทนหรือ KEY_HASH (ในโค้ดเดิมเห็นใช้ F ปรับ Narrow)
            // ขอเปลี่ยน KEY_F เดิมที่ปรับ Narrow มาใช้ Logic ใหม่ของเรา
            // หรือถ้าจะเอา # (KEY_HASH) จริงๆ ต้องเช็คว่า driver map ไว้ไหม
            // ปกติ K5 ปุ่ม # คือ KEY_F (Function) หรือ KEY_STAR
            // ขอใช้ KEY_HASH ตามที่ขอ
        }
        
        // หมายเหตุ: บน UV-K5 บาง Driver ปุ่ม # อาจจะเป็น KEY_F หรือ KEY_PTT 
        // แต่ถ้าอิงตาม Driver มาตรฐาน -> KEY_INVALID หรือต้องเช็ค map
        // สมมติว่าใช้ KEY_F (ปุ่มข้างๆ 0) ทำหน้าที่นี้ หรือถ้ามี KEY_HASH (ปุ่มขวาล่าง)
        // ถ้า KEY_F เดิมทำหน้าที่ปรับ BW อยู่แล้ว ผมจะแก้ KEY_F เดิมให้ใช้อันใหม่
        
        // --- KEY # (ซึ่งจริงๆ บน K5 ปุ่มขวาล่างมักจะ map เป็น KEY_F หรือ KEY_HASH แล้วแต่ driver)
        // ขอใช้ตำแหน่ง KEY_F เดิมในโค้ดคุณ มาปรับใช้กับฟังก์ชันใหม่
        //if (key == KEY_F || key == KEY_5) { // เพิ่ม KEY_5 เผื่อไว้ หรือใช้ KEY_F ตามเดิม
		if (key == KEY_F) {
            if (!keyHashWasPressed) {
                cwBwIndex++;
                if (cwBwIndex > 7) cwBwIndex = 0;
                SetCwBandwidth(cwBwIndex);
                Draw();
                keyHashWasPressed = true;
            }
        } else {
            keyHashWasPressed = false;
        }
        // *หมายเหตุ* ถ้าคุณใช้ปุ่ม # จริงๆ ให้แก้ if (key == KEY_F) เป็น if (key == KEY_HASH) 
        // (ถ้า Driver รู้จัก KEY_HASH)

        
        bool triggerDot  = (key == KEY_7);
        bool triggerDash = (key == KEY_9);
        
        // --- [NEW] PTT Handling (Straight Key) ---
        bool pttPressed = GPIO_IsPttPressed(); // ตรวจสอบสถานะ PTT
        if (pttPressed) {
            if (!txMode) {
                TX_Start();
                txMode = true;
				viewPage = LOG_PAGES - 1;
                Draw();
            }
            
            // 1. เริ่มส่งเสียงและ RF
            TX_Key(true);
            
            uint32_t pttDuration = 0;
            
            // 2. วนลูปจนกว่าจะปล่อย PTT (ส่งคีย์ค้าง)
            while(GPIO_IsPttPressed()) {
                SYSTEM_DelayMs(10);
                pttDuration += 10;
                
                // Safety: ถ้ากด KEY_EXIT ให้หลุดลูปฉุกเฉิน
                if (KEYBOARD_Poll() == KEY_EXIT) { isInitialized = false; break; }
            }
            
            // 3. ปล่อย PTT หยุดส่ง
            TX_Key(false);
            
            // 4. คำนวณความยาวเพื่อแปลงเป็น Dot หรือ Dash
            // เกณฑ์: ถ้าน้อยกว่า 2 เท่าของความเร็ว Dot ให้เป็น . ถ้ามากกว่าเป็น -
            // หรือใช้ค่า Fixed เช่น 200ms ก็ได้ แต่ใช้อิงตาม txDotLen จะยืดหยุ่นตามความเร็วที่ตั้ง
            if (pttDuration > 0) {
                char symbol;
                // Threshold สำหรับแยก . กับ - (ปรับตัวคูณได้ตามความถนัด)
                if (pttDuration < (txDotLen * 2)) {
                    symbol = '.';
                } else {
                    symbol = '-';
                }
                
                if(symbolCount < 7) {
                    symbolBuf[symbolCount++] = symbol;
                }
                lastTxTime = 0; // Reset Auto Space Timer
            }
            
            // 5. หน่วงเวลาเล็กน้อยหลังปล่อยเพื่อไม่ให้สัญญาณติดกันเกินไป
            SYSTEM_DelayMs(txDotLen);
            
        } 
        
        // --- KEY/Paddle Sending Logic (Auto/Manual) ---
        else if (triggerDot || triggerDash) {
            
            bool canSend = true;
            
            // [NEW] Logic สำหรับ Manual Mode
            if (isManualMode) {
                if (triggerDot) {
                    if (manualDotProcessed) canSend = false; // ถ้ากดค้างและเคยส่งแล้ว ห้ามส่งซ้ำ
                    else manualDotProcessed = true; // จำว่าส่งแล้ว
                }
                if (triggerDash) {
                    if (manualDashProcessed) canSend = false;
                    else manualDashProcessed = true;
                }
            } else {
                // Auto Mode: ไม่ต้องเช็ค Processed flag ปล่อยให้วนลูปส่งรัวๆ
            }

            if (canSend) {
                if (!txMode) {
                    TX_Start();
                    txMode = true;
                    viewPage = LOG_PAGES - 1;
                    Draw(); 
                }

                TX_Key(true);
                
                if(symbolCount < 7) {
                    symbolBuf[symbolCount++] = triggerDot ? '.' : '-';
                }
                
                lastTxTime = 0; 

                int duration = triggerDot ? txDotLen : (txDotLen * 3);
                SYSTEM_DelayMs(duration);
                
                TX_Key(false);
                SYSTEM_DelayMs(txDotLen); 
            }
        } else {
            // Keys released - Reset Manual Flags
            manualDotProcessed = false;
            manualDashProcessed = false;

            if (txMode) {
                // Check Auto Space Timeout (ระยะห่างตัวอักษร)
                // ถ้าใช้ PTT หรือ Manual Key เราอาจจะต้องจัดการ timeout เอง
                // โค้ดเดิม: ถ้า txMode ค้างแต่ไม่มี trigger ให้หยุด
                // ปรับปรุง: รอสักพักถ้าไม่มี input ค่อยปิด txMode
                TX_Stop();
                txMode = false;
                
                isSignalOn = false;
                stateDuration = 0;
                Draw(); 
            }
            
            // ... (ปุ่มปรับ Speed, Exit, Menu เดิม คงไว้) ...
            if (key == KEY_SIDE1) { 
                if (txDotLen > MIN_DOT_MS) txDotLen -= 5;
                Draw();
                SYSTEM_DelayMs(100); 
            } 
            else if (key == KEY_SIDE2) { 
                if (txDotLen < MAX_DOT_MS) txDotLen += 5;
                Draw();
                SYSTEM_DelayMs(100);
            }

            if(key == KEY_EXIT) isInitialized = false;
            if(key == KEY_MENU) noiseFloor = BK4819_GetRSSI();

            if (key == KEY_0) {
                uint8_t *mod = &gEeprom.VfoInfo[gEeprom.TX_VFO].Modulation;
                if (*mod == MODULATION_FM) *mod = MODULATION_AM;
                else if (*mod == MODULATION_AM) *mod = MODULATION_USB;
                else *mod = MODULATION_FM;
                RADIO_SetModulation(*mod);
                Draw();
                SYSTEM_DelayMs(200); 
            }
/*             if (key == KEY_F) { 
                uint8_t *bw = &gEeprom.VfoInfo[gEeprom.TX_VFO].CHANNEL_BANDWIDTH;
                if (*bw == BK4819_FILTER_BW_WIDE) *bw = BK4819_FILTER_BW_NARROW;
                else if (*bw == BK4819_FILTER_BW_NARROW) *bw = BK4819_FILTER_BW_NARROWER;
                else *bw = BK4819_FILTER_BW_WIDE;
                BK4819_SetFilterBandwidth(*bw, false);
                Draw();
                SYSTEM_DelayMs(200);
            } */
            if (key == KEY_UP || key == KEY_DOWN) {
                if (key == lastKey) keyHoldCounter++; else { keyHoldCounter = 0; lastKey = key; }
                int step = 1; int throttle = 10;
                if (keyHoldCounter > 1000) { step = 500; throttle = 8; }
                else if (keyHoldCounter > 800) { step = 200; throttle = 10; }
                else if (keyHoldCounter > 400) { step = 100; throttle = 15; }
                else if (keyHoldCounter > 200) { step = 50; throttle = 20; }
                else if (keyHoldCounter > 100) { step = 10; throttle = 30; }
                if (keyHoldCounter == 0 || (keyHoldCounter > 15 && keyHoldCounter % throttle == 0)) {
                    uint32_t *freq = &gEeprom.VfoInfo[gEeprom.TX_VFO].pRX->Frequency;
                    if (key == KEY_UP) *freq -= step; else *freq += step;
                    BK4819_SetFrequency(*freq);
                    BK4819_PickRXFilterPathBasedOnFrequency(*freq);
                    BK4819_ToggleGpioOut(BK4819_GPIO0_PIN28_RX_ENABLE, true);
                    BK4819_RX_TurnOn(); 
					
					// [IMPORTANT] เปลี่ยนความถี่แล้วต้องย้ำ Bandwidth ของเราใหม่
                    SetCwBandwidth(cwBwIndex);
					
                    Draw();
                    lastDrawTime = 0; isSignalOn = false; stateDuration = 0;
                }
            } else {
                if (key == KEY_INVALID || (key != KEY_UP && key != KEY_DOWN && key != KEY_1 && key != KEY_2 && key != KEY_6 && key != KEY_8 && key != KEY_F)) {
                    keyHoldCounter = 0;
                    if (key != KEY_INVALID) lastKey = key; else lastKey = KEY_INVALID;
                }
            }
            
            // Auto-space logic (จบตัวอักษร)
            // ถ้าเวลาผ่านไปนานกว่า 3 DotLen และมี Symbol ใน Buffer ให้ Process
/*             if (symbolCount > 0) {
                 lastTxTime += 10; // นับเวลาเพิ่มเมื่อไม่มีการกด
                 if (lastTxTime > (txDotLen * 4)) { // รอประมาณ 4 dot length
                     ProcessSymbolBuffer();
                     Draw();
                     lastTxTime = 0;
                 }
            }*/
        }  
        
        // --- RX Decoder Logic ---
        if (!txMode) {
            currentRSSI = BK4819_GetRSSI();
            bool signalDetected = (currentRSSI > (noiseFloor + RSSI_THRESHOLD_OFFSET));
            
            if (signalDetected) {
                if (!isSignalOn) {
                    if (stateDuration >= (dotLen * 5)) {
                       ProcessSymbolBuffer();
                       AppendChar(' ');
                    }
                    isSignalOn = true;
                    stateDuration = 0;
                }
            } else {
                if (isSignalOn) {
                    if (stateDuration > MIN_PULSE_MS) { 
                        if (stateDuration < (dotLen * 1.5)) {
                            if(symbolCount < 7) symbolBuf[symbolCount++] = '.';
                             UpdateDotLen(stateDuration);
                        } else {
                            if(symbolCount < 7) symbolBuf[symbolCount++] = '-';
                            UpdateDotLen(stateDuration / 3);
                        }
                    }
                    isSignalOn = false;
                    stateDuration = 0;
                } else {
                    if (symbolCount > 0 && stateDuration > (dotLen * 3)) {
                         ProcessSymbolBuffer();
                    }
                }
            }
            stateDuration += 10; 
        }

        lastDrawTime += 10;
        if (keyHoldCounter == 0 && lastDrawTime >= 150) {
            Draw();
            lastDrawTime = 0;
        }
        
        SYSTEM_DelayMs(10); 
    }
	
    //RestoreUART();

    if(txMode) TX_Stop(); 
    BK4819_SetAF(BK4819_AF_MUTE); 
    AUDIO_AudioPathOff();         
}
