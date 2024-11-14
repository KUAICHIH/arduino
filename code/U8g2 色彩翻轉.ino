#include <Wire.h>
#include <MPU6050.h>
#include <U8g2lib.h>

// 初始化 U8g2 OLED 設置 (使用硬體 SPI)
U8G2_SSD1306_128X64_NONAME_F_4W_HW_SPI u8g2(U8G2_R0, /* cs=*/ 10, /* dc=*/ 9, /* reset=*/ 8);

// MPU6050 物件
MPU6050 mpu;

// 按鈕設定
const int BUTTON_PIN = 2;
int displayMode = 0;  // 顯示模式
bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

void setup() {
  // 初始化I2C
  Wire.begin();
  
  // 初始化按鈕
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // 初始化U8g2顯示器
  u8g2.begin();

  // 初始化MPU6050
  mpu.initialize();
  
  // 檢查MPU6050連接
  if (!mpu.testConnection()) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(0, 20, "MPU6050 connection failed");
    u8g2.sendBuffer();
    while(1);
  }
}

void loop() {
  // 讀取MPU6050數據
  int16_t ax, ay, az;
  int16_t gx, gy, gz;
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
  
  // 按鈕處理
  int reading = digitalRead(BUTTON_PIN);
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading == LOW) {
      displayMode = (displayMode + 1) % 2;
    }
  }
  lastButtonState = reading;
  
  // 清除緩衝區並填滿螢幕
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);  // 設置顏色為白
  u8g2.drawBox(0, 0, 128, 64); // 填滿螢幕

  // 顯示反色文字
  u8g2.setDrawColor(0);  // 設置顏色為黑色，顯示反色文字
  if (displayMode == 0) {
    // 設置大字體顯示標題
    u8g2.setFont(u8g2_font_ncenB10_tr);
    u8g2.drawStr(0, 14, "GymGram-270F");
    
    // 設置小字體顯示數據
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.setCursor(0, 30); u8g2.print("Banana ");
    u8g2.setCursor(0, 45); u8g2.print("Push Up : 15");
    u8g2.setCursor(0, 60); u8g2.print("Time : 59");
  } else {
    // 顯示陀螺儀數據
    u8g2.setFont(u8g2_font_ncenB14_tr);
    u8g2.drawStr(0, 14, "Gyroscope:");
    u8g2.setFont(u8g2_font_ncenB10_tr);
    u8g2.setCursor(0, 32); u8g2.print("X: "); u8g2.print(gx);
    u8g2.setCursor(0, 44); u8g2.print("Y: "); u8g2.print(gy);
    u8g2.setCursor(0, 56); u8g2.print("Z: "); u8g2.print(gz);
  }

  // 更新顯示
  u8g2.sendBuffer();

  // 短暫延遲以避免刷新太快
  delay(100);
}
