/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation of case comms testing functions
*/

#ifdef INCLUDE_CASE_COMMS

#include "case_comms_test.h"
#include <cc_protocol.h>

#include <logging.h>
#include <panic.h>
#include <stdlib.h>

#ifdef GC_SECTIONS
/* Move all functions in KEEP_PM section to ensure they are not removed during
 * garbage collection */
#pragma unitcodesection KEEP_PM
#endif

/* State held by case comms test. */
typedef struct
{
     uint8* cc_test_buffer;
     uint16 cc_test_buffer_data_len;
     cc_tx_status_t cc_test_tx_status;
     uint16 max_msg_len;
} case_comms_test_t;
case_comms_test_t* case_comms_test = NULL;

static bool CaseCommsTest_IsInitialised(void)
{
    if (case_comms_test == NULL)
    {
        DEBUG_LOG_ALWAYS("CaseCommsTest_IsInitialised not initialised");
        return FALSE;
    }

    return TRUE;
}

static void CaseCommsTest_HandleTxStatus(cc_tx_status_t status, unsigned mid)
{
    /* no need for initialisation protection, can only be called if initialised and the test CID
       is registered */
    DEBUG_LOG_ALWAYS("CaseCommsTest_HandleTxStatus mid:%d enum:cc_tx_status_t:%d", mid, status);
    case_comms_test->cc_test_tx_status = status;
}

static void CaseCommsTest_HandleRxInd(unsigned mid, const uint8* msg, unsigned length, cc_dev_t source_dev)
{
    /* no need for initialisation protection, can only be called if initialised and the test CID
       is registered */
    DEBUG_LOG_ALWAYS("CaseCommsTest_HandleRxInd rx %d byte on mid:%d from enum:cc_dev_t:%d", length, mid, source_dev);
    case_comms_test->cc_test_buffer_data_len = length <= case_comms_test->max_msg_len ? length : case_comms_test->max_msg_len;
    memcpy(case_comms_test->cc_test_buffer, msg, case_comms_test->cc_test_buffer_data_len);
}

void CaseCommsTest_Init(unsigned max_msg_len)
{
    cc_chan_config_t cfg;
    
    DEBUG_LOG_ALWAYS("CaseCommsTest_Init");

    if (!case_comms_test)
    {
        case_comms_test = PanicUnlessMalloc(sizeof(case_comms_test_t));
        case_comms_test->max_msg_len = max_msg_len;
        case_comms_test->cc_test_buffer = PanicUnlessMalloc(max_msg_len);
        case_comms_test->cc_test_buffer_data_len = 0;
        case_comms_test->cc_test_tx_status = CASECOMMS_STATUS_UNKNOWN;

        /* register for the case comms test channel */
        cfg.cid = CASECOMMS_CID_TEST;
        cfg.tx_sts = CaseCommsTest_HandleTxStatus;
        cfg.rx_ind = CaseCommsTest_HandleRxInd;
        CcProtocol_RegisterChannel(&cfg);
    }
}

bool CaseCommsTest_TxMsg(cc_dev_t dest, unsigned mid, uint8* msg, uint16 len, bool expect_response)
{
    bool tx_result = FALSE;

    if (CaseCommsTest_IsInitialised())
    {
        /* reset transmit status */
        case_comms_test->cc_test_tx_status = CASECOMMS_STATUS_UNKNOWN;

        /* transmit message */
        if (expect_response)
        {
            tx_result = CcProtocol_Transmit(dest, CASECOMMS_CID_TEST, mid, msg, len);
        }
        else
        {
            tx_result = CcProtocol_TransmitNotification(dest, CASECOMMS_CID_TEST, mid, msg, len);
        }

        DEBUG_LOG_ALWAYS("CaseCommsTest_TxMsg %d bytes to enum:cc_dev_t:%d mid:%d resp:%d sts:%d", len, dest, mid, expect_response, tx_result);
    }

    return tx_result;
}

uint16 CaseCommsTest_RxMsg(uint8* msg)
{
    uint16 len = 0;

    if (CaseCommsTest_IsInitialised())
    {
        len = case_comms_test->cc_test_buffer_data_len;

        DEBUG_LOG_ALWAYS("CaseCommsTest_GetRxMsg len:%d", len);

        memcpy(msg, case_comms_test->cc_test_buffer, len);
        case_comms_test->cc_test_buffer_data_len = 0;
    }

    return len;
}

uint16 CaseCommsTest_PollRxMsgLen(void)
{
    uint16 len = 0;

    if (CaseCommsTest_IsInitialised())
    {
        DEBUG_LOG_ALWAYS("CaseCommsTest_PollRxMsgLen len:%d", case_comms_test->cc_test_buffer_data_len);
        len = case_comms_test->cc_test_buffer_data_len;
    }

    return len;
}

cc_tx_status_t CaseCommsTest_PollTxStatus(void)
{
    cc_tx_status_t sts = CASECOMMS_STATUS_UNKNOWN;

    if (CaseCommsTest_IsInitialised())
    {
        DEBUG_LOG_ALWAYS("CaseCommsTest_GetTxStatus enum:cc_tx_status_t:%d", case_comms_test->cc_test_tx_status);
        sts = case_comms_test->cc_test_tx_status;
    }

    return sts;
}

#endif /* INCLUDE_CASE_COMMS */
