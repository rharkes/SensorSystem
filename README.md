# SensorSystem
A SH35 sensor in a MOD-1030 packadge from Embedded Adventures to measure temperature and relative humidity.
A Sensirion SGP30 Indoor Air Quality Sensor to measure CO2. 

The Arduino code corrects the SGP30 for the absolute humidity read from the SH35. It also sends the current values over a serial connection upon request.

The matlab code is just a wrapper to read the arduino and plot graphs.