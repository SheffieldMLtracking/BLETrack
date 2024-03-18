# BLETrack
Code for Bluetooth Low Energy bee tracking system.

+ "Transmitter" folder contains the code to be flashed to the BT832X transmitter board.
+ "Receiver" folder contains the code to be flashed to the DA14531 chip on-tag.

NOTE: When programming the transmitter:
+ Create a new build for the nrf52dk_nrf52832 and add the included device tree overlay.
+ flashthisfirst.c must be flashed to the BT832X first - then main.c can be flashed. 
