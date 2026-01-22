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

#include <string.h>

#include "driver/st7565.h"
#include "external/printf/printf.h"
#include "font.h"
#include "ui/helper.h"
#include "ui/inputbox.h"
#include "misc.h"

#ifndef ARRAY_SIZE
    #define ARRAY_SIZE(arr) (sizeof(arr)/sizeof((arr)[0]))
#endif

void UI_GenerateChannelString(char *pString, const uint8_t Channel)
{
    unsigned int i;

    if (gInputBoxIndex == 0)
    {
        sprintf(pString, "CH-%02u", Channel + 1);
        return;
    }

    pString[0] = 'C';
    pString[1] = 'H';
    pString[2] = '-';
    for (i = 0; i < 2; i++)
        pString[i + 3] = (gInputBox[i] == 10) ? '-' : gInputBox[i] + '0';
}

void UI_GenerateChannelStringEx(char *pString, const bool bShowPrefix, const uint8_t ChannelNumber)
{
    if (gInputBoxIndex > 0) {
        for (unsigned int i = 0; i < 3; i++) {
            pString[i] = (gInputBox[i] == 10) ? '-' : gInputBox[i] + '0';
        }

        pString[3] = 0;
        return;
    }

    if (bShowPrefix) {
        // BUG here? Prefixed NULLs are allowed
        sprintf(pString, "CH-%03u", ChannelNumber + 1);
    } else if (ChannelNumber == 0xFF) {
        strcpy(pString, "NULL");
    } else {
        sprintf(pString, "%03u", ChannelNumber + 1);
    }
}

void UI_PrintStringBuffer(const char *pString, uint8_t * buffer, uint32_t char_width, const uint8_t *font)
{
    const size_t Length = strlen(pString);
    const unsigned int char_spacing = char_width + 1;
    for (size_t i = 0; i < Length; i++) {
        const unsigned int index = pString[i] - ' ' - 1;
        if (pString[i] > ' ' && pString[i] < 127) {
            const uint32_t offset = i * char_spacing + 1;
            memcpy(buffer + offset, font + index * char_width, char_width);
        }
    }
}

/* void UI_PrintString(const char *pString, uint8_t Start, uint8_t End, uint8_t Line, uint8_t Width)
{
    size_t i;
    size_t Length = strlen(pString);

    if (End > Start)
        Start += (((End - Start) - (Length * Width)) + 1) / 2;

    for (i = 0; i < Length; i++)
    {
        const unsigned int ofs   = (unsigned int)Start + (i * Width);
        if (pString[i] > ' ' && pString[i] < 127)
        {
            const unsigned int index = pString[i] - ' ' - 1;
            memcpy(gFrameBuffer[Line + 0] + ofs, &gFontBig[index][0], 7);
            memcpy(gFrameBuffer[Line + 1] + ofs, &gFontBig[index][7], 7);
        }
    }
} */

// เช็คว่าเป็นสระบน/ล่าง หรือวรรณยุกต์หรือไม่
bool IsThaiDiacritic(uint8_t charCode) {
    // 0xD1 = ไม้หันอากาศ
    // 0xD4-0xDA = สระอิ ถึง สระอู
    // 0xE7-0xEE = ไม้ไต่คู้, วรรณยุกต์, การันต์
    if (charCode == 0xD1 || 
       (charCode >= 0xD4 && charCode <= 0xDA) || 
       (charCode >= 0xE7 && charCode <= 0xEE)) {
        return true;
    }
    return false;
}

void UI_PrintString(const char *pString, uint8_t Start, uint8_t End, uint8_t Line, uint8_t Width)
{
    size_t i;
    size_t Length = strlen(pString);

    // สูตรจัดกึ่งกลางเดิม
    if (End > Start)
        Start += (((End - Start) - (Length * Width)) + 1) / 2;

    unsigned int currentX = Start;

    for (i = 0; i < Length; i++)
    {
        uint8_t charCode = (uint8_t)pString[i];

        if (charCode >= 0xA1)
        {
            // --- ภาษาไทย ---
            const unsigned int index = charCode - 0xA1;
            bool isOverlay = IsThaiDiacritic(charCode);
            
            // ตำแหน่งที่จะวาด
            unsigned int drawPos = currentX;

            if (isOverlay) {
                // ถ้าเป็นสระลอย ถอยหลังกลับมา 8 pixel (7px ตัวอักษร + 1px ช่องไฟ)
                if (drawPos >= 8) drawPos -= 8; 
            }

            // เตรียมข้อมูลฟอนต์
            const uint8_t *pTop = &THAI_FONT_BIG[index][0];
            const uint8_t *pBot = &THAI_FONT_BIG[index][7];

            if (isOverlay) {
                // เช็คว่าเป็นวรรณยุกต์ที่ต้องการขยับขึ้นหรือไม่?
                // 0xE8=่, 0xE9=้, 0xEA=๊, 0xEB=๋, 0xEC=์
/*                 bool shiftUp = (charCode >= 0xE8 && charCode <= 0xEC);

                // สระลอย: ใช้ OR เพื่อไม่ให้ลบตัวเดิม
                for (int k = 0; k < 7; k++) {
                    uint8_t valTop = pTop[k];
                    uint8_t valBot = pBot[k];

                    // *** ส่วนที่เพิ่ม: ขยับวรรณยุกต์ขึ้น 1 Pixel ***
                    if (shiftUp) {
                        // หลักการ: เลื่อนบิตไปทางขวา (ขึ้นบน)
                        // เอา Bit 0 ของแถวล่าง มาใส่ที่ Bit 7 ของแถวบน
                        valTop = (valTop >> 1) | ((valBot & 0x01) << 7);
                        valBot = (valBot >> 1);
                    }

                    gFrameBuffer[Line + 0][drawPos + k] |= valTop;
                    gFrameBuffer[Line + 1][drawPos + k] |= valBot;
                }
                // สระลอย วาดเสร็จไม่ต้องขยับ currentX */
				
				// สระลอย: ใช้ OR เพื่อไม่ให้ลบตัวเดิม
				// [กรณีสระซ้อนทับ] ใช้ OR (|=) เพื่อไม่ให้ลบตัวอักษรเดิมที่อยู่ข้างล่าง
                for (int k = 0; k < 7; k++) {
                    gFrameBuffer[Line + 0][drawPos + k] |= pTop[k];
                    gFrameBuffer[Line + 1][drawPos + k] |= pBot[k];
                }
				// *สำคัญ* วาดสระเสร็จ ไม่ต้องขยับ currentX (เพราะมันซ้อนทับ)
				// *สำคัญ* สระลอย วาดเสร็จแล้ว "ไม่ต้อง" ขยับ currentX 
				// เพื่อให้ตัวต่อไปมาต่อท้ายตัวพยัญชนะได้เลย
            } 
            else {
                // พยัญชนะ: วาดทับเลย (memcpy)
                memcpy(gFrameBuffer[Line + 0] + drawPos, pTop, 7);
                memcpy(gFrameBuffer[Line + 1] + drawPos, pBot, 7);

                // ขยับไป 8 พิกเซล (7 พิกเซลของตัวอักษร + 1 พิกเซลช่องว่าง)
                currentX += 8; 
            }
        }
        else 
        {
            // --- ภาษาอังกฤษ ---
            if (charCode > ' ' && charCode < 127) {
                const unsigned int index = charCode - ' ' - 1;
                memcpy(gFrameBuffer[Line + 0] + currentX, &gFontBig[index][0], 7);
                memcpy(gFrameBuffer[Line + 1] + currentX, &gFontBig[index][7], 7);
            }
            currentX += Width;
        }
    }
}

/* void UI_PrintStringSmall(const char *pString, uint8_t Start, uint8_t End, uint8_t Line, uint8_t char_width, const uint8_t *font)
{
    const size_t Length = strlen(pString);
    const unsigned int char_spacing = char_width + 1;

    if (End > Start) {
        Start += (((End - Start) - Length * char_spacing) + 1) / 2;
    }

    UI_PrintStringBuffer(pString, gFrameBuffer[Line] + Start, char_width, font);
} */

void UI_PrintStringSmall(const char *pString, uint8_t Start, uint8_t End, uint8_t Line, uint8_t char_width, const uint8_t *font)
{
    const size_t Length = strlen(pString);
    const unsigned int char_spacing = char_width + 1;

    // 1. ส่วนคำนวณตำแหน่ง (ใช้ของเดิม 100% ไม่แตะต้อง)
    if (End > Start) {
        Start += (((End - Start) - Length * char_spacing) + 1) / 2;
    }

    // 2. สร้าง Pointer ชี้ไปที่ตำแหน่งเริ่มวาดบนจอ
    uint8_t *pBuffer = gFrameBuffer[Line] + Start;

    // 3. วาดทีละตัว (แทนการเรียก UI_PrintStringBuffer)
    for (size_t i = 0; i < Length; i++)
    {
        uint8_t charCode = (uint8_t)pString[i];
        const uint8_t *pBitmap = 0;

        if (charCode >= 0xA1) {
            // --- ภาษาไทย ---
            // ดึงจากฟอนต์ไทย (ลบ 0xA1 เพื่อเริ่มที่ Index 0)
            pBitmap = THAI_FONT_SMALL_Normal[charCode - 0xA1];
        } 
        else {
            // --- ภาษาอังกฤษ ---
            // ถ้าเป็น Space (32) ปล่อย pBitmap เป็น 0 (ไม่วาด)
            // ถ้าเป็นตัวอักษรอื่น (33+) ให้คำนวณ Index
            if (charCode >= 33) {
                // ลบ 33 เพราะฟอนต์ใน font.c เริ่มที่ '!' (33)
                unsigned int index = charCode - 33; 
                pBitmap = font + (index * char_width);
            }
        }

        // ถ้ามีข้อมูลภาพ ให้วาดลงจอ (แปะทับลงไปเลยเหมือนเดิม)
        if (pBitmap) {
            memcpy(pBuffer + 1, pBitmap, char_width);
        }

        // ขยับ Pointer บนหน้าจอไปตำแหน่งถัดไป
        pBuffer += char_spacing;
    }
}


void UI_PrintStringSmallNormal(const char *pString, uint8_t Start, uint8_t End, uint8_t Line)
{
    UI_PrintStringSmall(pString, Start, End, Line, ARRAY_SIZE(gFontSmall[0]), (const uint8_t *)gFontSmall);
}

void UI_PrintStringSmallBold(const char *pString, uint8_t Start, uint8_t End, uint8_t Line)
{
#ifdef ENABLE_SMALL_BOLD
    const uint8_t *font = (uint8_t *)gFontSmallBold;
    const uint8_t char_width = ARRAY_SIZE(gFontSmallBold[0]);
#else
    const uint8_t *font = (uint8_t *)gFontSmall;
    const uint8_t char_width = ARRAY_SIZE(gFontSmall[0]);
#endif

    UI_PrintStringSmall(pString, Start, End, Line, char_width, font);
}

void UI_PrintStringSmallBufferNormal(const char *pString, uint8_t * buffer)
{
    UI_PrintStringBuffer(pString, buffer, ARRAY_SIZE(gFontSmall[0]), (uint8_t *)gFontSmall);
}

void UI_PrintStringSmallBufferBold(const char *pString, uint8_t * buffer)
{
#ifdef ENABLE_SMALL_BOLD
    const uint8_t *font = (uint8_t *)gFontSmallBold;
    const uint8_t char_width = ARRAY_SIZE(gFontSmallBold[0]);
#else
    const uint8_t *font = (uint8_t *)gFontSmall;
    const uint8_t char_width = ARRAY_SIZE(gFontSmall[0]);
#endif
    UI_PrintStringBuffer(pString, buffer, char_width, font);
}

void UI_DisplayFrequency(const char *string, uint8_t X, uint8_t Y, bool center)
{
    const unsigned int char_width  = 13;
    uint8_t           *pFb0        = gFrameBuffer[Y] + X;
    uint8_t           *pFb1        = pFb0 + 128;
    bool               bCanDisplay = false;

    uint8_t len = strlen(string);
    for(int i = 0; i < len; i++) {
        char c = string[i];
        if(c=='-') c = '9' + 1;
        if (bCanDisplay || c != ' ')
        {
            bCanDisplay = true;
            if(c>='0' && c<='9' + 1) {
                memcpy(pFb0 + 2, gFontBigDigits[c-'0'],                  char_width - 3);
                memcpy(pFb1 + 2, gFontBigDigits[c-'0'] + char_width - 3, char_width - 3);
            }
            else if(c=='.') {
                *pFb1 = 0x60; pFb0++; pFb1++;
                *pFb1 = 0x60; pFb0++; pFb1++;
                *pFb1 = 0x60; pFb0++; pFb1++;
                continue;
            }

        }
        else if (center) {
            pFb0 -= 6;
            pFb1 -= 6;
        }
        pFb0 += char_width;
        pFb1 += char_width;
    }
}

/*
void UI_DisplayFrequency(const char *string, uint8_t X, uint8_t Y, bool center)
{
    const unsigned int char_width  = 13;
    uint8_t           *pFb0        = gFrameBuffer[Y] + X;
    uint8_t           *pFb1        = pFb0 + 128;
    bool               bCanDisplay = false;

    if (center) {
        uint8_t len = 0;
        for (const char *ptr = string; *ptr; ptr++)
            if (*ptr != ' ') len++; // Ignores spaces for centering

        X -= (len * char_width) / 2; // Centering adjustment
        pFb0 = gFrameBuffer[Y] + X;
        pFb1 = pFb0 + 128;
    }

    for (; *string; string++) {
        char c = *string;
        if (c == '-') c = '9' + 1; // Remap of '-' symbol

        if (bCanDisplay || c != ' ') {
            bCanDisplay = true;
            if (c >= '0' && c <= '9' + 1) {
                memcpy(pFb0 + 2, gFontBigDigits[c - '0'], char_width - 3);
                memcpy(pFb1 + 2, gFontBigDigits[c - '0'] + char_width - 3, char_width - 3);
            } else if (c == '.') {
                memset(pFb1, 0x60, 3); // Replaces the three assignments
                pFb0 += 3;
                pFb1 += 3;
                continue;
            }
        }
        pFb0 += char_width;
        pFb1 += char_width;
    }
}
*/

void UI_DrawPixelBuffer(uint8_t (*buffer)[128], uint8_t x, uint8_t y, bool black)
{
    const uint8_t pattern = 1 << (y % 8);
    if(black)
        buffer[y/8][x] |= pattern;
    else
        buffer[y/8][x] &= ~pattern;
}

static void sort(int16_t *a, int16_t *b)
{
    if(*a > *b) {
        int16_t t = *a;
        *a = *b;
        *b = t;
    }
}

#ifdef ENABLE_FEAT_F4HWN
    /*
    void UI_DrawLineDottedBuffer(uint8_t (*buffer)[128], int16_t x1, int16_t y1, int16_t x2, int16_t y2, bool black)
    {
        if(x2==x1) {
            sort(&y1, &y2);
            for(int16_t i = y1; i <= y2; i+=2) {
                UI_DrawPixelBuffer(buffer, x1, i, black);
            }
        } else {
            const int multipl = 1000;
            int a = (y2-y1)*multipl / (x2-x1);
            int b = y1 - a * x1 / multipl;

            sort(&x1, &x2);
            for(int i = x1; i<= x2; i+=2)
            {
                UI_DrawPixelBuffer(buffer, i, i*a/multipl +b, black);
            }
        }
    }
    */

    void PutPixel(uint8_t x, uint8_t y, bool fill) {
      UI_DrawPixelBuffer(gFrameBuffer, x, y, fill);
    }

    void PutPixelStatus(uint8_t x, uint8_t y, bool fill) {
      UI_DrawPixelBuffer(&gStatusLine, x, y, fill);
    }

    void GUI_DisplaySmallest(const char *pString, uint8_t x, uint8_t y,
                                    bool statusbar, bool fill) {
      uint8_t c;
      uint8_t pixels;
      const uint8_t *p = (const uint8_t *)pString;

      while ((c = *p++) && c != '\0') {
		
		// สร้าง Pointer เพื่อชี้ไปยังข้อมูลของตัวอักษรนั้นๆ
        const uint8_t *pGlyph;
		
		if (c >= 0xA1) {
            // --- กรณีภาษาไทย ---
            // ดึงจาก THAI_FONT_3x5 (ลบ 0xA1 เพื่อเริ่ม Index 0)
            pGlyph = THAI_FONT_3x5[c - 0xA1];
        } 
        else {
            // --- กรณีภาษาอังกฤษ (เดิม) ---
            // ป้องกัน index ติดลบ
            if (c < 0x20) c = 0x20; 
            
            // ดึงจาก gFont3x5 (ลบ 0x20 ตาม Logic เดิม)
            pGlyph = gFont3x5[c - 0x20];
        }
		
        //c -= 0x20;
		
		// วาดตัวอักษรทีละคอลัมน์ (i = 0 ถึง 2)
        for (int i = 0; i < 3; ++i) {
          //pixels = gFont3x5[c][i];
		  
		  pixels = pGlyph[i]; // ดึงข้อมูลจาก Pointer ที่เราเลือกไว้ข้างบน
		  
          for (int j = 0; j < 6; ++j) {
            if (pixels & 1) {
              if (statusbar)
                PutPixelStatus(x + i, y + j, fill);
              else
                PutPixel(x + i, y + j, fill);
            }
            pixels >>= 1;
          }
        }
		// ขยับไปตัวถัดไป 4 pixel (กว้าง 3 + เว้น 1)
        x += 4;
      }
    }
#endif
    
void UI_DrawLineBuffer(uint8_t (*buffer)[128], int16_t x1, int16_t y1, int16_t x2, int16_t y2, bool black)
{
    if(x2==x1) {
        sort(&y1, &y2);
        for(int16_t i = y1; i <= y2; i++) {
            UI_DrawPixelBuffer(buffer, x1, i, black);
        }
    } else {
        const int multipl = 1000;
        int a = (y2-y1)*multipl / (x2-x1);
        int b = y1 - a * x1 / multipl;

        sort(&x1, &x2);
        for(int i = x1; i<= x2; i++)
        {
            UI_DrawPixelBuffer(buffer, i, i*a/multipl +b, black);
        }
    }
}

void UI_DrawRectangleBuffer(uint8_t (*buffer)[128], int16_t x1, int16_t y1, int16_t x2, int16_t y2, bool black)
{
    UI_DrawLineBuffer(buffer, x1,y1, x1,y2, black);
    UI_DrawLineBuffer(buffer, x1,y1, x2,y1, black);
    UI_DrawLineBuffer(buffer, x2,y1, x2,y2, black);
    UI_DrawLineBuffer(buffer, x1,y2, x2,y2, black);
}


void UI_DisplayPopup(const char *string)
{
    UI_DisplayClear();

    // for(uint8_t i = 1; i < 5; i++) {
    //  memset(gFrameBuffer[i]+8, 0x00, 111);
    // }

    // for(uint8_t x = 10; x < 118; x++) {
    //  UI_DrawPixelBuffer(x, 10, true);
    //  UI_DrawPixelBuffer(x, 46-9, true);
    // }

    // for(uint8_t y = 11; y < 37; y++) {
    //  UI_DrawPixelBuffer(10, y, true);
    //  UI_DrawPixelBuffer(117, y, true);
    // }
    // DrawRectangle(9,9, 118,38, true);
    UI_PrintString(string, 9, 118, 2, 8);
    UI_PrintStringSmallNormal("Press EXIT", 9, 118, 6);
}

void UI_DisplayClear()
{
    memset(gFrameBuffer, 0, sizeof(gFrameBuffer));
}
