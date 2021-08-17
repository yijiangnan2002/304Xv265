/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    case_comms
\brief      Case comms protocol configuration.
*/
/*! \addtogroup case_comms
@{
*/

#ifndef CC_PROTOCOL_CONFIG_H
#define CC_PROTOCOL_CONFIG_H

#ifdef INCLUDE_CASE_COMMS

/*! Definition of the PIO to use for TX and RX in Scheme B single wire UART chargercomms.
    \note This must be an LED PIO.
*/
#define CcProtocol_ConfigSchemeBTxRxPio()   (CHIP_LED_BASE_PIO + 4)

/*! Time to wait before sending poll to an Earbud to get outstanding response message. */
#define CcProtocol_ConfigPollScheduleTimeoutMs()   (1)

/*! Number of transmit failures before deciding to transmit a broadcast reset. */
#define CcProtocol_ConfigNumFailsToReset()          (1)

#endif /* INCLUDE_CASE_COMMS */
#endif  /* CC_PROTOCOL_CONFIG_H */
/*! @} End of group documentation */
