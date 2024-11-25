#include <Wire.h>
#include <MPU6050_tockn.h>
#include <SPI.h>
#include <U8g2lib.h>

// MPU6050, LED, OLED
MPU6050 mpu(Wire);
const int redPin = 5;
const int greenPin = 6; 
const int bluePin = 7;
const int buttonPin = 2;
U8G2_SSD1306_128X64_NONAME_F_4W_HW_SPI u8g2(U8G2_R0, /* cs=*/ 10, /* dc=*/ 9, /* reset=*/ 8);

// 計數和時間追踪
int count_push_up = 0;
int count_sit_up = 0;
int count_squat = 0;
unsigned long exerciseTime = 0;
unsigned long lastTimeUpdate = 0;

// 動作偵測參數
bool isDown = false;
bool isDetecting = false;
bool isExercising = false;

// 按鈕控制參數
unsigned long buttonPressStartTime = 0;
const unsigned long LONG_PRESS_TIME = 1000;
bool isLongPress = false;
unsigned long lastMotionTime = 0;
const unsigned long TIMEOUT_DURATION = 10000; // 10秒無動作視為結束

// 動作模式
enum Mode { PUSH_UP, SIT_UP, SQUAT };
Mode currentMode = PUSH_UP;

// LED 顏色設定
const int redColor[3] = {255, 0, 0};
const int blueColor[3] = {0, 0, 255};
const int purpleColor[3] = {255, 0, 255};
const int yellowColor[3] = {255, 255, 0};
const int greenColor[3] = {0, 255, 0};

// MPU6050 閾值設定
struct MotionThresholds {
    // 伏地挺身參數（使用X軸）
    float pushUpXMin;      // X軸最小角度
    float pushUpXMax;      // X軸最大角度
    
    // 仰臥起坐參數
    float sitUpYMin;       
    float sitUpYMax;      
    
    // 深蹲參數
    float squatDisplacementThreshold;   
    float squatResetThreshold;          
    
    // 動作確認延遲(毫秒)
    unsigned long confirmDelay;
} thresholds = {
    // 伏地挺身閾值（使用X軸角度範圍）
    .pushUpXMin = -45.0,    // 向下彎曲的最小角度
    .pushUpXMax = -10.0,    // 向上回彈的最大角度
    
    // 仰臥起坐閾值
    .sitUpYMin = 20.0,    
    .sitUpYMax = 45.0,    
    
    // 深蹲閾值
    .squatDisplacementThreshold = 20.0,
    .squatResetThreshold = 5.0,
    
    // 確認延遲
    .confirmDelay = 200    
};

// 動作狀態追踪
struct MotionState {
    float lastX;
    float lastY;
    float accumulatedDisplacementX;  // X軸累積位移
    unsigned long lastChangeTime;
    bool inValidPosition;
    int stableCount;
} motionState = {0, 0, 0, 0, false, 0};

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
    
    // 顯示當前模式
    const char* modeText;
    switch (currentMode) {
        case PUSH_UP:
            modeText = "Push Up Mode";
            break;
        case SIT_UP:
            modeText = "Sit Up Mode";
            break;
        case SQUAT:
            modeText = "Squat Mode";
            break;
    }
    u8g2.drawStr(0, 10, modeText);
    
    // 顯示計數
    u8g2.setCursor(0, 30);
    switch (currentMode) {
        case PUSH_UP:
            u8g2.print("Count: ");
            u8g2.print(count_push_up);
            break;
        case SIT_UP:
            u8g2.print("Count: ");
            u8g2.print(count_sit_up);
            break;
        case SQUAT:
            u8g2.print("Count: ");
            u8g2.print(count_squat);
            break;
    }
    
    // 顯示運動時間
    if (isExercising) {
        u8g2.setCursor(0, 50);
        u8g2.print("Time: ");
        u8g2.print(exerciseTime);
        u8g2.print("s");
    }
    
    u8g2.sendBuffer();
}

void detectMotion() {
    if (!isDetecting) return;
    
    mpu.update();
    unsigned long currentTime = millis();
    bool motionDetected = false;
    float currentX = mpu.getAngleX();
    float currentY = mpu.getAngleY();
    
    switch (currentMode) {
        case PUSH_UP:
            // 使用X軸角度範圍來偵測伏地挺身
            if (currentX <= thresholds.pushUpXMin) {  // 當角度小於最小閾值，表示下降到底部
                if (!isDown) {
                    isDown = true;
                }
            } else if (currentX >= thresholds.pushUpXMax && isDown) {  // 當角度大於最大閾值且之前已下降，表示完成一次
                count_push_up++;
                isDown = false;
                motionDetected = true;
                Serial.println("Push up count: " + String(count_push_up));
                Serial.print("Current X angle: ");
                Serial.println(currentX);
            }
            break;
            
        case SIT_UP:
            // 仰臥起坐偵測
            if (currentY > thresholds.sitUpYMax) {
                if (!isDown) {
                    count_sit_up++;
                    isDown = true;
                    motionDetected = true;
                    Serial.println("Sit up count: " + String(count_sit_up));
                }
            } else if (currentY < thresholds.sitUpYMin) {
                isDown = false;
            }
            break;
            
        case SQUAT:
            // 深蹲偵測
            motionState.accumulatedDisplacementX += abs(currentX - motionState.lastX);
            
            if (motionState.accumulatedDisplacementX >= thresholds.squatDisplacementThreshold) {
                if (!isDown) {
                    count_squat++;
                    isDown = true;
                    motionDetected = true;
                    Serial.println("Squat count: " + String(count_squat));
                    Serial.print("Accumulated X displacement: ");
                    Serial.println(motionState.accumulatedDisplacementX);
                    motionState.accumulatedDisplacementX = 0;
                }
            } else if (motionState.accumulatedDisplacementX < thresholds.squatResetThreshold) {
                isDown = false;
            }
            break;
    }
    
    // 更新動作狀態
    motionState.lastX = currentX;
    motionState.lastY = currentY;
    
    if (motionDetected) {
        if (!isExercising) {
            isExercising = true;
            exerciseTime = 0;
        }
        lastMotionTime = currentTime;
        displayText();
        flashOnce(greenColor);
    }
    
    // 檢查超時
    if (isExercising && (currentTime - lastMotionTime >= TIMEOUT_DURATION)) {
        isDetecting = false;
        isExercising = false;
        setColor(greenColor[0], greenColor[1], greenColor[2]);
        delay(1000);
        setColor(0, 0, 0);
        displayText();
    }
}

void setup() {
    Serial.begin(115200);
    Wire.begin();
    mpu.begin();
    u8g2.begin();
    
    // 校準陀螺儀
    Serial.println("Calibrating MPU6050...");
    mpu.calcGyroOffsets(true);
    Serial.println("Calibration done!");
    
    pinMode(buttonPin, INPUT_PULLUP);
    pinMode(redPin, OUTPUT);
    pinMode(greenPin, OUTPUT);
    pinMode(bluePin, OUTPUT);
    
    // 初始化LED為關閉狀態
    setColor(0, 0, 0);
    displayText();
}

void loop() {
    unsigned long currentMillis = millis();
    static bool lastButtonState = HIGH;
    bool buttonState = digitalRead(buttonPin);
    
    // 按鈕按下處理
    if (buttonState == LOW && lastButtonState == HIGH) {
        buttonPressStartTime = currentMillis;
        isLongPress = false;
    }
    
    // 長按處理
    if (buttonState == LOW) {
        if (!isLongPress && (currentMillis - buttonPressStartTime >= LONG_PRESS_TIME)) {
            isLongPress = true;
            if (!isExercising) {
                isDetecting = !isDetecting;
                if (isDetecting) {
                    setColor(blueColor[0], blueColor[1], blueColor[2]);
                    // 重置動作狀態
                    motionState = {0, 0, 0, 0, false, 0};
                } else {
                    setColor(0, 0, 0);
                }
                displayText();
            }
        }
    }
    
    // 短按處理 - 切換模式
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
    
    // 動作偵測和計時更新
    if (isDetecting) {
        detectMotion();
        
        if (isExercising && currentMillis - lastTimeUpdate >= 1000) {
            exerciseTime++;
            lastTimeUpdate = currentMillis;
            displayText();
        }
    }
    
    // 序列埠輸出當前姿態數據(用於調試)
    if (Serial.available() > 0) {
        char cmd = Serial.read();
        if (cmd == 'd') {
            Serial.println("Current MPU6050 Data:");
            Serial.print("AngleX: "); Serial.println(mpu.getAngleX());
            Serial.print("AngleY: "); Serial.println(mpu.getAngleY());
            Serial.print("Accumulated X Displacement: "); 
            Serial.println(motionState.accumulatedDisplacementX);
        }
    }
}
