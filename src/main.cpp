#include <Adafruit_GFX.h> // Core graphics library
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <HTTPClient.h>
#include <Key.h>
#include <Keypad.h>
#include <Keypad_I2C.h>
#include <MFRC522.h>
#include <SPI.h>
#include <WiFiManager.h>
#include <Wire.h>
#include <secret.h>

// INFO: Variable declaration
#define I2C_KEYPAD_ADDR 0x20
#define I2C_LCD_ADDR 0x3C
#define SS_RFID_PIN 15
#define RST_RFID_PIN 4
#define TOUCH 25
#define RELAY 27
#define BUZZ_PIN 17
#define DELAY_TIME 3000
const byte ROWS = 4;
const byte COLS = 4;
unsigned long previousMillis = 0;
unsigned long interval = 30000;
unsigned long messageTimestamp = 0;
char keys[ROWS][COLS] = {
    {'D', '#', '0', '*'},
    {'C', '9', '8', '7'},
    {'B', '6', '5', '4'},
    {'A', '3', '2', '1'},
};
byte rowPins[ROWS] = {0, 1, 2, 3};
byte colPins[COLS] = {4, 5, 6, 7};
byte rfidCount = 0;
String SECRET = "?id=" + String(API_ID) + "&key=" + String(API_KEY);
String BASE_URL = "https://103.179.57.88"; //"http://smartroom.id";
String ADMIN_AUTH_URL;
String CHECKIN_URL;
String DEVICE_ID;
String cardIdContainer;
String pinContainer;
String responsesTime = "";
boolean isCardExist = false;
boolean is_checkin = true;
boolean isAdmin = false;
boolean changeMode = false;
boolean localChange = false;

// INFO: Create instance of object
WiFiManager wm;
Keypad_I2C keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS,
                  I2C_KEYPAD_ADDR, PCF8574);
// Adafruit_ILI9341 display = Adafruit_ILI9341(SS_LCD_PIN, DC_LCD_PIN,
// MOSI_LCD_PIN, SCLK_LCD_PIN, RST_LCD_PIN);
MFRC522 rfid(SS_RFID_PIN, RST_RFID_PIN);
Adafruit_SSD1306 display(128, 64, &Wire, -1);

void setupWiFi() {
  bool res;
  res = wm.autoConnect("Smart Door", "t4np454nd1"); // password protected ap

  if (!res) {
    Serial.println("Failed to connect");
  } else {
    Serial.println("Connected to the WiFi network");
  }
}

String readFromEEPROM(int addrOffset) {
  int newStrLen = EEPROM.read(addrOffset);
  char data[newStrLen + 1];
  for (int i = 0; i < newStrLen; i++) {
    data[i] = EEPROM.read(addrOffset + 1 + i);
  }
  data[newStrLen] = '\0';
  return String(data);
}

void writeToEEPROM(int addrOffset, const String &strToWrite) {
  int len = strToWrite.length();
  Serial.println("Store new data to EEPROM");
  EEPROM.write(addrOffset, len);
  for (int i = 0; i < len; i++) {
    EEPROM.write(addrOffset + 1 + i, strToWrite[i]);
    EEPROM.commit();
  }
}

void initializeDevice() {
  Serial.println("Initialize device id");
  String url = BASE_URL + "/api/v2/room/h/init" + SECRET;
  HTTPClient clinet;
  clinet.begin(url);
  clinet.addHeader("Content-Type", "application/json");
  int httpCode = clinet.POST("");
  Serial.print("HTTP STATUS: ");
  Serial.println(httpCode);
  if (httpCode > 0) {
    String payload = clinet.getString();
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, payload);
    Serial.print("Data: ");
    String id = doc["data"]["device_id"];
    Serial.println(id);
    writeToEEPROM(0, id);
  }
  clinet.end();
  return;
}

int sendDataToServer(String endpoint, String payload) {
  String url = BASE_URL + endpoint + SECRET;
  Serial.print("URL: ");
  Serial.println(url);
  HTTPClient clinet;
  clinet.begin(url);
  clinet.addHeader("Content-Type", "application/json");
  int httpCode = clinet.POST(payload);
  Serial.print("HTTP STATUS: ");
  Serial.println(httpCode);
  clinet.end();
  return httpCode;
}

void setUpText() {
  display.clearDisplay();
  display.setTextWrap(false);
  display.setTextColor(WHITE);
  display.setTextSize(2);
  display.setCursor(22, 3);
  display.println(F("WELCOME"));
  display.setCursor(0, 26);
  display.println("SMART DOOR");
  display.setCursor(27, 50);
  display.println("SYSTEM");
  display.display();
  delay(2000);
}

void writePin(String pin) {
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(2);
  display.setTextWrap(false);
  if (is_checkin) {
    display.setCursor(7, 3);
    display.println("SMART DOOR");
  }

  if (!is_checkin) {
    display.setCursor(20, 3);
    display.println("REGISTER");
  }

  display.setCursor(29, 30);
  if (pin.length() == 0) {
    display.println("------");
  } else {
    String text = "";
    for (size_t i = 0; i < pin.length(); i++) {
      text += "*";
    }

    for (size_t i = 0; i < (6 - (pin.length())); i++) {
      text += "-";
    }
    display.println(text);
  }
  if (!isCardExist) {
    display.setCursor(25, 50);
    display.setTextSize(1);
    display.println("TAP YOUR CARD");
  }

  if (isCardExist) {
    display.setCursor(25, 50);
    display.setTextSize(1);
    display.println("CARD RECORDED");
  }

  display.display();
}

void writeAdminPin(String pin) {
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(2);
  display.setTextWrap(false);
  display.setCursor(2, 3);
  display.println("ADMIN AUTH");

  display.setCursor(29, 30);
  if (pin.length() == 0) {
    display.println("------");
  } else {
    String text = "";
    for (size_t i = 0; i < pin.length(); i++) {
      text += "*";
    }

    for (size_t i = 0; i < (6 - (pin.length())); i++) {
      text += "-";
    }
    display.println(text);
  }
  if (!isCardExist) {
    display.setCursor(25, 50);
    display.setTextSize(1);
    display.println("ENTER ROOM PIN");
  }

  if (isCardExist) {
    display.setCursor(25, 50);
    display.setTextSize(1);
    display.println("CARD RECORDED");
  }

  display.display();
}

void alert(String text) {
  display.clearDisplay();
  display.fillScreen(WHITE);
  display.setTextColor(BLACK);
  display.setTextSize(2);
  display.setTextWrap(true);
  display.setCursor(10, 10);
  display.println(text);
  display.display();
  delay(1500);
}

void loading() {
  display.clearDisplay();
  display.fillScreen(WHITE);
  display.setTextColor(BLACK);
  display.setTextSize(2);
  display.setCursor(25, 29);
  display.println("LOADING");
  display.display();
}

void BUZZER_ON() {
  digitalWrite(BUZZ_PIN, HIGH);
  delay(250);
  digitalWrite(BUZZ_PIN, LOW);
}

void BUZZER_SUCCESS() {
  digitalWrite(BUZZ_PIN, HIGH);
  delay(1250);
  digitalWrite(BUZZ_PIN, LOW);
}

void BUZZER_FAILED() {
  digitalWrite(BUZZ_PIN, HIGH);
  delay(100);
  digitalWrite(BUZZ_PIN, LOW);
  delay(100);
  digitalWrite(BUZZ_PIN, HIGH);
  delay(100);
  digitalWrite(BUZZ_PIN, LOW);
  delay(100);
  digitalWrite(BUZZ_PIN, HIGH);
  delay(250);
  digitalWrite(BUZZ_PIN, LOW);
}

void setup() {
  // INFO: Pin mode setup
  pinMode(TOUCH, INPUT);
  pinMode(BUZZ_PIN, OUTPUT);
  pinMode(RELAY, OUTPUT);

  // INFO: Start object
  Serial.begin(9600);
  EEPROM.begin(512);
  WiFi.mode(WIFI_STA);
  Wire.begin();
  keypad.begin(makeKeymap(keys));
  display.begin(SSD1306_SWITCHCAPVCC, I2C_LCD_ADDR);
  display.setRotation(2);
  SPI.begin();
  delay(100);
  Serial.println("Init RFC522 module....");
  rfid.PCD_Init();

  setUpText();

  // INFO: Setup WiFi
  setupWiFi();
  Serial.println("Connecting to WiFi..");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Success connected to wifi");

  delay(500);
  // INFO: Setup DEVICE ID
  DEVICE_ID = readFromEEPROM(0);
  if (DEVICE_ID.length() == 0) {
    initializeDevice();
  } else {
    String endpoint = BASE_URL + "/api/v2/room/h/detail/" + DEVICE_ID + SECRET;
    HTTPClient clinet;
    clinet.begin(endpoint);
    int httpCode = clinet.GET();
    clinet.end();
    if (httpCode != 200) {
      Serial.println("Failed to get room detail, try to");
      initializeDevice();
    } else {
      Serial.println("Success to get room information");
    }
  }

  // INFO: DEBUG
  Serial.print("DEVICE ID: ");
  DEVICE_ID = readFromEEPROM(0);
  Serial.println(DEVICE_ID);
  ADMIN_AUTH_URL = "/api/v2/room/h/validate/" + DEVICE_ID;
  CHECKIN_URL = "/api/v2/room/h/check-in/" + DEVICE_ID;
  Serial.println("Waiting for card");
  BUZZER_SUCCESS();
}

void loop() {
  char key = keypad.getKey();

  // INFO: Touch Sensor
  if (digitalRead(TOUCH) == HIGH) {
    BUZZER_ON();
    digitalWrite(RELAY, LOW);
    alert("OPENING\n FROM\n INSIDE");
    delay(DELAY_TIME);
  }

  // INFO: Check Server Connection
  if (key == '*') {
    BUZZER_ON();
    Serial.print("ALT#CHECK SERVER CONNECTION!");
    alert("CHECK \n SERVER \n CONNECTION");
    String endpoint = "/api/v2/room/h/online/" + DEVICE_ID + "/";
    String payload = "{\"responsesTime\":\"" + responsesTime + "\"}";
    int httpCode = sendDataToServer(endpoint, payload);
    if (httpCode < 0) {
      alert("REQUEST \n TIME \n OUT");
      Serial.print("ALT#REQUEST TIME OUT!");
    } else {
      if (httpCode == 200) {
        alert("CONNECTION \n SUCCESS");
        Serial.print("ALT#CONNECTION SUCCESS!");
        responsesTime = ""; // reset response time
      } else {
        alert("CONNECTION \n FAILED");
        Serial.print("ALT#FAILED CONNECT TO-SERVER!");
      }
    }
  }

  // INFO: Device ID
  if (key == 'A') {
    BUZZER_ON();
    alert("ROOM ID\n " + DEVICE_ID);
    delay(2500);
    if (isAdmin) {
      alert("LOCAL PIN\n " + String(DEVICE_SEC_PIN));
      delay(2500);
    }
  }

  // INFO: Changing Mode
  if (key == '#') {
    BUZZER_ON();
    pinContainer = "";
    cardIdContainer = "";
    isCardExist = false;
    if (changeMode == false) {
      changeMode = true;
    } else {
      changeMode = false;
    }
  }

  // INFO: Clearing all store data
  if (key == 'C') {
    BUZZER_SUCCESS();
    alert("CLEAR ALL \n DATA");
    pinContainer = "";
    cardIdContainer = "";
    isCardExist = false;
    rfid.PCD_Init();
    Serial.println("Clear all data");
  }

  // INFO: Reset Wifi settings
  if (key == 'B') {
    if (isAdmin) {
      BUZZER_ON();
      alert("RESET \n WIFI \n SETTINGS");
      wm.resetSettings();
      setupWiFi();
    } else {
      BUZZER_FAILED();
      alert("ADMIN \n ONLY");
      delay(1500);
    }
  }

  // INFO: Storing input pin
  if (key) {
    if (key != 'A' && key != 'B' && key != 'C' && key != 'D' && key != '*' &&
        key != '#') {
      BUZZER_ON();
      if (pinContainer.length() < 6) {
        pinContainer += key;
      }
      Serial.print("Current Pin : ");
      Serial.println(pinContainer);
    }
  }

  // INFO: Changing Mode
  if (key == 'D') {
    BUZZER_ON();
    if (changeMode) {
      if (!localChange) {
        String payload = "{\"pin\":\"" + pinContainer + "\"}";
        loading();
        int httpCode = sendDataToServer(ADMIN_AUTH_URL, payload);
        if (httpCode == 200) {
          BUZZER_SUCCESS();
          alert("SUCCES \n CHANGING \n MODE");
          changeMode = false;
          if (!isAdmin) {
            isAdmin = true;
            is_checkin = false;
          } else {
            isAdmin = false;
            is_checkin = true;
          }
        } else {
          if (httpCode < 0) {
            BUZZER_FAILED();
            alert("REQUEST \n TIME \n OUT");
            delay(1500);
            alert("USE \n LOCAL \n PIN");
            delay(1500);
            localChange = true;
          } else {
            BUZZER_FAILED();
            alert("FAILED \n CHANGING \n MODE");
          }
        }
        // After enter, clear all data
        Serial.println("FINISH SENDING DATA, CLEAR ALL STORE DATA");
        pinContainer = "";
        cardIdContainer = "";
        isCardExist = false;
      } else {
        if (pinContainer == DEVICE_SEC_PIN) {
          changeMode = false;
          if (!isAdmin) {
            isAdmin = true;
            is_checkin = false;
          } else {
            isAdmin = false;
            is_checkin = true;
          }
          alert("SUCCES \n CHANGING \n MODE");
          Serial.print("ALT#SUCCES CHANGING MODE!");
          localChange = false;
        } else {
          Serial.print("ALT#FAILED CHANGGING MODE!");
          alert("FAILED \n CHANGING \n MODE");
          localChange = true;
        }
        pinContainer = "";
        cardIdContainer = "";
        isCardExist = false;
      }
    }
  }

  // INFO: Reading RFID
  if (!isCardExist) {
    if (rfid.PICC_IsNewCardPresent()) { // new tag is available
      if (rfid.PICC_ReadCardSerial()) { // NUID has been readed
        for (byte i = 0; i < rfid.uid.size; i++) {
          cardIdContainer.concat(
              String(rfid.uid.uidByte[i] < 0x10 ? " 0" : " "));
          cardIdContainer.concat(String(rfid.uid.uidByte[i], HEX));
        }
        BUZZER_ON();
        Serial.print("CARD ID : ");
        Serial.println(cardIdContainer);
        isCardExist = true;

        // INFO: Sending Data To Server
        if (!changeMode) {
          Serial.print("Data send to server : ");
          String payload = "{\"cardNumber\":\"" + cardIdContainer + "\"" + "," +
                           "\"pin\":\"" + pinContainer + "\"}";
          Serial.println(payload);
          loading();
          if (is_checkin == true) {
            unsigned long StartTime = millis();
            int httpCode = sendDataToServer(CHECKIN_URL, payload);
            unsigned long executionTime = millis() - StartTime;
            Serial.print("Response time: ");
            Serial.print(executionTime);
            String strTime = String(executionTime);
            responsesTime += strTime + ","; // Generate report for servers

            if (httpCode == 200) {
              BUZZER_SUCCESS();
              digitalWrite(RELAY, LOW);
              alert("SUCCESS \n OPEN \n THE ROOM");
              Serial.println("RUANGAN BERHASIL TERBUKA");
              delay(DELAY_TIME);
            }

            if (httpCode == 401) {
              BUZZER_FAILED();
              alert("ENTER \n CORRECT \n PIN");
              Serial.println("RUANGAN GAGAL TERBUKA, PIN SALAH");
            }

            if (httpCode == 400) {
              BUZZER_FAILED();
              alert("FAILED \n OPEN \n THE ROOM");
              Serial.println("RUANGAN GAGAL TERBUKA");
            }

            if (httpCode < 0) {
              BUZZER_FAILED();
              alert("REQUEST \n TIME \n OUT");
            }
          }

          if (is_checkin == false) {
            if (pinContainer.length() == 6) {
              int httpCode =
                  sendDataToServer("/api/v1/card/h/register", payload);
              if (httpCode == 201) {
                BUZZER_SUCCESS();
                alert("SUCCESS \n REGISTER \n CARD");
                Serial.println("CARD \n SUCCESSFULLY \n REGISTER");
              }
              if (httpCode == 500) {
                BUZZER_FAILED();
                alert("CARD \n ALREADY \n REGISTER");
                Serial.println("CARD ALREADY REGISTER");
              }

              if (httpCode < 0) {
                BUZZER_FAILED();
                alert("REQUEST \n TIME \n OUT");
              }
            } else {
              BUZZER_FAILED();
              alert("ENTER \n 6 MIN \n PIN");
              Serial.println("GAGAL MENDAFTARKAN KARTU, MINIMUM 6 PIN");
            }
          }

          Serial.println("FINISH SENDING DATA, CLEAR ALL STORE DATA");
          pinContainer = "";
          cardIdContainer = "";
          isCardExist = false;
        }

        // INFO: Reset RFID
        rfid.PCD_Init();
      }
    }
  }

  digitalWrite(RELAY, HIGH);
  // INFO: Wifi Functionality
  if (WiFi.status() != WL_CONNECTED) {
    alert("disconnect");
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
      Serial.print(millis());
      alert("Recon..\n to WiFi");
      WiFi.disconnect();
      WiFi.reconnect();
      previousMillis = currentMillis;
    }
  } else {
    if (changeMode) {
      writeAdminPin(pinContainer);
    }

    if (!changeMode) {
      writePin(pinContainer);
    }
  }

  // INFO: Update Online Status
  uint64_t now = millis();
  if (now - messageTimestamp > 300000) {
    messageTimestamp = millis();
    String endpoint = "/api/v2/room/h/online/" + DEVICE_ID + "/";
    String payload = "{\"responsesTime\":\"" + responsesTime + "\"}";
    int httpCode = sendDataToServer(endpoint, payload);
    if (httpCode == 200) {
      responsesTime = "";
    }
    Serial.println("Updating online time and reset rfid");
    rfid.PCD_Init();
  }
}