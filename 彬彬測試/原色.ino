#include <Wire.h>
#include <MPU6050_tockn.h>
#include <U8g2lib.h>

HardwareSerial &S = Serial;

// 機器設定
String machinename = "GymGram-270F";
String machineid = "23f83a00";
String displayName = "banana";

// MPU6050, LED, OLED
MPU6050 mpu(Wire);
const int redPin = 5;
const int greenPin = 6;
const int bluePin = 7;
const int buttonPin = 2;
U8G2_SSD1306_128X64_NONAME_F_4W_HW_SPI u8g2(U8G2_R0, /* cs=*/ 10, /* dc=*/ 9, /* reset=*/ 8);

// 動作計數
int count_push_up = 0;
int count_sit_up = 0;
int count_squat = 0;

// 狀態模式
bool isDown = false;
bool isDetecting = false;
bool isCountdown = false;
bool isCountdownPaused = false;
bool hasStartedCountdown = false;
bool isUserVerified = true;  // 直接設為 true，因為是單機版

// 按鈕控制相關
unsigned long buttonPressStartTime = 0;
const unsigned long LONG_PRESS_TIME = 1000;
bool isLongPress = false;
bool firstMotionDetected = false;  
unsigned long lastMotionTime = 0;    
unsigned long exerciseTime = 0;      
const unsigned long TIMEOUT_DURATION = 15000; // 10秒沒動作就超時
bool isExercising = false;          

unsigned long lastSecondUpdate;
unsigned long countdownStart = 0;
unsigned long remainingTime = 0;
int countdownTime = 10;

// 動作列舉
enum Mode { PUSH_UP, SIT_UP, SQUAT };
Mode currentMode = PUSH_UP;

// Color set
const int redColor[3] = {255, 0, 0};
const int blueColor[3] = {0, 0, 255};
const int purpleColor[3] = {255, 0, 255};
const int yellowColor[3] = {255, 255, 0};
const int greenColor[3] = {0, 255, 0};

// 重製區
void resetAllStates() {
    S.println("\n[Reset] Resetting all states...");
    
    isUserVerified = true;
    isDetecting = false;
    isCountdown = false;
    isCountdownPaused = false;
    hasStartedCountdown = false;
    firstMotionDetected = false;
    isExercising = false;
    
    count_push_up = 0;
    count_sit_up = 0;
    count_squat = 0;
    exerciseTime = 0;
    lastMotionTime = 0;
    
    displayText();
    setColor(0, 0, 0);
}

void setColor(int red, int green, int blue) {
  analogWrite(redPin, red);
  analogWrite(greenPin, green);
  analogWrite(bluePin, blue);
}

void flashOnce(const int color[3]) {
  setColor(color[0], color[1], color[2]);
  delay(500);
  setColor(0, 0, 0);
}

void displayText() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);

  u8g2.drawStr(0, 10, machinename.c_str());
  u8g2.drawStr(0, 25, displayName.c_str());

  u8g2.setCursor(0, 40);
  switch (currentMode) {
    case PUSH_UP:
      u8g2.print("Push Up: ");
      u8g2.print(count_push_up);
      break;
    case SIT_UP:
      u8g2.print("Sit Up: ");
      u8g2.print(count_sit_up);
      break;
    case SQUAT:
      u8g2.print("Squat: ");
      u8g2.print(count_squat);
      break;
  }

  if (isExercising) {
    u8g2.setCursor(0, 55);
    u8g2.print("Time: ");
    u8g2.print(exerciseTime);
    u8g2.print("s");
  }

  u8g2.sendBuffer();
}

void detectMotion() {
  if (!isDetecting) return;
  if (isCountdownPaused) return;
  
  mpu.update();
  unsigned long currentTime = millis();
  bool motionDetected = false;
  
  if (currentMode == PUSH_UP) {
    if (mpu.getAngleX() > 30) {
      if (!isDown) {
        count_push_up++;
        isDown = true;
        motionDetected = true;
        S.println("伏地挺身次數: " + String(count_push_up));
      }
    } else if (mpu.getAngleX() < 10) {
      isDown = false;
    }
  }
  
  else if (currentMode == SIT_UP) {
    if (mpu.getAngleY() > 45) {
      if (!isDown) {
        count_sit_up++;
        isDown = true;
        motionDetected = true;
        S.println("仰臥起坐次數: " + String(count_sit_up));
      }
    } else if (mpu.getAngleY() < 20) {
      isDown = false;
    }
  }
  
  else if (currentMode == SQUAT) {
    if (mpu.getAngleX() > 30) {
      if (!isDown) {
        count_squat++;
        isDown = true;
        motionDetected = true;
        S.println("深蹲次數: " + String(count_squat));
      }
    } else if (mpu.getAngleX() < 10) {
      isDown = false;
    }
  }

  if (motionDetected) {
    if (!isExercising) {
      isExercising = true;
      exerciseTime = 0;
    }
    lastMotionTime = currentTime;
    displayText();
  }

  if (isExercising && (currentTime - lastMotionTime >= TIMEOUT_DURATION)) {
    isDetecting = false;
    isExercising = false;
    if (exerciseTime >= 10) {  // 如果運動時間大於等於10秒，則扣除10秒
      exerciseTime -= 10;
    }
    setColor(greenColor[0], greenColor[1], greenColor[2]);
    
    // 單機版本移除上傳部分
    resetAllStates();
  }
}

void setup() {
  S.begin(115200);
  Wire.begin();
  mpu.begin();
  u8g2.begin();
  mpu.calcGyroOffsets(true);
  
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);

  // 初始化狀態
  isUserVerified = true;
  isDetecting = false;
  isCountdown = false;
  isCountdownPaused = false;
  hasStartedCountdown = false;
  
  setColor(0, 0, 0);
  displayText();
}

void loop() {
    static unsigned long lastTimeUpdate = 0;
    unsigned long currentMillis = millis();

    // 按鈕狀態處理
    static bool lastButtonState = HIGH;
    bool buttonState = digitalRead(buttonPin);
    
    // 按鈕按下
    if (buttonState == LOW && lastButtonState == HIGH) {
        buttonPressStartTime = currentMillis;
        isLongPress = false;
    }
    
    // 按鈕持續按住
    if (buttonState == LOW) {
        if (!isLongPress && (currentMillis - buttonPressStartTime >= LONG_PRESS_TIME)) {
            isLongPress = true;
            if (!isExercising) {
                if (!isDetecting) {
                    isDetecting = true;
                    setColor(blueColor[0], blueColor[1], blueColor[2]);
                } else {
                    isCountdownPaused = !isCountdownPaused;
                    if (isCountdownPaused) {
                        setColor(redColor[0], redColor[1], redColor[2]);
                    } else {
                        setColor(blueColor[0], blueColor[1], blueColor[2]);
                    }
                }
                displayText();
            }
        }
    }
    
    // 按鈕釋放 - 切換運動模式
    if (buttonState == HIGH && lastButtonState == LOW) {
        if (!isLongPress && !isDetecting && !isExercising) {
            switch (currentMode) {
                case PUSH_UP:
                    currentMode = SIT_UP;
                    flashOnce(purpleColor);
                    break;
                case SIT_UP:
                    currentMode = SQUAT;
                    flashOnce(yellowColor);
                    break;
                case SQUAT:
                    currentMode = PUSH_UP;
                    flashOnce(blueColor);
                    break;
            }
            displayText();
        }
    }
    
    lastButtonState = buttonState;

    // 運動偵測和計時更新
    if (isDetecting && !isCountdownPaused) {
        detectMotion();
        
        if (isExercising && currentMillis - lastTimeUpdate >= 1000) {
            exerciseTime++;
            lastTimeUpdate = currentMillis;
            displayText();
        }
    }
}
