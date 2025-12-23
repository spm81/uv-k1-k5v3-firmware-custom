#ifndef MORSE_APP_H
#define MORSE_APP_H

#include <stdint.h>
#include <stdbool.h>

// --- Config ---
#define MORSE_WPM_DEFAULT 20
#define MORSE_TONE_DEFAULT 700

// --- Structure ---
typedef struct {
    char character;       // ตัวอักษรที่โชว์ 'A'
    const char* code;     // รหัส ".-"
} MorseChar;

// --- Game State ---
typedef struct {
    int currentQuestionIndex;   // Index ของคำตอบที่ถูกในตาราง morse_table
    int choices[4];             // Index ของตัวเลือก 4 ตัว (มีคำตอบถูก 1 + ตัวหลอก 3)
    int score;
    uint32_t questionStartTime; // เวลาที่เริ่มปล่อยโจทย์ (สำหรับจับเวลา)
    bool isTimedOut;            // Flag บอกว่าหมดเวลาหรือยัง
} MorseGameCtx;

// --- Global Variables (External) ---
extern const MorseChar morse_table[42]; // A-Z, 0-9, Special
extern uint8_t setting_wpm;
extern uint16_t setting_tone;

// --- Function Prototypes ---
void Morse_PlaySound(const char* code);
void Morse_GenerateQuiz(MorseGameCtx *ctx);
void Morse_CheckTimeout(MorseGameCtx *ctx);

// *** เพิ่ม 2 บรรทัดนี้ครับ ***
void Morse_App_Loop(void);
void Morse_HandleKeys(int key_code, bool is_long_press); 
// *************************

#endif