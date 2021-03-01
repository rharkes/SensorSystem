/****************************************************************************** 
CO2 sensor externally controlled. 
Goal is to find the PID parameters using the Åström–Hägglund method
Goal is to run it in Bang-Bang mode and have a Oscilating CO2 concentration that is 50/50 above and below the setpoint.
Bang-Bang mode is switching between two modes (min and max) whenever the value is above or below the setpoint.
We check externally if the CO2 is above or below setpoint and change the onTime accordingly.



   
R.Harkes NKI
(c) 2018 GPLv3

Based on: Mod_1030 demo by Embedded Adventures
******************************************************************************/

// SHT35 MOD-1030 Humidity and Temperature Sensor Arduino test sketch
// Written originally by Embedded Adventures 
#include <NDIR_I2C.h>
#define CO2PIN 4
#define SERIALCOM Serial

char c;
String userInput;
unsigned long windowStartTime, now, windowSize, onTime;
double fraction;
long RawCOtwo;

NDIR_I2C CO2Sensor(0x4D);
void setup() {
  windowSize=30000;
  onTime=1000;
  fraction = onTime/windowSize;
  pinMode(CO2PIN, OUTPUT);
  digitalWrite(CO2PIN,LOW);
  onTime = 1000; // start at lowest option
  SERIALCOM.begin(9600);
  while(!SERIALCOM);
  if (CO2Sensor.begin()) {
        delay(10000);
    } else {
        SERIALCOM.println("ERROR: Failed to connect to the sensor.");
        while(1);
    }
}

void loop() {
  while(SERIALCOM.available()){ //Read user input and store untill line-end
      c = SERIALCOM.read(); 
      userInput += c;
  }
  if ((c == '\n')||(c == '\r'))  //command interpreter
  {
    c = ' ';
    userInput.remove(userInput.length()-1,1);
    if (userInput =="*IDN?")
    {
      Identify();
    } else if (userInput =="?"){ //write value to serial port as 4 bytes
      if (CO2Sensor.measure()) {
        RawCOtwo = CO2Sensor.ppm; //uint32 with ppm of co2
      }
      SERIALCOM.write(RawCOtwo);SERIALCOM.write(RawCOtwo>> 8);SERIALCOM.write(RawCOtwo>> 16);SERIALCOM.write(RawCOtwo>> 24);
    } else if (userInput.length() == 3){ //setpoint given
      fraction = ((double) userInput.substring(0,3).toInt())/1000; //in tenths of percent
      onTime = fraction*windowSize;
      SERIALCOM.print(onTime);
    } else {
      SERIALCOM.print("unknown command: ");
      SERIALCOM.print(userInput);
    }
    userInput="";
  }
  /************************************************
     turn the output pin on/off based on outputVal
   ************************************************/
  now = millis();
  if (now - windowStartTime > windowSize)
  { //time to shift the Relay Window
    windowStartTime += windowSize;
  }
  if (onTime > now - windowStartTime) digitalWrite(CO2PIN, HIGH);
  else digitalWrite(CO2PIN, LOW);
  //END PID
}
//Identify the arduino
void Identify()
{
  SERIALCOM.println("SensorSystem BangBang");
  
}
