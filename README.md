<img width="1448" height="1086" alt="e109e23b-ccc2-4565-a07a-57565fcec990" src="https://github.com/user-attachments/assets/35d36f6b-4ee3-4a3c-9dcc-2b905430217d" />

# LCD-Cockpit-Displays

This project uses inexpensive ESP32 microprocessors and small LCD displays to render real time information for use in FPV cockpits.

Currently the main processor being used is the Waveshare ESP32 S3 with 1.69 LCD attached.  Standalone ESP32 versions will be developed for use with a variety of displays.

Standalone versions provide basic artificial horizon (AHI) functionality.  MAVLink versions connect to an appropriately configured UART on your flight controller to derive real time telemetry data.

If you are new to Arduino, check the "how to flash" file above.

**Features**

- Selection of graphical AHIs, gauges etc.  Screens can be changed with physical buttons or RC channels.
- Selection of data pages (nav, battery health etc).  Screens can be changed with physical buttons or RC channels.
- Highly customisable - choose which screens you want to switch between, adjust stall speed for airspeed indicators etc.
- High performance - dual core processors and efficient rendering.  Target FPS for graphical elements is ~20FPS for smooth movement 
