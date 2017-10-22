///////////////////////////////////////////////////////////////////////////////
//
//  File           : tagline_driver.c
//  Description    : This is the implementation of the driver interface
//                   between the OS and the low-level hardware.
//
//  Author         : Galip R. Cagan
//  Created        : 

// Include Files
#include <stdlib.h>
#include <string.h>
#include <cmpsc311_log.h>

// Project Includes
#include "raid_bus.h"
#include "tagline_driver.h"

//Struct
typedef struct tagline_Struct{		//Tagline struct  where all values will be saved
	uint64_t req_type;
	uint64_t num_block;
	uint64_t disk_num;		// these are the fields for the extract to be saved 
	uint64_t unknown;		// not really used them though
	uint64_t r_val;
	uint64_t block_ID;

	uint64_t backup_disk;
	uint64_t backup_block;		//these fields are for the memmory mapping
	uint64_t primary_disk;
	uint64_t primary_block;
	uint64_t owrite;
	uint64_t bowrite;
	uint64_t data;

} tagline_Struct;		

//Global initializes
tagline_Struct **tagline_arr;     	// initializing a global double array by double pointer
uint32_t maxlineVal;			//  maxlines to pass in disk_signal
int block_arr[RAID_DISKS] = {0};	//in order to keep track from 0-n disks		helps for writing
int back_block_arr[RAID_DISKS]= {0};	// in order to keep track from n-0 disk
int	disk;     //start for primary disk
int	back_disk = (int)((RAID_DISKS+1)/2); 

// Functions
////////////////////////////////////////////////////////////////////////////////
//
// Function     : create_raid_request
// Description  : Creates raid request
//
// Inputs       : unit64_t of requirment, # block, # disks, unknown, return value and block ID 
// Outputs      : 0 if successful, -1 if failure
RAIDOpCode create_raid_request(uint64_t req_type, uint64_t num_block, uint64_t disk_num, uint64_t unknown, uint64_t r_val,
                                 uint64_t block_ID){
 // creating the request of 64 bits by using the logical or to the req_val
	uint64_t req_val;
	req_val = 0x0;				
    req_val |= req_type << 56;		//packing the fields together
    req_val |= (num_block) <<48;
    req_val |= disk_num << 40;
    req_val |= unknown << 34;
    req_val |= r_val << 33;
    req_val |= block_ID;

    return (req_val);		//return 64 bit opcode
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : extract_raid_response
// Description  : Extract raid response values to save in global structure array
//
// Inputs       : RAIDOpCode opcode
// Outputs      : 0 if successful, -1 if failure
uint32_t extract_raid_response(RAIDOpCode opcode ){
//extract values from the bus calls opcode which is 64 bits
	uint64_t req_type, num_block, disk_num,  unknown, r_val, block_ID;
	
	block_ID = (opcode & 0xffff);			//shifting an using logical and to take out the wanted fields
	r_val = ((opcode>>=32) & 0x01);
	unknown = ((opcode>>=1) &0x7f);
	disk_num = ((opcode>>=7)&0xff);
	num_block = ((opcode>>=8)&0xff);
	req_type = (opcode>>=8);

	(*tagline_arr)->block_ID = block_ID;		//saving the values but I didn't use these in my function
	(*tagline_arr)->r_val = r_val;
	(*tagline_arr)->unknown = unknown;
	(*tagline_arr)->disk_num = disk_num;
	(*tagline_arr)->num_block = num_block;
	(*tagline_arr)->req_type = req_type;

      logMessage(LOG_INFO_LEVEL, "Block:%u    Result:%u    Unknown:%u    Disk:%u    #Block:%u    Req:%u",block_ID, r_val,
			unknown, disk_num, num_block, req_type);

	return block_ID; 		//return blockid to see disk fail or not
}
////////////////////////////////////////////////////////////////////////////////
//
// Function     : raid_disk_signal
// Description  : Tells if there is disk fail. If so formats disk and then read backup disk(vice versa)  
//			and then reads backup to write in primary (vice versa)
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure
int raid_disk_signal(){

	logMessage(LOG_INFO_LEVEL, "!!!!!!!!!!!!!THERE IS DISK SIGNAL!!!!!!!!!!!!!!");

	char buf_arr[RAID_BLOCK_SIZE];			//make temporary buffer
	int disk = 0, i = 0, j = 0;			//settings values
	uint32_t returnVal;				// returnval for extract

	for(disk = 0; disk < RAID_DISKS ; disk++){	
		returnVal = extract_raid_response(raid_bus_request(create_raid_request(RAID_STATUS,0,disk,0,0,0), NULL));
		  if(returnVal == 2){  //checking for extract if there is a fail
			logMessage(LOG_INFO_LEVEL, "!!!!!!!!!!!!!*******TRYING TO RECOVER DISK*****!!!!!!!!!!!!!!");
			raid_bus_request(create_raid_request(RAID_FORMAT,0,disk,0,0,0), NULL);
			// formats the not working disk
			for( i = 0; i < maxlineVal ; i++){  //loops through to find the right tag and block
				for(j =0 ; j <MAX_TAGLINE_BLOCK_NUMBER; j++){
					if(tagline_arr[i][j].primary_disk == disk){
				        	raid_bus_request(create_raid_request(RAID_READ,1,tagline_arr[i][j].backup_disk ,0 ,0,
							tagline_arr[i][j].backup_block),buf_arr);
						raid_bus_request(create_raid_request(RAID_WRITE,1,tagline_arr[i][j].primary_disk ,0 ,0,
							tagline_arr[i][j].primary_block),buf_arr);
					}
					else if (tagline_arr[i][j].backup_disk == disk){
						raid_bus_request(create_raid_request(RAID_READ,1,tagline_arr[i][j].primary_disk ,0 ,0,
							tagline_arr[i][j].backup_block),buf_arr);
						raid_bus_request(create_raid_request(RAID_WRITE,1,tagline_arr[i][j].backup_disk ,0 ,0,
							tagline_arr[i][j].primary_block),buf_arr);
					}
				}
			}
		  }
	}	
	return 0;
}


////////////////////////////////////////////////////////////////////////////////
//
// Function     : tagline_driver_init
// Description  : Initialize the driver with a number of maximum lines to process
//
// Inputs       : maxlines - the maximum number of tag lines in the system
// Outputs      : 0 if successful, -1 if failure
int tagline_driver_init(uint32_t maxlines) {
	
	int i;
	int disks;				//initialze disks for hardware

		disks = 0;	            //bus request for initiliaze
		i = 0;
        tagline_arr =(struct tagline_Struct **)malloc(sizeof(tagline_Struct *)*maxlines);    // alloacting memmor for the first pointer arr
		for(i=0;i< maxlines; i++ ){	//allocatioing 2 dimensional array that's why for loop				
        	tagline_arr[i] =(struct tagline_Struct *)malloc(sizeof(tagline_Struct)*(MAX_TAGLINE_BLOCK_NUMBER));
		}
		raid_bus_request(create_raid_request(RAID_INIT,(RAID_DISKBLOCKS/RAID_TRACK_BLOCKS),RAID_DISKS,0,0,0),NULL); //initialize
        									//theres a divde to make sure size is correct
 		for(disks = 0; disks<RAID_DISKS ; disks++){//as there might be more disks we have go through all of them	
			raid_bus_request(create_raid_request(RAID_FORMAT,0,disks,0,0,0), NULL); // formatting all disks
	 	}
		maxlineVal =(uint32_t)maxlines;	// saving maxlines value to use later
        // Return successfully
        logMessage(LOG_INFO_LEVEL, "TAGLINE: initialized storage (maxline=%u)", maxlines);
        return(0);
 }

////////////////////////////////////////////////////////////////////////////////
//
// Function     : tagline_read
// Description  : Read a number of blocks from the tagline driver
//
// Inputs       : tag - the number of the tagline to read from
//                bnum - the starting block to read from
//                blks - the number of blocks to read
//                bug - memory block to read the blocks into
// Outputs      : 0 if successful, -1 if failure

int tagline_read(TagLineNumber tag, TagLineBlockNumber bnum, uint8_t blks, char *buf) {

	int i;

	for(i = 0; i<blks ; i++){
	raid_bus_request(create_raid_request(RAID_READ,1,(tagline_arr[tag][bnum+i].primary_disk), 0 , 0,
		 (tagline_arr[tag][bnum+i].primary_block)),&buf[i*RAID_BLOCK_SIZE]);  //note have to multiply 
	}												 // to get each block	
	// Return successfully
	logMessage(LOG_INFO_LEVEL, "TAGLINE : read %u blocks from tagline %u, starting block %u.",
			blks, tag, bnum);
	return(0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : tagline_write
// Description  : Write a number of blocks from the tagline driver
//
// Inputs       : tag - the number of the tagline to write from
//                bnum - the starting block to write from
//                blks - the number of blocks to write
//               bug - the place to write the blocks into
// Outputs      : 0 if successful, -1 if failure

int tagline_write(TagLineNumber tag, TagLineBlockNumber bnum, uint8_t blks, char *buf){
		
	int i;//, disk, back_disk;	
	i = 0;
 //start for backup disk

 for(i = 0; i <blks; i++){

	if(!(tagline_arr[tag][bnum+i].owrite)){//regular write

		if((back_disk == (int)((RAID_DISKS+1)/2)) && block_arr[disk] == 0){
			back_block_arr[back_disk] = (int)((RAID_DISKBLOCKS/2)-1);
		}

		raid_bus_request(create_raid_request(RAID_WRITE,1,disk ,0 ,0,(block_arr[disk])),&buf[i*RAID_BLOCK_SIZE]);
		raid_bus_request(create_raid_request(RAID_WRITE,1,back_disk,0,0,(back_block_arr[back_disk])),&buf[i*RAID_BLOCK_SIZE]);

		tagline_arr[tag][bnum+i].primary_disk = disk;	//primary values being saved for data structture
		tagline_arr[tag][bnum+i].primary_block = block_arr[disk];
		block_arr[disk]++;

		tagline_arr[tag][bnum+i].backup_disk = back_disk;	//backup values bein saved for data structure
		tagline_arr[tag][bnum+i].backup_block = back_block_arr[back_disk];
		back_block_arr[back_disk]++;

		if(block_arr[disk] >= (int)(RAID_DISKBLOCKS)){
				disk++;
				block_arr[disk] = 0;
		}
		if(back_block_arr[back_disk] >= (int)(RAID_DISKBLOCKS)){
			back_disk++;
			back_block_arr[back_disk] = 0;
		}

		tagline_arr[tag][bnum+i].owrite = 1;			
	}

	else if((tagline_arr[tag][bnum+i].owrite == 1)){ //over write
		logMessage(LOG_INFO_LEVEL,"!!!!!!!!!!!!!!!YOU ARE OVER_WRITING!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");

		raid_bus_request(create_raid_request(RAID_WRITE,1,tagline_arr[tag][bnum+i].primary_disk ,0 ,0,
			tagline_arr[tag][bnum+i].primary_block),&buf[i*RAID_BLOCK_SIZE]);
		raid_bus_request(create_raid_request(RAID_WRITE,1,tagline_arr[tag][bnum+i].backup_disk ,0 ,0,
			tagline_arr[tag][bnum+i].backup_block),&buf[i*RAID_BLOCK_SIZE]);
		block_arr[disk]++;
		back_block_arr[back_disk]++;

		// over write in order to keep track of the blocks that are already written
	}

	if(block_arr[disk] >= (int)(RAID_DISKBLOCKS)){
		disk++;
		block_arr[disk] = 0;
	}
	if(back_block_arr[back_disk] >= (int)(RAID_DISKBLOCKS)){
		back_disk++;
		back_block_arr[back_disk] = 0;
	}

 }
	// Return successfully
	logMessage(LOG_INFO_LEVEL, "TAGLINE : wrote %u blocks to tagline %u, starting block %u.",
			blks, tag, bnum);
	
	return(0);
}
////////////////////////////////////////////////////////////////////////////////
//
// Function     : tagline_close
// Description  : Close the tagline interface
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int  tagline_close() {//closing tagline 
	
	free(tagline_arr);		//de allocating memmory for double array

	raid_bus_request(create_raid_request(RAID_CLOSE,0,0,0,0,0), NULL);   // bus request to close
	// Return successfully
	logMessage(LOG_INFO_LEVEL, "TAGLINE storage device: closing completed.");
	return(0);
}
