////////////////////////////////////////////////////////////////////////////////
//
//  File           : lcloud_client.c
//  Description    : This is the network API for the LionCloud
//                   assignment for CMPSC311.
//
//   Author        : Samuel Johnson
//   Last Modified : 4/30/2020
//

// Includes 
#include <stdint.h>
#include <lcloud_controller.h>

//
// Functional Prototypes

LCloudRegisterFrame client_lcloud_bus_request( uint64_t reg, void *buf );
    //Send stuff over the network