/****************************************************************************** 
Arduino code for a temperature, RH, and humidity sensor / controller.
To be operated over a serial connection.
Consists of:
1) a SHT35 MOD-1030 Humidity and Temperature Sensor
2) a SandboxElectronics/NDIR

The NDIR sends 8 bytes. The CO2 concentration is a uint32 in ppm.


It logs every second and sends the most recent value on request.
To operate, Send message of format '?\r'
The response will be 4+2+2+2 = 10 bytes containing the raw values for CO2 Temp RH AH
//(raw_T / 65535.00) * 175 - 45; (raw_RH / 65535.0) * 100.0; AH_Raw/2^8;
CO2, Temp, RH, AH
   
R.Harkes NKI
(c) 2018 GPLv3

Based on: Mod_1030 demo by Embedded Adventures
******************************************************************************/

// SHT35 MOD-1030 Humidity and Temperature Sensor Arduino test sketch
// Written originally by Embedded Adventures 
#include <Wire.h>
#include <SHT35.h>
#include <NDIR_I2C.h>
#define debug false
#define CO2PIN 4
#define INTPIN 3
#define RSTPIN 2
#define SERIALCOM Serial
char c;
String userInput;
long t1, t2, COtwo, CO2setp;
short RawTemp, RawRH, AbsHum; 
float Temp, RH;
NDIR_I2C CO2Sensor(0x4D);

void setup() {
  pinMode(CO2PIN, OUTPUT);
  digitalWrite(CO2PIN,LOW);
  SERIALCOM.begin(115200);
  while(!SERIALCOM);
  Wire.begin();
  mod1030.init(0, 0, RSTPIN);
  mod1030.hardReset();
  t1 = millis();
  COtwo = 0;
  RawTemp = 0;
  RawRH = 0;
  AbsHum = 0;
  CO2setp = 50000; //50.000 ppm = 5%
  
  if (CO2Sensor.begin()) {
        delay(10000);
    } else {
        Serial.println("ERROR: Failed to connect to the sensor.");
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
    } else if (userInput =="?"){ //write values as 8 bytes
        SERIALCOM.write(COtwo);SERIALCOM.write(COtwo>> 8);SERIALCOM.write(COtwo>> 16);SERIALCOM.write(COtwo>> 24);
        SERIALCOM.write(RawTemp);SERIALCOM.write(RawTemp>> 8);
        SERIALCOM.write(RawRH);SERIALCOM.write(RawRH>> 8);
        SERIALCOM.write(AbsHum);SERIALCOM.write(AbsHum>> 8);        
    } else if (userInput.length() == 5){ //setpoint given
      CO2setp = userInput.toInt();
      SERIALCOM.println(CO2setp);
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
    RawTemp = (((Temp+45)/175)*65535);
    RawRH = (RH/100)*65535;
    //measure CO2 level
    if (CO2Sensor.measure()) {
        COtwo = CO2Sensor.ppm;
    } else {
        Serial.println("Sensor communication error.");
    }
    if (COtwo<CO2setp){
      digitalWrite(CO2PIN,HIGH); 
    } else{
      digitalWrite(CO2PIN,LOW); 
    }
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
