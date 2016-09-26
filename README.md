# OS-X-ALPS-DRIVER

This is a driver created for my personal use and optimized for my ALPS V3 Pinnacle on my Dell Inspiron N5110

The Driver is based on Rehabman's VoodooPS2 driver for OS X and bpedman's Linux port of the ALPS driver

What's new compared to the bpedman merge:
- Fixed bitmap processing. It's now able to track and report coordinates for 2 fingers simultaneously
- Removed the hardcoded ALPS profile from the kext and added it to the info.plist
- Fined tuned (and removed all synaptics specific variables) from the ALPS profile
- Applied Sierra OS X memory management fix by RehabMan ti avoid kernel Panic
- more...
