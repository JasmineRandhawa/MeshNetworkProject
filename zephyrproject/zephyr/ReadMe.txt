=========================== Steps to build entire "/zephyr/Project" ================================

--Step 1: -- delete "/zephyr/build"  and "/zephyr/.cache" folder and 
          -- flash one of the boards with Beacode code using below commands one by one:
				pyocd erase -c chip -t nrf52
				west build -b reel_board_v2 Project/Beacon
				west flash

Step 2: 
		  -- every beacon has unique device_UUID, 
		  -- currently value of variable "my_device_uuid"  is 
		  -- set to  "{0xbb, 0xbb}" in  "/Project/Beacon/src/mesh.c  line:16"
		  -- so if there is more than one beacon board ,
		  -- then  change the "my_device_uuid" variable value to 
		  -- some other value like "{0xbb, 0xcc}" in  "/Project/Beacon/src/mesh.c line: 16"
		  -- and save "/Project/Beacon/src/mesh.c" file after making chnages 
		  -- then delete "/zephyr/build"  and "/zephyr/.cache" folder and
		  -- flash second beacon board using below commands one by one:
				pyocd erase -c chip -t nrf52
				west build -b reel_board_v2 Project/Beacon
				west flash

--Step 3: -- delete "/zephyr/build"  and "/zephyr/.cache" folder and 
          -- flash one of the boards with Beacode code using below commands one by one:
				pyocd erase -c chip -t nrf52
				west build -b reel_board_v2 Project/Provisioner
				west flash

=====================================================================================================

-- Overview of project ---
The provisioner board first create configiration database . Then it provisions and configures itself.
The provisioner then detects the other other beacon boards and provision and configure them one by one.
The Beacon boards after getting provisioned starts sending messages to all its neighbours including provisioner.
The provisioner also replies and send messsages to all beacon nodes.
