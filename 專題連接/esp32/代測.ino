#include <SPI.h>
#include <MFRC522.h>
#include <WiFiClientSecure.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// RFID 模組接線 (RC522)
#define SDA_PIN 5    // ESP32 上的 SDA 腳位
#define RST_PIN 0    // ESP32 上的 RST 腳位

// 光敏電阻接線
#define LIGHT_SENSOR_PIN 34  // ESP32 上的類比輸入腳位

// WiFi 設定
const char* ssid = "LAPTOP-H200NENE 7672";
const char* password = "11024211";

// API 端點
const char* api_url = "http://192.168.137.1:3002/card";

MFRC522 rfid(SDA_PIN, RST_PIN);

bool cardReadCompleted = false;
bool rfidDetectionEnabled = true;
String currentCardUID = "";

const int LIGHT_THRESHOLD = 1000;
const int DARK_THRESHOLD = 1000;

// 首先獲取當前的 JSON 數據
String getCurrentJson() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(api_url);
    
    int httpResponseCode = http.GET();
    if (httpResponseCode == 200) {
      String response = http.getString();
      http.end();
      return response;
    }
    http.end();
  }
  return ""; // 如果獲取失敗，返回空字符串
}

// 更新 JSON 並發送 PUT 請求
void updateJsonAndSendPutRequest(String cardUID, int lightValue) {
  // 首先獲取當前的 JSON
  String currentJsonStr = getCurrentJson();
  
  // 如果無法獲取當前 JSON，使用預設結構
  if (currentJsonStr.isEmpty()) {
    currentJsonStr = "{\"cards\":[]}";
  }
  
  // 解析 JSON
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, currentJsonStr);
  
  if (error) {
    Serial.println("JSON 解析失敗!");
    return;
  }
  
  // 更新特定卡片的狀態
  JsonArray cards = doc["cards"].as<JsonArray>();
  bool found = false;
  
  for (JsonObject card : cards) {
    if (card["machineid"].as<String>() == cardUID) {
      // 根據光線值更新狀態
      if (lightValue < DARK_THRESHOLD) {
        card["condition"] = false;
      } else {
        card["condition"] = true;
      }
      found = true;
      break;
    }
  }
  
  // 如果找不到對應的卡片，添加新記錄
  if (!found) {
    JsonObject newCard = cards.createNestedObject();
    newCard["machineid"] = cardUID;
    if (lightValue < DARK_THRESHOLD) {
      newCard["condition"] = false;
    } else {
      newCard["condition"] = true;
    }
  }
  
  // 發送更新後的 JSON
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(api_url);
    http.addHeader("Content-Type", "application/json");
    
    String jsonString;
    serializeJson(doc, jsonString);
    Serial.println("準備發送的 JSON: " + jsonString);
    
    int httpResponseCode = http.PUT(jsonString);
    if (httpResponseCode == 200) {
      Serial.println("成功更新遠端 API");
    } else {
      Serial.print("更新遠端 API 失敗，狀態碼: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  } else {
    Serial.println("未連接 WiFi，無法更新遠端 API");
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial);
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("正在連接 WiFi...");
  }
  Serial.println("已連接 WiFi");
  
  SPI.begin(18, 19, 23, 5);
  delay(50);
  
  rfid.PCD_Init();
  delay(100);
  
  rfid.PCD_SetAntennaGain(rfid.RxGain_max);
  
  pinMode(LIGHT_SENSOR_PIN, INPUT);
}

void loop() {
  int lightValue = 4095 - analogRead(LIGHT_SENSOR_PIN);
  
  printSystemStatus(lightValue);
  
  if (rfidDetectionEnabled) {
    rfid.PCD_StopCrypto1();
    
    if (rfid.PICC_IsNewCardPresent()) {
      Serial.println("偵測到卡片...");
      
      if (rfid.PICC_ReadCardSerial()) {
        Serial.println("成功讀取卡片序號!");
        
        String uid = "";
        for (byte i = 0; i < rfid.uid.size; i++) {
          if (rfid.uid.uidByte[i] < 0x10) {
            uid += "0";
          }
          uid += String(rfid.uid.uidByte[i], HEX);
        }
        
        cardReadCompleted = true;
        currentCardUID = uid;
        
        updateJsonAndSendPutRequest(currentCardUID, lightValue);
        
        rfid.PICC_HaltA();
      } else {
        Serial.println("讀取卡片失敗，請重試");
      }
    }
  }
  
  static bool wasDark = false; // 新增變數追蹤先前的光線狀態
  
  if (lightValue < DARK_THRESHOLD && cardReadCompleted) {
    if (rfidDetectionEnabled) {
      rfidDetectionEnabled = false;
      wasDark = true; // 記錄進入暗狀態
      Serial.println("\n環境變暗且已讀取卡片，停止 RFID 偵測");
      // 更新狀態為 false
      updateJsonAndSendPutRequest(currentCardUID, lightValue);
    }
  } else if (lightValue > LIGHT_THRESHOLD) {
    if (wasDark && !currentCardUID.isEmpty()) {
      // 如果先前是暗的且有記錄卡片，更新狀態為 true
      updateJsonAndSendPutRequest(currentCardUID, lightValue);
      Serial.println("\n環境變亮，更新卡片狀態為開啟");
    }
    
    if (!rfidDetectionEnabled || cardReadCompleted) {
      rfidDetectionEnabled = true;
      cardReadCompleted = false;
      wasDark = false; // 重設暗狀態標記
      currentCardUID = "";
      Serial.println("\n環境變亮，重新啟動 RFID 偵測");
      rfid.PCD_Init();
      rfid.PCD_SetAntennaGain(rfid.RxGain_max);
    }
  }
  
  delay(100);
}

void printSystemStatus(int lightValue) {
  static unsigned long lastPrintTime = 0;
  unsigned long currentTime = millis();
  
  if (currentTime - lastPrintTime >= 1000) {
    Serial.println("\n------- 系統狀態同步 -------\n");
    
    Serial.print("光線強度: ");
    Serial.println(lightValue);
    Serial.println();
    
    Serial.print("卡片讀取完成狀態: ");
    Serial.println(cardReadCompleted ? "已完成" : "未完成");
    Serial.println();
    
    Serial.print("RFID偵測啟用狀態: ");
    Serial.println(rfidDetectionEnabled ? "已啟用" : "已停用");
    Serial.println();
    
    Serial.print("當前卡片UID: ");
    Serial.println(currentCardUID.length() > 0 ? currentCardUID : "無");
    Serial.println();
    
    Serial.println("-------------------------\n");
    
    lastPrintTime = currentTime;
  }
}
