--Step 1: Flash one of the boards with Beacode code using below commands:
	pyocd erase -c chip -t nrf52
	west build -b reel_board_v2 Project/Beacon
	west flash

-- every beacon has unique device_UUID, so if there is more than one beacon board , 
-- then  change the "my_device_uuid" variable value to  "{0Xbb, 0Xcc}" in  "/Beacon/src/mesh.c line: 16"
-- and run below commands
	pyocd erase -c chip -t nrf52
	west build -b reel_board_v2 Project/Beacon
	west flash

--Step 2: Flash the main board with Provisioner code using below commands:
	pyocd erase -c chip -t nrf52
	west build -b reel_board_v2 Project/Provisioner
	west flash


The provisioner board first create configiration database . Then it provisions and configures itself.
The provisioner then detects the other other beacon boards and provision and configure them one by one.
The Beacon boards after getting provisioned starts sending messages to all its neighbours including provisioner.
The provisioner also replies and send messsages to all beacon nodes.
