//ESP32-WROOM-DA-Module

#include <HardwareSerial.h>
#include <LiquidCrystal_I2C.h>
#include <WiFiManager.h>     // https://github.com/tzapu/WiFiManager //https://microdigisoft.com/
#include <HTTPClient.h>
// Google script ID and required credentials
String GOOGLE_SCRIPT_ID = "AKfycbziCv5KrLd04WmRT_k1B10jjVmOscfB7w6_DkPbByOzOITigoVcK06lYO42ExSkvbdJ";
WiFiManager wm;

#define CONNECTION_RETRY_DELAY 500   // Retry every 5 seconds in ms
// RS485 setup with ESp32
HardwareSerial RS485Serial_1(2);
HardwareSerial RS485Serial_2(0);

// Initialize the LCD screen
LiquidCrystal_I2C lcd(0x27, 16, 2);
String message = "Pas de message";
void setup() {
  Serial.begin(115200);
  RS485Serial_1.begin(9600, SERIAL_8N1, 16, -1); //16 on RO & 17 on DI
  RS485Serial_2.begin(9600, SERIAL_8N1, 3, -1); //16 on RO & 17 on DI
  pinMode(RE, OUTPUT);
  pinMode(DE, OUTPUT);
  digitalWrite(DE, LOW);
  digitalWrite(RE, LOW);
  // ---- LCD SETUP ----//
  lcd.init();  // initialize the lcd
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("START           ");

  // Set the timeout for WiFi configuration
  wm.setConfigPortalTimeout(30);  // Set timeout to 60 seconds
  int connectionAttempts = 0;
  bool connected = false;

  while (!connected) {
    connected = wm.autoConnect("SYSTEM_Lavage");
    if (!connected) {
      Serial.println("Connection failed. Retrying in 5 seconds...");
        lcd.setCursor(0, 0);
        lcd.print("ESSAI WIFI: " + String(connectionAttempts) + "   ");
      delay(CONNECTION_RETRY_DELAY);
      connectionAttempts++;
    }
  }
  lcd.setCursor(0, 1);
  lcd.print("WiFi OK");
  delay(1000);
}

void loop() {
  if (!digitalRead(0)) {  // User press RESET button
    lcd.clear();
    lcd.setCursor(0, 1);
    if(WiFi.status() == WL_CONNECTED) {
      // Check Internet connectivity
      if (isConnectedToInternet()) {
        // If connected, display a message or perform actions that require internet
        lcd.print("WIFI OK NET OK"); // Ensure the message overwrites previous text
        Serial.println("WIFI OK NET OK");
      } else {
        // If not connected, display a different message or handle accordingly
        lcd.print("WIFI OK NET NOK"); // Ensure the message overwrites previous text
        Serial.println("WIFI OK NET NOK");
      }
    } else {
      lcd.print("WIFI NOK NET NOK");
      Serial.println("WIFI NOK NET NOK");
    }
    lcd.print("Reset dans 3 sec");
    delay(3000);
    if (!digitalRead(0)) {
      lcd.print("     RESET      ");
      delay(500);
      wm.resetSettings();
      ESP.restart();
    }
  }else if (RS485Serial_1.available()) {
    message = RS485Serial_1.readStringUntil('\n');
    Serial.print("Received S1: ");
    Serial.println(message);
  }else if(RS485Serial_2.available()) {
    message = RS485Serial_2.readStringUntil('\n');
    Serial.print("Received S2: ");
    Serial.println(message);
  }
    lcd.setCursor(0, 0);
    lcd.print("CAISSE");
    if (message.startsWith("G:")) { // PIN CAISSE to Google
      message.remove(0, 2);  // Remove the indicator from the message
      Send_Data_to_Google(message);
    }else if (message.startsWith("1:")) { //RS485 CAISSE
        lcd.setCursor(7, 0);
        lcd.print("1");
    }else if (message.startsWith("2:")) { //RS485 CAISSE
        lcd.setCursor(9, 0);
        lcd.print("2");
    }else if (message.startsWith("3:")) { //RS485 CAISSE
        lcd.setCursor(11, 0);
        lcd.print("3");
    }else if (message.startsWith("4:")) { //RS485 CAISSE
        lcd.setCursor(13, 0);
        lcd.print("4");
    }
  
  static unsigned long lastLCDTime = 0;
  if (millis() - lastLCDTime >= 20000) {
    lcd.clear();
    message = "Pas de message";
    lastLCDTime = millis();
    lcd.setCursor(0, 1);
    if(WiFi.status() == WL_CONNECTED) {
      // Check Internet connectivity
      if (isConnectedToInternet()) {
        // If connected, display a message or perform actions that require internet
        lcd.print("WIFI OK NET OK"); // Ensure the message overwrites previous text
        Serial.println("WIFI OK NET OK");
      } else {
        // If not connected, display a different message or handle accordingly
        lcd.print("WIFI OK NET NOK"); // Ensure the message overwrites previous text
        Serial.println("WIFI OK NET NOK");
      }
    } else {
      lcd.print("WIFI NOK NET NOK");
      Serial.println("WIFI NOK NET NOK");
    }
  }
 delay(10);
}

bool isConnectedToInternet() {
  HTTPClient http;
  http.begin("http://www.google.com"); // URL to perform the GET request
  int httpCode = http.GET();
  // httpCode will be negative on error
  if (httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      Serial.printf("[HTTP] GET... code: %d\n", httpCode);
      // file found at server
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
          return true;
      }
  } else {
      Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();
  return false;
}

// ---- SEND DATA TO GOOGLE ---- //
String Send_Data_to_Google(String Message_to_http) {
  String urlFinal = "https://script.google.com/macros/s/" + GOOGLE_SCRIPT_ID + "/exec?" + Message_to_http;
  Serial.print("POST data to spreadsheet:");
  Serial.println(urlFinal);
  HTTPClient http;
  http.begin(urlFinal.c_str());
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  int httpCode = http.GET();
  Serial.print("HTTP Status Code: ");
  Serial.println(httpCode);
  //---------------------------------------------------------------------
  //getting response from google sheet
  String payload;
  if (httpCode > 0) {
    payload = http.getString();
    //Serial.println("Payload: "+payload);
  }
  //---------------------------------------------------------------------
  http.end();
  return payload;
}
// ---- GET DATA FROM GOOGLE ---- //
String Get_Data_from_Google(String Message_to_http) {  // Not used here
  HTTPClient http;
  String url = "https://script.google.com/macros/s/" + GOOGLE_SCRIPT_ID + "/exec?" + Message_to_http;
  Serial.print("Making a request from URL :");
  Serial.println(url);
  http.begin(url.c_str());  //Specify the URL and certificate
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  int httpCode = http.GET();
  String payload;
  if (httpCode > 0) {  //Check for the returning code
    payload = http.getString();
    return payload;
  } else {
    Serial.println("Error on HTTP request");
    return "Error on HTTP request";
  }
  http.end();
}
