/**************************************************************************//**

 @file     SDDPStatus.h
 @brief    ...
 @section  Platforms Platform(s)
            All
            
******************************************************************************/
#ifndef _SDDP_STATUS_H_
#define _SDDP_STATUS_H_

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////
// Includes
///////////////////////////////////////


///////////////////////////////////////
// Defines
///////////////////////////////////////


///////////////////////////////////////
// Typedefs
///////////////////////////////////////
/**************************************************************************//**
 @enum     SDDPStatus
           SDDP status codes:        
           SDDP_STATUS_SUCCESS - success
           SDDP_STATUS_FATAL_ERROR - global error
           SDDP_STATUS_INVALID_PARAM - invalid parameter or state data provided
           SDDP_STATUS_NETWORK_ERROR - network interface problem
           SDDP_STATUS_TIME_ERROR - time not implemented          
******************************************************************************/
typedef enum 
{
    SDDP_STATUS_SUCCESS         =  0,
    SDDP_STATUS_FATAL_ERROR     = -1,
    SDDP_STATUS_INVALID_PARAM   = -2,
    SDDP_STATUS_NETWORK_ERROR   = -3,
    SDDP_STATUS_TIME_ERROR      = -4,
    SDDP_STATUS_UNINITIALIZED   = -5,
} SDDPStatus;


///////////////////////////////////////
// Functions
///////////////////////////////////////


#ifdef __cplusplus
}
#endif
#endif
