# SensorSystem
A SH35 sensor in a MOD-1030 packadge from Embedded Adventures to measure temperature and relative humidity.
A SandboxElectronics/NDIR Sensor to measure CO2. 

The Arduino code reads out the sensors and sends the current values over a serial connection upon request or continuous. It also controls a NORGREN 83J solenoid valve coil to open or close the CO2 inlet.

The matlab code is just a wrapper to read the arduino and plot graphs.
<p align="center">
<img src="Images\example.png">
</p>
