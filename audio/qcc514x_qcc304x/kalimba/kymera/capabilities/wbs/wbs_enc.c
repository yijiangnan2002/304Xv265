/****************************************************************************
 * Copyright (c) 2014 - 2017 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 * \file  wbs_enc.c
 * \ingroup  operators
 *
 *  WBS_ENC operator
 *
 */

/****************************************************************************
Include Files
*/
#include "wbs_private.h"
#include "capabilities.h"

/****************************************************************************
Private Type Definitions
*/

/****************************************************************************
Private Constant Declarations
*/

/** The WBS encoder capability function handler table */
const handler_lookup_struct wbs_enc_handler_table =
{
    wbs_enc_create,           /* OPCMD_CREATE */
    wbs_enc_destroy,          /* OPCMD_DESTROY */
    base_op_start,            /* OPCMD_START */
    base_op_stop,             /* OPCMD_STOP */
    wbs_enc_reset,            /* OPCMD_RESET */
    wbs_enc_connect,          /* OPCMD_CONNECT */
    wbs_enc_disconnect,       /* OPCMD_DISCONNECT */
    wbs_buffer_details,       /* OPCMD_BUFFER_DETAILS */
    wbs_enc_get_data_format,  /* OPCMD_DATA_FORMAT */
    wbs_enc_get_sched_info    /* OPCMD_GET_SCHED_INFO */
};

/* Null terminated operator message handler table */
const opmsg_handler_lookup_table_entry wbs_enc_opmsg_handler_table[] =
    {{OPMSG_COMMON_ID_GET_CAPABILITY_VERSION,              base_op_opmsg_get_capability_version},
    {OPMSG_COMMON_ID_FADEOUT_ENABLE,               wbs_enc_opmsg_enable_fadeout},
    {OPMSG_COMMON_ID_FADEOUT_DISABLE,              wbs_enc_opmsg_disable_fadeout},
    {OPMSG_SCO_SEND_ID_SET_TO_AIR_INFO,            wbs_enc_opmsg_set_to_air_info},
    {OPMSG_WBS_ENC_ID_FORWARD_ALL_KICKS,           wbs_enc_opmsg_forward_all_kicks},
    {0, NULL}};


const CAPABILITY_DATA wbs_enc_cap_data =
    {
        WBS_ENC_CAP_ID,             /* Capability ID */
        0, 1,                           /* Version information - hi and lo parts */
        1, 1,                           /* Max number of sinks/inputs and sources/outputs */
        &wbs_enc_handler_table,         /* Pointer to message handler function table */
        wbs_enc_opmsg_handler_table,    /* Pointer to operator message handler function table */
        wbs_enc_process_data,           /* Pointer to data processing function */
        0,                              /* Reserved */
        sizeof(WBS_ENC_OP_DATA)         /* Size of capability-specific per-instance data */
    };
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_WBS_ENC, WBS_ENC_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_WBS_ENC, WBS_ENC_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */

/** Memory owned by an encoder instance */
const malloc_t_entry wbs_enc_malloc_table[WBS_ENC_MALLOC_TABLE_LENGTH] =
{
    {6, MALLOC_PREFERENCE_NONE, offsetof(sbc_codec, pre_post_proc_struc)},
    {SBC_ANALYSIS_BUFF_LENGTH, MALLOC_PREFERENCE_DM2, offsetof(sbc_codec, analysis_xch1)}
};

const scratch_table wbs_enc_scratch_table =
{
    WBS_DM1_SCRATCH_TABLE_LENGTH,
    SBC_ENC_DM2_SCRATCH_TABLE_LENGTH,
    0,
    wbs_scratch_table_dm1,
    sbc_scratch_table_dm2,
    NULL
};

/****************************************************************************
Private Function Definitions
*/
static inline WBS_ENC_OP_DATA *get_instance_data(OPERATOR_DATA *op_data)
{
    return (WBS_ENC_OP_DATA *) base_op_get_instance_data(op_data);
}

/* ******************************* Helper functions ************************************ */


/* initialise various working data params of the specific operator */
static void wbs_enc_reset_working_data(OPERATOR_DATA *op_data)
{
    WBS_ENC_OP_DATA* x_data = get_instance_data(op_data);

/*    sco_common_tx_reset_working_data(op_data);
 * TODO this could be common with sco_send see B-150926 */
    if(x_data != NULL)
    {
        /* Initialise fadeout-related parameters */
        x_data->fadeout_parameters.fadeout_state = NOT_RUNNING_STATE;
        x_data->fadeout_parameters.fadeout_counter = 0;
        x_data->fadeout_parameters.fadeout_flush_count = 0;
    }

    /* Now reset the encoder - re-using old but slightly massaged function in ASM */
    wbs_enc_reset_sbc_data(op_data);
}


/* free the memory allocated for SBC encoder (shared and non-shared) */
static void wbs_enc_free_state_data(OPERATOR_DATA* op_data)
{
    WBS_ENC_OP_DATA* x_data = get_instance_data(op_data);

    if (x_data->codec_data != NULL)
    {
        /* free the shared encoder data */
        mem_table_free_shared((void *)(x_data->codec_data),
                            wbs_sbc_shared_malloc_table, WBS_SBC_SHARED_TABLE_LENGTH);

        /* free only encoder shared data since a DEC was found */
        mem_table_free_shared((void *)(x_data->codec_data),
                    wbs_sbc_enc_shared_malloc_table, WBS_SBC_ENC_SHARED_TABLE_LENGTH);

        /* free non-shared memory */
        mem_table_free((void *)x_data->codec_data, wbs_enc_malloc_table,
                                                WBS_ENC_MALLOC_TABLE_LENGTH);

        /* now free the codec data object */
        pdelete(x_data->codec_data);
        x_data->codec_data = NULL;
    }
}



/* ********************************** API functions ************************************* */

bool wbs_enc_create(OPERATOR_DATA *op_data, void *message_data, unsigned *response_id, void **response_data)
{
    WBS_ENC_OP_DATA* wbs_enc = get_instance_data(op_data);
    bool new_allocation;

    /* call base_op create, which also allocates and fills response message */
    if(!base_op_create(op_data, message_data, response_id, response_data))
    {
        return FALSE;
    }

    /* create SBC data object */
    if((wbs_enc->codec_data = xzpnew(sbc_codec)) == NULL)
    {
        /* Change the already allocated response to command failed. No extra error info. */
        base_op_change_response_status(response_data, STATUS_CMD_FAILED);
        return TRUE;
    }

    if(!mem_table_zalloc_shared((uintptr_t *)(wbs_enc->codec_data),
                    wbs_sbc_enc_shared_malloc_table, WBS_SBC_ENC_SHARED_TABLE_LENGTH ,
                    &new_allocation))
    {
        wbs_enc_free_state_data(op_data);
        /* Change the already allocated response to command failed. No extra error info. */
        base_op_change_response_status(response_data, STATUS_CMD_FAILED);
        return TRUE;
    }

    if(!mem_table_zalloc_shared((uintptr_t *)(wbs_enc->codec_data),
                            wbs_sbc_shared_malloc_table, WBS_SBC_SHARED_TABLE_LENGTH,
                            &new_allocation))
    {
        wbs_enc_free_state_data(op_data);
        /* Change the already allocated response to command failed. No extra error info. */
        base_op_change_response_status(response_data, STATUS_CMD_FAILED);
        return TRUE;
    }

    /* now allocate the non-shareable parts */
    if(!mem_table_zalloc((uintptr_t *)(wbs_enc->codec_data),
                            wbs_enc_malloc_table, WBS_ENC_MALLOC_TABLE_LENGTH))
    {
        wbs_enc_free_state_data(op_data);
        /* Change the already allocated response to command failed. No extra error info. */
        base_op_change_response_status(response_data, STATUS_CMD_FAILED);
        return TRUE;
    }

    /* Now reserve the scratch memory */
    if (scratch_register())
    {
        if (mem_table_scratch_tbl_reserve(&wbs_enc_scratch_table))
        {
            /* Successfully allocated everything! */
            /* initialise some more WBS encoder-specific data  */
            wbs_enc_reset_working_data(op_data);
            wbsenc_init_encoder(op_data);
            return TRUE;
        }
        /* Fail free all the scratch memory we reserved */
        scratch_deregister();
    }
    /* Clear up all the allocated memory. */
    wbs_enc_free_state_data(op_data);
    /* Change the already allocated response to command failed. No extra error info. */
    base_op_change_response_status(response_data, STATUS_CMD_FAILED);
    return TRUE;
}


bool wbs_enc_destroy(OPERATOR_DATA *op_data, void *message_data, unsigned *response_id, void **response_data)
{
    if(base_op_destroy(op_data, message_data, response_id, response_data))
    {
        /* Free all the scratch memory we reserved */
        scratch_deregister();
        /* now destroy all the SBC encoder and shared codec paraphernalia */
        wbs_enc_free_state_data(op_data);
        return TRUE;
    }

    return FALSE;
}


bool wbs_enc_reset(OPERATOR_DATA *op_data, void *message_data, unsigned *response_id, void **response_data)
{
    if(!base_op_reset(op_data, message_data, response_id, response_data))
    {
        return FALSE;
    }

    /* now initialise specific working data */
    wbs_enc_reset_working_data(op_data);
    wbsenc_init_encoder(op_data);

    return TRUE;
}

bool wbs_enc_connect(OPERATOR_DATA *op_data, void *message_data,
        unsigned *response_id, void **response_data)
{
    WBS_ENC_OP_DATA* wbs_enc = get_instance_data(op_data);
    return sco_common_connect(op_data, message_data, response_id, response_data,
            &wbs_enc->buffers, NULL);
}

bool wbs_enc_disconnect(OPERATOR_DATA *op_data, void *message_data,
        unsigned *response_id, void **response_data)
{
    WBS_ENC_OP_DATA* wbs_enc = get_instance_data(op_data);
    return sco_common_disconnect(op_data, message_data, response_id, response_data,
            &wbs_enc->buffers, NULL);
}

bool wbs_enc_get_data_format(OPERATOR_DATA *op_data, void *message_data, unsigned *response_id,
        void **response_data)
{
    return sco_common_get_data_format(op_data, message_data, response_id, response_data,
            AUDIO_DATA_FORMAT_FIXP, AUDIO_DATA_FORMAT_16_BIT_BYTE_SWAP);
}

bool wbs_enc_get_sched_info(OPERATOR_DATA *op_data, void *message_data, unsigned *response_id,
        void **response_data)
{
    return sco_common_get_sched_info(op_data, message_data, response_id, response_data,
            WBS_ENC_DEFAULT_INPUT_BLOCK_SIZE, WBS_ENC_DEFAULT_OUTPUT_BLOCK_SIZE);
}

/* ************************************* Data processing-related functions and wrappers **********************************/

void wbs_enc_process_data(OPERATOR_DATA *op_data, TOUCHED_TERMINALS *touched)
{
    WBS_ENC_OP_DATA* x_data = get_instance_data(op_data);
    unsigned available_space, samples_to_process;
    unsigned count = 0, max_count;

    patch_fn(wbs_encode_process_data);

    /* work out amount of input data to process, based on output space and input data amount */
    available_space = cbuffer_calc_amount_space_in_words(x_data->buffers.op_buffer);
    samples_to_process = cbuffer_calc_amount_data_in_words(x_data->buffers.ip_buffer);

    /* Loop until we've done all we need to.
     * The frame size should be a multiple of the output block size. Round up anyway just in case
     */
    max_count = (x_data->frame_size + WBS_ENC_DEFAULT_OUTPUT_BLOCK_SIZE - 1)/WBS_ENC_DEFAULT_OUTPUT_BLOCK_SIZE;

    while ((available_space >= WBS_ENC_DEFAULT_OUTPUT_BLOCK_SIZE) &&
           (samples_to_process >= WBS_ENC_DEFAULT_INPUT_BLOCK_SIZE) &&
           (count < max_count))
    {
        /* Is fadeout enabled? if yes, do it on the current input data */
        if(x_data->fadeout_parameters.fadeout_state != NOT_RUNNING_STATE)
        {
            /* the wrapper below takes output Cbuffer and fadeout params, use input block size */
            if(mono_cbuffer_fadeout(x_data->buffers.ip_buffer, WBS_ENC_DEFAULT_INPUT_BLOCK_SIZE,
                                     &(x_data->fadeout_parameters)))
            {
                common_send_simple_unsolicited_message(op_data, OPMSG_REPLY_ID_FADEOUT_DONE);
            }
        }

        /* Commit any scratch memory ideally this should be done later after the
         * decision to decode is made. */
        mem_table_scratch_tbl_commit(x_data->codec_data, &wbs_enc_scratch_table);

        wbsenc_process_frame(op_data);

        /* Free the scratch memory used */
        scratch_free();

        available_space -= WBS_ENC_DEFAULT_OUTPUT_BLOCK_SIZE;
        samples_to_process -= WBS_ENC_DEFAULT_INPUT_BLOCK_SIZE;

        touched->sources =  TOUCHED_SOURCE_0;
        count++;
    }

    /* Check if we want to kick the endpoint on Tesco rate (means forward all kicks) */
    if(x_data->forward_all_kicks_enabled)
    {
        touched->sources =  TOUCHED_SOURCE_0;
    }

    if (samples_to_process < WBS_ENC_DEFAULT_INPUT_BLOCK_SIZE)
    {
        /* If there isn't enough data to process another frame kick backwards */
        touched->sinks = TOUCHED_SINK_0;
    }

}


/* **************************** Operator message handlers ******************************** */


bool wbs_enc_opmsg_enable_fadeout(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    WBS_ENC_OP_DATA* x_data = get_instance_data(op_data);
    common_set_fadeout_state(&x_data->fadeout_parameters, RUNNING_STATE);

    return TRUE;
}

bool wbs_enc_opmsg_disable_fadeout(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    WBS_ENC_OP_DATA* x_data = get_instance_data(op_data);
    common_set_fadeout_state(&x_data->fadeout_parameters, NOT_RUNNING_STATE);

    return TRUE;
}

bool wbs_enc_opmsg_set_to_air_info(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    WBS_ENC_OP_DATA* x_data = get_instance_data(op_data);

    x_data->frame_size = OPMSG_FIELD_GET(message_data, OPMSG_WBS_ENC_SET_TO_AIR_INFO, FRAME_SIZE);

    return TRUE;
}


/*
 * Notes on motivation to introduce message OPMSG_WBS_ENC_ID_FORWARD_ALL_KICKS:
 *
 * In case of WBS encoded transmission via SCO, mainly 2 configurations are considered:
 *    case 1: Tesco=12, SOC_PACKET_SIZE=60 Bytes (default config, 2-EV3) --> Tesco period = 12 x 625us = 7500us
 *    case 2: Tesco=6, SCO_PACKET_SIZE=30 Bytes (EV3) --> Tesco period = 6 x 625us = 3750us
 *
 * The WBS_ENCODER operator is kicked at its input on the Tesco rate by a kicking object.
 * Despite of that and before introducing OPMSG_WBS_ENC_ID_FORWARD_ALL_KICKS to WBS_ENC capability, the WBS_ENCODER operator kicked itself at its output
 * the subsequent SCO_SINK_EP on a fix rate of 7500us (= 12 x 625 us) and delivers (multiple of) 60 Bytes independent whether Tesco is 12 or 6.
 * 
 * In case 1.) this is actually fine. 
 * Even in case 2.) this might be OK as long as SCO_SINK_EP considers in-place processing (one buffer for in and output), 
 * even though in the 2nd case, we get twice the amount of Bytes (2 SCO packets comprising 60 Bytes instead of 1 with 30 Bytes) 
 * on half the Tesco related rate (every 7500us instead of every 3750us).
 * 
 * However, there is an issue in case that SCO_SINK_EP delivers the received data 'SCO packet wise' (with SCO_PACKET_SIZE=30 Bytes) to a 
 * double TX output buffer (each of size=SCO_PACKET_SIZE) and doesn't have a ring buffer (where we could write as much as available):
 * In order to write to one of these TX output buffers by AUDIO_SS (resp. SCO_SINK_EP) whereas the BT_SS reads from the other one, 
 * the SCO_SINK_EP can only write (max.) 1 SCO packet per kick to the TX buffer and needs to be kicked on the Tesco related rate.
 * In particular, to preserve the double buffering principle, it cannot write two SCO packets in one kick on half the rate...
 *
 * Accordingly, the OPMSG_WBS_ENC_ID_FORWARD_ALL_KICKS message was introduced to configure the WBS_ENCODER operator in a way that it forwards all 
 * kicks from its input to its output in order to get the subsequent SCO_SINK_EP kicked on the Tesco related rate.
 *
 * By default, OPMSG_WBS_ENC_ID_FORWARD_ALL_KICKS = FALSE (means, behavior as before introducing this message)
 *
 */
bool wbs_enc_opmsg_forward_all_kicks(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    WBS_ENC_OP_DATA* x_data = get_instance_data(op_data);
    L4_DBG_MSG("wbs_enc_opmsg_forward_all_kicks  \n");

    x_data->forward_all_kicks_enabled = OPMSG_FIELD_GET(message_data, OPMSG_WBS_ENC_FORWARD_ALL_KICKS, FORWARD_ALL_KICKS);

    return TRUE;
}
