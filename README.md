# BLETrack
Code for Bluetooth Low Energy bee tracking system.

+ "Transmitter" folder contains the code to be flashed to the BT832X/BM832E transmitter board.
+ "Receiver" folder contains the code to be flashed to the DA14531 chip on-tag.

NOTE: When programming the transmitter:
+ Create a pristine build for the nrf52dk_nrf52832 and add the included device tree overlay.
+ Connect nrf52dk and BT832X/BM832E to the computer via JLINK (done via micro USB cable). Connect DEBUG OUT on nrf52dk to DEBUG IN on BT832X/BM832E:![nrf52dk_programming_example](https://github.com/SheffieldMLtracking/BLETrack/assets/48182877/72ba9a20-63c9-43b5-8ed9-a530d90849f3)

+ flashthisfirst.c must be flashed to the BT832X/BM832E first - then main.c can be flashed. 
