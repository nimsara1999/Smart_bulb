#include "EEPROM.h"
#include "WiFi.h"
#include <PubSubClient.h>
#include "HomeSpan.h"
#include "DEV_LED.h"
#include <string>
#include <cstring>
#define EEPROM_SIZE 64
#define ESPALEXA_ASYNC
#include <Espalexa.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ArduinoJson.h>
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

const char *mqtt_broker = "test.mosquitto.org";
const char *mqtt_username = "emqx";
const char *mqtt_password = "public";
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);
Espalexa espalexa;
bool mqttFlag=false;
String userId="u2445";
String deviceId="d1456";
String deviceName="Nimsara LED";

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;
bool applePoll = false;
bool alexaPoll = false;
bool subscribeFlag=false;
bool publishFlag=false;
bool alexaFlag=false;

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

void wifiTask();
String generateMqttTopic();

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    BLEDevice::startAdvertising();
  };

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
  }
};

class MyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) {
    std::string value = pCharacteristic->getValue();

    if (value.length() > 0) {
      Serial.print("Value : ");
      Serial.println(value.c_str());
      writeString(wifiAddr, value.c_str());
      wifiTask();
    }
  }

  void writeString(int add, String data) {
    int _size = data.length();
    for (int i = 0; i < _size; i++) {
      EEPROM.write(add + i, data[i]);
    }
    EEPROM.write(add + _size, '\0');
    EEPROM.commit();
  }
};


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
    bleTask();
  } else {
    digitalWrite(ledPin, LOW);
    wifiTask();
  }
}

void connectToApple() {
  delay(100);
  homeSpan.begin(Category::Lighting, deviceName);
  homeSpan.setPairingCode("11112222");
  applePoll = true;
    new SpanAccessory();                                                          
    new Service::AccessoryInformation();    
      new Characteristic::Identify();               
      new Characteristic::Name(deviceName); 
    new DEV_RgbLED(redPin,greenPin,bluePin);
}

void connectToAlexa(){
  espalexa.addDevice(deviceName, colorLightChanged);
  espalexa.begin();
  alexaPoll = true;
}

void bleTask() {
  Serial.println("Pairing Mode");
  digitalWrite(ledPin, HIGH);
  BLEDevice::init(deviceName);
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService* pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_INDICATE);
  pCharacteristic->setCallbacks(new MyCallbacks());
  pCharacteristic->addDescriptor(new BLE2902());
  pService->start();
  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);
  BLEDevice::startAdvertising();
  Serial.println("Waiting a client connection to notify...");
}

void wifiTask() {
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
  client.setCallback(callback);
  while (!client.connected()) {
    String client_id = "esp32-client-";
    client_id += String(WiFi.macAddress());
    Serial.printf("The client %s connects to the public mqtt broker\n", client_id.c_str());
    if (client.connect("smart-bulb")) {
        Serial.println("MQTT broker connected");
        mqttFlag=true;
        client.subscribe(generateMqttTopic(userId,deviceId,"setStatus"),1);
    } else {
      Serial.print("failed with state ");
      Serial.print(client.state());
      delay(1000);
     }
  }
}

void callback(char *topic, byte *payload, unsigned int length) {
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

const char* generateMqttTopic(String userId, String deviceId, String type) {
    String firstPart = "device/" + deviceId + "/user/" + userId;
    String topic;

    if (type == "setStatus") {
        topic = firstPart + "/setStatus";
    } else if (type == "getStatus") {
        topic = firstPart + "/getStatus";
    } else {
        return ""; 
    }
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

void loop() {
  if((WiFi.status() == WL_CONNECTED) && !client.connected()){
    mqttConnect();
  }
  static bool buttonState = false;
  static bool lastButtonState = false;
  // Read the current state of the button
  buttonState = digitalRead(buttonPin);
  // Check for button press
  if (buttonState != lastButtonState && buttonState == LOW) {
    modeIdx = modeIdx == 0 ? 1 : 0;
    EEPROM.write(modeAddr, modeIdx);
    EEPROM.commit();
    ESP.restart();
  }
  lastButtonState = buttonState;


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
    analogWrite(redPin,redCustom*2.55);
    analogWrite(greenPin,greenCustom*2.55);
    analogWrite(bluePin,blueCustom*2.55);
  }


  //when apple values changed(bulb values change automatically), 1)update mqtt values 2)update alexa values
  int* rgbValues = getRGB();
  int currRed=rgbValues[0];
  int currGreen=rgbValues[1];
  int currBlue=rgbValues[2];
  if((red!=currRed) || (green!=currGreen) || (blue!=currBlue)){
      red=currRed;
      green=currGreen;
      blue=currBlue;
      char* ObjRGB=convertToJson(red, green, blue);
      if(client.publish(generateMqttTopic(userId,deviceId,"setStatus"),ObjRGB),true){
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
    if(client.publish(generateMqttTopic(userId,deviceId,"setStatus"),ObjRGB),true){
      Serial.print("Published: ");
      Serial.println(ObjRGB);
    }
  }
}
