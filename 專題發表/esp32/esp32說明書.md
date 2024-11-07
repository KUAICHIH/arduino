---

## 使用ESP32和RC522 RFID讀取器實現卡片偵測與光線感測自動更新API說明書

### 1. 概述
本程式透過ESP32搭配RC522 RFID讀取器與光線感測器實現卡片偵測和光線狀態監測。當卡片偵測到或光線狀態發生變化時，系統會自動向API更新卡片UID和狀態資訊。

### 2. 硬體需求
- ESP32模組
- RC522 RFID讀取器
- 光線感測器（接於ESP32的類比引腳）

### 3. 接線說明
- **RC522 RFID模組接線**
  - SDA (SS) 接 ESP32的 GPIO 5
  - RST 接 ESP32的 GPIO 0
  - SCK、MOSI、MISO 分別接至 ESP32的相應 SPI 引腳（18, 23, 19）

- **光線感測器接線**
  - 輸出引腳接至 ESP32 的 GPIO 34 (LIGHT_SENSOR_PIN)

### 4. 程式功能
- **WiFi 連接**：系統開機後會自動連接至指定WiFi網路。
- **RFID 卡片偵測**：讀取並取得卡片UID，並透過HTTP PUT方法將卡片UID和光線狀態上傳至API。
- **光線感測**：監測環境光線強度，若光線強度變化，會更新狀態至API。
- **JSON 資料獲取與更新**：獲取當前API資料，解析後更新並回傳新的資料狀態。

### 5. API 說明
- **API 端點**：`http://192.168.137.1:3002/card`
- **請求方法**：PUT
- **資料格式**：
  ```json
  {
      "cards": [
          {
              "machineid": "卡片UID",
              "condition": "光線狀態"
          }
      ]
  }
  ```
  - `machineid`：卡片的UID
  - `condition`：`true` 表示黑暗，`false` 表示明亮

### 6. 程式邏輯
- **初始化**：初始化WiFi連線和SPI通訊，並重置RFID讀取器。
- **光線偵測**：以防抖動的方式檢測光線狀態變化，當光線狀態改變時，檢查卡片UID是否已被讀取並上傳新狀態。
- **RFID 偵測**：當RFID偵測到新卡片時，讀取UID並自動上傳卡片信息和光線狀態至API。
- **系統狀態輸出**：每隔一秒輸出一次系統的當前狀態，包括光線強度、光線狀態、卡片偵測狀態等。

### 7. 程式使用方式
1. **WiFi 設置**：修改程式中的SSID和密碼，以便ESP32連接至正確的WiFi網路。
   ```cpp
   const char* ssid = "LAPTOP-H200NENE 7672";
   const char* password = "11024211";
   ```

2. **啟動ESP32**：上電ESP32並監視序列埠輸出，確保WiFi連接成功並完成初始化。

3. **光線和RFID偵測**：
   - 當光線環境變亮或變暗，程序將自動更新狀態至API。
   - 當偵測到新卡片放置於讀取器上，卡片UID將記錄並上傳至API，且光線狀態亦會更新。

### 8. 程式代碼說明
#### WiFi 連接初始化
- 程式啟動後，ESP32會自動連接至指定的WiFi網路，若連接失敗，將重新啟動ESP32。
  ```cpp
  WiFi.begin(ssid, password);
  ```

#### 光線感測與防抖動機制
- 程式採用多次取樣求平均值來避免光線狀態的誤判。
  ```cpp
  bool getDebouncedLightState(int lightValue);
  ```

#### 卡片UID讀取及重試機制
- 程式透過RC522模組讀取卡片的UID，若讀取失敗則會重試最多3次。
  ```cpp
  String readCardUID();
  ```

#### JSON 解析與更新
- 獲取並解析JSON數據，若卡片UID或光線狀態改變，則會更新API。
  ```cpp
  updateJsonAndSendPutRequest(String cardUID, int lightValue);
  ```

#### 系統狀態輸出
- 程式定期顯示系統狀態，用於調試與監測系統狀態。
  ```cpp
  printSystemStatus(int lightValue);
  ```

### 9. 注意事項
- **RFID 卡片置放位置**：卡片需平穩放置於RC522感應區，以確保穩定讀取UID。
- **WiFi 設置**：確保SSID和密碼正確填寫，並確認ESP32所連接的WiFi路由器允許外部連接。
- **API 伺服器運行**：確保API伺服器正在運行，且能接收和解析PUT請求，避免因伺服器問題導致連接失敗。

### 10. 附加資訊
若需進一步調整程式中的參數，例如光線偵測閾值或超時時間，可在程式開頭調整以下常數：
- `LIGHT_THRESHOLD` 和 `DARK_THRESHOLD`：光線閾值
- `CARD_READ_TIMEOUT`：卡片讀取超時時間
- `MAX_RETRY_COUNT`：卡片讀取重試次數

---
