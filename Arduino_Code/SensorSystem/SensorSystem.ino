/****************************************************************************** 
Arduino code for a temperature, RH, and humidity sensor / controller.
To be operated over a serial connection.
Consists of:
1) a SHT35 MOD-1030 Humidity and Temperature Sensor
2) a Sensirion SGP30 Indoor Air Quality Sensor. 

It logs every second and sends the most recent value on request.
To operate, Send message of format '?\r'
The response will be 8 bytes containing the raw uint16 values for CO2 Temp RH AH
//(raw_T / 65535.00) * 175 - 45; (raw_RH / 65535.0) * 100.0; AH_Raw/2^8;
Temp, RH, CO2, AH
   
R.Harkes NKI
(c) 2018 GPLv3

Based on: Mod_1030 demo by Embedded Adventures
SGP30 demo by SparkFun Electronics
******************************************************************************/

// SHT35 MOD-1030 Humidity and Temperature Sensor Arduino test sketch
// Written originally by Embedded Adventures 
#include "SparkFun_SGP30_Arduino_Library.h" 
#include <Wire.h>
#include <SHT35.h>
#define debug false
#define RSTPIN  2
#define SERIALCOM Serial
SGP30 mySGP30; //create an object of the SGP30 class
char c;
String userInput;
long t1, t2;
short COtwo, RawTemp, RawRH, AbsHum; 
float Temp, RH;

void setup() {
  SERIALCOM.begin(115200);
  while(!SERIALCOM);
  Wire.begin();
  //Initialize sensor
  if (mySGP30.begin() == false) {
    SERIALCOM.println("No SGP30 Detected. Check connections.");
    while (1);
  }
  mySGP30.initAirQuality();
  mod1030.init(0, 0, RSTPIN);
  mod1030.hardReset();
  t1 = millis();
  COtwo = 0;
  RawTemp = 0;
  RawRH = 0;
  AbsHum = 0;
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
    } else if (userInput =="?"){ //write values as 8 bytes
        SERIALCOM.write(COtwo);SERIALCOM.write(COtwo>> 8);
        SERIALCOM.write(RawTemp);SERIALCOM.write(RawTemp>> 8);
        SERIALCOM.write(RawRH);SERIALCOM.write(RawRH>> 8);
        SERIALCOM.write(AbsHum);SERIALCOM.write(AbsHum>> 8);        
    } else {
      SERIALCOM.print("unknown command: ");
      SERIALCOM.print(userInput);
    }
    userInput="";
  }
  t2 = millis();
  if ( t2 >= t1 + 1000) //only will occur if 1 second has passed
  {
    t1 = t2;
    //Measure temperature and humidity
    mod1030.oneShotRead();
    Temp=mod1030.getCelsius();
    RH = mod1030.getHumidity();
    //Convert relative humidity to absolute humidity
    double absHumidity = RHtoAbsolute(RH, Temp);
    //Convert the double type humidity to a fixed point 8.8bit number
    AbsHum = doubleToFixedPoint(absHumidity);
    //Set the SGP30 to the correct humidity
    mySGP30.setHumidity(AbsHum);
    RawTemp = (((Temp+45)/175)*65535);
    RawRH = (RH/100)*65535;
    //measure CO2 and TVOC levels
    mySGP30.measureAirQuality();
    COtwo = mySGP30.CO2;
    if (debug){
      SERIALCOM.print("Temp =          ");SERIALCOM.print(Temp);SERIALCOM.print("\n");
      SERIALCOM.print("RH =            ");SERIALCOM.print(RH);SERIALCOM.print("\n");
      SERIALCOM.print("AbsHumidity =   ");SERIALCOM.print(absHumidity);SERIALCOM.print("\n");
      SERIALCOM.print("AbsHum =        ");SERIALCOM.print(AbsHum);SERIALCOM.print("\n");
      SERIALCOM.print("RawTemp =       ");SERIALCOM.print(RawTemp);SERIALCOM.print("\n");
      SERIALCOM.print("RawRH =         ");SERIALCOM.print(RawRH);SERIALCOM.print("\n");
      SERIALCOM.print("COtwo =         ");SERIALCOM.print(COtwo);SERIALCOM.print("\n");
    }
  }
}
//Identify the arduino
void Identify()
{
  SERIALCOM.println("SensorSystem");
  
}

void translateSREG() {
  SERIALCOM.print("\nSREG: ");
  SERIALCOM.println(mod1030.getSREG(), BIN);
  if (mod1030.alertPending())
    SERIALCOM.println("An alert is pending");
  if (mod1030.heaterStatus())
    SERIALCOM.println("Heater is ON");
  else
    SERIALCOM.println("Heater is OFF");
  if (mod1030.humidityTrackingAlert())
    SERIALCOM.println("RH alert");
  if (mod1030.temperatureTrackingAlert())
    SERIALCOM.println("Temperature alert");
  if (mod1030.resetDetected())
    SERIALCOM.println("A system reset occurred recently");
  if (mod1030.lastCommandStatus())
    SERIALCOM.println("Last command not processed");
  if (mod1030.lastWriteChecksumStatus())
    SERIALCOM.println("Last write transfer checksum failed");
}

double RHtoAbsolute (float relHumidity, float tempC) {
  double eSat = 6.11 * pow(10.0, (7.5 * tempC / (237.7 + tempC))); //millibars
  double vaporPressure = (relHumidity/100) * eSat ; 
  double absHumidity = 1000 * vaporPressure * 100 / ((tempC + 273) * 461.5); //Ideal gas law with unit conversions
  if (debug){
    SERIALCOM.print("eSat =          ");SERIALCOM.print(eSat);SERIALCOM.print("\n");
    SERIALCOM.print("vaporPressure = ");SERIALCOM.print(vaporPressure);SERIALCOM.print("\n");
    SERIALCOM.print("absHumidity =   ");SERIALCOM.print(absHumidity);SERIALCOM.print("\n");
  }
  return absHumidity;
}

uint16_t doubleToFixedPoint( double number) {
  int power = 1 << 8; //256
  double number2 = number * power;
  uint16_t value = floor(number2 + 0.5);
  return value;
}
