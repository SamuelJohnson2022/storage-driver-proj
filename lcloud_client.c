////////////////////////////////////////////////////////////////////////////////
//
//  File          : lcloud_client.c
//  Description   : This is the client side of the Lion Clound network
//                  communication protocol.
//
//  Author        : Patrick McDaniel
//  Last Modified : Sat 28 Mar 2020 09:43:05 AM EDT
//

// Include Files
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <stdint.h>

// Project Include Files
#include <lcloud_network.h>
#include <cmpsc311_log.h>
#include <cmpsc311_util.h>
#include <lcloud_filesys.h>

//Global variables
//Initialize the socket handle to -1
int socket_handle = -1;

//
// Functions

////////////////////////////////////////////////////////////////////////////////
//
// Function     : client_lcloud_bus_request
// Description  : This the client regstateeration that sends a request to the 
//                lion client server.   It will:
//
//                1) if INIT make a connection to the server
//                2) send any request to the server, returning results
//                3) if CLOSE, will close the connection
//
// Inputs       : reg - the request reqisters for the command
//                buf - the block to be read/written from (READ/WRITE)
// Outputs      : the response structure encoded as needed

LCloudRegisterFrame client_lcloud_bus_request( LCloudRegisterFrame reg, void *buf ) {

    struct sockaddr_in caddr;
    LCloudRegisterFrame networkFrame;
    LCloudRegisterFrame resultFrame;

    if(socket_handle == -1){ //If the socket is not open
        //Create the address
        caddr.sin_family = AF_INET;
        caddr.sin_port = htons(LCLOUD_DEFAULT_PORT);
        if(inet_aton(LCLOUD_DEFAULT_IP, &caddr.sin_addr) == 0){
            return(-1); //Return an error
        }

        //Create the socket
        socket_handle = socket(PF_INET, SOCK_STREAM, 0);

        // Connect with the socket
        if(connect(socket_handle, (const struct sockaddr *)&caddr, sizeof(caddr)) == -1){
            return(-1); //Didnt connect properly
        }

    }

    // Creates int variables for each of the register components to be used for error checking when extracting the result register
    uint64_t b0, b1, c0, c1, c2, d0, d1;

    //extract the op code
    extract_lcloud_registers(reg, &b0, &b1, &c0, &c1, &c2, &d0, &d1);

    if(c0 == LC_BLOCK_XFER && c2 == LC_XFER_READ){ //Case where we are performing a read operation
        //Convert to network byte order
        networkFrame = htonll64(reg);

        //Send the reg
        // write(socket_handle, (char *)&networkFrame, 8);
        if(write(socket_handle, (char *)&networkFrame, 8) != 8){
            logMessage( LOG_ERROR_LEVEL, "Network Error.");
            return(-1); //Error in the number of bytes written
        }

        //Read to recieve the resulting frame and convert it to host order
        //read(socket_handle, (char *)&resultFrame, 8);
        if(read(socket_handle, (char *)&resultFrame, 8) != 8){
            logMessage( LOG_ERROR_LEVEL, "Network Error.");
            return(-1); //Error in the number of bytes read
        }

        //Read to get the resulting returned value
        //read(socket_handle, buf, LC_DEVICE_BLOCK_SIZE);
        if(read(socket_handle, buf, LC_DEVICE_BLOCK_SIZE) != LC_DEVICE_BLOCK_SIZE){
            logMessage( LOG_ERROR_LEVEL, "Network Error.");
            return(-1); //Error in the number of bytes read
        }
        resultFrame = ntohll64(resultFrame);

        

        //Return the result frame
        return(resultFrame);

    } else if(c0 == LC_BLOCK_XFER && c2 == LC_XFER_WRITE){ //Write operation
        //Convert to network byte order
        networkFrame = htonll64(reg);

        //Send the reg then the buf to be used
        // write(socket_handle, (char *)&networkFrame, 8);
        if(write(socket_handle, (char *)&networkFrame, 8) != 8){
            logMessage( LOG_ERROR_LEVEL, "Network Error.");
            return(-1); //Error in the number of bytes written
        }
        //write(socket_handle, buf, LC_DEVICE_BLOCK_SIZE);
        if(write(socket_handle, buf, LC_DEVICE_BLOCK_SIZE) != LC_DEVICE_BLOCK_SIZE){
            logMessage( LOG_ERROR_LEVEL, "Network Error.");
            return(-1); //Error in the number of bytes written
        }

        //Read to recieve the resulting frame and convert it to host order
        // read(socket_handle, (char *)&resultFrame, 8);
        if(read(socket_handle, (char *)&resultFrame, 8) != 8){
            logMessage( LOG_ERROR_LEVEL, "Network Error.");
            return(-1); //Error in the number of bytes read
        }
        resultFrame = ntohll64(resultFrame);

        //Return the result frame
        return(resultFrame);

    } else if(c0 == LC_POWER_OFF){ //Power off operation
        //Convert to network byte order
        networkFrame = htonll64(reg);

        //Send the reg then the buf to be used
        //write(socket_handle, (char *)&networkFrame, 8);
        if(write(socket_handle, (char *)&networkFrame, 8) != 8){
            logMessage( LOG_ERROR_LEVEL, "Network Error.");
            return(-1); //Error in the number of bytes written
        }
        
        //Read to recieve the resulting frame and convert it to host order
        //read(socket_handle, (char *)&resultFrame, 8);
        if(read(socket_handle, (char *)&resultFrame, 8) != 8){
            logMessage( LOG_ERROR_LEVEL, "Network Error.");
            return(-1); //Error in the number of bytes read
        }
        resultFrame = ntohll64(resultFrame);

        //Close the socket
        close(socket_handle);
        socket_handle = -1;

        //Return the result frame
        return(resultFrame);

    } else { //All other operations

        //Convert to network byte order
        networkFrame = htonll64(reg);

        // write(socket_handle, (char *)&networkFrame, 8);

        // read(socket_handle, (char *)&resultFrame, 8);

        // Send the reg then the buf to be used
        if(write(socket_handle, (char *)&networkFrame, 8) != 8){
            logMessage( LOG_ERROR_LEVEL, "Network Error.");
            return(-1); //Error in the number of bytes written
        }
        
        //Read to recieve the resulting frame and convert it to host order
        if(read(socket_handle, (char *)&resultFrame, 8) != 8){
            logMessage( LOG_ERROR_LEVEL, "Network Error.");
            return(-1); //Error in the number of bytes read
        }
        resultFrame = ntohll64(resultFrame);


        //Return the result frame
        return(resultFrame);
        
    }   

}
