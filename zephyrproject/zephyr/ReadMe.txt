Step 1: Flash one of the boards with Beacode code using below commands:
	pyocd erase -c chip -t nrf52
	west build -b reel_board_v2 Project/Beacon
	west flash

Step 2: Flash the main board with Provisioner code using below commands:
	pyocd erase -c chip -t nrf52
	west build -b reel_board_v2 Project/Provisioner
	west flash


The provioner board first create configiration database . Then provisions and configures itself.
The provioner then detects the other other beacon board and provisions it.

The Beacon board after getting provioned starts sending status message that includes rssi to the provisioner board after every 10 sec
