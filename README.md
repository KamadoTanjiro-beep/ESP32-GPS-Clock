# ESP32 based GPS Clock cum Weather Station in Arduino
## Hardware
1. DOIT ESP32 devkit v1
2. BH1750 (Light Sensor)
3. ~AHT25 (Temperature and Humidity)~ _DO NOT USE THIS SENSOR, I AM GOING TO CHANGE IT TO BME 280 or BMP 280 or TMP117
   _HUGE ACCURACY ISSUE, MAYBE DUE TO LIBRARY OR HARDWARE, CAN'T BE SURE_
4. GPS Neo 6m 
5. ST7920 128X64 LCD Display
6. Buzzer 
### Optional
7. LiFePO4 AAA 80mAh battery (for GPS memory)
8. TP5000 Charging circuit
9. BMS and diode (IN4007)
10. Wires and other stuff (like Prototyping board, connectors, switch etc.. as needed)

## Upcoming changes
Check [issues](https://github.com/KamadoTanjiro-beep/ESP32-GPS-CLOCK-V1/issues)

## Schematics
<img src="https://github.com/KamadoTanjiro-beep/ESP32-GPS-CLOCK-V1/blob/main/Schematic/Schematic_GPS%20Clock-V1.png" alt="schematics_gps_clock_chikne97" width="800" height="600"> <br/>
GPS Clock ESP32 <br/><br/>
