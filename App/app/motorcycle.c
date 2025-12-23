/* Copyright 2025 Andrej A (Tunas1337)
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

#include "app/motorcycle.h"

// Game state
static bool gMotoInitialized = false;
static bool gMotoPaused = false;
static bool gGameOver = false;

static Motorcycle gMoto;
static GameState gGame;

static KeyboardState gMotoKbd = {KEY_INVALID, KEY_INVALID, 0};

static char gMotoStr[16];

// Physics constants
#define GRAVITY_EFFECT      11    // How much slope affects tilt per frame
#define PLAYER_TILT_POWER   4    // How much player input affects tilt
#define TILT_DAMPING        2    // Friction/air resistance
#define CRASH_ANGLE         89   // Angle at which game over occurs

// Terrain buffer - stores height of ground at each x position
// Heights are from bottom of screen (higher value = higher ground)
#define TERRAIN_BUFFER_SIZE 256
static int8_t gTerrain[TERRAIN_BUFFER_SIZE];
static uint16_t gTerrainOffset = 0;   // Current scroll position in terrain buffer

// RSSI-based terrain generation
static uint32_t gScanFreq = 0;        // Current frequency being scanned
static uint32_t gScanFreqStart = 0;   // Start of frequency range
static uint32_t gScanFreqEnd = 0;     // End of frequency range
static uint32_t gScanStep = 0;        // Frequency step size
static uint8_t  gRssiScanIdx = 0;     // Index in scan for generating terrain

// Hill generation parameters
#define HILL_MIN_HEIGHT     1
#define HILL_MAX_HEIGHT     55

// Motorcycle sprite points (relative coordinates centered at 0,0)
// Y positive = UP (rider is above wheels)
typedef struct {
    int8_t x;
    int8_t y;
} Point;

// Motorcycle defined as line segments for easy rotation
// Format: pairs of points defining line segments
// Note: positive Y is UP in sprite space, we flip when drawing
static const Point MOTO_LINES[] = {
    // Rear wheel - centered around x=-4
    {-6, 0}, {-5, 2},
    {-5, 2}, {-3, 2},
    {-3, 2}, {-2, 0},
    {-2, 0}, {-3, -2},
    {-3, -2}, {-5, -2},
    {-5, -2}, {-6, 0},
    // Front wheel - centered around x=6
    {4, 0}, {5, 2},
    {5, 2}, {7, 2},
    {7, 2}, {8, 0},
    {8, 0}, {7, -2},
    {7, -2}, {5, -2},
    {5, -2}, {4, 0},
    // Frame connecting wheels (bottom of bike)
    {-4, 0}, {6, 0},
    // Seat/body frame
    {-2, 0}, {-2, 4},
    {-2, 4}, {2, 4},
    {2, 4}, {4, 2},
    // Rider body (sitting on seat)
    {0, 4}, {0, 8},
    // Rider head
    {-1, 8}, {1, 9},
    {1, 9}, {1, 8},
    // Arms to handlebars
    {0, 6}, {3, 5},
};
#define MOTO_LINE_COUNT (sizeof(MOTO_LINES) / sizeof(MOTO_LINES[0]) / 2)

// Sine table for rotation (scaled by 128 for fixed-point math)
// Covers 0-90 degrees in 5-degree increments
static const int8_t SIN_TABLE[] = {
    1,    // 0 deg
    11,   // 5 deg
    22,   // 10 deg
    33,   // 15 deg
    44,   // 20 deg
    55,   // 25 deg
    64,   // 30 deg
    73,   // 35 deg
    81,   // 40 deg
    88,   // 45 deg
    94,   // 50 deg
    100,  // 55 deg
    104,  // 60 deg
    108,  // 65 deg
    112,  // 70 deg
    116,  // 75 deg
    120,  // 80 deg
    124,  // 85 deg
    128,  // 90 deg
};
#define SIN_TABLE_SIZE (sizeof(SIN_TABLE) / sizeof(SIN_TABLE[0]))

// Get sin value for angle (degrees, any range)
// Returns value scaled by 128
static int16_t getSin(int16_t angle) {
    // Normalize angle to 0-359
    while (angle < 0) angle += 360;
    while (angle >= 360) angle -= 360;
    
    bool negative = false;
    if (angle > 180) {
        angle = 360 - angle;
        negative = true;
    }
    if (angle > 90) {
        angle = 180 - angle;
    }
    
    int idx = angle / 5;
    if (idx >= (int)SIN_TABLE_SIZE) idx = SIN_TABLE_SIZE - 1;
    
    int16_t val = SIN_TABLE[idx];
    return negative ? -val : val;
}

// Get cos value for angle (degrees, any range)
// Returns value scaled by 128
static int16_t getCos(int16_t angle) {
    // cos(x) = sin(90 - x)
    return getSin(90 - angle);
}

// Read RSSI at a given frequency and return terrain height
static int8_t getFreqRssiAsHeight(uint32_t freq) {
    // Set frequency
    BK4819_SetFrequency(freq);
    BK4819_PickRXFilterPathBasedOnFrequency(freq);
    
    // Brief delay for RSSI to settle
    SYSTICK_DelayUs(800);
    
    // Read RSSI (0-511 typical range)
    uint16_t rssi = BK4819_GetRSSI();
    
    // Map RSSI to height (RSSI ~50-200 typical)
    int height = HILL_MIN_HEIGHT + ((rssi * (HILL_MAX_HEIGHT - HILL_MIN_HEIGHT)) / 256);
    
    if (height < HILL_MIN_HEIGHT) height = HILL_MIN_HEIGHT;
    if (height > HILL_MAX_HEIGHT) height = HILL_MAX_HEIGHT;
    
    return (int8_t)height;
}

// Initialize frequency scan range
static void initFreqScan(void) {
    gScanFreqStart = 43000000;  // 430.00000 MHz in 10Hz units
    gScanFreqEnd   = 44000000;  // 440.00000 MHz
    gScanStep      = 25000;      // 25 kHz steps (in 10Hz units)
    gScanFreq      = gScanFreqStart;
    gRssiScanIdx   = 0;
}

// Generate one terrain sample from RSSI
static void generateTerrainFromRssi(uint16_t idx) {
    
    int8_t height = getFreqRssiAsHeight(gScanFreq);
    
    // Smooth with previous value to avoid jarring transitions
    if (idx > 0) {
        uint16_t prevIdx = (idx - 1) % TERRAIN_BUFFER_SIZE;
        int8_t prevHeight = gTerrain[prevIdx];
        height = (height + prevHeight * 2) / 3;
    }
    
    gTerrain[idx % TERRAIN_BUFFER_SIZE] = height;
    
    // Advance frequency
    gScanFreq += gScanStep;
    if (gScanFreq > gScanFreqEnd) {
        gScanFreq = gScanFreqStart;  // Wrap around
    }
}

// Draw the status/score line
static void drawMotoStatus(void) {
    memset(gStatusLine, 0, sizeof(gStatusLine));
    
    // Distance traveled
    sprintf(gMotoStr, "Dist: %04u", gGame.distance);
    GUI_DisplaySmallest(gMotoStr, 0, 1, true, true);
    
    // Current frequency being scanned (in MHz)
    sprintf(gMotoStr, "%03lu.%02lu", gScanFreq / 100000, (gScanFreq / 1000) % 100);
    GUI_DisplaySmallest(gMotoStr, 50, 1, true, true);
    
    // Tilt indicator
    sprintf(gMotoStr, "T:%+3d", gMoto.tilt);
    GUI_DisplaySmallest(gMotoStr, 100, 1, true, true);
}

// Initialize terrain - start with flat, will fill with RSSI data
static void initTerrain(void) {
    // Initialize frequency scan
    initFreqScan();
    
    // Start with flat ground for the whole buffer
    for (int i = 0; i < TERRAIN_BUFFER_SIZE; i++) {
        gTerrain[i] = HILL_MIN_HEIGHT;
    }
    
    // Pre-generate terrain for visible area plus buffer using RSSI
    // This may take a moment as we scan frequencies
    for (int i = 0; i < MOTO_SCREEN_WIDTH + 40; i++) {
        generateTerrainFromRssi(i);
    }
    
    gTerrainOffset = 0;
}

// Get terrain height at screen x position
static int8_t getTerrainHeight(int16_t screenX) {
    int16_t terrainIdx = (gTerrainOffset + screenX) % TERRAIN_BUFFER_SIZE;
    while (terrainIdx < 0) terrainIdx += TERRAIN_BUFFER_SIZE;
    while (terrainIdx >= TERRAIN_BUFFER_SIZE) terrainIdx -= TERRAIN_BUFFER_SIZE;
    return gTerrain[terrainIdx];
}

// Get terrain slope at screen x position (returns angle in degrees, roughly)
static int8_t getTerrainSlope(int16_t screenX) {
    int8_t h1 = getTerrainHeight(screenX - 4);
    int8_t h2 = getTerrainHeight(screenX + 4);
    // Convert height difference to approximate angle
    // 8 pixels horizontal, diff in vertical
    int8_t diff = h2 - h1;
    // Rough conversion: each pixel diff ≈ 7 degrees for 8px horizontal
    return -(diff * 7) / 2;  // Negative because higher on right = tilting right
}

// Draw the terrain/hills
static void drawTerrain(void) {
    for (int16_t x = 0; x < MOTO_SCREEN_WIDTH; x++) {
        int8_t height = getTerrainHeight(x);
        int16_t groundY = MOTO_SCREEN_HEIGHT - height;
        
        // Draw ground from groundY to bottom of screen
        for (int16_t y = groundY; y < MOTO_SCREEN_HEIGHT; y++) {
            UI_DrawPixelBuffer(gFrameBuffer, x, y, true);
        }
    }
}

// Draw a line with rotation applied
// Sprite Y is positive-up, screen Y is positive-down, so we flip Y
static void drawRotatedLine(int16_t cx, int16_t cy, int8_t x1, int8_t y1, int8_t x2, int8_t y2, int16_t angle) {
    int16_t sinA = getSin(angle);
    int16_t cosA = getCos(angle);
    
    // Rotate point 1 (note: sprite y is positive-up, so negate for rotation)
    int16_t rx1 = (x1 * cosA + y1 * sinA) / 128;
    int16_t ry1 = (-x1 * sinA + y1 * cosA) / 128;
    
    // Rotate point 2
    int16_t rx2 = (x2 * cosA + y2 * sinA) / 128;
    int16_t ry2 = (-x2 * sinA + y2 * cosA) / 128;
    
    // Translate to world position (flip Y: subtract because screen Y goes down)
    int16_t sx1 = cx + rx1;
    int16_t sy1 = cy - ry1;
    int16_t sx2 = cx + rx2;
    int16_t sy2 = cy - ry2;
    
    // Clamp to screen bounds
    if (sx1 < 0) sx1 = 0;
    if (sx1 >= MOTO_SCREEN_WIDTH) sx1 = MOTO_SCREEN_WIDTH - 1;
    if (sx2 < 0) sx2 = 0;
    if (sx2 >= MOTO_SCREEN_WIDTH) sx2 = MOTO_SCREEN_WIDTH - 1;
    if (sy1 < 0) sy1 = 0;
    if (sy1 >= MOTO_SCREEN_HEIGHT) sy1 = MOTO_SCREEN_HEIGHT - 1;
    if (sy2 < 0) sy2 = 0;
    if (sy2 >= MOTO_SCREEN_HEIGHT) sy2 = MOTO_SCREEN_HEIGHT - 1;
    
    UI_DrawLineBuffer(gFrameBuffer, sx1, sy1, sx2, sy2, true);
}

// Draw the motorcycle at its current position with rotation
static void drawMotorcycle(void) {
    // Get terrain height at motorcycle position
    int8_t terrainHeight = getTerrainHeight(gMoto.x);
    
    // Position motorcycle on terrain
    int16_t cx = gMoto.x;
    int16_t cy = MOTO_SCREEN_HEIGHT - terrainHeight - 3;  // 3 pixels above ground
    
    // Use the motorcycle's current tilt directly (already includes physics)
    int16_t totalAngle = gMoto.tilt;
    
    // Draw each line segment of the motorcycle
    for (size_t i = 0; i < MOTO_LINE_COUNT; i++) {
        const Point *p1 = &MOTO_LINES[i * 2];
        const Point *p2 = &MOTO_LINES[i * 2 + 1];
        drawRotatedLine(cx, cy, p1->x, p1->y, p2->x, p2->y, totalAngle);
    }
}

// Initialize game state
static void initGame(void) {
    // Initialize terrain first
    initTerrain();
    
    // Initialize motorcycle
    gMoto.x = 32;           // Left side of screen
    gMoto.y = GROUND_Y - 2; // Will be adjusted by terrain
    gMoto.tilt = 0;         // Level
    gMoto.velocity_y = 0;
    gMoto.wheel_anim = 0;
    
    // Initialize game state
    gGame.scroll_offset = 0;
    gGame.distance = 0;
    
    gMotoPaused = false;
    gGameOver = false;
}

// Handle key press
static void onMotoKeyDown(uint8_t key) {
    bool wasPaused = gMotoPaused;
    
    // If game over, only respond to key 5 (restart) or EXIT
    if (gGameOver) {
        if (key == KEY_5) {
            // Restart game
            UI_DisplayClear();
            initGame();
        } else if (key == KEY_EXIT) {
            gMotoInitialized = false;
        }
        return;
    }
    
    switch (key) {
        case KEY_UP:
        case KEY_4:
            // Tilt left (counter-clockwise) - apply angular momentum
            if (!gMotoPaused) {
                gMoto.tilt -= PLAYER_TILT_POWER;
            }
            gMotoPaused = false;
            break;
            
        case KEY_DOWN:
        case KEY_6:
            // Tilt right (clockwise) - apply angular momentum
            if (!gMotoPaused) {
                gMoto.tilt += PLAYER_TILT_POWER;
            }
            gMotoPaused = false;
            break;
            
        case KEY_5:
            // Restart game
            UI_DisplayClear();
            initGame();
            break;
            
        case KEY_MENU:
            gMotoPaused = !gMotoPaused;
            gMotoKbd.counter = 0;
            if (gMotoPaused) {
                UI_PrintStringSmallBold("PAUSE", 0, 128, 3);
            }
            break;
            
        case KEY_EXIT:
            gMotoPaused = false;
            gMotoInitialized = false;
            break;
    }
    
    if (wasPaused && !gMotoPaused) {
        // Clear pause text
        for (uint8_t i = 0; i < 8; i++) {
            UI_DrawLineBuffer(gFrameBuffer, 32, 24 + i, 96, 24 + i, false);
        }
    }
}

// Poll keyboard
static KEY_Code_t getMotoKey(void) {
    KEY_Code_t btn = KEYBOARD_Poll();
    if (btn == KEY_INVALID && GPIO_IsPttPressed()) {
        btn = KEY_PTT;
    }
    return btn;
}

// Handle user input
static bool handleMotoInput(void) {
    gMotoKbd.prev = gMotoKbd.current;
    gMotoKbd.current = getMotoKey();
    
    if (gMotoKbd.current != KEY_INVALID && gMotoKbd.current == gMotoKbd.prev) {
        gMotoKbd.counter = 1;
    } else {
        gMotoKbd.counter = 0;
    }
    
    if (gMotoKbd.counter == 1) {
        onMotoKeyDown(gMotoKbd.current);
        
        if (gMotoKbd.current == KEY_MENU) {
            gMotoKbd.counter = 0;
            SYSTEM_DelayMs(250);
        }
    }
    
    return true;
}

// Game tick
static void motoTick(void) {
    handleMotoInput();
    handleMotoInput();
}

// Update game physics
static void updateGame(void) {
    // Update scroll offset (terrain moves)
    gGame.scroll_offset += ROAD_SCROLL_SPEED;
    gTerrainOffset += ROAD_SCROLL_SPEED;
    
    // Keep offset within buffer bounds using circular buffer
    while (gTerrainOffset >= TERRAIN_BUFFER_SIZE) {
        gTerrainOffset -= TERRAIN_BUFFER_SIZE;
    }
    
    // Generate new terrain ahead using RSSI readings
    // Generate a few samples ahead of visible area each frame
    for (int i = 0; i < ROAD_SCROLL_SPEED + 1; i++) {
        uint16_t genIdx = (gTerrainOffset + MOTO_SCREEN_WIDTH + 20 + i) % TERRAIN_BUFFER_SIZE;
        generateTerrainFromRssi(genIdx);
    }
    
    // Get terrain slope at motorcycle position
    int8_t terrainSlope = getTerrainSlope(gMoto.x);
    
    // Apply physics: terrain slope affects tilt (like gravity pulling the bike)
    // Positive slope (going uphill, front higher) = tends to tilt backward (positive)
    // Negative slope (going downhill, front lower) = tends to tilt forward (negative)
    gMoto.tilt += (terrainSlope * GRAVITY_EFFECT) / 10;
    
    // Apply damping (air resistance / friction) - slowly reduces extreme tilts
    if (gMoto.tilt > TILT_DAMPING) {
        gMoto.tilt -= TILT_DAMPING;
    } else if (gMoto.tilt < -TILT_DAMPING) {
        gMoto.tilt += TILT_DAMPING;
    }
    
    // Check for crash
    if (gMoto.tilt >= CRASH_ANGLE || gMoto.tilt <= -CRASH_ANGLE) {
        gGameOver = true;
    }
    
    // Update distance
    gGame.distance++;
    
    // Animate wheels
    gMoto.wheel_anim++;
}

// Main game loop
void APP_RunMotorcycle(void) {
    // Turn off LEDs
    BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, false);
    
    // Initialize game
    UI_DisplayClear();
    initGame();
    memset(gStatusLine, 0, sizeof(gStatusLine));
    gMotoInitialized = true;
    
    while (gMotoInitialized) {
        motoTick();
        
        if (gGameOver) {
            // Game over screen
            UI_DisplayClear();
            drawMotoStatus();
            drawTerrain();
            drawMotorcycle();  // Draw crashed motorcycle
            
            // Display game over message
            UI_PrintStringSmallBold("GAME OVER", 0, 128, 2);
            sprintf(gMotoStr, "Score: %u", gGame.distance);
            UI_PrintStringSmallBold(gMotoStr, 0, 128, 4);
            UI_PrintStringSmallBold("5=Restart", 0, 128, 6);
            
            ST7565_BlitStatusLine();
            ST7565_BlitFullScreen();
            SYSTEM_DelayMs(100);
        } else if (!gMotoPaused) {
            // Update game state
            updateGame();
            
            // Clear screen (except status line)
            UI_DisplayClear();
            
            // Draw game elements
            drawMotoStatus();
            drawTerrain();
            drawMotorcycle();
            
            // Frame delay
            SYSTEM_DelayMs(25);
            
            // Update display
            ST7565_BlitStatusLine();
            ST7565_BlitFullScreen();
        } else {
            // Paused - just update display
            ST7565_BlitStatusLine();
            ST7565_BlitFullScreen();
            SYSTEM_DelayMs(50);
        }
    }
}