/****************************************************************************** 
Arduino code for reset of the MH-Z16 SandboxElectronics/NDIR CO2 sensor
   
R.Harkes NKI
(c) 2018 GPLv3

******************************************************************************/

#define SERIALCOM Serial
#include <NDIR_I2C.h>

char c;
String userInput;
long t1;
long rawCOtwo;
NDIR_I2C CO2Sensor(0x4D);

void setup() {
  t1 = millis();
  SERIALCOM.begin(9600);
  while(!SERIALCOM);
  SERIALCOM.println("Welcome");
  NDIR_I2C CO2Sensor(0x4D);
  if (CO2Sensor.begin()) {
      delay(10000);
      SERIALCOM.println("READY");
  } else {
      SERIALCOM.println("ERROR: Failed to connect to the sensor.");
      while(1);
  }
  CO2Sensor.disableAutoCalibration();
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
    if (userInput =="reset")
    {
      CO2Sensor.reset();
      SERIALCOM.println("Reset");
    } else if (userInput =="calibrateZero"){ 
      CO2Sensor.calibrateZero();
      SERIALCOM.println("Cal Zero");
    } else {
      SERIALCOM.print("unknown command: ");
      SERIALCOM.print(userInput);
    }
    userInput="";
  }
  if ( millis() >= t1 + 1000) //only will occur if 1 second has passed
  {
    t1 = millis();
    if (CO2Sensor.measure()) {
     rawCOtwo = CO2Sensor.ppm;
    } else {
        SERIALCOM.println("Sensor communication error.");
    }
    SERIALCOM.println(rawCOtwo);
  }
}
