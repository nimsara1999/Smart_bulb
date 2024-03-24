#include "Arduino.h"

////////////////////////////////////
//   DEVICE-SPECIFIC LED SERVICES //
////////////////////////////////////

#include "extras/PwmPin.h"                          // library of various PWM functions

////////////////////////////////////
int R, G, B;
float setR,setG,setB;
boolean test;

struct DEV_RgbLED : Service::LightBulb {       // RGB LED (Command Cathode)

  LedPin *redPin, *greenPin, *bluePin;
  
  SpanCharacteristic *power;                   // reference to the On Characteristic
  SpanCharacteristic *H;                       // reference to the Hue Characteristic
  SpanCharacteristic *S;                       // reference to the Saturation Characteristic
  SpanCharacteristic *V;                       // reference to the Brightness Characteristic
  
  DEV_RgbLED(int red_pin, int green_pin, int blue_pin) : Service::LightBulb(){       // constructor() method

    power=new Characteristic::On();                    
    H=new Characteristic::Hue(0);              // instantiate the Hue Characteristic with an initial value of 0 out of 360
    S=new Characteristic::Saturation(0);       // instantiate the Saturation Characteristic with an initial value of 0%
    V=new Characteristic::Brightness(100);     // instantiate the Brightness Characteristic with an initial value of 100%
    V->setRange(5,100,1);                      // sets the range of the Brightness to be from a min of 5%, to a max of 100%, in steps of 1%
    
    this->redPin=new LedPin(red_pin);        // configures a PWM LED for output to the RED pin
    this->greenPin=new LedPin(green_pin);    // configures a PWM LED for output to the GREEN pin
    this->bluePin=new LedPin(blue_pin);      // configures a PWM LED for output to the BLUE pin
 
    char cBuf[128];
    sprintf(cBuf,"Configuring RGB LED: Pins=(%d,%d,%d)\n",redPin->getPin(),greenPin->getPin(),bluePin->getPin());
    Serial.print(cBuf);
    
  } // end constructor

  boolean update(){                         // update() method

    boolean p;
    float v, h, s, r, g, b;

    h=H->getVal<float>();                      // get and store all current values.  Note the use of the <float> template to properly read the values
    s=S->getVal<float>();
    v=V->getVal<float>();                      // though H and S are defined as FLOAT in HAP, V (which is brightness) is defined as INT, but will be re-cast appropriately
    p=power->getVal();

    //Serial.print("VHS from apple: ");Serial.print(v);Serial.print(",");Serial.print(h);Serial.print(",");Serial.println(s);

    char cBuf[128];
    sprintf(cBuf,"Updating RGB LED: Pins=(%d,%d,%d): ",redPin->getPin(),greenPin->getPin(),bluePin->getPin());
    LOG1(cBuf);

    if(power->updated()){
      p=power->getNewVal();
      sprintf(cBuf,"Power=%s->%s, ",power->getVal()?"true":"false",p?"true":"false");
    } else {
      sprintf(cBuf,"Power=%s, ",p?"true":"false");
    }
    LOG1(cBuf);
      
    if(H->updated()){
      h=H->getNewVal<float>();
      sprintf(cBuf,"H=%.0f->%.0f, ",H->getVal<float>(),h);
    } else {
      sprintf(cBuf,"H=%.0f, ",h);
    }
    LOG1(cBuf);

    if(S->updated()){
      s=S->getNewVal<float>();
      sprintf(cBuf,"S=%.0f->%.0f, ",S->getVal<float>(),s);
    } else {
      sprintf(cBuf,"S=%.0f, ",s);
    }
    LOG1(cBuf);

    if(V->updated()){
      v=V->getNewVal<float>();
      sprintf(cBuf,"V=%.0f->%.0f  ",V->getVal<float>(),v);
    } else {
      sprintf(cBuf,"V=%.0f  ",v);
    }
    LOG1(cBuf);

    // Here we call a static function of LedPin that converts HSV to RGB.
    // Parameters must all be floats in range of H[0,360], S[0,1], and V[0,1]
    // R, G, B, returned [0,1] range as well

    LedPin::HSVtoRGB(h,s/100.0,v/100.0,&r,&g,&b);   // since HomeKit provides S and V in percent, scale down by 100

    R=p*r*100;                                      // since LedPin uses percent, scale back up by 100, and multiple by status fo power (either 0 or 1)
    G=p*g*100;
    B=p*b*100;

    sprintf(cBuf,"RGB=(%d,%d,%d)\n",R,G,B);
    LOG1(cBuf);
    redPin->set(R);                      // update each ledPin with new values
    greenPin->set(G);    
    bluePin->set(B);    
      
    return(true);                               // return true
  
  } // update

  void loop() override {
    if(test){
      float h,v,s;
      float minVal = min(min(setR, setG), setB);
      float maxVal = max(max(setR, setG), setB);
      float delta = maxVal - minVal;
      // Calculate the Hue
      if (delta == 0) {
          h = 0; // undefined, but we'll set it to 0
      } else if (maxVal == setR) {
          h = 60 * (static_cast<int>((setG - setB) / delta) % 6);
      } else if (maxVal == setG) {
          h = 60 * ((setB - setR) / delta + 2);
      } else {
          h = 60 * ((setR - setG) / delta + 4);
      }

      // Make sure hue is in the range [0, 360]
      if (h < 0) h += 360;

      // Calculate the Saturation
      if (maxVal == 0) {
          s = 0;
      } else {
          s = delta / maxVal;
      }

      // Calculate the Value
      v = maxVal;
      s=s*100;
      test=false;

      //Serial.print("RGB: ");Serial.print(setR);Serial.print(",");Serial.print(setG);Serial.print(",");Serial.println(setB);
      //Serial.print("VHS: ");Serial.print(v);Serial.print(",");Serial.print(h);Serial.print(",");Serial.println(s);
      power->setVal(true);
      V->setVal(v);
      H->setVal(h);
      S->setVal(s);
    } 
  }
};
      
//////////////////////////////////

int* getRGB() {
  static int rgb[3]; // Static array to hold RGB values
  // Assign RGB values to the array
  rgb[0] = R;
  rgb[1] = G;
  rgb[2] = B;
  return rgb; // Return the array
}

void updateAppleRgb(int R, int G, int B) {
  test=true;
  setR=R;
  setG=G;
  setB=B;
}
