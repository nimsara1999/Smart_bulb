#include "EEPROM.h"
#include "WiFi.h"
#include "HomeSpan.h"
#include "DEV_LED.h"
#include <string>
#include <cstring>
#include <ESP32Firebase.h>
#define EEPROM_SIZE 64

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

#define REFERENCE_URL "https://mqtt-rgb-led-flutter-app-default-rtdb.firebaseio.com/"  //Firebase project reference url
Firebase firebase(REFERENCE_URL);

WiFiClient espClient;
String userId="u2445";
String deviceId="d1456";

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;
bool applePoll = false;

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
int* rgbValues;
int redFromApple; 
int greenFromApple;
int blueFromApple;
int redFromFirebase;
int greenFromFirebase; 
int blueFromFirebase;

int modeIdx;
bool buttonState;
bool lastButtonState;

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
  homeSpan.begin(Category::Lighting, "Nimsara LED");
  homeSpan.setPairingCode("11112222");
  applePoll = true;

    new SpanAccessory();                                                          
    new Service::AccessoryInformation();    
      new Characteristic::Identify();               
      new Characteristic::Name("RGB LED"); 
    new DEV_RgbLED(redPin,greenPin,bluePin);  
  
}

void bleTask() {
  Serial.println("Pairing Mode");
  digitalWrite(ledPin, HIGH);
  BLEDevice::init("ESP32 Nimsara");
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
        delay(100);
      }
    }
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

void loop() {
  //Manage pairing button------------------------------
  buttonState = false;
  lastButtonState = false;
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

  //Update RGB from custom app side-> set bulb color & update Apple-------------------
  redFromFirebase = firebase.getInt("devices/device1/color/red");
  greenFromFirebase = firebase.getInt("devices/device1/color/green");
  blueFromFirebase = firebase.getInt("devices/device1/color/blue");

  if((redFromFirebase!=red) || (greenFromFirebase!=green) || (blueFromFirebase!=blue)){
    updateRgbPins(redFromFirebase,greenFromFirebase,blueFromFirebase);
    analogWrite(redPin,redFromFirebase*2.55);
    analogWrite(greenPin,greenFromFirebase*2.55);
    analogWrite(bluePin,blueFromFirebase*2.55);
    red=redFromFirebase;
    green=greenFromFirebase;
    blue=blueFromFirebase;
  }

  //Update RGB from apple app side-> update firebase values----------------
  rgbValues = getRGB();
  redFromApple=rgbValues[0];
  greenFromApple=rgbValues[1];
  blueFromApple=rgbValues[2];

  if((red!=redFromApple) || (green!=greenFromApple) || (blue!=blueFromApple)){
      red=redFromApple;
      green=greenFromApple;
      blue=blueFromApple;
      firebase.pushInt("devices/device1/color/red", red);
      firebase.pushInt("devices/device1/color/green", green);
      firebase.pushInt("devices/device1/color/blue", blue);
  }
}
