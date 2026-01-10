#ifndef APP_GPS_H
#define APP_GPS_H

#include <stdint.h>
#include <stdbool.h>

// 1. โครงสร้างข้อมูลสำหรับ Smart Beacon (ตัวแปรย่อย)
typedef struct {
    int min_interval_s;  
    int max_interval_s;  
    int low_speed_kmh;   
    int high_speed_kmh;  
    int turn_angle;      
} SmartBeacon_t;

// 2. โครงสร้างข้อมูลหลักสำหรับการตั้งค่า APRS
typedef struct {
    bool aprs_on;           // เปิด/ปิด
    char callsign[10];      // E25WOP
    int  ssid;              // 7
    char dest_call[10];     // APDR16
    char digipath[20];      // WIDE1-1
    char icon_table;        // /
    char icon_symbol;       // [
    char comment[40];       // Comment
    
    uint32_t tx_freq;       // 144.390
    uint8_t tx_power;       // 1=Mid
    
    bool use_gps;           // ใช้ GPS หรือ Fixed
    char fixed_lat[12];     
    char fixed_lon[12];     

    bool smart_beacon;      // เปิด Smart Beacon
    int  manual_interval;   // วินาที
	int preamble;
	
	int bl_time;            // *** เพิ่มตัวแปรนี้: เวลาดับไฟ (วินาที), 0=เปิดตลอด ***
	
    SmartBeacon_t sb_conf;  // ค่าตั้ง Smart Beacon
} APRS_Config_t;

// 3. ประกาศตัวแปร Global ให้ไฟล์อื่น (app_aprs.c) มองเห็น
extern APRS_Config_t aprs_config;

extern char val_lat[16];
extern char val_lon[16];

extern char val_alt[10];
extern char val_speed[10];
extern char val_course[10];

// 4. ฟังก์ชันหลัก
void APP_RunGPS(void);

// 5. ประกาศฟังก์ชันส่ง (เพื่อให้ gps_app.c เรียกใช้ได้)
void APRS_SendBeacon_Now(void); 

void APRS_Send_Message(char *target_call, char *message_text);

#endif /* APP_GPS_H */
