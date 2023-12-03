//ESP32-WROOM-DA-Module

#include <HardwareSerial.h>
#include <LiquidCrystal_I2C.h>
#include <WiFiManager.h>     // https://github.com/tzapu/WiFiManager //https://microdigisoft.com/
#include <HTTPClient.h>
// Google script ID and required credentials
String GOOGLE_SCRIPT_ID = "XXXX";
WiFiManager wm;

#define CONNECTION_RETRY_DELAY 500   // Retry every 5 seconds in ms
// RS485 setup with ESp32
HardwareSerial RS485Serial(1);
#define RE 4  // Connect RE terminal with 32 of ESP
#define DE 5  // Connect DE terminal with 33 of ESP
// Initialize the LCD screen
LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
  Serial.begin(115200);
  RS485Serial.begin(115200, SERIAL_8N1, 16, 17); //16 on RO & 17 on DI
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
  wm.setConfigPortalTimeout(60);  // Set timeout to 60 seconds
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
    lcd.print("Reset dans 3 sec");
    delay(3000);
    if (!digitalRead(0)) {
      lcd.print("     RESET      ");
      delay(500);
      wm.resetSettings();
      ESP.restart();
    }
  }else if (RS485Serial.available()) {
    String message = RS485Serial.readStringUntil('\n');
    Serial.print("Received: ");
    Serial.println(message);
    if (message.startsWith("G:")) { // PIN CAISSE to Google
      message.remove(0, 2);  // Remove the indicator from the message
      Send_Data_to_Google(message);
    }else if (message.startsWith("1:")) { //RS485 CAISSE
        lcd.setCursor(0, 0);
        lcd.print("CAISSE 1");
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
  }else if(WiFi.status() == WL_CONNECTED) {
    lcd.setCursor(0, 1);
    lcd.print("WIFI OK");
  } else {
    lcd.setCursor(0, 1);
    lcd.print("WIFI NOK");
  }
  static unsigned long lastLCDTime = 0;
  if (millis() - lastLCDTime >= 10000) {
    lcd.clear();
    lastLCDTime = millis();
  }
 delay(10);
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

