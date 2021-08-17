/*!
\copyright  Copyright (c) 2017 - 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       kymera_sco.c
\brief      Kymera SCO
*/

#include <vmal.h>
#include <anc_state_manager.h>

#include "kymera_config.h"
#include "kymera_private.h"
#include "kymera_aec.h"
#include "kymera_cvc.h"
#include "kymera_adaptive_anc.h"
#include "av.h"
#include "kymera_mic_if.h"
#include "kymera_output_if.h"

#if defined(KYMERA_SCO_USE_3MIC)
#define MAX_NUM_OF_MICS_SUPPORTED (3)
#elif defined (KYMERA_SCO_USE_2MIC)
#define MAX_NUM_OF_MICS_SUPPORTED (2)
#else
#define MAX_NUM_OF_MICS_SUPPORTED (1)
#endif

#define AWBSDEC_SET_BITPOOL_VALUE    0x0003
#define AWBSENC_SET_BITPOOL_VALUE    0x0001

#ifdef ENABLE_ADAPTIVE_ANC
#define AEC_TX_BUFFER_SIZE_MS 45
#else
#define AEC_TX_BUFFER_SIZE_MS 15
#endif

typedef struct set_bitpool_msg_s
{
    uint16 id;
    uint16 bitpool;
}set_bitpool_msg_t;

static kymera_chain_handle_t the_sco_chain;

static bool kymera_ScoMicGetConnectionParameters(microphone_number_t *mic_ids, Sink *mic_sinks, uint8 *num_of_mics, uint32 *sample_rate, Sink *aec_ref_sink);
static bool kymera_ScoMicDisconnectIndication(const mic_change_info_t *info);

static const mic_callbacks_t kymera_MicScoCallbacks =
{
    .MicGetConnectionParameters = kymera_ScoMicGetConnectionParameters,
    .MicDisconnectIndication = kymera_ScoMicDisconnectIndication,
};

static const microphone_number_t kymera_ScoMandatoryMicIds[MAX_NUM_OF_MICS_SUPPORTED] =
{
    microphone_none,
};

static mic_user_state_t kymera_ScoMicState =
{
    mic_user_state_non_interruptible,
};

static const mic_registry_per_user_t kymera_MicScoRegistry =
{
    .user = mic_user_sco,
    .callbacks = &kymera_MicScoCallbacks,
    .mandatory_mic_ids = &kymera_ScoMandatoryMicIds[0],
    .num_of_mandatory_mics = 0,
    .mic_user_state = &kymera_ScoMicState,
};

static const output_registry_entry_t output_info =
{
    .user = output_user_sco,
    .connection = output_connection_mono,
};

void OperatorsAwbsSetBitpoolValue(Operator op, uint16 bitpool, bool decoder)
{
    set_bitpool_msg_t bitpool_msg;
    bitpool_msg.id = decoder ? AWBSDEC_SET_BITPOOL_VALUE : AWBSENC_SET_BITPOOL_VALUE;
    bitpool_msg.bitpool = (uint16)(bitpool);

    PanicFalse(VmalOperatorMessage(op, &bitpool_msg, SIZEOF_OPERATOR_MESSAGE(bitpool_msg), NULL, 0));
}

kymera_chain_handle_t appKymeraGetScoChain(void)
{
    return the_sco_chain;
}

void appKymeraDestroyScoChain(void)
{
    ChainDestroy(the_sco_chain);
    the_sco_chain = NULL;

    /* Update state variables */
    appKymeraSetState(KYMERA_STATE_IDLE);

    Kymera_LeakthroughResumeChainIfSuspended();
}

const appKymeraScoChainInfo *appKymeraScoFindChain(const appKymeraScoChainInfo *info, appKymeraScoMode mode, uint8 mic_cfg)
{
    while (info->mode != NO_SCO)
    {        
        if ((info->mode == mode) && (info->mic_cfg == mic_cfg))
        {
            return info;
        }
        
        info++;
    }
    return NULL;
}

static void appKymeraScoConfigureChain(uint16 wesco)
{
    kymera_chain_handle_t sco_chain = appKymeraGetScoChain();
    kymeraTaskData *theKymera = KymeraGetTaskData();
    PanicNull((void *)theKymera->sco_info);
    
    const uint32_t rate = theKymera->sco_info->rate;

    /*! \todo Need to decide ahead of time if we need any latency.
        Simple enough to do if we are legacy or not. Less clear if
        legacy but no peer connection */
    /* Enable Time To Play if supported */
    if (appConfigScoChainTTP(wesco) != 0)
    {
        Operator sco_op = PanicZero(ChainGetOperatorByRole(sco_chain, OPR_SCO_RECEIVE));
        OperatorsStandardSetTimeToPlayLatency(sco_op, appConfigScoChainTTP(wesco));
        OperatorsStandardSetBufferSize(sco_op, appConfigScoBufferSize(rate));
    }

    Kymera_SetVoiceUcids(sco_chain);

    if(theKymera->chain_config_callbacks && theKymera->chain_config_callbacks->ConfigureScoInputChain)
    {
        kymera_sco_config_params_t params = {0};
        params.sample_rate = theKymera->sco_info->rate;
        params.mode = theKymera->sco_info->mode;
        params.wesco = wesco;
        theKymera->chain_config_callbacks->ConfigureScoInputChain(sco_chain, &params);
    }
}



static void kymera_PopulateScoConnectParams(microphone_number_t *mic_ids, Sink *mic_sinks, uint8 num_mics, Sink *aec_ref_sink)
{
    kymera_chain_handle_t sco_chain = appKymeraGetScoChain();
    PanicZero(mic_ids);
    PanicZero(mic_sinks);
    PanicZero(aec_ref_sink);
    PanicFalse(num_mics <= 3);

    mic_ids[0] = appConfigMicVoice();
    mic_sinks[0] = ChainGetInput(sco_chain, EPR_CVC_SEND_IN1);
    if(num_mics > 1)
    {
        mic_ids[1] = appConfigMicExternal();
        mic_sinks[1] = ChainGetInput(sco_chain, EPR_CVC_SEND_IN2);
    }
    if(num_mics > 2)
    {
        mic_ids[2] = appConfigMicInternal();
        mic_sinks[2] = ChainGetInput(sco_chain, EPR_CVC_SEND_IN3);
    }
    aec_ref_sink[0] = ChainGetInput(sco_chain, EPR_CVC_SEND_REF_IN);
}

static bool kymera_ScoMicDisconnectIndication(const mic_change_info_t *info)
{
    UNUSED(info);
    DEBUG_LOG_ERROR("kymera_ScoMicDisconnectIndication: SCO shouldn't have to get disconnected");
    Panic();
    return TRUE;
}

static bool kymera_ScoMicGetConnectionParameters(microphone_number_t *mic_ids, Sink *mic_sinks, uint8 *num_of_mics, uint32 *sample_rate, Sink *aec_ref_sink)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    DEBUG_LOG("kymera_ScoMicGetConnectionParameters");

    *sample_rate = theKymera->sco_info->rate;
    *num_of_mics = Kymera_GetNumberOfMics();
    kymera_PopulateScoConnectParams(mic_ids, mic_sinks, theKymera->sco_info->mic_cfg, aec_ref_sink);
    return TRUE;
}


bool appKymeraHandleInternalScoStart(Sink sco_snk, const appKymeraScoChainInfo *info,
                                     uint8 wesco, int16 volume_in_db)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    DEBUG_LOG("appKymeraHandleInternalScoStart, sink 0x%x, mode %u, wesco %u, state %u", sco_snk, info->mode, wesco, appKymeraGetState());

    /* If there is a tone still playing at this point,
     * it must be an interruptible tone, so cut it off */
    appKymeraTonePromptStop();

    Kymera_LeakthroughStopChainIfRunning();

    /* Can't start voice chain if we're not idle */
    PanicFalse((appKymeraGetState() == KYMERA_STATE_IDLE) || (appKymeraGetState() == KYMERA_STATE_ADAPTIVE_ANC_STARTED));

    /* SCO chain must be destroyed if we get here */
    PanicNotNull(appKymeraGetScoChain());

    /* Move to SCO active state now, what ever happens we end up in this state
      (even if it's temporary) */
    appKymeraSetState(KYMERA_STATE_SCO_ACTIVE);

    /* Create appropriate SCO chain */
    the_sco_chain = appKymeraScoCreateChain(info);
    kymera_chain_handle_t sco_chain = PanicNull(appKymeraGetScoChain());

    /* Connect to Mic interface */
    if (!Kymera_MicConnect(mic_user_sco))
    {
        DEBUG_LOG_ERROR("appKymeraHandleInternalScoStart: Mic connection was not successful. Sco should always be prepared.");
        Panic();
    }    

    /* Configure chain specific operators */
    appKymeraScoConfigureChain(wesco);

    /* Create an appropriate Output chain */
    kymera_output_chain_config output_config;
    KymeraOutput_SetDefaultOutputChainConfig(&output_config, theKymera->sco_info->rate, KICK_PERIOD_VOICE, 0);

    output_config.chain_type = output_chain_mono;
    output_config.chain_include_aec = TRUE;
    PanicFalse(Kymera_OutputPrepare(output_user_sco, &output_config));

    /* Get sources and sinks for chain endpoints */
    Source sco_ep_src = ChainGetOutput(sco_chain, EPR_SCO_TO_AIR);
    Sink sco_ep_snk = ChainGetInput(sco_chain, EPR_SCO_FROM_AIR);

    /* Connect SCO to chain SCO endpoints */
    /* Get SCO source from SCO sink */
    Source sco_src = StreamSourceFromSink(sco_snk);

    StreamConnect(sco_ep_src, sco_snk);
    StreamConnect(sco_src, sco_ep_snk);
    
    /* Connect chain */
    ChainConnect(sco_chain);
 
    /* Connect to the Ouput chain */
    output_source_t sources = {.mono = ChainGetOutput(sco_chain, EPR_SCO_SPEAKER)};
    PanicFalse(Kymera_OutputConnect(output_user_sco, &sources));
    KymeraOutput_ChainStart();

    /* The chain can fail to start if the SCO source disconnects whilst kymera
    is queuing the SCO start request or starting the chain. If the attempt fails,
    ChainStartAttempt will stop (but not destroy) any operators it started in the chain. */
    if (ChainStartAttempt(sco_chain))
    {
        appKymeraHandleInternalScoSetVolume(volume_in_db);
        
        if(theKymera->enable_cvc_passthrough)
        {
            kymera_EnableCvcPassthroughMode();
        }

        Kymera_LeakthroughSetAecUseCase(aec_usecase_sco_leakthrough);
    }
    else
    {
        DEBUG_LOG("appKymeraHandleInternalScoStart, could not start chain");
        /* Stop/destroy the chain, returning state to KYMERA_STATE_IDLE.
        This needs to be done here, since between the failed attempt to start
        and the subsequent stop (when appKymeraScoStop() is called), a tone
        may need to be played - it would not be possible to play a tone in a
        stopped SCO chain. The state needs to be KYMERA_STATE_SCO_ACTIVE for
        appKymeraHandleInternalScoStop() to stop/destroy the chain. */
        appKymeraHandleInternalScoStop();
        return FALSE;
    }
    return TRUE;
}

void appKymeraHandleInternalScoStop(void)
{
    DEBUG_LOG("appKymeraHandleInternalScoStop, state %u", appKymeraGetState());

    /* Get current SCO chain */
    kymera_chain_handle_t sco_chain = appKymeraGetScoChain();
    if (appKymeraGetState() != KYMERA_STATE_SCO_ACTIVE)
    {
        if (!sco_chain)        
        {
            /* Attempting to stop a SCO chain when not ACTIVE. This happens when the user
            calls appKymeraScoStop() following a failed attempt to start the SCO
            chain - see ChainStartAttempt() in appKymeraHandleInternalScoStart().
            In this case, there is nothing to do, since the failed start attempt
            cleans up by calling this function in state KYMERA_STATE_SCO_ACTIVE */
            DEBUG_LOG("appKymeraHandleInternalScoStop, not stopping - already idle");
            return;
        }
        else
        {
            Panic();
        }
    }
    
    Source sco_ep_src = ChainGetOutput(sco_chain, EPR_SCO_TO_AIR);
    Sink sco_ep_snk = ChainGetInput(sco_chain, EPR_SCO_FROM_AIR);

    /* Disable AEC_REF sidetone path */
    Kymera_AecEnableSidetonePath(FALSE);

    /* A tone still playing at this point must be interruptable */
    appKymeraTonePromptStop();

    /* Stop chains */
    ChainStop(sco_chain);

    /* Disconnect SCO from chain SCO endpoints */
    StreamDisconnect(sco_ep_src, NULL);
    StreamDisconnect(NULL, sco_ep_snk);
   
    Kymera_MicDisconnect(mic_user_sco);

    Kymera_OutputDisconnect(output_user_sco);

    /* Destroy chains */
    appKymeraDestroyScoChain();
    sco_chain = NULL;
}

void appKymeraHandleInternalScoSetVolume(int16 volume_in_db)
{
    DEBUG_LOG("appKymeraHandleInternalScoSetVolume, vol %d", volume_in_db);

    switch (KymeraGetTaskData()->state)
    {
        case KYMERA_STATE_SCO_ACTIVE:
        case KYMERA_STATE_SCO_SLAVE_ACTIVE:
        {
            KymeraOutput_SetMainVolume(volume_in_db);
        }
        break;
        default:
            break;
    }
}

void appKymeraHandleInternalScoMicMute(bool mute)
{
    DEBUG_LOG("appKymeraHandleInternalScoMicMute, mute %u", mute);

    switch (KymeraGetTaskData()->state)
    {
        case KYMERA_STATE_SCO_ACTIVE:
        {
            Operator cvc_send_op;
            if (GET_OP_FROM_CHAIN(cvc_send_op, appKymeraGetScoChain(), OPR_CVC_SEND))
            {
                OperatorsStandardSetControl(cvc_send_op, cvc_send_mute_control, mute);
            }
            else
            {
                /* This is just in case fall-back when CVC send is not present,
                   otherwise input mute should be applied in CVC send operator. */
                Operator aec_op = Kymera_GetAecOperator();
                if (aec_op != INVALID_OPERATOR)
                {
                    OperatorsAecMuteMicOutput(aec_op, mute);
                }
            }
        }
        break;

        default:
            break;
    }
}

uint8 appKymeraScoVoiceQuality(void)
{
    uint8 quality = appConfigVoiceQualityWorst();

    if (appConfigVoiceQualityMeasurementEnabled())
    {
        Operator cvc_send_op;
        if (GET_OP_FROM_CHAIN(cvc_send_op, appKymeraGetScoChain(), OPR_CVC_SEND))
        {
            uint16 rx_msg[2], tx_msg = OPMSG_COMMON_GET_VOICE_QUALITY;
            PanicFalse(OperatorMessage(cvc_send_op, &tx_msg, 1, rx_msg, 2));
            quality = MIN(appConfigVoiceQualityBest() , rx_msg[1]);
            quality = MAX(appConfigVoiceQualityWorst(), quality);
        }
    }
    else
    {
        quality = appConfigVoiceQualityWhenDisabled();
    }

    DEBUG_LOG("appKymeraScoVoiceQuality %u", quality);

    return quality;
}

void Kymera_ScoInit(void)
{
    Kymera_OutputRegister(&output_info);
    Kymera_MicRegisterUser(&kymera_MicScoRegistry);
}
