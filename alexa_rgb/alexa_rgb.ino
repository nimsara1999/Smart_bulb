#define ESPALEXA_ASYNC
#include <Espalexa.h>
#include <WiFi.h>

// define the GPIO connected with Relays and switches
#define redPin 4  //D23
#define greenPin 18  //D23
#define bluePin 19  //D23

// prototypes
boolean connectWifi();

//callback function prototype
void colorLightChanged(uint8_t brightness, uint32_t rgb);

// Change this!!
const char* ssid = "SSID";
const char* password = "PASSWORD";

boolean wifiConnected = false;

Espalexa espalexa;

void setup(){
  Serial.begin(115200);

  pinMode(redPin,OUTPUT);
  pinMode(greenPin,OUTPUT);
  pinMode(bluePin,OUTPUT);
  // Initialise wifi connection
  wifiConnected = connectWifi();
  
  if(wifiConnected){
    espalexa.addDevice("Color Light", colorLightChanged);

    espalexa.begin();
    
  } else
  {
    while (1) {
      Serial.println("Cannot connect to WiFi. Please check data and reset the ESP.");
      delay(2500);
    }
  }
}
 
void loop()
{
   espalexa.loop();
   delay(1);
}

//the color device callback function has two parameters
void colorLightChanged(uint8_t brightness, uint32_t rgb) {
  //do what you need to do here, for example control RGB LED strip
  uint8_t red = (rgb >> 16) & 0xFF;
  uint8_t green = (rgb >>  8) & 0xFF;
  uint8_t blue = rgb & 0xFF;

  red = (red * brightness) / 255;
  green = (green * brightness) / 255;
  blue = (blue * brightness) / 255;

  Serial.print(", Red: ");
  Serial.print(red); //get red component
  Serial.print(", Green: ");
  Serial.print(green); //get green
  Serial.print(", Blue: ");
  Serial.println(blue); //get blue

  analogWrite(redPin,red);
  analogWrite(greenPin,green);
  analogWrite(bluePin,blue);
}

// connect to wifi – returns true if successful or false if not
boolean connectWifi(){
  boolean state = true;
  int i = 0;
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");
  Serial.println("Connecting to WiFi");

  // Wait for connection
  Serial.print("Connecting...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (i > 40){
      state = false; break;
    }
    i++;
  }
  Serial.println("");
  if (state){
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }
  else {
    Serial.println("Connection failed.");
  }
  return state;
}
