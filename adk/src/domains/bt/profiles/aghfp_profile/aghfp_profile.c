/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Application domain AGHFP component.
*/

#include "aghfp_profile.h"
#include "aghfp_profile_instance.h"
#include "aghfp_profile_sm.h"
#include "aghfp_profile_private.h"
#include "aghfp_profile_audio.h"
#include "connection_manager.h"
#include "panic.h"

#include <logging.h>
#include <bandwidth_manager.h>
#include <device_properties.h>
#include <system_state.h>
#include <telephony_messages.h>
#include <device_list.h>
#include <ui.h>
#include <feature.h>

/*! \brief Application HFP component main data structure. */
agHfpTaskData aghfp_profile_task_data;

/*! \brief Call status state lookup table

    This table is used to convert the call setup and call indicators into
    the appropriate HFP state machine state
*/
const aghfpState aghfp_call_state_table[4] =
{
    /* aghfp_call_setup_none              */ AGHFP_STATE_CONNECTED_IDLE,
    /* aghfp_call_setup_incoming          */ AGHFP_STATE_CONNECTED_INCOMING,
    /* aghfp_call_setup_outgoing          */ AGHFP_STATE_CONNECTED_IDLE, /* Not currently supporting outgoing */
    /* aghfp_call_setup_remote_alert      */ AGHFP_STATE_CONNECTED_IDLE, /* Not currently supporting outgoing */
};

static void aghfpProfile_TaskMessageHandler(Task task, MessageId id, Message message);

static void aghfpProfile_InitTaskData(void)
{
    /* set up common hfp profile task handler. */
    aghfp_profile_task_data.task.handler = aghfpProfile_TaskMessageHandler;

    /* create list for SLC notification clients */
    TaskList_InitialiseWithCapacity(aghfpProfile_GetSlcStatusNotifyList(), AGHFP_SLC_STATUS_NOTIFY_LIST_INIT_CAPACITY);

    /* create list for general status notification clients */
    TaskList_InitialiseWithCapacity(aghfpProfile_GetStatusNotifyList(), AGHFP_STATUS_NOTIFY_LIST_INIT_CAPACITY);

    /* create lists for connection/disconnection requests */
    TaskList_WithDataInitialise(&aghfp_profile_task_data.connect_request_clients);
    TaskList_WithDataInitialise(&aghfp_profile_task_data.disconnect_request_clients);
}

/*! \brief Entering `Initialising HFP` state
*/
static void aghfpProfile_InitAghfpLibrary(void)
{
    uint16 supported_features = aghfp_incoming_call_reject |
                                aghfp_esco_s4_supported |
                                aghfp_codec_negotiation;

    uint16 supported_qce_codec = 0;

#ifdef INCLUDE_SWB
    if (FeatureVerifyLicense(APTX_ADAPTIVE_MONO_DECODE))
    {
        DEBUG_LOG("License found for AptX adaptive mono decoder");
        supported_qce_codec = CODEC_64_2_EV3;
    }
    else
    {
        DEBUG_LOG("No license found for AptX adaptive mono decoder");
    }
#endif
    AghfpInitQCE(&aghfp_profile_task_data.task,
                 aghfp_handsfree_18_profile,
                 supported_features,
                 supported_qce_codec);
}


/*! \brief Send SLC status indication to all clients on the list.
 */
static void aghfpProfile_SendSlcStatus(bool connected, const bdaddr* bd_addr)
{
    Task next_client = NULL;

    while (TaskList_Iterate(TaskList_GetFlexibleBaseTaskList(aghfpProfile_GetSlcStatusNotifyList()), &next_client))
    {
        MAKE_AGHFP_MESSAGE(APP_AGHFP_SLC_STATUS_IND);
        message->slc_connected = connected;
        message->bd_addr = *bd_addr;
        MessageSend(next_client, APP_AGHFP_SLC_STATUS_IND, message);
    }
}

/*! \brief Handle SLC connect confirmation
*/
static void aghfpProfile_HandleHfpSlcConnectCfm(const AGHFP_SLC_CONNECT_CFM_T *cfm)
{
    aghfpInstanceTaskData* instance = AghfpProfileInstance_GetInstanceForAghfp(cfm->aghfp);

    PanicNull(instance);

    aghfpState state = AghfpProfile_GetState(instance);

    DEBUG_LOG("aghfpProfile_HandleHfpSlcConnectCfm(%p) enum:aghfpState:%d enum:aghfp_connect_status:%d",
              instance, state, cfm->status);

    switch (state)
    {
        case AGHFP_STATE_CONNECTING_LOCAL:
        case AGHFP_STATE_CONNECTING_REMOTE:
        {
            /* Check if SLC connection was successful */
            if (cfm->status == aghfp_connect_success)
            {
                /* Store SLC sink */
                instance->slc_sink = cfm->rfcomm_sink;

                AghfpProfile_SetState(instance, aghfp_call_state_table[instance->bitfields.call_setup]);

                aghfpProfile_SendSlcStatus(TRUE, &instance->hf_bd_addr);

                return;
            }
            /* Not a successful connection so set to disconnected state */
            AghfpProfile_SetState(instance, AGHFP_STATE_DISCONNECTED);
            AghfpProfileInstance_Destroy(instance);
        }
        return;

        default:
        DEBUG_LOG("SLC connect confirmation received in wrong state.");
        return;
    }
}

/*! \brief Handle HFP Library initialisation confirmation
*/
static void aghfpProfile_HandleAghfpInitCfm(const AGHFP_INIT_CFM_T *cfm)
{
    DEBUG_LOG("aghfpProfile_HandleAghfpInitCfm status enum:aghfp_init_status:%d", cfm->status);

    if (cfm->status == aghfp_init_success)
    {
        AghfpProfileInstance_SetAghfp(cfm->aghfp);
        MessageSend(SystemState_GetTransitionTask(), APP_AGHFP_INIT_CFM, 0);
    }
    else
    {
        Panic();
    }
}

/*! \brief Handle SLC connect indication
*/
static void aghfpProfile_HandleHfpSlcConnectInd(const AGHFP_SLC_CONNECT_IND_T *ind)
{
    aghfpInstanceTaskData* instance = AghfpProfileInstance_GetInstanceForBdaddr(&ind->bd_addr);

    if (instance == NULL)
    {
        instance = AghfpProfileInstance_Create(&ind->bd_addr, TRUE);
    }

    PanicNull(instance);

    aghfpState state = AghfpProfile_GetState(instance);

    DEBUG_LOG("aghfpProfile_HandleHfpSlcConnectInd(%p) enum:aghfpState:%d", instance, state);

    switch (state)
    {
        case AGHFP_STATE_DISCONNECTED:
        {
            instance->hf_bd_addr = ind->bd_addr;
            AghfpProfile_SetState(instance, AGHFP_STATE_CONNECTING_REMOTE);
        }
        break;

        default:
        break;
    }
}

/*! \brief Handle HF answering an incoming call
*/
static void aghfpProfile_HandleCallAnswerInd(AGHFP_ANSWER_IND_T *ind)
{
    aghfpInstanceTaskData *instance = AghfpProfileInstance_GetInstanceForAghfp(ind->aghfp);

    PanicNull(instance);

    DEBUG_LOG("aghfpProfile_HandleCallAnswerInd(%p)", instance);

    AghfpSendOk(instance->aghfp);
    aghfpState state = AghfpProfile_GetState(instance);

    if (state == AGHFP_STATE_CONNECTED_INCOMING)
    {
        instance->bitfields.call_status = aghfp_call_active;
        AghfpProfile_SetState(instance, AGHFP_STATE_CONNECTED_ACTIVE);
    }
}

/*! \brief Handle HF rejecting an incoming call or ending an ongoing call
*/
static void aghfpProfile_HandleCallHangUpInd(AGHFP_CALL_HANG_UP_IND_T *ind)
{
    aghfpInstanceTaskData *instance = AghfpProfileInstance_GetInstanceForAghfp(ind->aghfp);

    PanicNull(instance);

    DEBUG_LOG("aghfpProfile_HandleCallHangUpInd(%p)", instance);

    AghfpSendOk(instance->aghfp);
    aghfpState state = AghfpProfile_GetState(instance);

    if (state == AGHFP_STATE_CONNECTED_ACTIVE ||
        state == AGHFP_STATE_CONNECTED_INCOMING)
    {
        Ui_InformContextChange(ui_provider_telephony, context_voice_connected);
        AghfpProfile_SetState(instance, AGHFP_STATE_CONNECTED_IDLE);
    }
}

/*! \brief Handle disconnect of the SLC
*/
static void aghfpProfile_HandleSlcDisconnectInd(AGHFP_SLC_DISCONNECT_IND_T *message)
{
    aghfpInstanceTaskData *instance = AghfpProfileInstance_GetInstanceForAghfp(message->aghfp);

    DEBUG_LOG("aghfpProfile_HandleSlcDisconnectInd(%p) enum:aghfp_disconnect_status:%d",
              instance, message->status);

    if (instance)
    {
        aghfpProfile_SendSlcStatus(FALSE, &instance->hf_bd_addr);

        instance->slc_sink = NULL;
        AghfpProfile_SetState(instance, AGHFP_STATE_DISCONNECTED);
        AghfpProfileInstance_Destroy(instance);
    }
}

/*! \brief Handle audio connect confirmation
*/
static void aghfpProfile_HandleAgHfpAudioConnectCfm(AGHFP_AUDIO_CONNECT_CFM_T *cfm)
{
    aghfpInstanceTaskData *instance = AghfpProfileInstance_GetInstanceForAghfp(cfm->aghfp);

    PanicNull(instance);

    DEBUG_LOG("agHfpProfile_HandleAgHfpAudioConnectCfm(%p) enum:aghfp_audio_connect_status:%d", instance, cfm->status);

    if (cfm->status == aghfp_audio_connect_success)
    {
        TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(appAgHfpGetStatusNotifyList()), APP_AGHFP_SCO_CONNECTED_IND);
        instance->sco_sink = cfm->audio_sink;
        AghfpProfile_StoreConnectParams(cfm);
        if (instance->bitfields.in_band_ring && instance->bitfields.call_setup == aghfp_call_setup_incoming)
        {
            MAKE_AGHFP_MESSAGE(AGHFP_INTERNAL_HFP_RING_REQ);
            message->addr = instance->hf_bd_addr;
            MessageSend(AghfpProfile_GetInstanceTask(instance), AGHFP_INTERNAL_HFP_RING_REQ, message);
        }
    }
    else
    {
        DEBUG_LOG_ERROR("agHfpProfile_HandleAgHfpAudioConnectCfm: Connection failure. Status %d", cfm->status);
    }
}

/*! \brief Handle audio disconnect confirmation
*/
static void aghfpProfile_HandleHfpAudioDisconnectInd(AGHFP_AUDIO_DISCONNECT_IND_T *ind)
{
  aghfpInstanceTaskData *instance = AghfpProfileInstance_GetInstanceForAghfp(ind->aghfp);

  PanicNull(instance);

  DEBUG_LOG("aghfpProfile_HandleHfpAudioDisconnectInd(%p) enum:aghfp_audio_disconnect_status:%d", instance, ind->status);

  switch (ind->status)
  {
  case aghfp_audio_disconnect_success:
      TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(appAgHfpGetStatusNotifyList()), APP_AGHFP_SCO_DISCONNECTED_IND);
      instance->sco_sink = NULL;
      break;
  case aghfp_audio_disconnect_in_progress:
      break;
  default:
     break;
  }
}

/*! \brief Handle send call HFP indications confirmation. Note this is
           the HFP indications not a AGHFP library ind
*/
static void aghfpProfile_HandleSendCallIndCfm(AGHFP_SEND_CALL_INDICATOR_CFM_T* cfm)
{
    DEBUG_LOG("AGHFP_SEND_CALL_INDICATOR_CFM enum:aghfp_lib_status:%d", cfm->status);
}

/*! \brief Handle audio connected indications confirmation
*/
static void aghfpProfile_HandleAudioConnectInd(AGHFP_AUDIO_CONNECT_IND_T * message)
{
    UNUSED(message);
    DEBUG_LOG("aghfpProfile_HandleAudioConnectInd");
}

/*! \brief Handle unknown AT commands from the HF.
*/
static void aghfpProfile_HandleUnrecognisedAtCommand(AGHFP_UNRECOGNISED_AT_CMD_IND_T* message)
{
    AghfpSendError((message)->aghfp);
}


/*! \brief Handles messages into the AGHFP profile
*/
static void aghfpProfile_TaskMessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);

    DEBUG_LOG("aghfpProfile_TaskMessageHandler MESSAGE:AghfpMessageId:0x%04X", id);

    switch (id)
    {
        case AGHFP_INIT_CFM:
            aghfpProfile_HandleAghfpInitCfm((AGHFP_INIT_CFM_T *)message);
            break;

        case AGHFP_SLC_CONNECT_IND:
            aghfpProfile_HandleHfpSlcConnectInd((AGHFP_SLC_CONNECT_IND_T *)message);
            break;

        case AGHFP_SLC_CONNECT_CFM:
            aghfpProfile_HandleHfpSlcConnectCfm((AGHFP_SLC_CONNECT_CFM_T *)message);
            break;

        case AGHFP_SEND_CALL_INDICATOR_CFM:
            aghfpProfile_HandleSendCallIndCfm((AGHFP_SEND_CALL_INDICATOR_CFM_T*)message);
            break;

        case AGHFP_ANSWER_IND:
            aghfpProfile_HandleCallAnswerInd((AGHFP_ANSWER_IND_T *)message);
            break;

        case AGHFP_CALL_HANG_UP_IND:
            aghfpProfile_HandleCallHangUpInd((AGHFP_CALL_HANG_UP_IND_T *)message);
            break;

        case AGHFP_SLC_DISCONNECT_IND:
            aghfpProfile_HandleSlcDisconnectInd((AGHFP_SLC_DISCONNECT_IND_T *)message);
            break;

        case AGHFP_AUDIO_CONNECT_IND:
            aghfpProfile_HandleAudioConnectInd((AGHFP_AUDIO_CONNECT_IND_T *)message);
            break;

        case AGHFP_AUDIO_CONNECT_CFM:
             aghfpProfile_HandleAgHfpAudioConnectCfm((AGHFP_AUDIO_CONNECT_CFM_T *)message);
             break;

        case AGHFP_AUDIO_DISCONNECT_IND:
             aghfpProfile_HandleHfpAudioDisconnectInd((AGHFP_AUDIO_DISCONNECT_IND_T*)message);
             break;

        case AGHFP_UNRECOGNISED_AT_CMD_IND:
            aghfpProfile_HandleUnrecognisedAtCommand((AGHFP_UNRECOGNISED_AT_CMD_IND_T*)message);
            break;
    default:
        DEBUG_LOG("aghfpProfile_TaskMessageHandler default handler MESSAGE:AghfpMessageId:0x%04X", id);
    }
}

/*! \brief Retrieve the device object using its source
*/
static device_t aghfpProfileInstance_FindDeviceFromVoiceSource(voice_source_t source)
{
    return DeviceList_GetFirstDeviceWithPropertyValue(device_property_voice_source, &source, sizeof(voice_source_t));
}

/*! \brief Connect HFP to the HF
*/
void AghfpProfile_Connect(const bdaddr *bd_addr)
{
    DEBUG_LOG_FN_ENTRY("AghfpProfile_Connect");

    aghfpInstanceTaskData* instance = AghfpProfileInstance_GetInstanceForBdaddr(bd_addr);

    if (!instance)
    {
        instance = AghfpProfileInstance_Create(bd_addr, TRUE);
    }

    /* Check if not already connected */
    if (!AghfpProfile_IsConnectedForInstance(instance))
    {
        /* Store address of AG */
        instance->hf_bd_addr = *bd_addr;

        MAKE_AGHFP_MESSAGE(AGHFP_INTERNAL_HFP_CONNECT_REQ);

        /* Send message to HFP task */
        message->addr = *bd_addr;
        MessageSendConditionally(AghfpProfile_GetInstanceTask(instance), AGHFP_INTERNAL_HFP_CONNECT_REQ, message,
                                 ConManagerCreateAcl(bd_addr));
    }
}

/*! \brief Disconnect from the HF
*/
void AghfpProfile_Disconnect(const bdaddr *bd_addr)
{
    DEBUG_LOG_FN_ENTRY("AghfpProfile_Disconnect");

    aghfpInstanceTaskData* instance = AghfpProfileInstance_GetInstanceForBdaddr(bd_addr);

    if (!instance)
    {
        DEBUG_LOG_INFO("AghfpProfile_Disconnect - No instance found");
        return;
    }

    if (!AghfpProfile_IsDisconnected(instance))
    {
        MAKE_AGHFP_MESSAGE(AGHFP_INTERNAL_HFP_DISCONNECT_REQ);

        message->instance = instance;
        MessageSendConditionally(AghfpProfile_GetInstanceTask(instance), AGHFP_INTERNAL_HFP_DISCONNECT_REQ,
                                 message, AghfpProfileInstance_GetLock(instance));
    }
}

/*! \brief Is HFP connected */
bool AghfpProfile_IsConnectedForInstance(aghfpInstanceTaskData * instance)
{
    return ((AghfpProfile_GetState(instance) >= AGHFP_STATE_CONNECTED_IDLE) && (AghfpProfile_GetState(instance) <= AGHFP_STATE_CONNECTED_ACTIVE));
}

/*! \brief Is HFP connected */
Task AghfpProfile_GetInstanceTask(aghfpInstanceTaskData * instance)
{
    PanicNull(instance);
    return &instance->task;
}

/*! \brief Is HFP disconnected */
bool AghfpProfile_IsDisconnected(aghfpInstanceTaskData * instance)
{
    return ((AghfpProfile_GetState(instance) < AGHFP_STATE_CONNECTING_LOCAL) || (AghfpProfile_GetState(instance) > AGHFP_STATE_DISCONNECTING));
}

/*! \brief Is HFP SCO active with the specified HFP instance. */
bool AghfpProfile_IsScoActiveForInstance(aghfpInstanceTaskData * instance)
{
    PanicNull(instance);
    return (instance->sco_sink != NULL);
}


bool AghfpProfile_Init(Task init_task)
{
    DEBUG_LOG_FN_ENTRY("AghfpProfile_Init");
    UNUSED(init_task);

    aghfpProfile_InitTaskData();

    aghfpProfile_InitAghfpLibrary();

    ConManagerRegisterConnectionsClient(&aghfp_profile_task_data.task);

    return TRUE;
}

void AghfpProfile_CallIncomingInd(const bdaddr *bd_addr)
{
    DEBUG_LOG_FN_ENTRY("AghfpProfile_CallIncomingInd");

    aghfpInstanceTaskData *instance = AghfpProfileInstance_GetInstanceForBdaddr(bd_addr);

    if (!instance)
    {
        return;
    }

    aghfpState state = AghfpProfile_GetState(instance);

    switch(state)
    {
        case AGHFP_STATE_CONNECTED_IDLE:
            AghfpProfile_SetState(instance, AGHFP_STATE_CONNECTED_INCOMING);
        break;
        case AGHFP_STATE_DISCONNECTED:
        case AGHFP_STATE_DISCONNECTING:
        case AGHFP_STATE_CONNECTING_LOCAL:
        case AGHFP_STATE_CONNECTING_REMOTE:
        case AGHFP_STATE_CONNECTED_INCOMING:
        case AGHFP_STATE_CONNECTED_ACTIVE:
        case AGHFP_STATE_CONNECTED_OUTGOING:
        case AGHFP_STATE_NULL:
            return;
    }
}

void AghfpProfile_CallOutgoingInd(const bdaddr *bd_addr)
{
    DEBUG_LOG_FN_ENTRY("AghfpProfile_CallOutgoingInd");

    aghfpInstanceTaskData *instance = AghfpProfileInstance_GetInstanceForBdaddr(bd_addr);

    if (!instance)
    {
        return;
    }

    aghfpState state = AghfpProfile_GetState(instance);

    switch(state)
    {
        case AGHFP_STATE_CONNECTED_IDLE:
            AghfpProfile_SetState(instance, AGHFP_STATE_CONNECTED_OUTGOING);
        break;
        case AGHFP_STATE_DISCONNECTED:
        case AGHFP_STATE_DISCONNECTING:
        case AGHFP_STATE_CONNECTING_LOCAL:
        case AGHFP_STATE_CONNECTING_REMOTE:
        case AGHFP_STATE_CONNECTED_INCOMING:
        case AGHFP_STATE_CONNECTED_ACTIVE:
        case AGHFP_STATE_CONNECTED_OUTGOING:
        case AGHFP_STATE_NULL:
            return;
    }
}

void AghfpProfile_OutgoingCallAnswered(const bdaddr *bd_addr)
{
    DEBUG_LOG_FN_ENTRY("AghfpProfile_OutgoingCallAnswered");

    aghfpInstanceTaskData *instance = AghfpProfileInstance_GetInstanceForBdaddr(bd_addr);

    if (!instance)
    {
        return;
    }

    aghfpState state = AghfpProfile_GetState(instance);

    switch(state)
    {
        case AGHFP_STATE_CONNECTED_IDLE:
        case AGHFP_STATE_DISCONNECTED:
        case AGHFP_STATE_DISCONNECTING:
        case AGHFP_STATE_CONNECTING_LOCAL:
        case AGHFP_STATE_CONNECTING_REMOTE:
        case AGHFP_STATE_CONNECTED_INCOMING:
        case AGHFP_STATE_CONNECTED_ACTIVE:
            return;
        case AGHFP_STATE_CONNECTED_OUTGOING:
            AghfpProfile_SetState(instance, AGHFP_STATE_CONNECTED_ACTIVE);
        case AGHFP_STATE_NULL:
            return;
    }
}

void AghfpProfile_EnableInBandRinging(const bdaddr *bd_addr, bool enable)
{
    DEBUG_LOG("AghfpProfile_EnableInBandRinging");

    aghfpInstanceTaskData *instance = AghfpProfileInstance_GetInstanceForBdaddr(bd_addr);

    if(!instance)
    {
        return;
    }

    aghfpState state = AghfpProfile_GetState(instance);

    switch(state)
    {
    case AGHFP_STATE_CONNECTED_IDLE:
        instance->bitfields.in_band_ring = enable;
        AghfpInBandRingToneEnable(instance->aghfp, enable);
    default:
        return;
    }
}


aghfpInstanceTaskData * AghfpProfileInstance_GetInstanceForSource(voice_source_t source)
{
    aghfpInstanceTaskData* instance = NULL;

    if (source != voice_source_none)
    {
        device_t device = aghfpProfileInstance_FindDeviceFromVoiceSource(source);

        if (device != NULL)
        {
            instance = AghfpProfileInstance_GetInstanceForDevice(device);
        }
    }

    DEBUG_LOG_V_VERBOSE("HfpProfileInstance_GetInstanceForSource(%p) enum:voice_source_t:%d",
                         instance, source);

    return instance;
}

void AghfpProfile_ClientRegister(Task task)
{
    TaskList_AddTask(TaskList_GetFlexibleBaseTaskList(aghfpProfile_GetSlcStatusNotifyList()), task);
}

void AghfpProfile_RegisterStatusClient(Task task)
{
    TaskList_AddTask(TaskList_GetFlexibleBaseTaskList(aghfpProfile_GetStatusNotifyList()), task);
}
