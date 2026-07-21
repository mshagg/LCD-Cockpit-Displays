The standalone scripts are available in both portrait and landscape.  Functionality of both is the same.

Currently there are four screens, which can be switched between by short pressing the PWR button on the waveshare board.  The screen will remember which screen was selected.

**Wiring**

The screen only requires power.  Connect the purple wire on the breakout lead to GND and the grey wire to 5v.

<img width="600" height="382" alt="ESP32-S3-LCD-1 69_240718_001" src="https://github.com/user-attachments/assets/4a0e4e70-47c3-499e-a530-6ff10dd3b643" />

**Use**

Note the IMU calibrates itself at boot, the plane should be level when powering on.

Short press the PWR button on the board to switch between displays.  The device will remember the last used display and select that at the next boot.

<img width="623" height="684" alt="PXL_20260721_052223771" src="https://github.com/user-attachments/assets/02f913bd-54f6-48a4-98f7-369aa3726673" />
