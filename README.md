# APS
Automatic Phase Selector using ESP32 and PZEM004T / Direct measurement
The automatic phase selector is a device that automatically control the relay and couples a single-phase load to it. 
This is useful in areas where the power grid is unstable and the phases can often be unbalanced. 

The automatic phase selector will ensure that your load is always connected to the most stable phase, which will help to prevent power outages and damage to your equipment.
In this project, I will use an ESP32 microcontroller and a PZEM004T sensor to create an automatic phase selector. 
The ESP32 will be responsible for collecting data from the PZEM004T sensor, which measures the voltage of each phase. 

The ESP32 will then use this data to calculate the phase priority and operate the relays with ondelay timer.
Optionally, we can use a WiFi/LAN and Bluetooth interface.
Optionally, we can change setting of Over voltage, Under voltage and Ondelay time.
