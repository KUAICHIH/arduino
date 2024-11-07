#include <WiFiNINA.h>
#include <ArduinoHttpClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <MPU6050_tockn.h>
#include <SPI.h>
#include <U8g2lib.h>

// WiFi 資料參數設定區
const char* ssid = "LAPTOP-H200NENE 7672";
const char* password = "11024211";
// Post
const char* serverName = "192.168.137.108";  // post IP
const int serverPort = 3000;
const char* severendpoint = "/api/updaterecord";
// Get
const char* externalApiHost = "192.168.137.108";  // get API
const int externalport = 3000;
const char* externalendpoint = "/api/updatemachine/products";
// 新增卡片狀態檢查用的 API 參數
const char* cardApiHost = "192.168.137.108";
const int cardApiPort = 3000;
const char* cardApiEndpoint = "/api/updatecondition";

String machinename = "NHUgym-270F";  // 用於OLED顯示
String machineid = "123";  // 用於設備驗證

WiFiClient wifi;
HttpClient client = HttpClient(wifi, serverName, serverPort);

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
bool isUserVerified = false;  

// 重試參數
const int MAX_RETRIES = 5;           
const int RETRY_DELAY = 2000;        
const int WIFI_CONNECT_TIMEOUT = 10;  
const int DELETE_WAIT = 1000;        
const int SUCCESS_DISPLAY = 1000;     
const int ERROR_DISPLAY = 2000;       

// 按鈕控制相關
unsigned long buttonPressStartTime = 0;
const unsigned long LONG_PRESS_TIME = 1000;
bool isLongPress = false;
bool firstMotionDetected = false;  
unsigned long lastMotionTime = 0;    
unsigned long exerciseTime = 0;      
const unsigned long TIMEOUT_DURATION = 10000; // 10秒沒動作就超時
bool isExercising = false;          

unsigned long lastSecondUpdate;
unsigned long countdownStart = 0;
unsigned long remainingTime = 0;
int countdownTime = 10;

// 新增用於追蹤模式切換和卡片檢查的時間變數
unsigned long lastModeChange = 0;        // 最後一次模式切換的時間
unsigned long lastCardCheck = 0;         // 最後一次檢查卡片的時間
const unsigned long MODE_TIMEOUT = 5000;  // 模式切換超時時間（5秒）

String displayName = "";     
String userId = "";         
const int jsonSize = 512;   

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
    Serial.println("\n[Reset] Resetting all states...");
    
    isUserVerified = false;
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

  u8g2.drawStr(0, 10, machinename.c_str());  // 改為 machinename
  
  if (!isUserVerified) {
    u8g2.drawStr(0, 35, "Waiting for user...");
  } else {
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
  }

  u8g2.sendBuffer();
}


void getExternalData() {
  if (isUserVerified) return;

  WiFiClient client;

  if (client.connect(externalApiHost, externalport)) {
    Serial.println("Connected to API server");

    // 發送 GET 請求
    client.print(String("GET ") + externalendpoint + " HTTP/1.1\r\n" +
                 "Host: " + externalApiHost + "\r\n" +
                 "Connection: close\r\n\r\n");

    // 設置超時
    unsigned long timeout = millis();
    while (client.available() == 0) {
      if (millis() - timeout > 5000) {
        Serial.println(">>> Client Timeout !");
        client.stop();
        flashOnce(redColor);
        return;
      }
    }

    // 跳過 HTTP 標頭
    while (client.available()) {
      String line = client.readStringUntil('\n');
      if (line == "\r") {  // 空行表示標頭結束
        break;
      }
    }

    // 讀取回應並過濾非 JSON 行
    String response = "";
    while (client.available()) {
      String line = client.readStringUntil('\n');
      line.trim();  // 去除前後空格
      
      // 檢查是否為 JSON 行，忽略非 JSON 行
      if (line.startsWith("{") || line.startsWith("[")) {
        response = line;
        break;  // 停止讀取，因為只需要第一個有效 JSON 行
      }
    }

    if (response.isEmpty()) {
      Serial.println("No valid JSON response found");
      flashOnce(redColor);
      client.stop();
      return;
    }

    Serial.println("\nReceived JSON Response:");
    Serial.println(response);

    // 解析 JSON
    StaticJsonDocument<jsonSize> doc;
    DeserializationError error = deserializeJson(doc, response);

    if (error) {
      Serial.print("JSON parsing failed: ");
      Serial.println(error.c_str());
      flashOnce(redColor);
      client.stop();
      return;
    }

    // 檢查是否為空陣列
    if (doc.size() == 0) {
      Serial.println("Empty JSON array");
      flashOnce(redColor);
      client.stop();
      return;
    }

    JsonObject item = doc[0];

    // 檢查必要欄位
    if (!item.containsKey("id") || !item.containsKey("user") || 
        !item.containsKey("username") || !item.containsKey("machinename") || 
        !item.containsKey("machinenid")) {
      Serial.println("Missing required fields in object");
      flashOnce(redColor);
      client.stop();
      return;
    }

    Serial.println("\nParsed data:");
    Serial.print("Received machinenid: ");
    Serial.println(item["machinenid"].as<const char*>());
    Serial.print("Expected machineid: ");
    Serial.println(machineid);
    Serial.print("Username: ");
    Serial.println(item["username"].as<const char*>());
    Serial.print("User ID: ");
    Serial.println(item["user"].as<const char*>());

    // 比對機器 ID
    if (String(item["machinenid"].as<const char*>()) == machineid) {
      displayName = item["username"].as<String>();
      userId = item["user"].as<String>();
      isUserVerified = true;

      Serial.println("User verified successfully!");
      displayText();
      flashOnce(greenColor);
    } else {
      Serial.println("Machine ID mismatch! Verification failed.");
      flashOnce(redColor);
    }

    client.stop();
  } else {
    Serial.println("Connection to API server failed");
    flashOnce(redColor);
  }
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
        Serial.println("伏地挺身次數: " + String(count_push_up));
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
        Serial.println("仰臥起坐次數: " + String(count_sit_up));
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
        Serial.println("深蹲次數: " + String(count_squat));
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
    
    bool uploadSuccess = false;
    while (!uploadSuccess) {
      uploadSuccess = sendExerciseData();
      if (!uploadSuccess) {
        delay(RETRY_DELAY);
      }
    }
  }
}


bool sendExerciseData() {
    int retryCount = 0;
    bool uploadSuccess = false;
    
    while (!uploadSuccess && retryCount < MAX_RETRIES) {
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("\n[WiFi] Disconnected. Attempting to reconnect...");
            setColor(redColor[0], redColor[1], redColor[2]);
            WiFi.begin(ssid, password);
            
            int connectAttempts = 0;
            while (WiFi.status() != WL_CONNECTED && connectAttempts < WIFI_CONNECT_TIMEOUT) {
                delay(500);
                Serial.print(".");
                connectAttempts++;
            }
            
            if (WiFi.status() != WL_CONNECTED) {
                Serial.println("[WiFi] Reconnection failed");
                retryCount++;
                flashOnce(redColor);
                continue;
            }
        }
        
        StaticJsonDocument<200> putData;
        putData["user"] = userId;
        putData["machineid"] = machineid;  // 改為使用 machineid
        
        String ActionType;
        int Frequency;
        
        switch (currentMode) {
            case PUSH_UP:
                ActionType = "Push Up";
                Frequency = count_push_up;
                break;
            case SIT_UP:
                ActionType = "Sit Up";
                Frequency = count_sit_up;
                break;
            case SQUAT:
                ActionType = "Squat";
                Frequency = count_squat;
                break;
        }
        
        putData["ActionType"] = ActionType;
        putData["Frequency"] = Frequency;
        putData["usetime"] = exerciseTime;
        
        String putJsonString;
        serializeJson(putData, putJsonString);
        
        Serial.println("\n[Upload] Sending data:");
        Serial.println(putJsonString);
        
        setColor(purpleColor[0], purpleColor[1], purpleColor[2]);
        client.put(severendpoint, "application/json", putJsonString);
        
        int statusCode = client.responseStatusCode();
        String response = client.responseBody();
        
        Serial.print("[Upload] Status code: ");
        Serial.println(statusCode);
        Serial.print("[Upload] Response: ");
        Serial.println(response);
        
        if (statusCode == 200 && response.indexOf("ok") != -1) {
            Serial.println("[Upload] Success!");
            uploadSuccess = true;
            
            setColor(greenColor[0], greenColor[1], greenColor[2]);
            delay(SUCCESS_DISPLAY);
            
            resetAllStates();
            
            Serial.println("[Verify] Getting new user data...");
            getExternalData();
            
            return true;
        } else {
            Serial.println("[Upload] Failed or invalid response");
            flashOnce(redColor);
            retryCount++;
            
            if (retryCount < MAX_RETRIES) {
                Serial.print("[Retry] Waiting ");
                Serial.print(RETRY_DELAY / 1000);
                Serial.println(" seconds before next attempt...");
                delay(RETRY_DELAY);
            }
        }
    }
    
    if (!uploadSuccess) {
        Serial.println("\n[Error] Failed to upload after maximum retries");
        setColor(redColor[0], redColor[1], redColor[2]);
        delay(ERROR_DISPLAY);
        setColor(0, 0, 0);
    }
    
    return false;
}


bool checkCardStatus() {
    WiFiClient cardClient;
    
    if (cardClient.connect(cardApiHost, cardApiPort)) {
        Serial.println("\n[卡片檢查] 已連接到卡片狀態伺服器");
        
        // 發送 GET 請求
        cardClient.print(String("GET ") + cardApiEndpoint + " HTTP/1.1\r\n" +
                        "Host: " + cardApiHost + "\r\n" +
                        "Connection: close\r\n\r\n");
        
        // 等待回應，設定超時機制
        unsigned long timeout = millis();
        while (cardClient.available() == 0) {
            if (millis() - timeout > 5000) {
                Serial.println("[卡片檢查] 連線超時！");
                cardClient.stop();
                return true;  // 連線超時時回傳 true 以避免錯誤中斷使用
            }
        }
        
        // 讀取回應內容
        String response = "";
        bool isBody = false;
        
        while (cardClient.available()) {
            String line = cardClient.readStringUntil('\n');
            if (line == "\r") {
                isBody = true;  // 找到回應的主體部分
                continue;
            }
            if (isBody) {
                response += line;
            }
        }
        
        Serial.println("[卡片檢查] 收到回應：");
        Serial.println(response);

        // 清除非 JSON 部分
        int jsonIndex = response.indexOf("{"); // 找到 JSON 開始的索引
        if (jsonIndex == -1) {
            Serial.println("[卡片檢查] 未找到 JSON 格式，將不處理。");
            cardClient.stop();
            return false;  // 未找到 JSON 格式時回傳 false
        }
        
        // 提取有效的 JSON 部分
        response = response.substring(jsonIndex);
        
        // 檢查是否為有效的 JSON 格式
        if (!response.startsWith("{") && !response.startsWith("[")) {
            Serial.println("[卡片檢查] 收到非 JSON 格式的回應，將不處理。");
            cardClient.stop();
            return false;  // 非 JSON 格式時回傳 false
        }

        // 解析 JSON 回應
        StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, response);

        if (error) {
            Serial.print("[卡片檢查] JSON 解析失敗：");
            Serial.println(error.c_str());
            cardClient.stop();
            return false;  // JSON 解析失敗時回傳 false
        }

        // 尋找匹配的機器 ID
        JsonArray cards = doc["products"]; // 根據您提供的 JSON 鍵名進行修改
        
        for (JsonVariant card : cards) {
            if (card["machineid"].as<String>() == machineid) {
                bool condition = card["condition"].as<String>() == "true"; // 將字串轉換為布林值
                Serial.print("[卡片檢查] 找到匹配的機器 ID,狀態：");
                Serial.println(condition);
                cardClient.stop();
                return condition;  // 返回卡片狀態
            }
        }
        
        Serial.println("[卡片檢查] 回應中未找到對應的機器 ID");
        cardClient.stop();
        return false;  // 找不到機器 ID 時回傳 false
    }
    
    Serial.println("[卡片檢查] 無法連接到卡片狀態伺服器");
    return false;  // 連線失敗時回傳 false
}



void setup() {
  Serial.begin(115200);
  Wire.begin();
  mpu.begin();
  u8g2.begin();
  mpu.calcGyroOffsets(true);
  
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);

  // 初始化狀態
  isUserVerified = false;
  isDetecting = false;
  isCountdown = false;
  isCountdownPaused = false;
  hasStartedCountdown = false;

  // 連接 WiFi
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("\nConnected to WiFi");
  Serial.println(WiFi.localIP());
  
  setColor(0, 0, 0);
  displayText();
}


void loop() {
    static unsigned long lastGetRequest = 0;
    static unsigned long lastTimeUpdate = 0;
    unsigned long currentMillis = millis();
    
    // 未驗證用戶時的請求處理
    if (!isUserVerified && currentMillis - lastGetRequest >= 5000) {
        getExternalData();
        lastGetRequest = currentMillis;
    }

    // 當用戶已驗證但閒置時檢查卡片狀態
    if (isUserVerified && !isDetecting && !isExercising) {
        // 如果超過 5 秒沒有切換模式，且距離上次檢查已經過了 5 秒
        if (currentMillis - lastModeChange >= MODE_TIMEOUT &&
            currentMillis - lastCardCheck >= 5000) {
            
            Serial.println("\n[卡片檢查] 因閒置狀態開始檢查卡片狀態...");
            if (!checkCardStatus()) {
                Serial.println("[卡片檢查] 卡片狀態為 false，重置所有狀態...");
                flashOnce(redColor);  // 閃爍紅燈提示
                resetAllStates();     // 重置所有狀態
                lastCardCheck = currentMillis;
                return;
            }
            lastCardCheck = currentMillis;
        }
    }

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
            if (isUserVerified && !isExercising) {
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
                lastModeChange = currentMillis;  // 重置模式切換計時器
                displayText();
            }
        }
    }
    
    // 按鈕釋放 - 切換運動模式
    if (buttonState == HIGH && lastButtonState == LOW) {
        if (isUserVerified && !isLongPress && !isDetecting && !isExercising) {
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
            lastModeChange = currentMillis;  // 重置模式切換計時器
            displayText();
        }
    }
    
    lastButtonState = buttonState;

    // 運動偵測和計時更新
    if (isUserVerified && isDetecting && !isCountdownPaused) {
        detectMotion();
        
        if (isExercising && currentMillis - lastTimeUpdate >= 1000) {
            exerciseTime++;
            lastTimeUpdate = currentMillis;
            displayText();
        }
    }
}
