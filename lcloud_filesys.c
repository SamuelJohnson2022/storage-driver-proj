////////////////////////////////////////////////////////////////////////////////
//
//  File           : lcloud_filesys.c
//  Description    : This is the implementation of the Lion Cloud device 
//                   filesystem interfaces.
//
//   Author        : *** Samuel Johnson ***
//   Last Modified : *** 5/1/2020 ***
//

// Include files
#include <stdlib.h>
#include <string.h>
#include <cmpsc311_log.h>
#include <math.h>

// Project include files
#include <lcloud_filesys.h>
#include <lcloud_controller.h>
#include <lcloud_cache.h>
#include <lcloud_client.h>



//
// File system interface implementation

// Global Variables
//int dev_Num; // The current device number of the powered on device

int firstOpen = 1;

//int usedBlocks[16][10][64] = {[0 ... 15][0 ... 9][0 ... 63] = 0}; // Array tracking which blocks have been used to store data, with organization [sector][block]

//Structures
typedef struct block {
    int device; // Device where the block is found
    int sector; // Sector where the block is found
    int blockNum; // Offset, in bytes, from the start of the sector where the block begins
} block;

typedef struct device {
    int on; // Whether or not the device is on
    int sectors; // Number of sectors in the device
    int blocks; // Number of blocks in the device
    int *usedBlocks;
} device;

typedef struct file {
    char name[120]; // String for the name of the file, max size of 20 characters
    LcFHandle handle; // Index of the pointer to the file in the file descriptor table
    int position;
    int size; // Size of the file in bytes
    block blocks[10000]; // Array containing all of the blocks where the file is contined, in order of how they are stored
    int blockCount; // Integer contained the number of blocks this file is stored in
    int open; // 1 if open, 0 if closed
} file;

LcFHandle fileHandleCounter = 0; //Variable containing an int of the current file pointer index

device devOn[16]; //Array containing all of the devices

//Table containing all of the file handles
file *fhTable; // Pointer to the start of an array containing the pointers to each file

// List of all open files (Maybe Assign 3)
//LcFHandle *openFileList; // Pointer to the start of an array containing the list of all open files

////////////////////////////////////////////////////////////////////////////////
//
// Function     : create_lcloud_registers
// Description  : packs all of the instruction information into the registers
//
// Inputs       : 
//              b0: 0 when sending, 1 when responding
//              b1: 0 when sending to devices, 1 for a success from device
//              c0: OP CODE (all codes found in Icloud_controller.h)
//              c1: LC_POWER_ON/OFF : 0, LC_DEVPROBE : 0, LC_BLOCK_XFER : the device ID for the device to read from
//              c2: LC_POWER_ON/OFF : 0, LC_DEVPROBE : 0, LC_BLOCK_XFER : LC_XFER_WRITE/LC_XFER_READ
//              d0: LC_POWER_ON/OFF : 0, LC_DEVPROBE : 2^x = d0 (where x is the device id, 16 devices), LC_BLOCK_XFER : Block to read/write from
//              d1: LC_POWER_ON/OFF : 0, LC_DEVPROBE : 0, LC_BLOCK_XFER : Sector to read/write from
//                
// Outputs      : LCloud Register Frame packed with all of the separate register values

LCloudRegisterFrame create_lcloud_registers(uint64_t b0, uint64_t b1, uint64_t c0, uint64_t c1, uint64_t c2, uint64_t d0, uint64_t d1) {
    // Packs a register frame by shifting each of the integer inputs over by their respective amounts based on the frame specifications, with d0 as the LSB    
    LCloudRegisterFrame packedReg = d0 | (d1<<16) | (c2<<32) | (c1<<40) | (c0<<48) | (b1<<56) | (b0<<60); 
    return(packedReg); // returns the register frame
} 

////////////////////////////////////////////////////////////////////////////////
//
// Function     : get_Next_Block
// Description  : returns an avaliable block to write to
//
// Inputs       : Nothing
// Outputs      : A block containing the locaition of a free block

block get_Next_Block() {

    // Variables used in function
    int currentBlock;
    int currentSector;
    block retBlock;

    // Loop through the devices
    for(int i = 0; i < 16; i++){

        //Reset the value for block and sector
        currentBlock = 0;
        currentSector = 0;

        // Make sure we are only looping through online devices
        if(devOn[i].on == 1){

            // Inefficient way of finding a new block, but works nonetheless.
            while(*(devOn[i].usedBlocks + currentSector*devOn[i].blocks + currentBlock) == 1){
               

                if(currentSector == (devOn[i].sectors - 1) && currentBlock == (devOn[i].blocks - 1)){ // All the block are filled
                            break;
                        }

                if(currentBlock != (devOn[i].blocks - 1)){ // Increments the value for block until its at the max
                    currentBlock ++;
                } else if(currentSector != devOn[i].sectors - 1){ // Increments the sector and resets the block to 0 
                    //logMessage( LOG_ERROR_LEVEL, "Sector: %d, Block: %d", sector, currentBlock);
                    currentBlock = 0;
                    currentSector ++;
                }
            }

            //logMessage( LOG_ERROR_LEVEL, "If: %d, Block: %d, Sector: %d", devOn[i].usedBlocks[currentSector][currentBlock], currentBlock, currentSector);

            //When the loop breaks we return the block containing the next location
            retBlock.blockNum = currentBlock;
            retBlock.sector = currentSector;
            retBlock.device = i;

            if(!(currentSector == (devOn[i].sectors - 1) && currentBlock == (devOn[i].blocks - 1))){ // All the block are filled
                return retBlock; // Return the block only if we are not full, else we iterate the for loop                
            }
        }

        
    }

    // Returns an error and set the location to -1, -1
    logMessage( LOG_ERROR_LEVEL, "No avaliable blocks");
    retBlock.sector = -1;
    retBlock.blockNum = -1;
    retBlock.device = -1;

    return retBlock;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : extract_lcloud_registers
// Description  : extract particular registers from a register frame
//
// Inputs       : path, then the addresses of ints to store the the register contents into, starting with b0 -> d1
// Outputs      : a 0 for success, -1 for failure to extract

int extract_lcloud_registers(LCloudRegisterFrame resp, uint64_t*b0, uint64_t*b1, uint64_t*c0, uint64_t*c1, uint64_t*c2, uint64_t*d0, uint64_t*d1) {
    
    *b0 = (resp>>60);
    *b1 = (resp<<4)>>60;
    *c0 = (resp<<8)>>56;
    *c1 = (resp<<16)>>56;
    *c2 = (resp<<24)>>56;
    *d0 = (resp<<32)>>48;
    *d1 = (resp<<48)>>48;
    return( 0 ); // Return 0 for success
} 

////////////////////////////////////////////////////////////////////////////////
//
// Function     : lcopen
// Description  : Open the file for for reading and writing
//
// Inputs       : path - the path/filename of the file to be read
// Outputs      : file handle if successful test, -1 if failure

LcFHandle lcopen( const char *path ) {
    
    // Creates int variables for each of the register components to be used for error checking when extracting the result register
    uint64_t b0, b1, c0, c1, c2, d0, d1; 

    uint64_t listOfDevices;

    //Turn on the device
    if(firstOpen){

        lcloud_initcache(LC_CACHE_MAXBLOCKS); // Initiate the cache

        //Pack the registers with the command to turn on the device
        LCloudRegisterFrame instructionFrame = create_lcloud_registers(0, 0, LC_POWER_ON, 0, 0, 0, 0);

        //Sends the instruction to the io bus and sets the returned frame into the result variable
        LCloudRegisterFrame resultFrame = client_lcloud_bus_request(instructionFrame, NULL);

        // Checks to make sure that the operation was successful
        if ( (instructionFrame == -1) || (resultFrame == -1) ||
            (extract_lcloud_registers(resultFrame, &b0, &b1, &c0, &c1, &c2, &d0, &d1)) ||
            (b0 != 1) || (b1 != 1) || (c0 != 0) ) {
            logMessage( LOG_ERROR_LEVEL, "Failure to turn on device");
            return( -1 );
            }
        
        // Probes the io_bus to determine which devices are avaliable
        // Packs the registers with the command to probe the device
        instructionFrame = create_lcloud_registers(0, 0, LC_DEVPROBE, 0, 0, 0, 0);

        //Sends the instruction to the io bus and sets the returned frame into the result variable
        resultFrame = client_lcloud_bus_request(instructionFrame, NULL);

        // Checks the returned register for errors
        if ( (instructionFrame == -1) || (resultFrame == -1) ||
            (extract_lcloud_registers(resultFrame, &b0, &b1, &c0, &c1, &c2, &d0, &d1)) ||
            (b0 != 1) || (b1 != 1) || (c0 != 1) ) {
            logMessage( LOG_ERROR_LEVEL, "Failure to probe devices.");
            return( -1 );
            }

        listOfDevices = d0;

        // d0 contains the mask to the device number so we convert to a decimal number using log base 2
        for(int i = 0; i < 16; i++){

            // If the device is on, set its value to 1 and initialize the device. Otherwise, set it to 0
            if((listOfDevices & 1) == 1){
                devOn[i].on = 1;

                // Packs the registers with the command to initialize the device
                instructionFrame = create_lcloud_registers(0, 0, LC_DEVINIT, i, 0, 0, 0);

                //Sends the instruction to the io bus and sets the returned frame into the result variable
                resultFrame = client_lcloud_bus_request(instructionFrame, NULL);

                // Checks the returned register for errors
                if ( (instructionFrame == -1) || (resultFrame == -1) ||
                    (extract_lcloud_registers(resultFrame, &b0, &b1, &c0, &c1, &c2, &d0, &d1)) ||
                    (b0 != 1) || (b1 != 1) || (c0 != LC_DEVINIT) ) {
                    logMessage( LOG_ERROR_LEVEL, "Failure to initialize devices.");
                    return( -1 );
                    }
                
                // Set the values for the number of sectors and the number of blocks for the current device
                devOn[i].sectors = d0;
                devOn[i].blocks = d1;

                // Allocate memory for the usedBlocks array and set the values to 0s
                devOn[i].usedBlocks = (int *)malloc(devOn[i].sectors*devOn[i].blocks*sizeof(int));

                // Make sure that all of the values start out as 0
                for(int k = 0; k < d0; k++){
                    for (int j = 0; j < d1; j++) {
                        *(devOn[i].usedBlocks + k*d1 + j) = 0;
                    }
                }


            } else { 
                devOn[i].on = 0;
            }

            //Left shift the d0 value by 1
            listOfDevices = listOfDevices >> 1;

            //logMessage( LOG_ERROR_LEVEL, "Did open work2? sector = %d, block = %d, on = %d, device = %d.", devOn[i].sectors, devOn[i].blocks, devOn[i].on, i);            
        }

        //Set the first open variable to 0
        firstOpen = 0;
    }

    //If file not in open file list
    file newFile; //Creates a new file
    // Sets the name of the newly created file to be the 128 bit string at the path poiter
    strncpy(newFile.name, path, 32*sizeof(int)); 

    newFile.handle = fileHandleCounter; // Sets the value of the index in the fhHandle table

    //(Maybe for assignment 3)
    fhTable = realloc(fhTable, (fileHandleCounter+1)*sizeof(file)); // allocates memory to be able to store a new file in the fh table

    //Sets the value of open in the new file to 1 and position and size to 0
    newFile.open = 1;
    newFile.position = 0;
    newFile.size = 0;

    fhTable[fileHandleCounter] = newFile;
    
    //logMessage( LOG_ERROR_LEVEL, "Did open work? fh = %s, fh = %d.", fhTable[newFile.handle].name, newFile.handle);

    fileHandleCounter ++; // Increments the fileHandleCounter

    return( newFile.handle ); // Returns the file handle
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : lcread
// Description  : Read data from the file 
//
// Inputs       : fh - file handle for the file to read from
//                buf - place to put the data
//                len - the length of the read
// Outputs      : number of bytes read, -1 if failure
int lcread( LcFHandle fh, char *buf, size_t len ) {

    int bytesRead = 0; // Variable to track the number of bytes read
    char buf_256[256]; //256 byte buffer to be used for receiving data from the io bus
    int i = fhTable[fh].position/256; // Counter used to loop through the blocks, starts at the first block of the position
    LCloudRegisterFrame instructionFrame; // Register frame to be used to pack instructions into
    LCloudRegisterFrame resultFrame; // Register frame to be used to store the result of the io bus call
    int sector; // Sector value to be used in the io bus call
    int block; // Block number to be used in the io bus call
    int dev_Num; // Device number to be used in the io bus call
    char *cacheCheck; // Buffer used to access the cache

    // Checks if the file is open
    if(fhTable[fh].open == 0){
         logMessage( LOG_ERROR_LEVEL, "File not open.");
                return( -1 );   
    }

    // Creates int variables for each of the register components to be used for error checking when extracting the result register
    uint64_t b0, b1, c0, c1, c2, d0, d1; 

    while((len > 0) && fhTable[fh].blocks[i].blockNum != -1){ // Only executes if we read a length greater than 0 and there is file content still to be read
        
        if(fhTable[fh].position%256 != 0 && ((fhTable[fh].position%256 + len) > 256)){ // Preliminary case to make the reading done on block boundaries
            sector = fhTable[fh].blocks[i].sector; // Determine the sector from the current block counter 
            block = fhTable[fh].blocks[i].blockNum; // Determine the block from the current block counter
            dev_Num = fhTable[fh].blocks[i].device; // Determine the device from the current block counter

            cacheCheck = lcloud_getcache(dev_Num, sector, block);

            if(cacheCheck != NULL){ // Check if the desired block is in the cache
                memcpy(buf_256, cacheCheck, 256); // Copy over the cache data into the buf_256 value
            } else {
                instructionFrame = create_lcloud_registers(0, 0, LC_BLOCK_XFER, dev_Num, LC_XFER_READ, block, sector); //Pack the instruction frame

                resultFrame = client_lcloud_bus_request(instructionFrame, buf_256); // Call the io bus with the instruction and save the result in the result frame

                // Checks to make sure that the operation was successful
                if ( (instructionFrame == -1) || (resultFrame == -1) ||
                    (extract_lcloud_registers(resultFrame, &b0, &b1, &c0, &c1, &c2, &d0, &d1)) ||
                    (b0 != 1) || (b1 != 1) || (c0 != LC_BLOCK_XFER) ) {
                    logMessage( LOG_ERROR_LEVEL, "Failure to read an entire block.");
                    return( -1 );
                    }

                // In the case that it is not in the cache, we need to put it into the cache
                lcloud_putcache(dev_Num, sector, block, buf_256); 
            
            }

            memcpy(&buf[bytesRead], &buf_256[fhTable[fh].position%256], 256 - fhTable[fh].position%256); // Copy over the bytes from the io call into the passed buffer from the function call

            bytesRead += 256 - fhTable[fh].position%256; // Increase the bytes read by 256 minus the relative position 
            len -= 256 - fhTable[fh].position%256; // Decreases the length by 256 minus the relative position
            i++; // Increment the block counter
            fhTable[fh].position += 256 - fhTable[fh].position%256; // Set the position to the end of the read

        }

        else if((int)(len/256) > 0){ // Case where the next block we read is the entire block
            
            sector = fhTable[fh].blocks[i].sector; // Determine the sector from the current block counter 
            block = fhTable[fh].blocks[i].blockNum; // Determine the block from the current block counter
            dev_Num = fhTable[fh].blocks[i].device; // Determine the device from the current block counter


            cacheCheck = lcloud_getcache(dev_Num, sector, block);

            if(cacheCheck != NULL){ // Check if the desired block is in the cache
                memcpy(buf_256, cacheCheck, 256); // Copy over the cache data into the buf_256 value
            } else {
                instructionFrame = create_lcloud_registers(0, 0, LC_BLOCK_XFER, dev_Num, LC_XFER_READ, block, sector); //Pack the instruction frame

                resultFrame = client_lcloud_bus_request(instructionFrame, buf_256); // Call the io bus with the instruction and save the result in the result frame

                // Checks to make sure that the operation was successful
                if ( (instructionFrame == -1) || (resultFrame == -1) ||
                    (extract_lcloud_registers(resultFrame, &b0, &b1, &c0, &c1, &c2, &d0, &d1)) ||
                    (b0 != 1) || (b1 != 1) || (c0 != LC_BLOCK_XFER) ) {
                    logMessage( LOG_ERROR_LEVEL, "Failure to read an entire block.");
                    return( -1 );
                    }

                // In the case that it is not in the cache, we need to put it into the cache
                lcloud_putcache(dev_Num, sector, block, buf_256); 
            
            }

            memcpy(&buf[bytesRead], buf_256, 256); // Copy over the bytes from the io call into the passed buffer from the function call

            bytesRead += 256; // Increase the bytes read by 256 
            len -= 256; // Decreases the length by 256 
            i++; // Increment the block counter
            fhTable[fh].position += 256; // Move the position to the end of the read

        } //Maybe include the case of reading over 4096 bytes here 
        else if (len > 256 && fhTable[fh].blocks[i+1].blockNum == -1) { // Case where we read over the length of the file

            sector = fhTable[fh].blocks[i].sector; // Determine the sector from the current block counter 
            block = fhTable[fh].blocks[i].blockNum; // Determine the block from the current block counter
            dev_Num = fhTable[fh].blocks[i].device; // Determine the device from the current block counter

            instructionFrame = create_lcloud_registers(0, 0, LC_BLOCK_XFER, dev_Num, LC_XFER_READ, block, sector); //Pack the instruction frame

            resultFrame = client_lcloud_bus_request(instructionFrame, buf_256); // Call the io bus with the instruction and save the result in the result frame

            // Checks to make sure that the operation was successful
            if ( (instructionFrame == -1) || (resultFrame == -1) ||
                (extract_lcloud_registers(resultFrame, &b0, &b1, &c0, &c1, &c2, &d0, &d1)) ||
                (b0 != 1) || (b1 != 1) || (c0 != LC_BLOCK_XFER) ) {
                logMessage( LOG_ERROR_LEVEL, "Failure to read when going over the max of the file");
                return( -1 );
                }

            memcpy(&buf[bytesRead], buf_256, fhTable[fh].size%256); // Copy over the bytes from the io call into the passed buffer from the function call

            bytesRead += fhTable[fh].size%256; // Increase the bytes read by 256 
            fhTable[fh].position += fhTable[fh].size%256; // Move the position to the end of the read
            break; // break to stop the loop
        } else { // Case where the next block to be read is less than the entire block (We are reading the final block)

            sector = fhTable[fh].blocks[i].sector; // Determine the sector from the current block counter 
            block = fhTable[fh].blocks[i].blockNum; // Determine the block from the current block counter
            dev_Num = fhTable[fh].blocks[i].device; // Determine the device from the current block counter

            cacheCheck = lcloud_getcache(dev_Num, sector, block);

            if(cacheCheck != NULL){ // Check if the desired block is in the cache
                memcpy(buf_256, cacheCheck, 256); // Copy over the cache data into the buf_256 value
            } else {
                instructionFrame = create_lcloud_registers(0, 0, LC_BLOCK_XFER, dev_Num, LC_XFER_READ, block, sector); //Pack the instruction frame

                resultFrame = client_lcloud_bus_request(instructionFrame, buf_256); // Call the io bus with the instruction and save the result in the result frame

                // Checks to make sure that the operation was successful
                if ( (instructionFrame == -1) || (resultFrame == -1) ||
                    (extract_lcloud_registers(resultFrame, &b0, &b1, &c0, &c1, &c2, &d0, &d1)) ||
                    (b0 != 1) || (b1 != 1) || (c0 != LC_BLOCK_XFER) ) {
                    logMessage( LOG_ERROR_LEVEL, "Failure to read an entire block.");
                    return( -1 );
                    }

                // In the case that it is not in the cache, we need to put it into the cache
                lcloud_putcache(dev_Num, sector, block, buf_256); 
            
            }

            memcpy(&buf[bytesRead], &buf_256[fhTable[fh].position%256], len%256); // Copy over the bytes from the io call into the passed buffer from the function call

            bytesRead += len%256; // Increase the blocks read by what is left of len
            fhTable[fh].position += len%256; // Move the position to the end of the read
            len -= len%256; // Set len to 0 to indicate that we are done reading
            
            }
    }

    return( bytesRead ); // Returns the number of bytes since the function was successful
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : lcwrite
// Description  : write data to the file
//
// Inputs       : fh - file handle for the file to write to
//                buf - pointer to data to write
//                len - the length of the write
// Outputs      : number of bytes written if successful test, -1 if failure

int lcwrite( LcFHandle fh, char *buf, size_t len ) {

    //logMessage( LOG_ERROR_LEVEL, "Bruh");

    //logMessage( LOG_ERROR_LEVEL, "LC failure writing blkc [%d/%d/%s]. (2)", fhTable[fh].position, fhTable[fh].size, fhTable[fh].name );

    block nextBlock;
    int bytesWrote = 0; // Variable to track the number of bytes read
    char buf_256[256]; //256 byte buffer to be used for receiving data from the io bus
    LCloudRegisterFrame instructionFrame; // Register frame to be used to pack instructions into
    LCloudRegisterFrame resultFrame; // Register frame to be used to store the result of the io bus call
    int sector; // Sector value to be used in the io bus call
    int currentBlock; // Block number to be used in the io bus call
    int tempPosition; // Value to temporarily hold the position so it can be changed
    int dev_Num;

    // Checks if the file is open
    if(fhTable[fh].open == 0){
         logMessage( LOG_ERROR_LEVEL, "File not open.");
                return( -1 );   
    }

    // Creates int variables for each of the register components to be used for error checking when extracting the result register
    uint64_t b0, b1, c0, c1, c2, d0, d1; 

    while(len >= 256){
        //Case we are writing starting partway in a block
        if((fhTable[fh].position + bytesWrote)%256 != 0){
            
            // Gets the block we need to write into, the last block in the blocks list
            nextBlock = fhTable[fh].blocks[(int)(fhTable[fh].position/256)];

            // Save the current position to be restored after
            tempPosition = fhTable[fh].position;

            // Change the position to the start of the block 
            lcseek(fh, (fhTable[fh].position - fhTable[fh].position%256));

            //Now we read the contents of that block and copy them into buf_256
            lcread(fh, buf_256, 256);

            // Restore the position from before copying over the block data
            fhTable[fh].position = tempPosition;

            // Set the block variables to be the location to be used
            currentBlock = nextBlock.blockNum;
            sector = nextBlock.sector;
            dev_Num = nextBlock.device;

            //Get the memory to write into the buffer to be used in the instruction from the passed argument buffer
            memcpy(&buf_256[fhTable[fh].position%256], &buf[bytesWrote], (256-fhTable[fh].position%256));
            
            /* Do the write operation, check result (From PDM)*/
            instructionFrame = create_lcloud_registers(0, 0, LC_BLOCK_XFER, dev_Num, LC_XFER_WRITE, currentBlock, sector );
            if ( (instructionFrame == -1) || ((resultFrame = client_lcloud_bus_request(instructionFrame, buf_256)) == -1) ||
            (extract_lcloud_registers(resultFrame, &b0, &b1, &c0, &c1, &c2, &d0, &d1)) ||
            (b0 != 1) || (b1 != 1) || (c0 != LC_BLOCK_XFER) ) {
                logMessage( LOG_ERROR_LEVEL, "LC failure writing blkc [%d/%d/%d] (1).", dev_Num, sector, currentBlock );
                return( -1 );
            }

            // Update the cache
            lcloud_putcache(dev_Num, sector, currentBlock, buf_256);

            // Update status variable to reflect a succesful write
            bytesWrote += (256-fhTable[fh].position%256);
            len -= (256-fhTable[fh].position%256);
            
            // If we increased the total size of the file, we have to update it
            if(fhTable[fh].size < fhTable[fh].position + bytesWrote){

                fhTable[fh].size = fhTable[fh].position + bytesWrote;
            }


        } else if((fhTable[fh].position + bytesWrote)%256 == 0) { // Case where we are writing at the start of a block

            if((fhTable[fh].position + bytesWrote)>=fhTable[fh].size){
                // Gets an unused block to be written into
                nextBlock = get_Next_Block();
            }
            else{
                // Gets the block at that position
                nextBlock = fhTable[fh].blocks[(int)((fhTable[fh].position+bytesWrote)/256)];
            }
            

            // Returns an error since there are no avaliable blocks
            if(nextBlock.sector == -1){
               return(-1); 
            }

            // Set the block variables to be the location to be used
            currentBlock = nextBlock.blockNum;
            sector = nextBlock.sector;
            dev_Num = nextBlock.device;

            //logMessage( LOG_ERROR_LEVEL, "LC failure writing blkc [%d/%d/%d]. (2)", dev_Num, sector, currentBlock );

            //Get the memory to write into the buffer to be used in the instruction from the passed argument buffer
            memcpy(buf_256, &buf[bytesWrote], 256);
            
            /* Do the write operation, check result (From PDM)*/
            instructionFrame = create_lcloud_registers(0, 0, LC_BLOCK_XFER, dev_Num, LC_XFER_WRITE, currentBlock, sector );
            if ( (instructionFrame == -1) || ((resultFrame = client_lcloud_bus_request(instructionFrame, buf_256)) == -1) ||
            (extract_lcloud_registers(resultFrame, &b0, &b1, &c0, &c1, &c2, &d0, &d1)) ||
            (b0 != 1) || (b1 != 1) || (c0 != LC_BLOCK_XFER) ) {
                logMessage( LOG_ERROR_LEVEL, "LC failure writing blkc [%d/%d/%d]. (2)", dev_Num, sector, currentBlock );
                return( -1 );
            }

            // Update the cache
            lcloud_putcache(dev_Num, sector, currentBlock, buf_256);

            fhTable[fh].blocks[(int)((fhTable[fh].position+bytesWrote)/256)] = nextBlock; // Add the block to the list of blocks in the file

            // Update status variable to reflect a succesful write
            bytesWrote += 256;
            len -= 256;

            *(devOn[dev_Num].usedBlocks + sector*devOn[dev_Num].blocks + currentBlock) = 1; // Makes it so the block is counted as used
            
            // If we increased the total size of the file, we have to update it
            if(fhTable[fh].size < fhTable[fh].position + bytesWrote){

                fhTable[fh].size = fhTable[fh].position + bytesWrote;
            }
            
        }
    }

    // Last Case for writing the final block if len is not 0
    if(len > 0){
        if((fhTable[fh].position + bytesWrote)%256 == 0) { // Case where we are writing at the start of a block
            
            if((fhTable[fh].position + bytesWrote)>=fhTable[fh].size){
                // Gets an unused block to be written into
                nextBlock = get_Next_Block();
            }
            else{
                // Gets the block at that position
                nextBlock = fhTable[fh].blocks[(int)((fhTable[fh].position+bytesWrote)/256)];
            }

            // Returns an error since there are no avaliable blocks
            if(nextBlock.sector == -1){
               return(-1); 
            }

            // Set the block variables to be the location to be used
            currentBlock = nextBlock.blockNum;
            sector = nextBlock.sector;
            dev_Num = nextBlock.device;

            if((fhTable[fh].position + bytesWrote)<fhTable[fh].size){
                // Copy the contents of hte current block first

                // Save the current position to be restored after
                tempPosition = fhTable[fh].position;

                // Change the position to the start of the block 
                lcseek(fh, (fhTable[fh].position + bytesWrote));

                //Now we read the contents of that block and copy them into buf_256
                lcread(fh, buf_256, 256);

                // Restore the position from before copying over the block data
                fhTable[fh].position = tempPosition;
            }

            //Get the memory to write into the buffer to be used in the instruction from the passed argument buffer
            memcpy(buf_256, &buf[bytesWrote], len);
            
            /* Do the write operation, check result (From PDM)*/
            instructionFrame = create_lcloud_registers(0, 0, LC_BLOCK_XFER, dev_Num, LC_XFER_WRITE, currentBlock, sector);
            if ( (instructionFrame == -1) || ((resultFrame = client_lcloud_bus_request(instructionFrame, buf_256)) == -1) ||
            (extract_lcloud_registers(resultFrame, &b0, &b1, &c0, &c1, &c2, &d0, &d1)) ||
            (b0 != 1) || (b1 != 1) || (c0 != LC_BLOCK_XFER) ) {
                logMessage( LOG_ERROR_LEVEL, "LC failure writing blkc [%d/%d/%d]. (3)", dev_Num, sector, currentBlock );
                return( -1 );
            }

            // Update the cache
            lcloud_putcache(dev_Num, sector, currentBlock, buf_256);

            fhTable[fh].blocks[(int)((fhTable[fh].position+bytesWrote)/256)] = nextBlock; // Add the block to the list of blocks in the file

            // Update status variable to reflect a succesful write
            bytesWrote += len;
            len -= len;

            *(devOn[dev_Num].usedBlocks + sector*devOn[dev_Num].blocks + currentBlock) = 1; // Makes it so the block is counted as used
            
            // If we increased the total size of the file, we have to update it
            if(fhTable[fh].size < fhTable[fh].position + bytesWrote){

                fhTable[fh].size = fhTable[fh].position + bytesWrote;
            }


        } else { // We are not writing at the start of a block
            if(len > (256 - (fhTable[fh].position+bytesWrote)%256)){ // Case where we are going to need a new block

                // Gets the block we need to write into, the last block in the blocks list
                nextBlock = fhTable[fh].blocks[(int)((fhTable[fh].position + bytesWrote)/256)];

                // Save the current position to be restored after
                tempPosition = fhTable[fh].position;

                // Change the position to the start of the block 
                lcseek(fh, (fhTable[fh].position + bytesWrote - fhTable[fh].position%256));

                //Now we read the contents of that block and copy them into buf_256
                lcread(fh, buf_256, 256);

                // Restore the position from before copying over the block data
                fhTable[fh].position = tempPosition;

                // Set the block variables to be the location to be used
                currentBlock = nextBlock.blockNum;
                sector = nextBlock.sector;
                dev_Num = nextBlock.device;

                //Get the memory to write into the buffer to be used in the instruction from the passed argument buffer
                memcpy(&buf_256[fhTable[fh].position%256], &buf[bytesWrote], (256-fhTable[fh].position%256));
                
                /* Do the write operation, check result (From PDM)*/
                instructionFrame = create_lcloud_registers(0, 0, LC_BLOCK_XFER, dev_Num, LC_XFER_WRITE, currentBlock, sector );
                if ( (instructionFrame == -1) || ((resultFrame = client_lcloud_bus_request(instructionFrame, buf_256)) == -1) ||
                (extract_lcloud_registers(resultFrame, &b0, &b1, &c0, &c1, &c2, &d0, &d1)) ||
                (b0 != 1) || (b1 != 1) || (c0 != LC_BLOCK_XFER) ) {
                    logMessage( LOG_ERROR_LEVEL, "LC failure writing blkc [%d/%d/%d]. (4)", dev_Num, sector, currentBlock );
                    return( -1 );
                }

                // Update the cache
                lcloud_putcache(dev_Num, sector, currentBlock, buf_256);

                // Update status variable to reflect a succesful write
                bytesWrote += (256-fhTable[fh].position%256);
                len -= (256-fhTable[fh].position%256);
                
                // If we increased the total size of the file, we have to update it
                if(fhTable[fh].size < fhTable[fh].position + bytesWrote){

                    fhTable[fh].size = fhTable[fh].position + bytesWrote;
                }    

                //Now we finish the write in the next block

                if((fhTable[fh].position + bytesWrote)>=fhTable[fh].size){
                // Gets an unused block to be written into
                nextBlock = get_Next_Block();
                }
                else{
                    // Gets the block at that position
                    nextBlock = fhTable[fh].blocks[(int)((fhTable[fh].position+bytesWrote)/256)];
                }

                // Returns an error since there are no avaliable blocks
                if(nextBlock.sector == -1){
                return(-1); 
                }

                // Set the block variables to be the location to be used
                currentBlock = nextBlock.blockNum;
                sector = nextBlock.sector;
                dev_Num = nextBlock.device;

                if((fhTable[fh].position + bytesWrote)<fhTable[fh].size){
                    // Copy the contents of hte current block first

                    // Save the current position to be restored after
                    tempPosition = fhTable[fh].position;

                    // Change the position to the start of the block 
                    lcseek(fh, (fhTable[fh].position + bytesWrote));

                    //Now we read the contents of that block and copy them into buf_256
                    lcread(fh, buf_256, 256);

                    // Restore the position from before copying over the block data
                    fhTable[fh].position = tempPosition;
                }

                //Get the memory to write into the buffer to be used in the instruction from the passed argument buffer
                memcpy(buf_256, &buf[bytesWrote], len);
                
                /* Do the write operation, check result (From PDM)*/
                instructionFrame = create_lcloud_registers(0, 0, LC_BLOCK_XFER, dev_Num, LC_XFER_WRITE, currentBlock, sector );
                if ( (instructionFrame == -1) || ((resultFrame = client_lcloud_bus_request(instructionFrame, buf_256)) == -1) ||
                (extract_lcloud_registers(resultFrame, &b0, &b1, &c0, &c1, &c2, &d0, &d1)) ||
                (b0 != 1) || (b1 != 1) || (c0 != LC_BLOCK_XFER) ) {
                    logMessage( LOG_ERROR_LEVEL, "LC failure writing blkc [%d/%d/%d]. (5)", dev_Num, sector, currentBlock );
                    return( -1 );
                }

                // Update the cache
                lcloud_putcache(dev_Num, sector, currentBlock, buf_256);

                fhTable[fh].blocks[(int)((fhTable[fh].position+bytesWrote)/256)] = nextBlock; // Add the block to the list of blocks in the file

                // Update status variable to reflect a succesful write
                bytesWrote += len;
                len -= len;

                // Makes it so the block is counted as used
                *(devOn[dev_Num].usedBlocks + sector*devOn[dev_Num].blocks + currentBlock) = 1;
                
                // If we increased the total size of the file, we have to update it
                if(fhTable[fh].size < fhTable[fh].position + bytesWrote){

                    fhTable[fh].size = fhTable[fh].position + bytesWrote;
                }


            } else { // Case where we only need to finish writing in the current block
                // Gets the block we need to write into, the last block in the blocks list
                nextBlock = fhTable[fh].blocks[(int)(fhTable[fh].position/256)];

                // Save the current position to be restored after
                tempPosition = fhTable[fh].position;

                // Change the position to the start of the block 
                lcseek(fh, (fhTable[fh].position - fhTable[fh].position%256));

                //Now we read the contents of that block and copy them into buf_256
                lcread(fh, buf_256, 256);

                // Restore the position from before copying over the block data
                fhTable[fh].position = tempPosition;

                // Set the block variables to be the location to be used
                currentBlock = nextBlock.blockNum;
                sector = nextBlock.sector;
                dev_Num = nextBlock.device;

                //Get the memory to write into the buffer to be used in the instruction from the passed argument buffer
                memcpy(&buf_256[fhTable[fh].position%256], &buf[bytesWrote], len);
                
                /* Do the write operation, check result (From PDM)*/
                instructionFrame = create_lcloud_registers(0, 0, LC_BLOCK_XFER, dev_Num, LC_XFER_WRITE, currentBlock, sector );
                if ( (instructionFrame == -1) || ((resultFrame = client_lcloud_bus_request(instructionFrame, buf_256)) == -1) ||
                (extract_lcloud_registers(resultFrame, &b0, &b1, &c0, &c1, &c2, &d0, &d1)) ||
                (b0 != 1) || (b1 != 1) || (c0 != LC_BLOCK_XFER) ) {
                    logMessage( LOG_ERROR_LEVEL, "LC failure writing blkc [%d/%d/%d]. (6)", dev_Num, sector, currentBlock );
                    return( -1 );
                }

                // Update the cache
                lcloud_putcache(dev_Num, sector, currentBlock, buf_256);

                // Update status variable to reflect a succesful write
                bytesWrote += len;
                len = 0;
                
                // If we increased the total size of the file, we have to update it
                if(fhTable[fh].size < fhTable[fh].position + bytesWrote){

                    fhTable[fh].size = fhTable[fh].position + bytesWrote;
                }

            }
        }
    }

    //Update the position by the number of bytes wrote
    fhTable[fh].position += bytesWrote;

    return( bytesWrote ); // Returns the number of bytes wrote because the test was successful
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : lcseek
// Description  : Seek to a specific place in the file
//
// Inputs       : fh - the file handle of the file to seek in
//                off - offset within the file to seek to
// Outputs      : 0 if successful test, -1 if failure

int lcseek( LcFHandle fh, size_t off ) {
    // Locate the file using the file handle
    // (Maybe Assign 3) file * filePointer = *(fhTable + fh*sizeof(file*));

    //Check if the file is open
    if(fhTable[fh].open == 0){
        logMessage( LOG_ERROR_LEVEL, "The file is is not open and cannot be used by the seek function.");
        return( -1 ); // Return -1 for an error since the file was not open
    }

    // Check if len is less than or equal to the length of the file
    if(fhTable[fh].size < off){
        logMessage( LOG_ERROR_LEVEL, "The file is too short for the seek location.");
        return( -1 ); // Return -1 for an error since the offset was greater than the length of the file
    }

    //set the position value in the file struct to be off
    fhTable[fh].position = off;

    return( off ); //Return 0 for success
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : lcclose
// Description  : Close the file
//
// Inputs       : fh - the file handle of the file to close
// Outputs      : 0 if successful test, -1 if failure

int lcclose( LcFHandle fh ) {

    if(fh >= fileHandleCounter || fhTable[fh].open == 0){
        logMessage( LOG_ERROR_LEVEL, "The file handle was not valid or the file was not open.");
        return( -1 ); // Return -1 for an error since the file is not open or the file handle was invalid
    }

    // Changes the open variable in the file to be 0
    fhTable[fh].open = 0;


    return( 0 );
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : lcshutdown
// Description  : Shut down the filesystem
//
// Inputs       : none
// Outputs      : 0 if successful test, -1 if failure

int lcshutdown( void ) {

    // Closes all of the open files in the open files list
    int i;
    for(i = 0; i<fileHandleCounter; i++){
        fhTable[i].open = 0;

    }

    for(i = 0; i<16; i++){
        free(devOn[i].usedBlocks);
    }

    // Frees the table of all file handles
    free(fhTable);


    // Packs the instruction for shutting down the device into the instruction LCloudRegisterFrame
    LCloudRegisterFrame instructionFrame = create_lcloud_registers(0, 0, LC_POWER_OFF, 0, 0, 0, 0);

    //Sends the instruction to the io bus and sets the returned frame into the result variable
    LCloudRegisterFrame resultFrame = client_lcloud_bus_request(instructionFrame, NULL);

    // Creates int variables for each of the register components to be used for error checking when extracting the result register
    uint64_t b0, b1, c0, c1, c2, d0, d1; 
    
    //Checks the result frame for any error and returns a value of -1 if there was some failure as well as prints log message indication a shutdown error
    if ( (instructionFrame == -1) || (resultFrame == -1) ||
        (extract_lcloud_registers(resultFrame, &b0, &b1, &c0, &c1, &c2, &d0, &d1)) ||
        (b0 != 1) || (b1 != 1) || (c0 != LC_POWER_OFF) ) {
        logMessage( LOG_ERROR_LEVEL, "LC failure shutting down device");
        return( -1 );
        }

    // Close the cache
    lcloud_closecache();

    // Otherwise returns 0 for a successful test
    return( 0 );
}
