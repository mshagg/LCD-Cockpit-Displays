**Overview**

The code is infinitely adjustable, but two key changes you may wish to make are:
- IMU filtering (to increase/reduce responsiveness of the display, at the expense of IMU noise e.g. from vibrations)
- Pitch angle offset (if your cockpit is not 90 degrees perpendicular to gravity)

**IMU Filtering**

The standalone AHI uses the onboard QMI8658 IMU as a simple accelerometer-based attitude source. The raw accelerometer readings are intentionally filtered before being used to draw the artificial horizon, otherwise the display can look jittery.

In order to adjust this, change the smoothing alpha (default: 0.35).  Increasing this number by will increase responsiveness, decreasing will improve smoothness.  Practical range is ~0.2 to 0.6. 

This setting can be found in the imu_qmi8658.ino tab:

<img width="413" height="285" alt="Screenshot 2026-07-21 194700" src="https://github.com/user-attachments/assets/dc912b8f-6bd2-4e19-b163-99c7abd800ac" />

**Pitch angle offset**

Some users may mount the screen at a slight angle rather than perfectly vertical. For example, the display may be tilted back toward the pilot’s line of sight. In that case, the AHI may need a small pitch offset so that the displayed horizon appears level when the aircraft itself is level.

In order to adjust this, change the IMU pitch trim (default: 0).  It will likely need to increase.

<img width="408" height="355" alt="Screenshot 2026-07-21 200256" src="https://github.com/user-attachments/assets/45fae595-7139-44eb-acb5-6403b403108e" />


