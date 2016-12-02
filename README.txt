						MEM_SMR 
		A Linux Kernel module for simulating Shingled Magnetic Recording, 
		with EXT2-file-system-aware ability
																		
										sheisc@163.com			
########################## HOW TO USE ###################################
iron@ASUS:mem_smr$ make

Load STL kernel module with one of the following choices

	(1)save the context of STL(Smr Translation Layer)
	iron@ASUS:mem_smr$sudo ./load.sh save=1
	default value is 0
	(2)make the STL ext2-aware
	iron@ASUS:mem_smr$sudo ./load.sh aware=1
	default value is 1
	(3)reset all the internal counters .
	iron@ASUS:mem_smr$sudo ./load.sh reset=1
	default value is 0
	(4)favor the meta data of ext2 when swapping in/out data for HDD BUF
	iron@ASUS:mem_smr$sudo ./load.sh favor=1
	default value is 1
	(5)
	iron@ASUS:mem_smr$sudo ./load.sh favor=1 reset=1

iron@ASUS:mem_smr$sudo ./unload.sh
iron@ASUS:mem_smr$dmesg
