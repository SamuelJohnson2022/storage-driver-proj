////////////////////////////////////////////////////////////////////////////////
//
//  File           : lcloud_cache.c
//  Description    : This is the cache implementation for the LionCloud
//                   assignment for CMPSC311.
//
//   Author        : Patrick McDaniel
//   Last Modified : Thu 19 Mar 2020 09:27:55 AM EDT
//

// Includes 
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <cmpsc311_log.h>
#include <lcloud_cache.h>


typedef struct cacheBlock {
    int device; // device number of the cache block
    int sector; // sector number mof the cache block
    int block; // block number of the cache block
    char data[256]; // Array containing the data of the cache
    int time; // Time which the cache block was recently used
} cacheBlock;

cacheBlock *cache;

int cacheHits; // Number of cache hits
int cacheMisses; // Number of cache misses
int cacheSize; // Size of the cache

int cacheAccess; // Count to keep track of the time accessed

//
// Functions

////////////////////////////////////////////////////////////////////////////////
//
// Function     : lcloud_getcache
// Description  : Search the cache for a block 
//
// Inputs       : did - device number of block to find
//                sec - sector number of block to find
//                blk - block number of block to find
// Outputs      : cache block if found (pointer), NULL if not or failure

char * lcloud_getcache( LcDeviceId did, uint16_t sec, uint16_t blk ) {

    for(int i = 0; i < cacheSize; i++){

        if(cache[i].device == did && cache[i].block == blk && cache[i].sector == sec){ // Check if it is a cache hit

            cache[i].time = cacheAccess; // Update the time of the accessed cache block
            cacheAccess ++; // Increment the cache access time

            cacheHits ++; // Update the cache hits value

            return cache[i].data; // Returns the pointer to the cache data
        }

    }

    cacheMisses ++; // Increment the cache misses counter
    /* Return not found */
    return( NULL );
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : lcloud_putcache
// Description  : Put a value in the cache 
//
// Inputs       : did - device number of block to insert
//                sec - sector number of block to insert
//                blk - block number of block to insert
// Outputs      : 0 if succesfully inserted, -1 if failure

int lcloud_putcache( LcDeviceId did, uint16_t sec, uint16_t blk, char *block ) {

    cacheBlock newBlock; // Block used to put into the cache
    int oldest = cacheAccess; // Keeps track of the oldest cache access it finds
    int oldestIndex; // Keeps track of index of the oldest cache

    // Runs if the cache is full
    if(cacheAccess >= cacheSize - 1){

        // Iterates through the cache to find the oldest access
        for(int i = 0; i < cacheSize; i++){

            if(cache[i].device == did && cache[i].block == blk && cache[i].sector == sec){ // Check if it is a cache hit

                // Update the cache data
                memcpy(cache[i].data, block, 256);
                cache[i].time = cacheAccess; // Saves the time the cache was accessed
                cacheAccess ++; // Increment the cache access counter
                return(0);

            }

            if(cache[i].time < oldest){
                oldest = cache[i].time; // Updates the oldest known access time
                oldestIndex = i; // Saves the index of this value to be the oldest
            }

        }

        // Populate the newBlock with the info to be put into the cache
        newBlock.block = blk;
        newBlock.sector = sec;
        newBlock.device = did;

        memcpy(newBlock.data, block, 256); // Copies over the passed data into the cache

        newBlock.time = cacheAccess; // Saves the time the cache was accessed
        cacheAccess ++; // Increment the cache access counter

        cache[oldestIndex] = newBlock; // Adds the newBlock to the cache
        cacheMisses++; // Count this as a cache miss

        return ( 0 ); // Returns a 0 for success



    } else { // Runs if the cache is not full

        for(int i = 0; i < cacheSize; i++){

            if(cache[i].device == did && cache[i].block == blk && cache[i].sector == sec){ // Check if it is a cache hit

                // Update the cache data
                memcpy(cache[i].data, block, 256);
                cache[i].time = cacheAccess; // Saves the time the cache was accessed
                cacheAccess ++; // Increment the cache access counter
                return(0);

            }

            if(cache[i].block == -1){ // Checks if that cache block is empty

                // Populate the newBlock with the info to be put into the cache
                newBlock.block = blk;
                newBlock.sector = sec;
                newBlock.device = did;

                memcpy(newBlock.data, block, 256); // Copies over the passed data into the cache

                newBlock.time = cacheAccess; // Saves the time the cache was accessed
                cacheAccess ++; // Increment the cache access counter
                cacheMisses ++; //Count this as a cache miss

                cache[i] = newBlock; // Adds the newBlock to the cache

                return 0; // Returns a 0 to indicate a successful placement
            }
        }
    }
    
    /* Return a -1 if we get here somehow */
    return( -1 );
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : lcloud_initcache
// Description  : Initialze the cache by setting up metadata a cache elements.
//
// Inputs       : maxblocks - the max number number of blocks 
// Outputs      : 0 if successful, -1 if failure

int lcloud_initcache( int maxblocks ) {
    cacheAccess = 0; // Set the initial value of cacheAccess to 0

    // Block used to set the values of 
    cacheBlock newBlock;
    newBlock.device = -1;
    newBlock.sector = -1;
    newBlock.block = -1;
    newBlock.time = -1;

    cache = realloc(cache, maxblocks*sizeof(cacheBlock)); // Allocate memory for the cache based on the maxblocks value

    // Initialize all of cache blocks to have a location of -1,-1,-1
    for(int i = 0; i < maxblocks; i++){   
        cache[i] = newBlock;
    }

    cacheHits = 0; // Set the initial value of hits to 0
    cacheMisses = 0; // Set the initial value of misses to 0
    cacheSize = maxblocks;

    /* Return successfully */
    return( 0 );
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : lcloud_closecache
// Description  : Clean up the cache when program is closing
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int lcloud_closecache( void ) {

    float hitRatio;
    hitRatio = (float)cacheHits/(float)(cacheHits + cacheMisses);

    free(cache); // Free the memory allocated to the cache

    printf("\n\nHits: %d | Misses: %d | Hit Ratio : %.2f \n\n", cacheHits, cacheMisses, hitRatio); // Prints out the cache statistics

    /* Return successfully */
    return( 0 );
}