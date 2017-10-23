# CMPSC 311 #3 Tagline Devicer Driver
This project consisted of writing code for a device driver called “tagline” that sits between a virtual application and a virtualized hardware (called RAID array). Tagline is a storage abstraction similar to a data file which the application creates, reads and writes multiple blocks, block-level redundancy and disk failure recovery. The code translates tagline operations into low-level disk operations. It also determines how to store data on the virtualized hardware in order to finish its functionalities successfully. 

- All the code written is in "tagline_driver.c"
- To Run: type in "make", following with "./tagline_sim -v short-workload.dat"
		
