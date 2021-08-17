/*!
\copyright  Copyright (c) 2019-2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Header file for the init functions in the PEER PAIRING OVER LE service
*/

#ifndef PEER_PAIR_LE_INIT_H_
#define PEER_PAIR_LE_INIT_H_

#include "peer_pair_le_private.h"

/*! Initialise the peer service 

    \param ear_role Function used to determine if this device is Left or Right
*/
void peer_pair_le_init(ear_function ear_role);

/*! Start using the service, eg. Allocates memory for the peer service. */
void peer_pair_le_start_service(void);


/*! Disconnect LE link (if any) */
void peer_pair_le_disconnect(void);


/*! Stop using the service, eg. Deallocates memory for the peer service */
void peer_pair_le_stop_service(void);

/*! Release the override of local address (if set) 

    The local address module is overriden if pairing by address.
    so that public addresses are used. */
void peer_pair_le_release_local_addr_override(void);

/*! Empty a peerPairLeFoundDevice structure */
void PeerPairLe_DeviceSetEmpty(peerPairLeFoundDevice *device);

/*! Empty all peerPairLeFoundDevice structures */
void PeerPairLe_DeviceSetAllEmpty(void);

#endif /* PEER_PAIR_LE_INIT_H_ */
