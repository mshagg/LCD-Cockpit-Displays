**Setup**

The project requires the Arduino IDE software:

https://docs.arduino.cc/software/ide/

- Install Arduino IDE
- Install esp32 by Espressif Systems via the Boards Manager.  Built on esp32 v3.3.10.
- Select the ESP32S3 Dev Module (Tools - Board - esp32) as the Board
- Install the additional required libraries via the Library Manager:
  - GFX Library for Arduino
  - SensorLib (required for standalone versions, used to access IMU data)
- Select the COM port which appears after connecting the device to USB (Tools > Port)

**Editing and flashing code**

Each version of the display has its own folder which contains all of the code required to run (eg. standalone_LCD_landscape).  Open the corresponding .ino file (eg. standalone_LCD_landscape.ino).

If you configured the IDE environment per above, you should be able to compile and flash to the board (Sketch > Upload).

If you need to configure any display options, you can make the required edits in the IDE software before flashing.

**Hardware assumptions used by the code**

The built-in display is driven as an ST7789/ST7789V2 SPI display with these pins:

LCD_DC     GPIO4
LCD_CS     GPIO5
LCD_SCK    GPIO6
LCD_MOSI   GPIO7
LCD_RST    GPIO8
LCD_BL     GPIO15

The onboard QMI8658 IMU is read over I2C:

IMU_SDA    GPIO11
IMU_SCL    GPIO10

The PWR/function button is read on:

BUTTON_PWR_PIN    GPIO40
SYS_EN_PIN        GPIO41
