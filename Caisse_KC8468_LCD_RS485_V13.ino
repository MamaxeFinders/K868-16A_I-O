//NodeMCU-32S

#include <PCF8574.h> // Change library LATENCY
#include <LiquidCrystal_I2C.h>
#include <HardwareSerial.h>  //Library from Arduino
#include <DHT.h> //Adafruit lib https://urldefense.com/v3/__https://github.com/adafruit/DHT-sensor-library/blob/master/examples/DHTtester/DHTtester.ino__;!!Dl6pPzL6!f1NLhxYDNph0NM5BrmR5ROBmmrHJFT7AaXGXELbBxjAcaQ2hg9YNwBSDHwp0HUFtbU0-WcsGZG57yM6-qPP5oA$ 
#include <WiFiManager.h> // https://urldefense.com/v3/__https://github.com/tzapu/WiFiManager__;!!Dl6pPzL6!f1NLhxYDNph0NM5BrmR5ROBmmrHJFT7AaXGXELbBxjAcaQ2hg9YNwBSDHwp0HUFtbU0-WcsGZG57yM4I0ZK6aQ$ 
WiFiManager wm;
WiFiManagerParameter custom_field; // global param ( for non blocking w params )
#include <Preferences.h>
Preferences preferences;

// RS-485 pins for KC868-A16 board
#define RS485_TX_PIN 13
#define RS485_RX_PIN 16
HardwareSerial RS485Serial(1);  // Use the appropriate hardware serial port for your board (Serial2 or Serial3)

#define DHTPIN 32       // Digital pin connected to the DHT sensor
#define DHTTYPE DHT22   // DHT 11
DHT dht(DHTPIN, DHTTYPE);

int DeviceNumber = 0;
String DeviceName = "CAISSE";
unsigned int ALARMcount = 0;
unsigned int ALARMcountMAX = 3;
float creditAmount = 0.0;
unsigned long RemainingCredit = 0;
unsigned long previousTime = 0;
bool PRESOSTATstatus = false;
bool GELstatus = false;
bool ProgramStarted = false;
int SelectedProgram = 0;
int buttonIndex = -1;
int NUM_BUTTONS = 8;
int NUM_COMBINATIONS = 8;
#define PING_INTERVAL 14400000  // Check every 4 hours in ms

PCF8574 pcf8574_in1(0x22, 4, 5);   //input channel X1-8
PCF8574 pcf8574_in2(0x21, 4, 5);   //input channel X9-16
PCF8574 pcf8574_out1(0x24, 4, 5);  //output channel Y1-8
PCF8574 pcf8574_out2(0x25, 4, 5);  //output channel Y9-16

// CREDITS
const unsigned long CREDIT_DECREMENT_INTERVAL = 2000;                             // Interval in milliseconds between 2 decrement
const float CREDIT_DECREMENT_AMOUNT[] = { 2.4, 2.4, 2.2, 2.4, 0, 0, 0, 0 };  // Amount to decrement in cents every second
long CreditValue[] = { 100, 200, 300, 400 };                                      // Value of credit for each inputs

// COMBINAISONS DES SORTIES RELAIS Y1-8
unsigned int relay_out1_Durations[8][8] = {
  { 0, 0, 1, 0, 1, 0, 1, 0 },  // Combination  Button1
  { 1, 0, 1, 0, 0, 1, 0, 0 },  // Combination  Button2
  { 0, 1, 0, 0, 0, 1, 0, 0 },  // Combination  Button3
  { 0, 0, 0, 1, 0, 1, 0, 0 },  // Combination  Button4
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
int GELoutput = false;                                                                            // Location of GEL relay (EV eau) Output Y2
int TimePreStart = 3;                                                                         // Pre Activate the system for X seconds before program starts

int Standby_Output[] =   { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }; // Combination Standby    Output Y1-16
int GEL_Output[] =       { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }; // Combination Standby    Output Y1-16
int Ready_Output[] =     { 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 1 }; // Combination Ready      Output Y1-16
int PRESOSTAT_Output[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 }; // Combination PRESOSTAT  Output Y1-16
int allOFF_Output[] =    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }; // Combination All OFF    Output Y1-16

// Initialize the LCD screen
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ======================================== SETUP ======================================== //
void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP 
  // Load the last saved value from Preferences (or default if not set)
  preferences.begin("my-app", true);
  DeviceNumber = preferences.getInt("customValue", 0);
  Serial.print("DeviceNumber : ");Serial.println(DeviceNumber);
  preferences.end();
  delay(DeviceNumber*100);

  Serial.println("\n Starting");
  // add a custom input field
  //int customFieldLength = 40;
  const char* custom_radio_str = "<br/><label for='customfieldid'>Choisir CAISSE</label><br><br><input type='radio' name='customfieldid' value='1' checked> 1<br><input type='radio' name='customfieldid' value='2'> 2<br><input type='radio' name='customfieldid' value='3'> 3<br><input type='radio' name='customfieldid' value='4'> 4";
  new (&custom_field) WiFiManagerParameter(custom_radio_str); // custom html input
  wm.addParameter(&custom_field);
  wm.setSaveParamsCallback(saveParamCallback);
  std::vector<const char *> menu = {"wifi","info","sep","param","sep","restart","exit"};
  wm.setMenu(menu);
  wm.setClass("invert"); // set dark theme
  wm.setConfigPortalTimeout(60); // auto close configportal after n seconds
    bool res;
    char WiFiName[32];
    sprintf(WiFiName, "CAISSE_%d", DeviceNumber);
    res = wm.autoConnect(WiFiName);
    if(!res) {Serial.println("Failed to connect");} 
    else {Serial.println("connected...yeey :)");}

  RS485Serial.begin(115200, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
  pinMode(0, INPUT);  // Button DOWNLOAD used to reset 3sec
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

  String messageSEND = "message=PING&event=START&caisse=" + String(DeviceNumber) + "&gel=" + String(GELstatus) + "&presostat=" + String(PRESOSTATstatus)+ "&temp=" + String(dht.readTemperature())+ "&hum=" + String(dht.readHumidity());
  //Send_Data_to_Google(messageSEND);
  SendToRS485("G:"+messageSEND);
  // Display message on Serial port, LCD line 1, clear LCD before displaying
  displayMessage("      READY     ", String(DeviceName) + " " + String(DeviceNumber) + "    ", true);
  activateRelays(Standby_Output);
  delay(3000);
}

// ======================================== LOOP ======================================== //
void loop() {
  uint8_t inputStatus = pcf8574_in2.digitalReadAll();
  int InputIndex = getInputIndexINPUTSTATUS(inputStatus);
  // _______________________ FUNCTION 1 : CHECK INPUT STATUS _______________________ //
  if (!digitalRead(0)) {  // User press RESET button
    displayMessage("TEMP: " + String(dht.readTemperature()), "HUM: "+String(dht.readHumidity()), 1);
    String messageSEND = "message=PING&event=RESET&caisse=" + String(DeviceNumber) + "&gel=" + String(GELstatus) + "&presostat=" + String(PRESOSTATstatus)+ "&temp=" + String(dht.readTemperature())+ "&hum=" + String(dht.readHumidity());
    //Send_Data_to_Google(messageSEND);
    SendToRS485("G:"+messageSEND);
    delay(2000);
    displayMessage("   RESET        ", "   3 sec        ", 1);
    delay(3000);
    if (!digitalRead(0)) {
      displayMessage("  RESET ALL     ", "", 1);
      delay(500);
      wm.resetSettings();
      ESP.restart();
    }
  } else if (InputIndex > 0 && InputDef[InputIndex - 1] == "PRESOSTAT") {  // PRESOSTAT input
    activateRelays(PRESOSTAT_Output);
    displayMessage("   PROBLEM      ", "   PRESOSTAT   ", true);
    PRESOSTATstatus = true;
    creditAmount = 0;
    delay(3000);  //Wait for 3 min
    return;
  } else if (InputIndex > 0 && InputDef[InputIndex - 1] == "SHOCK") {  // SHOCK input
    pcf8574_out2.digitalWrite(ALARMoutput, LOW);
    ALARMcount++;
    displayMessage("    ALARM !     ", "      " + String(ALARMcount) + "/" + String(ALARMcountMAX) + "       ", true);
    if (ALARMcount >= ALARMcountMAX) {
      String messageSEND = "message=SHOCK&status=" + String(ProgramStarted) + "&caisse=" + String(DeviceNumber) + "&credit=" + String(creditAmount);
      //Send_Data_to_Google(messageSEND);
      SendToRS485("G:"+messageSEND);
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
    delay(2000);// Adjust the delay to avoid reading twice the COIN input
  } else if (InputIndex > 0 && InputDef[InputIndex - 1] == "GEL" && !ProgramStarted && !GELoutput) {  // GEL input
    GELoutput = true;
    Serial.println("GEL ACTIVE");
  } else if (InputDef[InputIndex - 1] != "GEL" && !ProgramStarted && GELoutput){
    GELoutput = false;
    Serial.println("GEL DEACTIVE");
  }
  // _______________________ FUNCTION 2 : CHECK BUTTON PROGRAM _______________________ //
  if (creditAmount > 0) {  // If credit > 0 listen buttons
    uint8_t Action_Input = pcf8574_in1.digitalReadAll();
    buttonIndex = getInputIndexBUTTON(Action_Input);
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
        //Send_Data_to_Google(messageSEND);
        SendToRS485("G:"+messageSEND);
        for (int i = TimePreStart; i > 0; i--) {
          displayMessage("", "       " + String(i) + "        ", false);
          delay(1000);
        }
      }
      pcf8574_out2.digitalWrite(PUMPoutput, LOW);  // Start PUMP
    unsigned long currentTime = millis(); // Get the current time
    // Check if the interval has elapsed
    if (currentTime - previousTime >= CREDIT_DECREMENT_INTERVAL) {
        previousTime = currentTime; // Update the previous time
        // Decrement the credit
        if (creditAmount >= CREDIT_DECREMENT_AMOUNT[SelectedProgram - 1]) {
            creditAmount -= CREDIT_DECREMENT_AMOUNT[SelectedProgram - 1];
            int wholePart = int(creditAmount/100); // Get whole part
            int fractionalPart = int((creditAmount/100 - wholePart) * 100); // Get fractional part
            displayMessage("PROGRAM  " + String(SelectedProgram) + "      ","CREDIT : " + String(wholePart) + "." + (fractionalPart < 10 ? "0" : "") + String(fractionalPart) + " E  ",false);
        } else {
            creditAmount = 0;
        }
      }
    }
  } else {  // STANDBY mode when Credit = 0
    if(GELoutput){activateRelays(GEL_Output);
    }else{activateRelays(Standby_Output);}
    buttonIndex = -1;
    SelectedProgram = 0;
    ProgramStarted = false;
    displayMessage("BONJOUR         ", "INSEREZ PIECE   ", false);
    static unsigned long lastRS485CheckTime = 0;
    if (millis() - lastRS485CheckTime >= 1000) {
      SendToRS485(String(DeviceNumber)+":OK");
      lastRS485CheckTime = millis();
    }
    static unsigned long lastPINGCheckTime = 0;
    if (millis() - lastPINGCheckTime >= PING_INTERVAL) {
      lcd.init();
      lcd.backlight();  // initialize the lcd
      activateRelays(allOFF_Output);
        String messageSEND = "message=PING&event=PING&caisse=" + String(DeviceNumber) + "&gel=" + String(GELstatus) + "&presostat=" + String(PRESOSTATstatus)+ "&temp=" + String(dht.readTemperature())+ "&hum=" + String(dht.readHumidity());
        //Send_Data_to_Goo gle(messageSEND);
        SendToRS485("G:"+messageSEND);
        PRESOSTATstatus = false;
        Serial.println("WiFi OK");
      lastPINGCheckTime = millis();
    }
  }
  delay(10);
}

String getParam(String name){
  //read parameter from server, for customhmtl input
  String value;
  if(wm.server->hasArg(name)) {
    value = wm.server->arg(name);
  }
  return value;
}

// ======================================== PARAM WIFI ======================================== //
void saveParamCallback(){
  String customValueStr = getParam("customfieldid");
  // Convert the string to an integer
  int customValueInt = customValueStr.toInt();
  // Save the updated value to Preferences.
  preferences.begin("my-app", false);
  preferences.putInt("customValue", customValueInt);
  preferences.end();
  Serial.println("[CALLBACK] saveParamCallback fired");
  Serial.println("PARAM customfieldid = " + getParam("customfieldid"));
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
    //RS485Serial.println(messageRS485);
  }
  if (messageL2 != "") {
    lcd.setCursor(0, 1);
    lcd.print(messageL2);
    int spacesToAdd = totalLength - messageL2.length();
    for (int i = 0; i < spacesToAdd; i++) { String(messageL2) += " "; }
    messageRS485 = "2:" + messageL2;
    //RS485Serial.println(messageRS485);
  }
}
// ---- GET VALUE INPUT STATUS ---- //
int getInputIndexINPUTSTATUS(uint8_t inputStatus) {
  for (int i = 0; i < 8; i++) {
    if (inputStatus & (1 << i)) == 0) {
      return i+1;  // Return the index of the set bit
    }
  }
  return -1; // Return -1 if no set bit is found
}
// ---- GET VALUE INPUT BUTTON ---- //
int getInputIndexBUTTON(uint8_t inputStatus) {
  for (int i = 7; i >= 0; i--) {
    if (inputStatus & (1 << i)) == 0) {
      return i+1;  // Return the index of the set bit
    }
  }
  return -1; // Return -1 if no set bit is found
}

void SendToRS485(String messageRS485){
  RS485Serial.println(messageRS485);
  delay(10);
  Serial.print("RS485:");
  Serial.println(messageRS485);
  delay(10);
}
