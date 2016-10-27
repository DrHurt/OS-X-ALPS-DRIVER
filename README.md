# OS-X-ALPS-DRIVER

This is a driver created for my personal use and optimized for my ALPS V3 Pinnacle on my Dell Inspiron N5110

The Driver is based on Rehabman's VoodooPS2 driver for OS X and bpedman's Linux port of the ALPS driver

Driver Features:
- Supports ALPS hardware versions V1-V5
- Supports Mac OS 10.6 to 10.12
- 1-finger tapping.
- Side (edge) scrolling (Vertical with inertia, and Horizontal).
- 2-finger tap for right click.
- 2-finger scrolling (vertical and horizontal with inertia).
- 3 and 4-finger gestures (V3+, check the log to find out your hw version).
- Pointer acceleration and smoothing.
- Trackstick (movement and scrolling).
==========================================
 
Changes:
- Touchpad initialization, packet decoding and processing updated Linux 4.7
- Bitmap processing overhauled. Can now provide coordinates for 2 fingers simultaneously.
- Added new device ids for better compatibility.
- Remove hardcoded variables allowing userspace (plist) settings to apply.
- Painstakingly optimized default settings to provide the best possible experience.
- Improve pointer motion calculation, mode switching,… etc.
- Lots of code cleanups, refactoring.
- Clean up the IOLog.
