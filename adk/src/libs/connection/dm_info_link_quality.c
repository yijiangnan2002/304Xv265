/****************************************************************************
Copyright (c) 2004 - 2019 Qualcomm Technologies International, Ltd.


FILE NAME
    dm_info_link_quality.c        

DESCRIPTION
    This file contains the management entity responsible for arbitrating 
    access to functionality in BlueStack that provides information on
    the setup of the local device or about the link to the remote device.

NOTES

*/


/****************************************************************************
    Header files
*/
#include "connection.h"
#include "connection_private.h"


/*****************************************************************************/
void ConnectionGetLinkQuality(Task theAppTask, Sink sink)
{
    /* All requests are sent through the internal state handler */    
    MAKE_CL_MESSAGE(CL_INTERNAL_DM_READ_LINK_QUALITY_REQ);
    message->theAppTask = theAppTask;
    message->sink = sink;
    /* message->bd_addr does not need to be set. */
    MessageSend(connectionGetCmTask(), CL_INTERNAL_DM_READ_LINK_QUALITY_REQ, message);
}


/*****************************************************************************/
void ConnectionGetLinkQualityBdaddr(Task theAppTask, const bdaddr *bd_addr)
{
    /* All requests are sent through the internal state handler */
    MAKE_CL_MESSAGE(CL_INTERNAL_DM_READ_LINK_QUALITY_REQ);
    message->theAppTask = theAppTask;
    message->sink = (Sink)NULL; /* make sink to null to use bd_addr instead */
    message->bd_addr = *bd_addr;
    MessageSend(connectionGetCmTask(), CL_INTERNAL_DM_READ_LINK_QUALITY_REQ, message);
}
