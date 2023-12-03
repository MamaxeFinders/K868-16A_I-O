#include <PCF8574.h> // Change library LATENCY
#include <LiquidCrystal_I2C.h>
#include <HardwareSerial.h>  //Library from Arduino
#include <WiFiManager.h>     // https://github.com/tzapu/WiFiManager //https://microdigisoft.com/esp32-with-wifimanager-to-manage-ssid-and-password-no-hard-coding/
#include <HTTPClient.h>
#include <EEPROM.h>
#include <DHT.h> //Adafruit lib https://github.com/adafruit/DHT-sensor-library/blob/master/examples/DHTtester/DHTtester.ino
// Google script ID and required credentials
String GOOGLE_SCRIPT_ID = "ADD YOUR GOOGLE SCRIPT ID HERE";
WiFiManager wm;
// RS-485 pins for KC868-A16 board
#define RS485_TX_PIN 13
#define RS485_RX_PIN 16
HardwareSerial RS485Serial(1);  // Use the appropriate hardware serial port for your board (Serial2 or Serial3)

#define DHTPIN 32     // Digital pin connected to the DHT sensor
#define DHTTYPE DHT11   // DHT 11
DHT dht(DHTPIN, DHTTYPE);

#define MAX_CONNECTION_ATTEMPTS 5
#define CONNECTION_RETRY_DELAY 5000   // Retry every 5 seconds in ms
#define WIFI_CHECK_INTERVAL 14400000  // Check every 4 hours in ms

int DeviceNumber = 0;  // Set 0 so null if no input
String DeviceName = "CAISSE";
int MAX_CARD_NUMBER = 4;       // Max number of Cards
#define CARD_NUMBER_ADDRESS 0  // EEPROM address to store the card number
unsigned int ALARMcount = 0;
unsigned int ALARMcountMAX = 3;
unsigned long creditAmount = 0;
unsigned long RemainingCredit = 0;
unsigned long previousTime = 0;  // Variable to store the previous time
bool PRESOSTATstatus = false;
bool GELstatus = false;
bool ProgramStarted = false;
int SelectedProgram = 0;
int buttonIndex = -1;
int NUM_BUTTONS = 8;
int NUM_COMBINATIONS = 8;
//NodeMCU-32S
PCF8574 pcf8574_in1(0x22, 4, 5);   //input channel X1-8
PCF8574 pcf8574_in2(0x21, 4, 5);   //input channel X9-16
PCF8574 pcf8574_out1(0x24, 4, 5);  //output channel Y1-8
PCF8574 pcf8574_out2(0x25, 4, 5);  //output channel Y9-16

// CREDITS
const unsigned long CREDIT_DECREMENT_INTERVAL = 4500;                         // Interval in milliseconds between 2 decrement
const unsigned long CREDIT_DECREMENT_AMOUNT[] = { 6, 5, 5, 6, 5, 5, 0, 0 };  // Amount to decrement in cents every second
long CreditValue[] = { 100, 200, 300, 400 };                                  // Value of credit for each inputs

// COMBINAISONS DES SORTIES RELAIS Y1-8
unsigned int relay_out1_Durations[8][8] = {
  { 1, 0, 1, 0, 1, 1, 0, 0 },  // Combination  Button1
  { 1, 0, 1, 0, 0, 0, 0, 0 },  // Combination  Button2
  { 0, 1, 0, 0, 0, 0, 0, 0 },  // Combination  Button3
  { 0, 0, 0, 1, 0, 0, 0, 0 },  // Combination  Button4
  { 0, 0, 0, 0, 0, 0, 0, 0 },  // Combination
  { 0, 0, 0, 0, 0, 0, 0, 0 },  // Combination
  { 0, 0, 0, 0, 0, 0, 0, 0 },  // Combination
  { 0, 0, 0, 0, 0, 0, 0, 0 }   // Combination  STOP
};

//1.Pompe 2.Injecteur Savon 3.EV eau 4.EV eau Chaude 5.EV eau Osmose 6.EV air 7. Bipasse PreLavage
String InputDef[] = { "COIN", "COIN", "COIN", "COIN", "NA", "SHOCK", "GEL", "PRESOSTAT" };    // Inputs function
String InputButton[] = { "BUTTON", "BUTTON", "BUTTON", "BUTTON", "NA", "NA", "NA", "STOP" };  // Inputs Buttons
int ALARMoutput = 6;                                                                          // Location of ALARM relay Output Y15
int PUMPoutput = 0;                                                                           // Location of ALARM relay Output Y9
int GELoutput = 1;                                                                            // Location of GEL relay (EV eau) Output Y2
int TimePreStart = 3;                                                                         // Pre Activate the system for X seconds before program starts

int Standby_Output[] =   { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }; // Combination Standby    Output Y1-16
int Ready_Output[] =     { 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 1 }; // Combination Ready      Output Y1-16
int PRESOSTAT_Output[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 }; // Combination PRESOSTAT  Output Y1-16
int allOFF_Output[] =    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }; // Combination All OFF    Output Y1-16

// Initialize the LCD screen
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ======================================== SETUP ======================================== //
void setup() {
  Serial.begin(115200);
  RS485Serial.begin(115200, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
  pinMode(0, INPUT);  // Button DOWNLOAD used to reset 3sec
  EEPROM.begin(8);    // Initialize the EEPROM
  Serial.println(F("DHTxx test!"));
  dht.begin();

  // Set all pins of PCF8574 instances as inputs or outputs
  for (int i = 0; i < 16; i++) {
    if (i < 8) {
      pcf8574_in1.pinMode(i, INPUT);
      pcf8574_in2.pinMode(i, INPUT);
    } else {
      pcf8574_out1.pinMode(i - 8, OUTPUT);
      pcf8574_out2.pinMode(i - 8, OUTPUT);
    }
  }
  // Check if all PCF8574 instances are initialized successfully
  if (pcf8574_in1.begin() && pcf8574_in2.begin() && pcf8574_out1.begin() && pcf8574_out2.begin()) {
    Serial.println("All PCF8574 instances initialized successfully.");
  } else {
    Serial.println("Failed to initialize one or more PCF8574 instances.");
  }

  lcd.init();
  lcd.backlight();  // initialize the lcd
  displayMessage("     LOADING    ", "                ", 1);
  // Create a custom parameter for card number
  WiFiManagerParameter customCardNumber("cardNumber", "Card Number", "0", 1);
  wm.addParameter(&customCardNumber);

  // Set the timeout for WiFi configuration
  wm.setConfigPortalTimeout(60);  // Set timeout to 60 seconds
  int connectionAttempts = 0;
  bool connected = false;

  while (connectionAttempts < MAX_CONNECTION_ATTEMPTS && !connected) {
    connected = wm.autoConnect("Carte_Lavage");
    if (!connected) {
      Serial.println("Connection failed. Retrying in 5 seconds...");
      displayMessage("", "   ESSAI : " + String(connectionAttempts) + "/" + String(MAX_CONNECTION_ATTEMPTS) + "  ", 1);
      delay(CONNECTION_RETRY_DELAY);
      connectionAttempts++;
    }
  }

  // Retrieve the card number from the custom parameter
  int enteredCardNumber = atoi(customCardNumber.getValue());
  // Validate the card number and SAVE to EEPROM
  if (enteredCardNumber >= 1 && enteredCardNumber <= MAX_CARD_NUMBER) {
    DeviceNumber = enteredCardNumber;  // Set the entered card number
    EEPROM.write(CARD_NUMBER_ADDRESS, DeviceNumber);
    EEPROM.commit();
    Serial.print("NEW ENTRY : ");
    Serial.println(DeviceNumber);
  } else {  // Load the saved card number from EEPROM
    DeviceNumber = EEPROM.read(CARD_NUMBER_ADDRESS);
    Serial.print("EEPROM VALUE : ");
    Serial.println(DeviceNumber);
  }

  // Display message on Serial port, LCD line 1, clear LCD before displaying
  displayMessage("      READY     ", String(DeviceName) + " " + String(DeviceNumber) + "    ", true);
  activateRelays(Standby_Output);
  delay(3000);
}

// ======================================== LOOP ======================================== //
void loop() {
  uint8_t inputStatus = pcf8574_in2.digitalReadAll();
  int InputIndex = getInputIndex(inputStatus);
  // _______________________ FUNCTION 1 : CHECK INPUT STATUS _______________________ //
  if (!digitalRead(0)) {  // User press RESET button
    displayMessage("TEMP: " + String(dht.readTemperature()), "HUM: "+String(dht.readHumidity()), 1);
    String messageSEND = "message=PING&caisse=" + String(DeviceNumber) + "&gel=" + String(GELstatus) + "&presostat=" + String(PRESOSTATstatus)+ "&temp=" + String(dht.readTemperature())+ "&hum=" + String(dht.readHumidity());
    Send_Data_to_Google(messageSEND);
    delay(2000);
    displayMessage("   RESET        ", "   3 sec        ", 1);
    delay(3000);
    if (!digitalRead(0)) {
      wm.resetSettings();
      displayMessage("  RESET ALL     ", "", 1);
      delay(500);
      ESP.restart();
    }
  } else if (InputIndex > 0 && InputDef[InputIndex - 1] == "PRESOSTAT") {  // PRESOSTAT input
    activateRelays(PRESOSTAT_Output);
    displayMessage("   PROBLEM      ", "   PRESOSTAT   ", true);
    PRESOSTATstatus = true;
    creditAmount = 0;
    String messageSEND = "message=SYSTEM&sensor=PRESOSTAT&caisse=" + String(DeviceNumber);
    Send_Data_to_Google(messageSEND);
    delay(3000);  //Wait for 5 min : 300000ms
    return;
  } else if (InputIndex > 0 && InputDef[InputIndex - 1] == "SHOCK") {  // SHOCK input
    pcf8574_out2.digitalWrite(ALARMoutput, LOW);
    ALARMcount++;
    displayMessage("    ALARM !     ", "      " + String(ALARMcount) + "/" + String(ALARMcountMAX) + "       ", true);
    if (ALARMcount >= ALARMcountMAX) {
      String messageSEND = "message=SHOCK&status=" + String(ProgramStarted) + "&caisse=" + String(DeviceNumber) + "&credit=" + String(creditAmount);
      Send_Data_to_Google(messageSEND);
      creditAmount = 0;
      ALARMcount = 0;
      ESP.restart();
    }
    delay(3000);  // Delay 3 second for alarm
    pcf8574_out2.digitalWrite(ALARMoutput, HIGH);
    return;
  } else if (InputIndex > 0 && InputDef[InputIndex - 1] == "COIN") {  // COIN inputs
    creditAmount += CreditValue[InputIndex - 1];
    displayMessage("", "CREDIT : " + String(float(creditAmount / 100)) + " E  ", 1);
    ALARMcount = 0;
    delay(500);// Adjust the delay to avoid reading twice the COIN input
  } else if (InputIndex > 0 && InputDef[InputIndex - 1] == "GEL" && !ProgramStarted) {  // GEL input
    pcf8574_out1.digitalWrite(GELoutput, LOW);
  } else if (!ProgramStarted){
    pcf8574_out1.digitalWrite(GELoutput, HIGH);
  }
  // _______________________ FUNCTION 2 : CHECK BUTTON PROGRAM _______________________ //
  if (creditAmount > 0) {  // If credit > 0 listen buttons
    uint8_t Action_Input = pcf8574_in1.digitalReadAll();
    buttonIndex = getInputIndex(Action_Input);
    if (buttonIndex > 0 && InputButton[buttonIndex - 1] == "STOP") {  // STOP input
      displayMessage("      STOP      ", "", true);
      ProgramStarted = false;
      creditAmount = 0;
      delay(2000);
    } else if (buttonIndex > 0 && InputButton[buttonIndex - 1] == "BUTTON") {  // BUTTON input
      SelectedProgram = buttonIndex;
      activateRelays(Ready_Output);
    } else if (SelectedProgram > 0) {  // PROGRAM selected
      for (int i = 0; i <= NUM_COMBINATIONS; i++) {
        if (relay_out1_Durations[SelectedProgram - 1][i]) {
          pcf8574_out1.digitalWrite(i, LOW);
        } else {
          pcf8574_out1.digitalWrite(i, HIGH);
        }
      }
      if (!ProgramStarted) {  // if first start
        ProgramStarted = true;
        displayMessage("    ! PRET !    ", "", true);
        String messageSEND = "message=PROGRAM&Program=" + String(SelectedProgram) + "&caisse=" + String(DeviceNumber) + "&credit=" + String(creditAmount);
        Send_Data_to_Google(messageSEND);
        for (int i = TimePreStart; i > 0; i--) {
          displayMessage("", "       " + String(i) + "        ", false);
          delay(1000);
        }
      }
      pcf8574_out2.digitalWrite(PUMPoutput, LOW);  // Start PUMP
      unsigned long currentTime = millis();  // Get the current time
      // Check if the interval has elapsed
      if (currentTime - previousTime >= CREDIT_DECREMENT_INTERVAL) {
        previousTime = currentTime;  // Update the previous time
        // Decrement the credit
        if (creditAmount >= CREDIT_DECREMENT_AMOUNT[SelectedProgram - 1]) {
          creditAmount -= CREDIT_DECREMENT_AMOUNT[SelectedProgram - 1];
          displayMessage("PROGRAM  " + String(SelectedProgram) + "      ", "CREDIT : " + String(creditAmount / 100) + "." + String(creditAmount % 100) + " E  ", false);
        } else {
          creditAmount = 0;
        }
      }
    }
  } else {  // STANDBY mode
    activateRelays(Standby_Output);
    buttonIndex = -1;
    SelectedProgram = 0;
    ProgramStarted = false;
    displayMessage("BONJOUR         ", "INSEREZ PIECE   ", false);
    static unsigned long lastWifiCheckTime = 0;
    if (millis() - lastWifiCheckTime >= WIFI_CHECK_INTERVAL) {
      lcd.init();
      lcd.backlight();  // initialize the lcd
      activateRelays(allOFF_Output);
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected. Retrying...");
        WiFi.reconnect();
      } else {
        String messageSEND = "message=PING&caisse=" + String(DeviceNumber) + "&gel=" + String(GELstatus) + "&presostat=" + String(PRESOSTATstatus)+ "&temp=" + String(dht.readTemperature())+ "&hum=" + String(dht.readHumidity());
        Send_Data_to_Google(messageSEND);
        PRESOSTATstatus = false;
        Serial.println("WiFi OK");
      }
      lastWifiCheckTime = millis();
    }
  }
  delay(10);
}

// ======================================== FUNCTIONS ======================================== //
// ---- ACTIVATE RELAYS ---- //
void activateRelays(const int* outputStatus) {
  for (int i = 0; i < 16; i++) {
    if (i < 8) {
      if (outputStatus[i] == 1) {
        pcf8574_out1.digitalWrite(i, LOW);
      } else if (outputStatus[i] == 0) {
        pcf8574_out1.digitalWrite(i, HIGH);
      }
    } else {
      if (outputStatus[i] == 1) {
        pcf8574_out2.digitalWrite(i - 8, LOW);
      } else if (outputStatus[i] == 0) {
        pcf8574_out2.digitalWrite(i - 8, HIGH);
      }
    }
  }
}
// ---- SEND MESSAGE TO LCD & RS485 & SERIAL ---- //
void displayMessage(const String& messageL1, const String& messageL2, bool clearLCD) {
  // Display message on Serial port
  int indicator = 0;
  int totalLength = 16;
  String messageSerial = messageL1 + " & " + messageL2;
  String messageRS485 = "";
  Serial.println(messageSerial);
  // Display message on LCD
  if (clearLCD) {
    lcd.clear();
    indicator = 2;
  }
  if (messageL1 != "") {
    lcd.setCursor(0, 0);
    lcd.print(messageL1);
    int spacesToAdd = totalLength - messageL1.length();
    for (int i = 0; i < spacesToAdd; i++) { String(messageL1) += " "; }
    messageRS485 = String(1 + indicator) + ":" + messageL1;
    RS485Serial.println(messageRS485);
  }
  if (messageL2 != "") {
    lcd.setCursor(0, 1);
    lcd.print(messageL2);
    int spacesToAdd = totalLength - messageL2.length();
    for (int i = 0; i < spacesToAdd; i++) { String(messageL2) += " "; }
    messageRS485 = "2:" + messageL2;
    RS485Serial.println(messageRS485);
  }
}
// ---- GET VALUE INPUT ---- //
int getInputIndex(uint8_t inputStatus) {
  int buttonIndex = -1;
  bool bitFound = false;
  // Count the number of set bits in inputStatus
  for (int i = 0; i < 8; i++) {
    if ((inputStatus & (1 << i)) == 0) {
      bitFound = true;
      buttonIndex = i + 1;  // Store the index of the set bit
    }
  }
  if (bitFound) {
    return buttonIndex;  // Return the button index if only one bit is set
  } else {
    return -1;  // Return -1 if no button is pressed or multiple buttons are pressed
  }
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
