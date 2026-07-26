**MAVLink/INAV Versions**

Refer to how to flash for Arduino IDE configuration:

https://github.com/mshagg/LCD-Cockpit-Displays/blob/main/How%20to%20flash.md

**Hardware requirements**

Note: wiring diagrams in each version's subfolder

_Dual 1.69":_

https://www.waveshare.com/wiki/ESP32-S3-LCD-1.69 (non-touch)

https://www.waveshare.com/wiki/1.69inch_LCD_Module

Note: 3D models of the displays are available on the wiki, which makes it super easy to design your cockpit in Fusion360.

**INAV configuration**

The flight controller needs to send MAVLink telemetry to the ESP32.  In order to do so you will require one free UART.  The ESP32 will connect to the Tx pin of this UART.

It is important that the FC Baud rate matches what the code is expecting.  IF your choose a different baud rate in INAV, you will need to make the corresponding change in mavlink_telemetry.ino.

The following screenshot shows MAVLink telemetry correctly set up on UART8:

<img width="1499" height="301" alt="Screenshot 2026-07-25 212807" src="https://github.com/user-attachments/assets/43994404-f2cc-492b-9541-16b6ecfb983a" />

The standard MAVLink telemetry settings in INAV may not send required data fields frequently enough for smooth drawing of the display.  Enter the following into CLI:

set mavlink_extra1_rate = 30

set mavlink_rc_chan_rate = 30

set mavlink_ext_status_rate = 2

set mavlink_pos_rate = 5

set mavlink_extra2_rate = 5

save

**Customisation**

The code is highly configurable through the config.ino tab.  This includes:

- Defining RC channels for screen selection
- Including/excluding/ordering specific screens
- Setting default pages
- Defining RC channels for gear and flaps
- Stall/never exceed speeds for airspeed gauges

 MORE DETAILED DOCUMENTATION FOR CUSTOMISATION ON THE WAY
