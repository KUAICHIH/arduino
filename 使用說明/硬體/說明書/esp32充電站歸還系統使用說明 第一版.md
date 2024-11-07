---

# **ESP32 RFID 資料上傳系統說明書**

## 目錄
1. [系統概述](#系統概述)
2. [硬體需求與連接](#硬體需求與連接)
3. [軟體需求與安裝](#軟體需求與安裝)
4. [程式功能說明](#程式功能說明)
5. [主要功能模組與程式碼說明](#主要功能模組與程式碼說明)
6. [系統操作流程](#系統操作流程)
7. [故障排除](#故障排除)

---

## 系統概述

本系統的目的是使用 ESP32 開發板搭配 RC522 RFID 模組和光線感測器，實現對 RFID 卡片的讀取並根據光線變化來確定卡片的狀態。當感測到卡片，且卡片條件變化（如離開或進入特定區域），系統會自動將卡片 UID 與狀態上傳至指定伺服器的 API，以便後端管理。

### 主要功能
- 偵測 RFID 卡片並取得 UID。
- 根據光線變化檢測卡片的狀態。
- 將卡片的 UID 與狀態打包成 JSON 格式，上傳至指定伺服器。
- WiFi 自動重連與 API 重試機制，保證資料的穩定傳輸。

---

## 硬體需求與連接

### 硬體需求
- **ESP32 開發板**
- **RC522 RFID 模組**
- **光線感測器**
- **接線跳線**

### 接線說明
| RFID RC522 引腳 | ESP32 引腳 |
|----------------|------------|
| SDA           | D5         |
| RST           | D0         |
| SCK           | D18        |
| MOSI          | D23        |
| MISO          | D19        |
| GND           | GND        |
| VCC           | 3.3V       |

光線感測器接入 **ESP32 的 34 號引腳** (LIGHT_SENSOR_PIN)。

---

## 軟體需求與安裝

### 1. 安裝 Arduino IDE
1. 下載 Arduino IDE，並在 **偏好設置** 中將 ESP32 板卡管理網址加入。
2. 進入「工具」>「開發板管理員」，搜尋並安裝 **ESP32** 套件。

### 2. 安裝程式庫
在 Arduino IDE 中，安裝以下所需的程式庫：
```cpp
#include <SPI.h>
#include <MFRC522.h>
#include <WiFiClientSecure.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
```

---

## 程式功能說明

### WiFi 設定
- **SSID** 與 **Password** 設定 WiFi 名稱及密碼。
- **API URL** 定義 API 的地址。

### 光線感測設定
- **DARK_THRESHOLD** 與 **LIGHT_THRESHOLD** 定義光線感測的明暗值，用於判斷環境是否處於黑暗狀態。

### RFID 重試機制
- **CARD_READ_TIMEOUT** 設定 RFID 卡片讀取的超時時間。
- **RETRY_DELAY** 設定 RFID 讀取失敗後的重試延遲時間。

---

## 主要功能模組與程式碼說明

### 1. WiFi 連線與自動重連
```cpp
void setup() {
    Serial.begin(115200);
    WiFi.begin(ssid, password);
    ...
}
```
此部分程式碼設定並連接 WiFi，若在 10 秒內無法連線，將重新啟動 ESP32。此處使用 `WiFi.status()` 確認連線狀態，並在主程式中自動檢查連線狀態，無連線時自動重連。

### 2. 獲取當前 JSON 資料
```cpp
String getCurrentJson() {
    HTTPClient http;
    http.begin(api_url);
    ...
    http.end();
    return response;
}
```
這段程式碼通過 HTTP GET 請求從 API 中獲取當前資料，並返回 JSON 資料，若失敗則輸出錯誤碼。

### 3. 卡片 UID 讀取與重試
```cpp
String readCardUID() {
    byte retryCount = 0;
    ...
    return uid;
}
```
此函數負責讀取 RFID 卡片 UID，若讀取失敗會在一定次數內重試，並在每次重試間隔 `RETRY_DELAY` 毫秒。

### 4. 更新 JSON 資料並發送 PUT 請求
```cpp
void updateJsonAndSendPutRequest(String cardUID, int lightValue) {
    ...
    http.PUT(jsonString);
    http.end();
}
```
此函數負責將讀取到的卡片 UID 及光線狀態打包成 JSON，並使用 HTTP PUT 請求發送到 API，若發送失敗則輸出錯誤訊息。

### 5. 光線狀態防抖
```cpp
bool getDebouncedLightState(int lightValue) {
    ...
    return average > DARK_THRESHOLD;
}
```
此函數通過多次讀取光線感測值來減少誤判，達到防抖效果。透過平均值來決定光線狀態。

### 6. 狀態顯示
```cpp
void printSystemStatus(int lightValue) {
    Serial.println("\n------- 系統狀態 -------");
    ...
    Serial.println("------------------------\n");
}
```
此函數在每秒輸出當前光線、卡片及系統狀態，方便監控。

---

## 系統操作流程

### 1. 開機與 WiFi 連線
插入電源後，系統將嘗試連接 WiFi，成功連線後開始檢測 RFID 卡片。

### 2. 卡片偵測
每當偵測到卡片，會讀取卡片 UID，並根據光線狀態更新卡片的進出狀態，然後發送至伺服器。

### 3. 光線狀態變更
根據光線感測的輸入判斷環境是否明亮。當光線變化時，會重新偵測卡片，並更新伺服器資料。

---

## 故障排除

- **WiFi 連接不上**：檢查 SSID 和密碼是否正確。
- **無法讀取卡片**：檢查 RC522 模組接線是否正確，若無法連接多次，可嘗試重新插拔模組。
- **API 上傳失敗**：檢查伺服器的連接狀況，或是確認 API 地址是否正確。

---
