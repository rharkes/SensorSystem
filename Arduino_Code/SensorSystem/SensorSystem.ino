/****************************************************************************** 
Arduino code for a temperature, RH, and humidity sensor / controller.
To be operated over a serial connection.
Consists of:
1) a SHT35 MOD-1030 Humidity and Temperature Sensor
2) a SandboxElectronics/NDIR CO2 sensor
3) a Grove-LCD RGB backlight V4.0

The NDIR sends 8 bytes, 32 bits. The CO2 concentration is a uint32 in ppm.

It logs every second and sends the most recent value on request.
To operate, Send message of format '?\r'
The response will be 4+2+2+2 = 10 bytes containing the raw values for CO2 Temp RH AH
//(raw_T / 65535.00) * 175 - 45; (raw_RH / 65535.0) * 100.0; AH_Raw/2^8;
CO2, Temp, RH, AH

The software uses a PID controller and a custom valve control
For PID see wikipedia
For valve control:
The valve controller gets a value between 0 and 1 from the PID. What to do with that value?
The valve can cycle infinitely fast, this is also bad for the valve.
It is set to intervals of 30 seconds.
It is on for the fraction of the time. (0.2 = 2 seconds on)
   
R.Harkes NKI
(c) 2018 GPLv3

******************************************************************************/

#include <Wire.h>
#include "rgb_lcd.h"
#include <SHT35.h>
#include <NDIR_I2C.h>
#include <PID_v1.h>
#define CO2PIN 4
#define INTPIN 3
#define RSTPIN 2
#define SERIALCOM Serial

rgb_lcd lcd;
short outputMode;
// 0 is silent
// 1 is debug
// 2 is reporting
byte displayMode;
char c;
String userInput;
long t1;
boolean hasTurnedOn;
//PID 
bool pidRunning = false;
double COtwo, COtwoSetPoint, CO2onFraction;
double kp,ki,kd; 
int windowSize = 30000; //of CO2 valve
unsigned long windowStartTime,CO2onTime;
//raw data
long rawCOtwo;
short CO2onTimeShort, rawTemperature, rawRelativeHumidity, absoluteHumidity; 
//output data
float temperature, relativeHumidity, absoluteHumidityFloat;
//init co2 sensor
NDIR_I2C CO2Sensor(0x4D);
PID myPID(&COtwo, &CO2onFraction, &COtwoSetPoint, 1,1,1,P_ON_E,DIRECT);

void setup() {
  lcd.begin(16, 2);
  lcd.setRGB(255, 0, 255);
  lcd.setCursor(0, 0);
  lcd.print("SETUP");
  lcd.setCursor(0, 1);
  lcd.print("Please wait");
  pinMode(CO2PIN, OUTPUT);
  digitalWrite(CO2PIN,LOW);
  myPID.SetOutputLimits(300/((double) windowSize), 1); // minimal on time is 0.3 second, maximum is always on.
  myPID.SetSampleTime(1000); //once every second should be enough. don't put too low because the differential error will become noisy
  //PID values from Åström–Hägglund tuning in BangBang mode. 
  //Peak-Peak CO2 concentration was 0.46% when switching between 0 and 4.5 seconds on time (30 seconds window, so 0% and 15%). 
  //The period was 193 seconds.
  // Tu = 193s ; 
  // Ku = (4*0.15) / (pi*0.0046) = 41.5187
  kp = 25.0; //0.6*Ku
  ki = 0.260; //1.2*Ku/Tu
  kd = 603; //3*Ku*Tu/40
  myPID.SetTunings(kp, ki, kd);
  // init serial
  SERIALCOM.begin(9600);
  while(!SERIALCOM);
  Wire.begin();
  // init temp and humidity sensor
  mod1030.init(0, 0, RSTPIN);
  mod1030.hardReset();
  t1 = millis();
  windowStartTime = millis();
  COtwo = 0;
  rawTemperature = 0;
  rawRelativeHumidity = 0;
  absoluteHumidity = 0;
  COtwoSetPoint = 0.05; //co2 fraction
  outputMode = 0;
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
    if (userInput =="*IDN?")
    {
      Identify();
    } else if (userInput =="?"){ //write values as 12 bytes
        SERIALCOM.write(rawCOtwo);SERIALCOM.write(rawCOtwo>> 8);SERIALCOM.write(rawCOtwo>> 16);SERIALCOM.write(rawCOtwo>> 24);
        SERIALCOM.write(rawTemperature);SERIALCOM.write(rawTemperature>> 8);
        SERIALCOM.write(rawRelativeHumidity);SERIALCOM.write(rawRelativeHumidity>> 8);
        SERIALCOM.write(absoluteHumidity);SERIALCOM.write(absoluteHumidity>> 8);
        CO2onTimeShort = (short)(CO2onFraction*65535);
        SERIALCOM.write(CO2onTimeShort);SERIALCOM.write(CO2onTimeShort>> 8);
    } else if (userInput == "start"){
      myPID.SetMode(AUTOMATIC);
      pidRunning = true;
    } else if (userInput == "stop"){
      myPID.SetMode(MANUAL);
      pidRunning = false;
      CO2onFraction = 0;
    } else if (userInput =="outputMode on") {
      outputMode = 1;
    } else if (userInput =="outputMode off") {
      outputMode = 0;
    } else if (userInput.length() == 5){ //setpoint given (50000 = 5%)
      COtwoSetPoint = ((double) userInput.toInt())/1000000;
      SERIALCOM.println(COtwoSetPoint);
    } else {
      SERIALCOM.print("unknown command: ");
      SERIALCOM.print(userInput);
    }
    userInput="";
  }
  /************************************************
     Measure CO2 level and compute on-fraction with PID
   ************************************************/
  if (CO2Sensor.measure()) {
     rawCOtwo = CO2Sensor.ppm;
     COtwo = ((double)rawCOtwo) / 1000000; //fraction CO2 in air
  } else {
      SERIALCOM.println("Sensor communication error.");
  }
  myPID.Compute();
  /************************************************
     turn the output pin on/off based on pid output
   ************************************************/
  unsigned long now = millis();
  if (now - windowStartTime > windowSize)
  { //time to shift the Relay Window
    windowStartTime += windowSize;
    hasTurnedOn = false;
  }
  CO2onTime = ((double) CO2onFraction) * ((double) windowSize);
  if ((CO2onTime > now - windowStartTime)&&!hasTurnedOn) {
    digitalWrite(CO2PIN, HIGH);
  } else {
    digitalWrite(CO2PIN, LOW);
    hasTurnedOn=true; //prevent quick switching of the relais when the PID is increasing the on-time
  }
  // END PID
  
  if ( millis() >= t1 + 1000) //only will occur if 1 second has passed
  {
    t1 = millis();
    //Measure temperature and humidity
    mod1030.oneShotRead();
    temperature = mod1030.getCelsius();
    relativeHumidity = mod1030.getHumidity();
    //Convert relative humidity to absolute humidity
    absoluteHumidityFloat = RHtoAbsolute(relativeHumidity, temperature);
    //Convert the double type humidity to a fixed point 8.8bit number
    absoluteHumidity = doubleToFixedPoint(absoluteHumidityFloat);
    //Calculate RawRH and RawTemp (16 bit)
    rawTemperature = (((temperature+45)/175)*65535);
    rawRelativeHumidity = (relativeHumidity/100)*65535;
    //Set the display
    if (temperature>36 & temperature<37.1 & abs(COtwo-COtwoSetPoint)<0.001){ // CO2 between 4.9% and 5.1%
      lcd.setRGB(0, 255, 0);
    } else {
      lcd.setRGB(255, 0, 0);
    }
    lcd.clear();
    displayMode = displayMode + 1;
    if (displayMode<5){ //display temperature and humidity
      lcd.setCursor(0, 0);
      lcd.print("T = " + String(temperature) + (char)223 +"C");
      lcd.setCursor(0, 1);
      lcd.print("RH= "+String(relativeHumidity)+"%");
    } else { //display CO2 and humidity
      if (pidRunning) {
        lcd.setCursor(0, 0);
        lcd.print("CO2 = " + String(COtwo*100) + "%");
        lcd.setCursor(0, 1);
        lcd.print("Valve open "+String(int(CO2onFraction*100)) + "%");
      } else {
        lcd.setCursor(0, 0);
        lcd.print("CO2 = " + String(COtwo*100) + "%");
        lcd.setCursor(0, 1);
        lcd.print("PID is off");
      }
    }
    if (displayMode==10){displayMode=0;}
    
    if (outputMode==1){
      SERIALCOM.print("Temp =             ");SERIALCOM.print(temperature);SERIALCOM.print("\n");
      SERIALCOM.print("RH =               ");SERIALCOM.print(relativeHumidity);SERIALCOM.print("\n");
      SERIALCOM.print("AbsHumidityFloat = ");SERIALCOM.print(absoluteHumidityFloat);SERIALCOM.print("\n");
      SERIALCOM.print("AbsHumidity8.8 =   ");SERIALCOM.print(absoluteHumidity);SERIALCOM.print("\n");
      SERIALCOM.print("RawTemp =          ");SERIALCOM.print(rawTemperature);SERIALCOM.print("\n");
      SERIALCOM.print("RawRH =            ");SERIALCOM.print(rawRelativeHumidity);SERIALCOM.print("\n");
      SERIALCOM.print("RawCOtwo =         ");SERIALCOM.print(rawCOtwo);SERIALCOM.print("\n");
      SERIALCOM.print("COtwo =            ");SERIALCOM.print(COtwo,4);SERIALCOM.print("\n");
      SERIALCOM.print("COtwo setp =       ");SERIALCOM.print(COtwoSetPoint,4);SERIALCOM.print("\n");
      SERIALCOM.print("PID output =       ");SERIALCOM.print(CO2onFraction,4);SERIALCOM.print("\n");
      SERIALCOM.print("kp = ");SERIALCOM.print(myPID.GetKp());
      SERIALCOM.print(" ; ki = ");SERIALCOM.print(myPID.GetKi());
      SERIALCOM.print(" ; kd = ");SERIALCOM.print(myPID.GetKd());
      SERIALCOM.print(" ; mode = ");SERIALCOM.print(myPID.GetMode());
      SERIALCOM.print(" ; direction = ");SERIALCOM.print(myPID.GetDirection());SERIALCOM.print("\n");
      SERIALCOM.print("----------------\n");
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
  if (outputMode==1){
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
