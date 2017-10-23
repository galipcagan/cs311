# CMPSC 311 #3 Tagline Devicer Driver
The assignment consists of writing a device driver for the tagline device. The code translates tagline operations in to low-level disk operations. The code determines how to store data on the disk in a way that allows later reads to be successfully completed. It does multiblock reads and writes, block-level redundancy and disk failure recovery.

- All the code written is in "tagline_driver.c"
- To Run: type in "make", following with "./tagline_sim -v short-workload.dat"
		
