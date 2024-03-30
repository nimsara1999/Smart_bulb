#include "EEPROM.h"
#include "WiFi.h"
#include <PubSubClient.h>
#include "HomeSpan.h"
#include "DEV_LED.h"
#include <string>
#include <cstring>
#include <WebServer.h>
#define EEPROM_SIZE 64
#define ESPALEXA_ASYNC
#include <Espalexa.h>
#include <ArduinoJson.h>

const char *mqtt_broker = "broker.emqx.io";
const char *mqtt_username = "nimsaraiot";
const char *mqtt_password = "Nimsaraiot1";
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);
Espalexa espalexa;
bool mqttFlag=false;
char* deviceID="LED_00001";
String deviceID1="LED_00001";

unsigned long buttonPressStartTime = 0; // Variable to store the time when the button is pressed
const int longPressDuration = 1500; // Duration in milliseconds for a long press

bool deviceConnected = false;
bool oldDeviceConnected = false;
bool applePoll = false;
bool alexaPoll = false;
bool subscribeFlag=false;
bool publishFlag=false;
bool alexaFlag=false;
bool apFlag = false;
bool appleAlexaPairingEnableFlag=false;
bool appleAlexaPairingOnetimeDisable=false;
bool blinkInbuiltLed=false;
unsigned long previousMillis = 0;
bool ledState = false; 

const int redPin=4;
const int greenPin=18;
const int bluePin=19;
const int ledPin = 2;
const int modeAddr = 0;
const int wifiAddr = 10;
const int buttonPin = 23;  // Change this to the pin connected to your push button
int red=0;
int green=0;
int blue=0;
int redCustom=1000;
int greenCustom=1000;
int blueCustom=1000;
int redAlexaGlb=0;
int greenAlexaGlb=0;
int blueAlexaGlb=0;

int modeIdx;

void colorLightChanged(uint8_t brightness, uint32_t rgb);
void wifiTask();
String generateMqttTopic();

const char* ssid = deviceID;
const char* password = "password";
const char* devicePassword = "11112222";

WebServer server(80);
bool shouldSaveConfig = false;

void handleRoot() {
  String dropdownOptions = ""; // String to store the options for the dropdown menu

  // Scan for available Wi-Fi networks
  int numNetworks = WiFi.scanNetworks();
  if (numNetworks == 0) {
    dropdownOptions = "<option value=''>No networks found</option>"; // Display message if no networks are found
  } else {
    // Populate dropdownOptions with available networks
    for (int i = 0; i < numNetworks; ++i) {
      dropdownOptions += "<option value='" + WiFi.SSID(i) + "'>" + WiFi.SSID(i) + "</option>";
    }
  }

  // Send the HTML response with the dropdown menu
  server.send(200, "text/html",
    "<form action='/save' method='post' style='font-family: Arial, sans-serif; padding: 20px; border-radius: 10px;'>"
    "Select Wi-Fi Network:<br>"
    "<select name='ssid' style='margin-bottom: 10px; padding: 8px; border: 1px solid #ccc; border-radius: 5px;'>" +
    dropdownOptions +
    "</select><br>"
    "Password: <input type='text' name='password' style='margin-bottom: 10px; padding: 8px; border: 1px solid #ccc; border-radius: 5px;'><br>"
    "Device Password: <input type='password' name='devicepassword' style='margin-bottom: 10px; padding: 8px; border: 1px solid #ccc; border-radius: 5px;'><br>"
    "<input type='submit' value='Submit' style='margin-bottom: 10px; padding: 8px; border: 1px solid #ccc; border-radius: 10px; background-color:#8F00FF;'>"
    "</form>");
}


void handleSave() {
  String newSsid = server.arg("ssid");
  String newPassword = server.arg("password");
  String devicePass = server.arg("devicepassword");

  if (devicePass == devicePassword) {
    Serial.println("Device password is correct.");
    shouldSaveConfig = true;
    server.send(200, "text/plain", "WiFi credentials saved. Rebooting...");
    std::string value = std::string(newSsid.c_str()) + "," + std::string(newPassword.c_str());
    writeStringEEPROM(wifiAddr, value.c_str());
    wifiTask();
  } else {
    Serial.println("Device password is incorrect.");
    server.send(403, "text/plain", "Forbidden");
  }
}


  void writeStringEEPROM(int add, String data) {
    int _size = data.length();
    for (int i = 0; i < _size; i++) {
      EEPROM.write(add + i, data[i]);
    }
    EEPROM.write(add + _size, '\0');
    EEPROM.commit();
  }


void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);  // Enable internal pull-up resistor
  pinMode(redPin,OUTPUT);
  pinMode(greenPin,OUTPUT);
  pinMode(bluePin,OUTPUT);

  if (!EEPROM.begin(EEPROM_SIZE)) {
    delay(1000);
  }

  modeIdx = EEPROM.read(modeAddr);
  Serial.print("modeIdx : ");
  Serial.println(modeIdx);

  if (modeIdx != 0) {
    digitalWrite(ledPin, HIGH);
    apTask();
  } else {
    digitalWrite(ledPin, LOW);
    wifiTask();
  }
}

void connectToApple() {
  delay(100);
  homeSpan.setPortNum(81);
  homeSpan.begin(Category::Lighting, deviceID);
  homeSpan.setPairingCode(devicePassword);
  homeSpan.setQRID(devicePassword);
  applePoll = true;
    new SpanAccessory();                                                          
    new Service::AccessoryInformation();    
      new Characteristic::Identify();               
      new Characteristic::Name(deviceID); 
    new DEV_RgbLED(redPin,greenPin,bluePin);
}

void connectToAlexa(){
  espalexa.addDevice(deviceID, colorLightChanged);
  espalexa.begin();
  espalexa.setDiscoverable(false);
  alexaPoll = true;
}

void apTask() {
  apFlag=true;
  Serial.println("Pairing Mode");
  digitalWrite(ledPin, HIGH);
  WiFi.softAP(ssid, password);
  delay(100);

  server.on("/", handleRoot);
  server.on("/save", handleSave);
  server.begin();

  Serial.println("Waiting a client connection to notify...");
}

void wifiTask() {
  WiFi.softAPdisconnect(true);
  apFlag=false;
  Serial.println("Normal Mode");
  digitalWrite(ledPin, LOW);
  String receivedData;
  receivedData = read_String(wifiAddr);
  if (receivedData.length() > 0) {
    String wifiName = getValue(receivedData, ',', 0);
    String wifiPassword = getValue(receivedData, ',', 1);
    if (wifiName.length() > 0 && wifiPassword.length() > 0) {
      Serial.print("WifiName : ");
      Serial.println(wifiName);
      Serial.print("wifiPassword : ");
      Serial.println(wifiPassword);
      if (!wifiConnect(wifiName, wifiPassword)) {
        EEPROM.write(modeAddr, 1);
        EEPROM.commit();
        ESP.restart();
      } else {
        connectToApple();
        connectToAlexa();
        mqttConnect();
        delay(100);
      }
    }
  }
}

void mqttConnect(){
  client.setServer(mqtt_broker, mqtt_port);
  client.setCallback(callbackMQTT);
  while (!client.connected()) {
    String client_id = "esp32-client-";
    client_id += String(WiFi.macAddress());
    Serial.printf("The client %s connects to the public mqtt broker\n", client_id.c_str());
    if (client.connect("smart-bulb",mqtt_username,mqtt_password)) {
        Serial.println("MQTT broker connected");
        mqttFlag=true;
       bool test=client.subscribe(generateMqttTopic(deviceID1,"setStatus"),0);
    } else {
      Serial.print("failed with state ");
      Serial.print(client.state());
      delay(1000);
     }
  }
}

void callbackMQTT(char *topic, byte *payload, unsigned int length) {
  if(!publishFlag){
    subscribeFlag = true;
    Serial.print("Message arrived in topic: ");
    Serial.println(topic);
    Serial.print("Message:");
    // Construct a JSON object
    StaticJsonDocument<200> jsonDoc; // Adjust the size according to your payload size
    // Parse the payload into the JSON object
    DeserializationError error = deserializeJson(jsonDoc, payload, length);
    if (error) {
      Serial.print("Failed to parse JSON: ");
      Serial.println(error.c_str());
      return;
    }
    String jsonString;
    serializeJson(jsonDoc, jsonString);
    parseJsonAndAssignValues(jsonDoc);
  }
  else{
    publishFlag=false;
  }
}

void parseJsonAndAssignValues(StaticJsonDocument<200> doc) {
  // Check if the JSON object contains the expected keys
  if (doc.containsKey("red") && doc.containsKey("green") && doc.containsKey("blue")) {
    // Assign values to variables
    redCustom = doc["red"];
    greenCustom = doc["green"];
    blueCustom = doc["blue"];
    Serial.print(redCustom);Serial.print(",");Serial.print(greenCustom);Serial.print(",");
    Serial.println(blueCustom);
  } else {
    Serial.println("JSON object does not contain expected keys.");
  }
}

String read_String(int add) {
  char data[100];
  int len = 0;
  unsigned char k;
  k = EEPROM.read(add);
  while (k != '\0' && len < 500) {
    k = EEPROM.read(add + len);
    data[len] = k;
    len++;
  }
  data[len] = '\0';
  return String(data);
}

String getValue(String data, char separator, int index) {
  int found = 0;
  int strIndex[] = { 0, -1 };
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

bool wifiConnect(String ssid, String password) {
  WiFi.begin(ssid, password);
  unsigned long startTime = millis();  // Get the current time
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) {
    delay(500);
    Serial.println("Connecting to WiFi..");
  }

  if (WiFi.status() == WL_CONNECTED) {  // If connected successfully
    Serial.println("Connected to the WiFi network");
    EEPROM.write(modeAddr, 0);
    EEPROM.commit();
    return true;
  }
  Serial.println("Failed to connect to WiFi within 10 seconds");
  return false;
}

const char* generateMqttTopic(String deviceId, String type) {
    String firstPart = "MQTT_LED_RGB/device/" + deviceId;
    String topic;

    if (type == "setStatus") {
        topic = firstPart + "/setStatus";
    } else if (type == "getStatus") {
        topic = firstPart + "/getStatus";
    } else {
        return ""; 
    }
    Serial.println(topic);
    // Convert std::string to const char*
    char* cstr = new char[topic.length() + 1];
    strcpy(cstr, topic.c_str());
    return cstr;
}

char* convertToJson(int red, int green, int blue) {
  // Create a DynamicJsonDocument
  DynamicJsonDocument doc(128);
  doc["red"] = red;
  doc["green"] = green;
  doc["blue"] = blue;
  char* jsonString = (char*) malloc(measureJson(doc) + 1); // Allocate memory
  serializeJson(doc, jsonString, measureJson(doc) + 1); // Serialize JSON to buffer
  return jsonString; 
}

//the color device callback function has two parameters
void colorLightChanged(uint8_t brightness, uint32_t rgb) {
  alexaFlag=true;
  //do what you need to do here, for example control RGB LED strip
  uint8_t redAlexa = (rgb >> 16) & 0xFF;
  uint8_t greenAlexa = (rgb >>  8) & 0xFF;
  uint8_t blueAlexa = rgb & 0xFF;

  redAlexa = (redAlexa * brightness/255);
  greenAlexa = (greenAlexa * brightness/255);
  blueAlexa = (blueAlexa * brightness/255);

  Serial.print(", Red: ");Serial.print(redAlexa); 
  Serial.print(", Green: ");Serial.print(greenAlexa); 
  Serial.print(", Blue: ");Serial.println(blueAlexa); 

  redAlexaGlb = map(redAlexa, 0, 255, 0, 100);
  greenAlexaGlb = map(greenAlexa, 0, 255, 0, 100);
  blueAlexaGlb = map(blueAlexa, 0, 255, 0, 100);

  analogWrite(redPin,redAlexa);
  analogWrite(greenPin,greenAlexa);
  analogWrite(bluePin,blueAlexa);
  
}


void updateAlexaWithCurrentRgbState(int deviceIndex,int red, int green, int blue) {
  // Assuming you have only one device and its index is 0
  EspalexaDevice* dev = espalexa.getDevice(deviceIndex);
  if (!dev) return;
  // Convert RGB to the format Alexa expects and update the device state
  dev->setColor(red, green, blue);
}


void pushButtonStatusManage(){
    // Read the current state of the button
  int buttonState = digitalRead(buttonPin);// Check if the button is pressed
  if (buttonState == LOW) {// If the button was not pressed before, store the current time
    if (buttonPressStartTime == 0) {
      buttonPressStartTime = millis();
    }
  } 
  else { // If the button is released, check if it was pressed for more than 2 seconds
    if (buttonPressStartTime != 0) {
      unsigned long buttonPressDuration = millis() - buttonPressStartTime;
      if (buttonPressDuration >= longPressDuration) {
        for (int i = 0; i < 2; i++) {
          digitalWrite(ledPin, LOW); // Turn LED on (might be HIGH depending on your board)
          delay(200); // Wait 200 ms
          digitalWrite(ledPin, HIGH); // Turn LED off (might be LOW depending on your board)
          delay(200); // Wait 200 ms
        }
        // Perform the given process
        modeIdx = EEPROM.read(modeAddr);
        modeIdx = modeIdx == 0 ? 1 : 0;
        EEPROM.write(modeAddr, modeIdx);
        EEPROM.commit();
        ESP.restart();
      }
      else{
        if(!modeIdx){
          if(appleAlexaPairingEnableFlag){
            appleAlexaPairingEnableFlag=false;
          }
          else{
            appleAlexaPairingEnableFlag=true;
          }
        }
      }
      buttonPressStartTime = 0; // Reset the button press start time
    }
  }
}


void loop() {
  if(appleAlexaPairingEnableFlag && !appleAlexaPairingOnetimeDisable){
    espalexa.setDiscoverable(true);
    //apple also need to include here
    appleAlexaPairingOnetimeDisable=true;
    Serial.println("Apple Alexa pairing enabled");
    blinkInbuiltLed=true;
  }
  if(!appleAlexaPairingEnableFlag && appleAlexaPairingOnetimeDisable){
    espalexa.setDiscoverable(false);
    //apple also need to include here
    appleAlexaPairingOnetimeDisable=false;
    Serial.println("Apple Alexa pairing disabled");
    blinkInbuiltLed=false;
    digitalWrite(ledPin, LOW);
  }


  if (blinkInbuiltLed) {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= 200) {
      previousMillis = currentMillis; // Update the previousMillis
      ledState = !ledState; // Toggle the LED state
      digitalWrite(ledPin, ledState ? HIGH : LOW); // Set the LED with the new state
    }
  } 

  
  if(apFlag){
    server.handleClient();
  }

  if((WiFi.status() == WL_CONNECTED) && !client.connected()){
    mqttConnect();
  }

  pushButtonStatusManage();

  if (applePoll) {
    homeSpan.poll();
  }
  
  if (alexaPoll){
    espalexa.loop();
  }
  
  if(mqttFlag){
    client.loop();
  }


  //When mqtt values changed, 1)update apple values. 2)update alexa values 3)update bulb colour
  if(subscribeFlag){
    subscribeFlag=false;
    updateAppleRgb(redCustom,greenCustom,blueCustom);
    analogWrite(redPin,redCustom);
    analogWrite(greenPin,greenCustom);
    analogWrite(bluePin,blueCustom);
    updateAlexaWithCurrentRgbState(0,redCustom,greenCustom,blueCustom);
  }


  //when apple values changed(bulb values change automatically), 1)update mqtt values 2)update alexa values
  int* rgbValues = getRGBFromApple();
  int currRed=rgbValues[0];
  int currGreen=rgbValues[1];
  int currBlue=rgbValues[2];
  if((red!=currRed) || (green!=currGreen) || (blue!=currBlue)){
      red=currRed;
      green=currGreen;
      blue=currBlue;
      red=currRed;
      updateAlexaWithCurrentRgbState(0,red*2.5,green*2.5,blue*2.5);
      char* ObjRGB=convertToJson(red*2.5, green*2.5, blue*2.5);
      if(client.publish(generateMqttTopic(deviceID1,"setStatus"),ObjRGB,true)){
        publishFlag=true;
        Serial.print("Published: ");
        Serial.println(ObjRGB);
      }
  }

  //when alexa values changed(bulb values change automatically), 1)update apple values. 2)update mqtt values 
  if(alexaFlag){
    alexaFlag=false;
    updateAppleRgb(redAlexaGlb,greenAlexaGlb,blueAlexaGlb);
    char* ObjRGB=convertToJson(redAlexaGlb, greenAlexaGlb, blueAlexaGlb);
    if(client.publish(generateMqttTopic(deviceID1,"setStatus"),ObjRGB,true)){
      Serial.print("Published: ");
      Serial.println(ObjRGB);
    }
  }
}
