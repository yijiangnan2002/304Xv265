/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       kymera_leakthrough.c
\brief      Kymera implementation to accommodate software leak-through.
*/

#ifdef ENABLE_AEC_LEAKTHROUGH

#include "kymera.h"
#include "kymera_private.h"
#include "kymera_va.h"
#include "kymera_aec.h"
#include "kymera_mic_if.h"
#include "kymera_output_if.h"

/*The value sidetone_exp[0] and sidetone_mantissa[0] corresponds to value of -46dB and are starting point for leakthrough ramp*/
#define SIDETONE_EXP_MINIMUM sidetone_exp[0]
#define SIDETONE_MANTISSA_MINIMUM sidetone_mantissa[0]

#define AEC_REF_SETTLING_TIME  (100)
#define appConfigLeakthroughMic()                        appConfigMicVoice()
#define MAX_NUM_OF_MICS_SUPPORTED                        (1)
#define appConfigLeakthroughNumMics()                    MAX_NUM_OF_MICS_SUPPORTED

#define LEAKTHROUGH_OUTPUT_RATE  (8000U)
#define ENABLED TRUE
#define DISABLED FALSE

static const output_registry_entry_t output_info =
{
    .user = output_user_aec_leakthrough,
    .connection = output_connection_none,
};

/*! The initial value in the Array given below corresponds to -46dB and ramp up is happening till 0dB with increment of 2dB/cycle */
static const unsigned long sidetone_exp[] = {
    0xFFFFFFFAUL, 0xFFFFFFFAUL, 0xFFFFFFFBUL, 0xFFFFFFFBUL,
    0xFFFFFFFBUL, 0xFFFFFFFCUL, 0xFFFFFFFCUL, 0xFFFFFFFCUL,
    0xFFFFFFFDUL, 0xFFFFFFFDUL, 0xFFFFFFFDUL, 0xFFFFFFFEUL,
    0xFFFFFFFEUL, 0xFFFFFFFEUL, 0xFFFFFFFFUL, 0xFFFFFFFFUL,
    0xFFFFFFFFUL, 0x00000000UL, 0x00000000UL, 0x00000000UL,
    0x00000001UL, 0x00000001UL, 0x00000001UL, 0x00000001UL};
static const unsigned long sidetone_mantissa[] = {
    0x290EA879UL, 0x33B02273UL, 0x2089229EUL, 0x28F5C28FUL,
    0x3390CA2BUL, 0x207567A2UL, 0x28DCEBBFUL, 0x337184E6UL,
    0x2061B89DUL, 0x28C423FFUL, 0x33525297UL, 0x204E1588UL,
    0x28AB6B46UL, 0x33333333UL, 0x203A7E5BUL, 0x2892C18BUL,
    0x331426AFUL, 0x2026F310UL, 0x287A26C5UL, 0x32F52CFFUL,
    0x2013739EUL, 0x28619AEAUL, 0x32D64618UL, 0x40000000UL};
static unsigned gain_index = 0;

static bool kymera_LeakthroughMicGetConnectionParameters(microphone_number_t *mic_ids, Sink *mic_sinks, uint8 *num_of_mics, uint32 *sample_rate, Sink *aec_ref_sink);
static bool kymera_LeakthroughMicDisconnectIndication(const mic_change_info_t *info);

static const mic_callbacks_t kymera_MicLeakthroughCallbacks =
{
    .MicGetConnectionParameters = kymera_LeakthroughMicGetConnectionParameters,
    .MicDisconnectIndication = kymera_LeakthroughMicDisconnectIndication,
};

static const microphone_number_t kymera_MandatoryLeakthroughMicIds[MAX_NUM_OF_MICS_SUPPORTED] =
{
    appConfigLeakthroughMic(),
};

static const mic_user_state_t kymera_LeakthroughMicState =
{
    mic_user_state_interruptible,
};

static const mic_registry_per_user_t kymera_MicLeakthroughRegistry =
{
    .user = mic_user_leakthrough,
    .callbacks = &kymera_MicLeakthroughCallbacks,
    .mandatory_mic_ids = kymera_MandatoryLeakthroughMicIds,
    .num_of_mandatory_mics = ARRAY_DIM(kymera_MandatoryLeakthroughMicIds),
    .mic_user_state = &kymera_LeakthroughMicState,
};

static void Kymera_LeakthroughUpdateAecOperatorUcid(void);
static void Kymera_LeakthroughEnableAecSideToneAfterTimeout(void);
static void Kymera_LeakthroughMuteDisconnect(bool disconnect_mic);
static void Kymera_LeakthroughConnectWithTone(void);
static void Kymera_LeakthroughUpdateAecOperatorAndSidetone(void);
static void Kymera_LeakthroughMuteDisconnect(bool disconnect_mic);

static uint32 kymera_leakthrough_mic_sample_rate;
static bool kymera_standalone_leakthrough_status;

static bool kymera_LeakthroughIsCurrentStepValueLastOne(void)
{
    return (gain_index >= ARRAY_DIM(sidetone_exp));
}

static uint32 kymera_GetLeakthroughMicSampleRate(void)
{
    return kymera_leakthrough_mic_sample_rate;
}

static void kymera_UpdateStandaloneLeakthroughStatus(bool status)
{
    DEBUG_LOG("kymera_UpdateStandaloneLeakthroughStatus: %d", status);
    kymera_standalone_leakthrough_status = status;
}

static void kymera_LeakthroughResetGainIndex(void)
{
    gain_index = 0;
}

static void kymera_SetMinLeakthroughSidetoneGain(void)
{
    Kymera_AecSetSidetoneGain(SIDETONE_EXP_MINIMUM,SIDETONE_MANTISSA_MINIMUM);
}

void Kymera_LeakthroughSetupSTGain(void)
{
    if(AecLeakthrough_IsLeakthroughEnabled())
    {
        kymeraTaskData *theKymera = KymeraGetTaskData();
        kymera_SetMinLeakthroughSidetoneGain();
        kymera_LeakthroughResetGainIndex();
        Kymera_AecEnableSidetonePath(TRUE);
        MessageSendLater(&theKymera->task, KYMERA_INTERNAL_AEC_LEAKTHROUGH_SIDETONE_GAIN_RAMPUP, NULL, ST_GAIN_RAMP_STEP_TIME_MS);
    }
}

void Kymera_LeakthroughStepupSTGain(void)
{
    if(!kymera_LeakthroughIsCurrentStepValueLastOne())
    {
        kymeraTaskData *theKymera = KymeraGetTaskData();
        Kymera_AecSetSidetoneGain(sidetone_exp[gain_index], sidetone_mantissa[gain_index]);
        gain_index++;
        MessageSendLater(&theKymera->task, KYMERA_INTERNAL_AEC_LEAKTHROUGH_SIDETONE_GAIN_RAMPUP, NULL,ST_GAIN_RAMP_STEP_TIME_MS);
    }
    else
    {
        /* End of ramp is achieved reset the gain index */
        kymera_LeakthroughResetGainIndex();
    }
}

void Kymera_setLeakthroughMicSampleRate(uint32 sample_rate)
{
    kymera_leakthrough_mic_sample_rate = sample_rate;
}

static void kymera_PopulateLeakthroughConnectParams(microphone_number_t *mic_ids, Sink *mic_sinks, uint8 num_mics)
{
    PanicZero(mic_ids);
    PanicZero(mic_sinks);
    PanicFalse(num_mics <= appConfigLeakthroughNumMics());

    if ( num_mics > 0 )
    {
        mic_ids[0] = appConfigLeakthroughMic();
        // Leakthrough doesn't use sinks
        mic_sinks[0] = NULL;
    }
}

static bool kymera_LeakthroughMicDisconnectIndication(const mic_change_info_t *info)
{
    UNUSED(info);
    return TRUE;
}

static bool kymera_LeakthroughMicGetConnectionParameters(microphone_number_t *mic_ids, Sink *mic_sinks, uint8 *num_of_mics, uint32 *sample_rate, Sink *aec_ref_sink)
{
    UNUSED(aec_ref_sink);

    *sample_rate = kymera_GetLeakthroughMicSampleRate();
    *num_of_mics = appConfigLeakthroughNumMics();
    kymera_PopulateLeakthroughConnectParams(mic_ids, mic_sinks, 1);
    return TRUE;
}

static void kymera_DisconnectLeakthroughMic(void)
{
    Kymera_MicDetachLeakthrough(mic_user_leakthrough);
}


static void kymera_ConnectLeakthroughMic(void)
{
    mic_connect_params_t mic_params;
    Sink mic_sinks[MAX_NUM_OF_MICS_SUPPORTED] = {NULL};
    microphone_number_t mic_ids[MAX_NUM_OF_MICS_SUPPORTED] = {0};

    /* Populate connection parameters */
    mic_params.sample_rate = kymera_GetLeakthroughMicSampleRate();
    mic_params.connections.num_of_mics = appConfigLeakthroughNumMics();
    mic_params.connections.mic_ids = mic_ids;
    mic_params.connections.mic_sinks = mic_sinks;
    kymera_PopulateLeakthroughConnectParams(mic_ids, mic_sinks, appConfigLeakthroughNumMics());

    /* Connect to Mic interface */
    Kymera_MicAttachLeakthrough(mic_user_leakthrough, &mic_params);
}

static void kymera_PrepareOutputChain(void)
{
    kymera_output_chain_config config = {0};
    KymeraOutput_SetDefaultOutputChainConfig(&config, LEAKTHROUGH_OUTPUT_RATE, KICK_PERIOD_LEAKTHROUGH, 0);
    PanicFalse(Kymera_OutputPrepare(output_user_aec_leakthrough, &config));
}

void Kymera_CreateLeakthroughChain(void)
{
    DEBUG_LOG("KymeraLeakthrough_CreateChain");

    appKymeraSetState(KYMERA_STATE_STANDALONE_LEAKTHROUGH);
    Kymera_SetAecUseCase(aec_usecase_standalone_leakthrough);

    kymera_PrepareOutputChain();

    KymeraOutput_ChainStart();
    kymera_ConnectLeakthroughMic();
    appKymeraConfigureDspPowerMode();
    kymera_UpdateStandaloneLeakthroughStatus(ENABLED);
    Kymera_LeakthroughEnableAecSideToneAfterTimeout();
}

void Kymera_DestroyLeakthroughChain(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    DEBUG_LOG("Kymera_DestroyLeakthroughChain");

    MessageCancelAll(&theKymera->task, KYMERA_INTERNAL_AEC_LEAKTHROUGH_SIDETONE_GAIN_RAMPUP);
    MessageCancelAll(&theKymera->task, KYMERA_INTERNAL_AEC_LEAKTHROUGH_SIDETONE_ENABLE);

    Kymera_LeakthroughMuteDisconnect(TRUE);


    Kymera_OutputDisconnect(output_user_aec_leakthrough);

    kymera_UpdateStandaloneLeakthroughStatus(DISABLED);
    Kymera_SetAecUseCase(aec_usecase_default);
    appKymeraSetState(KYMERA_STATE_IDLE);
}

static void Kymera_LeakthroughEnableAecSideToneAfterTimeout(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    MessageCancelAll(&theKymera->task, KYMERA_INTERNAL_AEC_LEAKTHROUGH_SIDETONE_ENABLE);
    MessageSendLater(&theKymera->task, KYMERA_INTERNAL_AEC_LEAKTHROUGH_SIDETONE_ENABLE, NULL, AEC_REF_SETTLING_TIME);
}

bool Kymera_IsStandaloneLeakthroughActive(void)
{
    DEBUG_LOG("Kymera_IsStandaloneLeakthroughActive: %d", kymera_standalone_leakthrough_status);
    return kymera_standalone_leakthrough_status;
}

void Kymera_LeakthroughStopChainIfRunning(void)
{
    if(Kymera_IsStandaloneLeakthroughActive())
    {
        Kymera_DestroyLeakthroughChain();
    }
}

void Kymera_LeakthroughResumeChainIfSuspended(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    DEBUG_LOG("Kymera_LeakthroughResumeChainIfSuspended");
    DEBUG_LOG("Kymera State is enum:appKymeraState:%u", theKymera->state);
    if(AecLeakthrough_IsLeakthroughEnabled() && (appKymeraGetState() == KYMERA_STATE_IDLE))
    {
        Kymera_CreateLeakthroughChain();
    }
}

static void Kymera_LeakthroughUpdateAecOperatorUcid(void)
{
    if(AecLeakthrough_IsLeakthroughEnabled())
    {
        uint8 ucid;
        Operator aec_ref = Kymera_GetAecOperator();
        ucid = Kymera_GetAecUcid();
        OperatorsStandardSetUCID(aec_ref,ucid);
    }
}

void Kymera_EnableLeakthrough(void)
{
    DEBUG_LOG("Kymera_EnableLeakthrough");
    kymeraTaskData *theKymera = KymeraGetTaskData();

    if(Kymera_IsVaCaptureActive())
    {
        Kymera_SetAecUseCase(aec_usecase_va_leakthrough);
        Kymera_LeakthroughUpdateAecOperatorAndSidetone();
    }
    else
    {
        switch(appKymeraGetState())
        {
            case KYMERA_STATE_IDLE:
            case KYMERA_STATE_TONE_PLAYING:
                MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_AEC_LEAKTHROUGH_CREATE_STANDALONE_CHAIN, NULL, &theKymera->lock);
                break;

            case KYMERA_STATE_A2DP_STREAMING:
            case KYMERA_STATE_A2DP_STREAMING_WITH_FORWARDING:
                Kymera_SetAecUseCase(aec_usecase_a2dp_leakthrough);
                Kymera_LeakthroughConnectWithTone();
                break;

            case KYMERA_STATE_SCO_ACTIVE:
            case KYMERA_STATE_SCO_SLAVE_ACTIVE:
                Kymera_SetAecUseCase(aec_usecase_sco_leakthrough);
                Kymera_LeakthroughUpdateAecOperatorAndSidetone();
                break;

            case KYMERA_STATE_LE_AUDIO_ACTIVE:
                Kymera_SetAecUseCase(aec_usecase_le_mic_leakthrough);
                Kymera_LeakthroughUpdateAecOperatorAndSidetone();
                break;

            default:
                break;
        }
    }
}

void Kymera_LeakthroughSetAecUseCase(aec_usecase_t usecase)
{
    if(AecLeakthrough_IsLeakthroughEnabled())
    {
        switch ( usecase ) {
            case aec_usecase_default:
                Kymera_LeakthroughMuteDisconnect(TRUE);
                Kymera_SetAecUseCase(usecase);
                break;
            case aec_usecase_a2dp_leakthrough:
                Kymera_SetAecUseCase(usecase);
                Kymera_LeakthroughConnectWithTone();
                break;
            case aec_usecase_sco_leakthrough:
                Kymera_SetAecUseCase(usecase);
                Kymera_LeakthroughUpdateAecOperatorAndSidetone();
                break;
            case aec_usecase_le_mic_leakthrough:
                Kymera_SetAecUseCase(usecase);
                Kymera_LeakthroughUpdateAecOperatorAndSidetone();
                break;
            default:
                DEBUG_LOG_ALWAYS("Kymera_LeakthroughSetAecUseCase = enum:aec_usecase_t:%d",usecase);
                Panic();
                break;
        }
    }
}

void Kymera_DisableLeakthrough(void)
{
    DEBUG_LOG("Kymera_DisableLeakthrough");
    kymeraTaskData *theKymera = KymeraGetTaskData();

    /* cancel all the pending messsage used for leakthrough ramp */
    MessageCancelAll(&theKymera->task,KYMERA_INTERNAL_AEC_LEAKTHROUGH_SIDETONE_GAIN_RAMPUP);

    /* Reset the gain index used for leakthrough ramp*/
    kymera_LeakthroughResetGainIndex();

    if(Kymera_IsVaCaptureActive())
    {
        Kymera_LeakthroughMuteDisconnect(FALSE);
        Kymera_SetAecUseCase(aec_usecase_voice_assistant);
        Kymera_LeakthroughUpdateAecOperatorUcid();
    }
    else
    {
        switch (appKymeraGetState())
        {
            case KYMERA_STATE_STANDALONE_LEAKTHROUGH:
                MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_AEC_LEAKTHROUGH_DESTROY_STANDALONE_CHAIN, NULL, &theKymera->lock);
                break;

            case KYMERA_STATE_A2DP_STREAMING:
            case KYMERA_STATE_A2DP_STREAMING_WITH_FORWARDING:
                Kymera_LeakthroughMuteDisconnect(TRUE);
                Kymera_SetAecUseCase(aec_usecase_default);
                Kymera_LeakthroughUpdateAecOperatorUcid();
                break;

            case KYMERA_STATE_SCO_ACTIVE:
            case KYMERA_STATE_SCO_SLAVE_ACTIVE:
                Kymera_LeakthroughMuteDisconnect(FALSE);
                Kymera_SetAecUseCase(aec_usecase_voice_call);
                Kymera_LeakthroughUpdateAecOperatorUcid();
                break;

            case KYMERA_STATE_LE_AUDIO_ACTIVE:
                Kymera_LeakthroughMuteDisconnect(FALSE);
                Kymera_SetAecUseCase(aec_usecase_le_mic_leakthrough);
                Kymera_LeakthroughUpdateAecOperatorUcid();
                break;

            default:
                break;
        }
    }
}

void Kymera_LeakthroughUpdateMode(leakthrough_mode_t mode)
{
    UNUSED(mode);
    /* Update the leakthrough operator ucid */
    Kymera_LeakthroughUpdateAecOperatorUcid();
    /*Update the sidetone Path */
    Kymera_LeakthroughEnableAecSideToneAfterTimeout();
}

static void Kymera_LeakthroughMuteDisconnect(bool disconnect_mic)
{
    kymera_SetMinLeakthroughSidetoneGain();
    Kymera_AecEnableSidetonePath(FALSE);
    if( disconnect_mic )
    {
        kymera_DisconnectLeakthroughMic();
    }
}

static void Kymera_LeakthroughConnectWithTone(void)
{
    kymera_ConnectLeakthroughMic();
    Kymera_LeakthroughEnableAecSideToneAfterTimeout();
}

static void Kymera_LeakthroughUpdateAecOperatorAndSidetone(void)
{
    Kymera_LeakthroughUpdateAecOperatorUcid();
    Kymera_LeakthroughEnableAecSideToneAfterTimeout();
}

void Kymera_LeakthroughInit(void)
{
    kymera_leakthrough_mic_sample_rate = DEFAULT_MIC_RATE;
    kymera_standalone_leakthrough_status = DISABLED;
    Kymera_OutputRegister(&output_info);
    Kymera_MicRegisterUser(&kymera_MicLeakthroughRegistry);
}

#endif  /* ENABLE_AEC_LEAKTHROUGH */
