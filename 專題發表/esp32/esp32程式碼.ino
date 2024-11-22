#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// 在全局定義一個簡短的引用別名
HardwareSerial &S = Serial;

// RFID 模組接線 (RC522)
#define SDA_PIN 5    
#define RST_PIN 0    
#define LIGHT_SENSOR_PIN 34  

// WiFi 設定
const char* ssid = "LAPTOP-H200NENE 7672";
const char* password = "11024211";
const char* api_url = "http://192.168.137.186:3000/api/updatecondition/products";

// RFID 相關常數
const unsigned long CARD_READ_TIMEOUT = 5000;  // 卡片讀取超時時間 (5秒)
const unsigned long RETRY_DELAY = 1000;        // 重試延遲
const byte MAX_RETRY_COUNT = 3;               // 最大重試次數

// 光線感測相關常數
const int LIGHT_THRESHOLD = 2000;
const int DARK_THRESHOLD = 1800;
const int LIGHT_SAMPLES = 5;                  // 光線採樣次數
const unsigned long DEBOUNCE_DELAY = 500;     // 光線狀態防抖延遲

MFRC522 rfid(SDA_PIN, RST_PIN);

// 狀態變數
bool cardReadCompleted = false;
bool rfidDetectionEnabled = true;
String currentCardUID = "";
unsigned long lastLightChangeTime = 0;
bool lastLightState = true;

// 光線感測防抖
bool getDebouncedLightState(int lightValue) {
    static int readings[LIGHT_SAMPLES];
    static byte readIndex = 0;
    static long total = 0;
    
    total = total - readings[readIndex];
    readings[readIndex] = lightValue;
    total = total + readings[readIndex];
    readIndex = (readIndex + 1) % LIGHT_SAMPLES;
    
    int average = total / LIGHT_SAMPLES;
    return average > DARK_THRESHOLD;
}

// 重置 RFID 讀取器
void resetRFIDReader() {
    rfid.PCD_Reset();
    delay(50);
    rfid.PCD_Init();
    delay(50);
    rfid.PCD_SetAntennaGain(rfid.RxGain_max);
}

// 讀取卡片 UID，包含重試機制
String readCardUID() {
    byte retryCount = 0;
    String uid = "";
    
    while (retryCount < MAX_RETRY_COUNT) {
        if (rfid.PICC_ReadCardSerial()) {
            for (byte i = 0; i < rfid.uid.size; i++) {
                if (rfid.uid.uidByte[i] < 0x10) {
                    uid += "0";
                }
                uid += String(rfid.uid.uidByte[i], HEX);
            }
            rfid.PICC_HaltA();
            return uid;
        }
        
        retryCount++;
        if (retryCount < MAX_RETRY_COUNT) {
            delay(RETRY_DELAY);
            resetRFIDReader();
        }
    }
    return "";
}

// 更新狀態並發送 PUT 請求
void updateJsonAndSendPutRequest(String cardUID, int lightValue) {
    if (WiFi.status() != WL_CONNECTED) {
        S.println("WiFi 未連接，正在重新連接...");
        WiFi.reconnect();
        delay(5000);
        if (WiFi.status() != WL_CONNECTED) {
            S.println("WiFi 重連失敗");
            return;
        }
    }

    // 只發送卡片ID和狀態
    DynamicJsonDocument doc(128);
    bool newCondition = lightValue >= DARK_THRESHOLD;
    
    doc["machineid"] = cardUID;
    doc["condition"] = newCondition;
    
    HTTPClient http;
    http.begin(api_url);
    http.addHeader("Content-Type", "application/json");
    
    String jsonString;
    serializeJson(doc, jsonString);
    S.println("準備發送的 JSON: " + jsonString);
    
    int httpResponseCode = http.PUT(jsonString);
    if (httpResponseCode != 200) {
        S.printf("API 更新失敗，狀態碼: %d\n", httpResponseCode);
    } else {
        S.println("成功更新 API");
    }
    http.end();
}

void setup() {
    S.begin(115200);
    
    // WiFi 連接
    WiFi.begin(ssid, password);
    unsigned long startAttemptTime = millis();
    
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
        delay(500);
        S.print(".");
    }
    
    if (WiFi.status() != WL_CONNECTED) {
        S.println("\nWiFi 連接失敗！正在重新啟動...");
        ESP.restart();
    }
    
    S.println("\nWiFi 已連接");
    
    // 初始化 SPI 和 RFID
    SPI.begin(18, 19, 23, 5);
    resetRFIDReader();
    
    pinMode(LIGHT_SENSOR_PIN, INPUT);
}

void loop() {
    int lightValue = 4095 - analogRead(LIGHT_SENSOR_PIN);
    bool currentLightState = getDebouncedLightState(lightValue);
    unsigned long currentTime = millis();
    
    // 處理光線狀態變化
    if (currentLightState != lastLightState && 
        (currentTime - lastLightChangeTime) > DEBOUNCE_DELAY) {
        
        lastLightState = currentLightState;
        lastLightChangeTime = currentTime;
        
        if (!currentLightState && cardReadCompleted) {
            rfidDetectionEnabled = false;
            updateJsonAndSendPutRequest(currentCardUID, lightValue);
        } else if (currentLightState) {
            if (!currentCardUID.isEmpty()) {
                updateJsonAndSendPutRequest(currentCardUID, lightValue);
            }
            rfidDetectionEnabled = true;
            cardReadCompleted = false;
            currentCardUID = "";
            resetRFIDReader();
        }
    }
    
    // RFID 卡片偵測
    if (rfidDetectionEnabled && !cardReadCompleted) {
        if (rfid.PICC_IsNewCardPresent()) {
            String uid = readCardUID();
            if (!uid.isEmpty()) {
                currentCardUID = uid;
                cardReadCompleted = true;
                updateJsonAndSendPutRequest(currentCardUID, lightValue);
            }
        }
    }
    
    // 系統狀態輸出
    static unsigned long lastStatusPrint = 0;
    if (currentTime - lastStatusPrint >= 1000) {
        printSystemStatus(lightValue);
        lastStatusPrint = currentTime;
    }
    
    delay(50);  // 短暫延遲以減少 CPU 負載
}

void printSystemStatus(int lightValue) {
    S.println("\n------- 系統狀態 -------");
    S.printf("光線強度: %d\n", lightValue);
    S.printf("光線狀態: %s\n", lastLightState ? "明亮" : "黑暗");
    S.printf("卡片狀態: %s\n", cardReadCompleted ? "已讀取" : "未讀取");
    S.printf("RFID狀態: %s\n", rfidDetectionEnabled ? "啟用" : "停用");
    S.printf("當前UID: %s\n", currentCardUID.isEmpty() ? "無" : currentCardUID.c_str());
    S.println("------------------------\n");
}
