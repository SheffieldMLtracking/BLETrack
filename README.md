![path](https://github.com/user-attachments/assets/af7e39d3-c430-487c-9193-fa6b5d785af8)

Code for Bluetooth Low Energy bee tracking system.

+ "Transmitter" folder contains the code to be flashed to the BT832X/BM832E transmitter board.
+ "Receiver" folder contains the code to be flashed to the DA14531 chip on-tag and the 'Active Scanner' code for testing & diagnostics.
+ "To Print" folder contains [FreeCAD](https://www.freecad.org/) files of the transmitter housing.
+ "Path Inference" folder contains code to process data from tag/dev board

## Notes on Programming Transmitter
### Transmitter programming:
+ Requires [nRF Connect SDK](https://www.nordicsemi.com/Products/Development-software/nRF-Connect-SDK).
+ Create a build for the nrf52dk_nrf52832 and add the included device tree overlay.
+ Connect nrf52dk and BT832X/BM832E to the computer via JLINK (done via micro USB cable). Connect DEBUG OUT on nrf52dk to DEBUG IN on BT832X/BM832E:![nrf52dk_programming_example](https://github.com/SheffieldMLtracking/BLETrack/assets/48182877/72ba9a20-63c9-43b5-8ed9-a530d90849f3)
+ flashthisfirst.c must be flashed to the BT832X/BM832E first - then main.c can be flashed.

NOTE: P0.31 is used for encoder output A, P0.30 for encoder output B and P0.11 for hall effect sensor input.

### Receiver programming:
+ Requires Keil uVision and configuration of environment as shown [here](https://lpccs-docs.renesas.com/UM-B-117-DA14531-Getting-Started-With-The-Pro-Development-Kit/01_abstract/abstract.html).
+ To program a receiver tag via flashing code to the OTP memory, follow [these](https://lpccs-docs.renesas.com/Tutorial_SDK6/otp_prog.html) instructions. **If you need to debug code on a tag, do not flash to the OTP.** DA14531 USB Development Kit can be used as a serial wire debug probe to debug code on an external DA14531 chip on the receiver tag using [this](https://lpccs-docs.renesas.com/Tutorial_SDK6/debug_probe.html) configuration of SWIO cables and DIP switches.
+ Note: binary for compiled DA14531 is found in Keil/DA14531_OUT/Objects.
+ Note: USB dev kit switches should be configured as shown: ![da14531_usb](https://github.com/user-attachments/assets/ea0dc939-af4d-4677-8ecf-7ada238475e3)

## Usage of the System
### Setting Up Transmitters
+ Charge transmitters before field trial!
+ Each transmitter is placed onto a tripod across the landscape. The 0 degree point on the tripod (denoted by the small magnet on the base the transmitter sits in) should be pointed north using a compass before placing the transmitter itself upon the tripod. The transmitter should be level with the ground and mounted as high up as possible.
+ As each transmitter is turned on, the time between turning each on should be taken using a stop watch. This time is later used to zero the time readouts in the packets.
+ Using the "Active Scanner" code downloaded to the DA14531 USB Dev Kit, the serial output from the dev kit - which can be accessed using a serial terminal program such as Tera Term - should be used to verify the functionality of the tramsitters. The packets recieved by the active scanner are in the following form and should be assessed to ensure that the transmitter is in fact transmitting and that the motor is reading out ~360 degrees of rotation, resetting at the zero point. If there is an issue, the transmitter may need reprogramming, regarching or the motor encoder may have been knocked out of place.
![Screenshot from 2024-10-29 13-43-06](https://github.com/user-attachments/assets/d90f2625-5866-4be1-9785-5963071bdc67)
+ Each transmitter currently has a range of approximately 100m (further at peak signal strength but not currently used in angle inference algorithm). The area tracked should be covered by at least 2, preferably 3 transmitters.

### Using a Receiever Tag
+ A programmed tag should be allowed to drain fully to hardware reset, then be charged and attatched to a bee.
+ Following retrieval, put the tag on charge and apply ground to the interrupt pin to switch the tag into transmit mode.
+ Using a bluetooth sniffer (reccomended [nrf52 BLE sniffer](https://www.nordicsemi.com/Products/Development-tools/nRF-Sniffer-for-Bluetooth-LE)) and Wireshark the transmitted packets can be read. The following filter shows only read packets:
<p align=center>btle.advertising_header.pdu_type == 0x01 && !_ws.expert && btle.advertising_address == 80:ea:ca:70:00:04</p> 
+ The data of each packet is encoded in the destination addr, in the same form as the transmitted packets except the RSSI of the packet is encoded in the leftmost byte.

### Performing Infernece
+ Instructions for performing path inference on data are located in PathInferenceFinalCut.inpyb
