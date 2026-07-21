**Overview**

The code is infinitely adjustable, but two key changes you may wish to make are:
- IMU filtering (to increase/reduce responsiveness of the display, at the expense of IMU noise e.g. from vibrations)
- Neutral pitch angle (if your cockpit is not 90 degrees perpendicular to gravity)

**IMU Filtering**

The standalone AHI uses the onboard QMI8658 IMU as a simple accelerometer-based attitude source. The raw accelerometer readings are intentionally filtered before being used to draw the artificial horizon, otherwise the display can look jittery.

In order to adjust this, change the smoothing alpha (default: 0.35).  Reducing this number by will increase responsiveness, increasing will improve smoothness.  Practical range is ~0.2 to 0.5. 

<img width="413" height="285" alt="Screenshot 2026-07-21 194700" src="https://github.com/user-attachments/assets/dc912b8f-6bd2-4e19-b163-99c7abd800ac" />
