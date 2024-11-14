#include <Wire.h>
#include <MPU6050.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// OLED 設定
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_DC 9
#define OLED_CS 10
#define OLED_RESET 8
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &SPI, OLED_DC, OLED_RESET, OLED_CS);

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
  
  // 初始化OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  display.clearDisplay();
  display.setTextSize(1.5);
  display.setTextColor(SSD1306_BLACK, SSD1306_WHITE); // 黑色文字，白色背景
  
  // 初始化MPU6050
  mpu.initialize();
  
  // 檢查MPU6050連接
  if (!mpu.testConnection()) {
    display.println("MPU6050 connection failed");
    display.display();
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
  
  // 填充整個畫面為白色
  display.fillScreen(SSD1306_WHITE);
  
  // 設定游標位置並顯示數據
  display.setCursor(10, 0);
  
  // 根據顯示模式顯示不同數據
  if (displayMode == 0) {
    // 顯示加速度計數據
    display.println("Accelerometer:");
    display.print("X: "); display.println(ax);
    display.print("Y: "); display.println(ay);
    display.print("Z: "); display.println(az);
  } else {
    // 顯示陀螺儀數據
    display.println("Gyroscope:");
    display.print("X: "); display.println(gx);
    display.print("Y: "); display.println(gy);
    display.print("Z: "); display.println(gz);
  }
  
  // 更新顯示
  display.display();
  
  // 短暫延遲以避免刷新太快
  delay(100);
}
