# BLETrack
Code for Bluetooth Low Energy bee tracking system.

+ "Transmitter" folder contains the code to be flashed to the BT832X/BM832E transmitter board.
+ "Receiver" folder contains the code to be flashed to the DA14531 chip on-tag.
+ "To Print" folder contains [FreeCAD](https://www.freecad.org/) files of the transmitter housing.

## Transmitter programming:
+ Requires [nRF Connect SDK](https://www.nordicsemi.com/Products/Development-software/nRF-Connect-SDK).
+ Create a build for the nrf52dk_nrf52832 and add the included device tree overlay.
+ Connect nrf52dk and BT832X/BM832E to the computer via JLINK (done via micro USB cable). Connect DEBUG OUT on nrf52dk to DEBUG IN on BT832X/BM832E:![nrf52dk_programming_example](https://github.com/SheffieldMLtracking/BLETrack/assets/48182877/72ba9a20-63c9-43b5-8ed9-a530d90849f3)
+ flashthisfirst.c must be flashed to the BT832X/BM832E first - then main.c can be flashed. 

## Receiver programming:
+ Requires Keil uVision and configuration of environment as shown [here](https://lpccs-docs.renesas.com/UM-B-117-DA14531-Getting-Started-With-The-Pro-Development-Kit/01_abstract/abstract.html).
+ To program a receiver tag via flashing code to the OTP memory, follow [these](https://lpccs-docs.renesas.com/Tutorial_SDK6/otp_prog.html) instructions. **If you need to debug code on a tag, do not flash to the OTP.** DA14531 USB Development Kit can be used as a serial wire debug probe to debug code on an external DA14531 chip on the receiver tag using [this](https://lpccs-docs.renesas.com/Tutorial_SDK6/debug_probe.html) configuration of SWIO cables and jumper switches.
