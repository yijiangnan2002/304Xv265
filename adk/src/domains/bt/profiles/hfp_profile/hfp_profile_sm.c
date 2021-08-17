/*!
\copyright  Copyright (c) 2008 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      HFP state machine component.
*/

#include "hfp_profile_sm.h"

#include "system_state.h"
#include "bt_device.h"
#include "device_properties.h"
#include "hfp_profile_config.h"
#include "link_policy.h"
#include "voice_sources.h"
#include "hfp_profile_audio.h"
#include "hfp_profile_instance.h"
#include "hfp_profile.h"
#include "hfp_profile_private.h"
#include "hfp_profile_sm.h"
#include "hfp_profile_telephony_control.h"
#include "hfp_profile_voice_source_link_prio_mapping.h"
#include "hfp_profile_volume.h"
#include "hfp_profile_volume_observer.h"
#include "telephony_messages.h"
#include "volume_messages.h"
#include "volume_utils.h"
#include "kymera.h"

#include <profile_manager.h>
#include <av.h>
#include <connection_manager.h>
#include <device.h>
#include <device_list.h>
#include <focus_voice_source.h>
#include <logging.h>
#include <mirror_profile.h>
#include <message.h>
#include "ui.h"

#include <logging.h>

/*! \brief Set flag to indicate HFP is supported on a device

    \param bd_addr Pointer to read-only device BT address.
    \param profile HFP profile version support by the device
*/
static void hfpProfile_SetHfpIsSupported(const bdaddr *bd_addr, hfp_profile profile)
{
    device_t device = BtDevice_SetSupportedProfile(bd_addr, DEVICE_PROFILE_HFP);
    if (device)
    {
        Device_SetPropertyU8(device, device_property_hfp_profile, (uint8)profile);
    }
}

/*! \brief Enter 'connected' state

    The HFP state machine has entered 'connected' state, this means that
    there is a SLC active.  At this point we need to retreive the remote device's
    support features to determine which (e)SCO packets it supports.  Also if there's an
    incoming or active call then answer/transfer the call to headset.
*/
static void appHfpEnterConnected(hfpInstanceTaskData* instance, voice_source_t source)
{
    hfp_link_priority_t link = HfpProfile_GetHfpLinkPrioForVoiceSource(source);

    DEBUG_LOG_INFO("appHfpEnterConnected(%p) enum:voice_source_t:%d", instance, source);

    /* Update most recent connected device */
    appDeviceUpdateMruDevice(&instance->ag_bd_addr);

    /* Mark this device as supporting HFP */
    hfpProfile_SetHfpIsSupported(&instance->ag_bd_addr, instance->profile);

    /* Read the remote supported features of the AG */
    ConnectionReadRemoteSuppFeatures(HfpProfile_GetInstanceTask(instance), instance->slc_sink);

    /* Clear detach pending flag */
    instance->bitfields.detach_pending = FALSE;

    /* Check if connected as HFP 1.5+ */
    if (instance->profile == hfp_handsfree_profile)
    {
        /* Inform AG of the current gain settings */
        /* hfp_primary_link indicates the link that was connected first */
        uint8 value = appHfpGetVolume(instance);

        HfpVolumeSyncSpeakerGainRequest(link, &value);

        device_t device = HfpProfileInstance_FindDeviceFromInstance(instance);
        PanicNull(device);
        if (!Device_GetPropertyU8(device, device_property_hfp_mic_gain, &value))
        {
            value = HFP_MICROPHONE_GAIN;
        }

        HfpVolumeSyncMicrophoneGainRequest(link, &value);
    }

    /* If this is completing a connect request, send confirmation for this device */
    ProfileManager_SendConnectCfmToTaskList(TaskList_GetBaseTaskList(&hfp_profile_task_data.connect_request_clients),
                                            &instance->ag_bd_addr, profile_manager_success, HfpProfile_FindClientSendConnectCfm);

    Telephony_NotifyConnected(source);

    /* Tell clients we have connected */
    MAKE_HFP_MESSAGE(APP_HFP_CONNECTED_IND);
    message->bd_addr = instance->ag_bd_addr;
    TaskList_MessageSend(TaskList_GetFlexibleBaseTaskList(appHfpGetStatusNotifyList()), APP_HFP_CONNECTED_IND, message);

#if defined(HFP_CONNECT_AUTO_ANSWER) || defined(HFP_CONNECT_AUTO_TRANSFER)
    if (instance->profile != hfp_headset_profile)
    {
#if defined(HFP_CONNECT_AUTO_ANSWER)
        /* Check if incoming call */
        if (appHfpGetState(instance) == HFP_STATE_CONNECTED_INCOMING)
        {
            /* Accept the incoming call */
            VoiceSources_AcceptIncomingCall(source);
        }
#endif
#if defined(HFP_CONNECT_AUTO_TRANSFER)
        /* Check if incoming call */
        if (appHfpGetState(instance) == HFP_STATE_CONNECTED_ACTIVE)
        {
            /* Check SCO is not active */
            if (instance->sco_sink == 0)
            {
                /* Attempt to transfer audio */
                HfpAudioTransferConnection(instance->hfp, hfp_audio_to_hfp, instance->sco_supported_packets ^ sync_all_edr_esco, 0);
            }
        }
#endif
    }
#endif
}

/*! \brief Exit 'connected' state

    The HFP state machine has exited 'connected' state, this means that
    the SLC has closed.  Make sure any SCO link is disconnected.
*/
static void appHfpExitConnected(hfpInstanceTaskData* instance, voice_source_t source)
{
    DEBUG_LOG_INFO("appHfpExitConnected(%p) enum:voice_source_t:%d", instance, source);

    /* Unregister for battery updates */
    appBatteryUnregister(HfpProfile_GetInstanceTask(instance));

    /* Reset hf_indicator_assigned_num */
     instance->bitfields.hf_indicator_assigned_num = hf_indicators_invalid;

    /* Check if SCO is still up */
    if (instance->sco_sink)
    {
        hfp_link_priority_t link = HfpProfile_GetHfpLinkPrioForVoiceSource(source);

        /* Disconnect SCO */
        HfpAudioDisconnectRequest(link);
    }

    /* Handle any pending config write */
    if (MessageCancelFirst(&hfp_profile_task_data.task, HFP_INTERNAL_CONFIG_WRITE_REQ) > 0)
    {
        device_t device = HfpProfileInstance_FindDeviceFromInstance(instance);
        HfpProfile_HandleConfigWriteRequest(device);
    }
}

/*! \brief Enter 'connected idle' state

    The HFP state machine has entered 'connected idle' state, this means that
    there is a SLC active but no active call in process.  When entering this
    state we clear the ACL lock that was when entering the 'connecting local'
    state, this allows any other pending connections to proceed.  Any suspended
    AV streaming can now resume.
*/
static void appHfpEnterConnectedIdle(hfpInstanceTaskData* instance, voice_source_t source)
{
    DEBUG_LOG_INFO("appHfpEnterConnectedIdle(%p) enum:voice_source_t:%d", instance, source);
}

/*! \brief Exit 'connected idle' state

    The HFP state machine has exited 'connected idle' state.
*/
static void appHfpExitConnectedIdle(hfpInstanceTaskData* instance, voice_source_t source)
{
    DEBUG_LOG_INFO("appHfpExitConnectedIdle(%p) enum:voice_source_t:%d", instance, source);
}

/*! \brief Enter 'connected outgoing' state

    The HFP state machine has entered 'connected outgoing' state, this means
    that we are in the process of making an outgoing call, just make sure
    that we have suspended AV streaming. Update UI to indicate active call.
*/
static void appHfpEnterConnectedOutgoing(hfpInstanceTaskData* instance, voice_source_t source)
{
    DEBUG_LOG_INFO("appHfpEnterConnectedOutgoing(%p) enum:voice_source_t:%d", instance, source);

    Telephony_NotifyCallActive(source);
}

/*! \brief Exit 'connected outgoing' state

    The HFP state machine has exited 'connected outgoing' state.
*/
static void appHfpExitConnectedOutgoing(hfpInstanceTaskData* instance, voice_source_t source)
{    
    DEBUG_LOG_INFO("appHfpExitConnectedOutgoing(%p) enum:voice_source_t:%d", instance, source);
}

/*! \brief Enter 'connected incoming' state

    The HFP state machine has entered 'connected incoming' state, this means
    that there's an incoming call in progress.  Update UI to indicate incoming
    call.
*/
static void appHfpEnterConnectedIncoming(hfpInstanceTaskData* instance, voice_source_t source)
{
    DEBUG_LOG_INFO("appHfpEnterConnectedIncoming(%p) enum:voice_source_t:%d", instance, source);
    
    appDeviceUpdateMruDevice(&instance->ag_bd_addr);

    TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(appHfpGetStatusNotifyList()), APP_HFP_SCO_INCOMING_RING_IND);

    Telephony_NotifyCallIncoming(source);
}

/*! \brief Exit 'connected incoming' state

    The HFP state machine has exited 'connected incoming' state, this means
    that the incoming call has either been accepted or rejected.  Make sure
    any ring tone is cancelled.
*/
static void appHfpExitConnectedIncoming(hfpInstanceTaskData* instance, voice_source_t source)
{
    DEBUG_LOG_INFO("appHfpExitConnectedIncoming(%p) enum:voice_source_t:%d", instance, source);

    /* Clear call accepted flag */
    instance->bitfields.call_accepted = FALSE;

    TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(appHfpGetStatusNotifyList()), APP_HFP_SCO_INCOMING_ENDED_IND);

    Telephony_NotifyCallIncomingEnded(source);

    /* Cancel HSP incoming call timeout */
    MessageCancelFirst(HfpProfile_GetInstanceTask(instance), HFP_INTERNAL_HSP_INCOMING_TIMEOUT);
}

/*! \brief Enter 'connected active' state

    The HFP state machine has entered 'connected active' state, this means
    that a call is in progress, just make sure that we have suspended AV
    streaming.
*/
static void appHfpEnterConnectedActive(hfpInstanceTaskData* instance, voice_source_t source)
{
    DEBUG_LOG_INFO("appHfpEnterConnectedActive(%p) enum:voice_source_t:%d", instance, source);

    appDeviceUpdateMruDevice(&instance->ag_bd_addr);
    BandwidthManager_FeatureStart(BANDWIDTH_MGR_FEATURE_ESCO);
}

/*! \brief Exit 'connected active' state

    The HFP state machine has exited 'connected active' state, this means
    that a call has just finished.  Make sure mute is cancelled.
*/
static void appHfpExitConnectedActive(hfpInstanceTaskData* instance, voice_source_t source)
{
    DEBUG_LOG_INFO("appHfpExitConnectedActive(%p) enum:voice_source_t:%d", instance, source);

    Telephony_NotifyMicrophoneUnmuted(source);

    instance->bitfields.mute_active = FALSE;

    Telephony_NotifyCallEnded(source);
    BandwidthManager_FeatureStop(BANDWIDTH_MGR_FEATURE_ESCO);
}

/*! \brief Enter 'disconnecting' state

    The HFP state machine has entered 'disconnecting' state, this means
    that the SLC should be disconnected.  Set the operation lock to block
    any pending operations.
*/
static void appHfpEnterDisconnecting(hfpInstanceTaskData* instance, voice_source_t source)
{
    hfp_link_priority link_priority;

    DEBUG_LOG_INFO("appHfpEnterDisconnecting(%p) enum:voice_source_t:%d", instance, source);

    /* Set operation lock */
    HfpProfileInstance_SetLock(instance, TRUE);


    link_priority = HfpProfile_GetHfpLinkPrioForVoiceSource(source);

    HfpSlcDisconnectRequest(link_priority);
}

/*! \brief Exit 'disconnecting' state

    The HFP state machine has exited 'disconnecting' state, this means
    that the SLC is now disconnected.  Clear the operation lock to allow
    any pending operations.
*/
static void appHfpExitDisconnecting(hfpInstanceTaskData* instance, voice_source_t source)
{
    DEBUG_LOG_INFO("appHfpExitDisconnecting(%p) enum:voice_source_t:%d", instance, source);

    /* Clear operation lock */
    HfpProfileInstance_SetLock(instance, FALSE);
}

/*! \brief Enter 'disconnected' state

    The HFP state machine has entered 'disconnected' state, this means
    that there is no active SLC.  Reset all flags, clear the ACL lock to
    allow pending connections to proceed.  Also make sure AV streaming is
    resumed if previously suspended.
*/
static void appHfpEnterDisconnected(hfpInstanceTaskData* instance, voice_source_t source)
{
    DEBUG_LOG_INFO("appHfpEnterDisconnected(%p) enum:voice_source_t:%d", instance, source);

    if (TaskList_Size(TaskList_GetBaseTaskList(&hfp_profile_task_data.connect_request_clients)) != 0)
    {
        if (instance->bitfields.disconnect_reason == APP_HFP_CONNECT_FAILED)
        {
            /* If this is due to an unsuccessful connect request, send confirmation for this device */
            ProfileManager_SendConnectCfmToTaskList(TaskList_GetBaseTaskList(&hfp_profile_task_data.connect_request_clients),
                                                    &instance->ag_bd_addr, profile_manager_failed, HfpProfile_FindClientSendConnectCfm);
        }
    }
    if (TaskList_Size(TaskList_GetBaseTaskList(&hfp_profile_task_data.disconnect_request_clients)) != 0)
    {
        if (instance->bitfields.disconnect_reason == APP_HFP_DISCONNECT_NORMAL)
        {
            ProfileManager_SendConnectCfmToTaskList(TaskList_GetBaseTaskList(&hfp_profile_task_data.disconnect_request_clients),
                                                    &instance->ag_bd_addr,
                                                    profile_manager_success,
                                                    HfpProfile_FindClientSendDisconnectCfm);
        }
    }

    /* Tell clients we have disconnected */
    MAKE_HFP_MESSAGE(APP_HFP_DISCONNECTED_IND);
    message->bd_addr = instance->ag_bd_addr;
    message->reason =  instance->bitfields.disconnect_reason;
    TaskList_MessageSend(TaskList_GetFlexibleBaseTaskList(appHfpGetStatusNotifyList()), APP_HFP_DISCONNECTED_IND, message);

    /* Clear status flags */
    instance->bitfields.caller_id_active = FALSE;
    instance->bitfields.voice_recognition_active = FALSE;
    instance->bitfields.voice_recognition_request = FALSE;
    instance->bitfields.mute_active = FALSE;
    instance->bitfields.in_band_ring = FALSE;
    instance->bitfields.call_accepted = FALSE;

    /* Clear call state indication */
    instance->bitfields.call_state = 0;
}

static void appHfpExitDisconnected(hfpInstanceTaskData* instance, voice_source_t source)
{
    DEBUG_LOG_INFO("appHfpExitDisconnected(%p) enum:voice_source_t:%d", instance, source);

    /* Reset disconnect reason */
    instance->bitfields.disconnect_reason = APP_HFP_CONNECT_FAILED;
}

/*! \brief Enter 'connecting remote' state

    The HFP state machine has entered 'connecting remote' state, this is due
    to receiving a incoming SLC indication. Set operation lock to block any
    pending operations.
*/
static void appHfpEnterConnectingRemote(hfpInstanceTaskData* instance, voice_source_t source)
{
    DEBUG_LOG_INFO("appHfpEnterConnectingRemote(%p) enum:voice_source_t:%d", instance, source);

    /* Set operation lock */
    HfpProfileInstance_SetLock(instance, TRUE);

    /* Clear detach pending flag */
    instance->bitfields.detach_pending = FALSE;
}

/*! \brief Exit 'connecting remote' state

    The HFP state machine has exited 'connecting remote' state.  Clear the
    operation lock to allow pending operations on this instance to proceed.
*/
static void appHfpExitConnectingRemote(hfpInstanceTaskData* instance, voice_source_t source)
{
    DEBUG_LOG_INFO("appHfpExitConnectingRemote(%p) enum:voice_source_t:%d", instance, source);

    /* Clear operation lock */
    HfpProfileInstance_SetLock(instance, FALSE);
}


/*! \brief Enter 'connecting local' state

    The HFP state machine has entered 'connecting local' state.  Set the
    'connect busy' flag and operation lock to serialise connect attempts,
    reset the page timeout to the default and attempt to connect SLC.
    Make sure AV streaming is suspended.
*/
static void appHfpEnterConnectingLocal(hfpInstanceTaskData* instance, voice_source_t source)
{
    DEBUG_LOG_INFO("appHfpEnterConnectingLocal(%p) enum:voice_source_t:%d", instance, source);

    /* Set operation lock */
    HfpProfileInstance_SetLock(instance, TRUE);

    TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(appHfpGetStatusNotifyList()), PAGING_START);
    
    /* Clear detach pending flag */
    instance->bitfields.detach_pending = FALSE;

    /* Start HFP connection
     * Previous version was using profile as hfp_handsfree_107_profile so check
     * here is done as ">=" to retain the compatibility. */
    if (instance->profile >= hfp_handsfree_profile)
    {
        DEBUG_LOG("Connecting HFP to AG (%x,%x,%lx)", instance->ag_bd_addr.nap, instance->ag_bd_addr.uap, instance->ag_bd_addr.lap);

        /* Issue connect request for HFP */
        HfpSlcConnectRequestEx(&instance->ag_bd_addr, hfp_handsfree_and_headset, hfp_handsfree_all,
                                BtDevice_WasQhsConnected(&instance->ag_bd_addr) ? hfp_connect_ex_qhs : hfp_connect_ex_none);
    }
    else
    {
        Panic();
    }
}

/*! \brief Exit 'connecting local' state

    The HFP state machine has exited 'connecting local' state, the connection
    attempt was successful or it failed.  Clear the 'connect busy' flag and
    operation lock to allow pending connection attempts and any pending
    operations on this instance to proceed.  AV streaming can resume now.
*/
static void appHfpExitConnectingLocal(hfpInstanceTaskData* instance, voice_source_t source)
{
    DEBUG_LOG_INFO("appHfpExitConnectingLocal(%p) enum:voice_source_t:%d", instance, source);

    /* Clear operation lock */
    HfpProfileInstance_SetLock(instance, FALSE);

    TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(appHfpGetStatusNotifyList()), PAGING_STOP);

    /* We have finished (successfully or not) attempting to connect, so
     * we can relinquish our lock on the ACL.  Bluestack will then close
     * the ACL when there are no more L2CAP connections */
    ConManagerReleaseAcl(&instance->ag_bd_addr);
}

/*! \brief Set HFP state

    Called to change state.  Handles calling the state entry and exit functions.
    Note: The entry and exit functions will be called regardless of whether or not
    the state actually changes value.
*/
void appHfpSetState(hfpInstanceTaskData* instance, hfpState state)
{
    /* copy old state */
    hfpState old_state = appHfpGetState(instance);
    voice_source_t source = HfpProfileInstance_GetVoiceSourceForInstance(instance);
    voice_source_t focused_source = voice_source_none;

    DEBUG_LOG("appHfpSetState(%p, enum:hfpState:%d -> enum:hfpState:%d)",
              instance, old_state, state);

    /* Handle state exit functions */
    switch (old_state)
    {
        case HFP_STATE_CONNECTING_LOCAL:
            appHfpExitConnectingLocal(instance, source);
            break;
        case HFP_STATE_CONNECTING_REMOTE:
            appHfpExitConnectingRemote(instance, source);
            break;
        case HFP_STATE_CONNECTED_IDLE:
            appHfpExitConnectedIdle(instance, source);
            if (state < HFP_STATE_CONNECTED_IDLE || state > HFP_STATE_CONNECTED_ACTIVE)
                appHfpExitConnected(instance, source);
            break;
        case HFP_STATE_CONNECTED_ACTIVE:
            appHfpExitConnectedActive(instance, source);
            if (state < HFP_STATE_CONNECTED_IDLE || state > HFP_STATE_CONNECTED_ACTIVE)
                appHfpExitConnected(instance, source);
            break;
        case HFP_STATE_CONNECTED_INCOMING:
            appHfpExitConnectedIncoming(instance, source);
            if (state < HFP_STATE_CONNECTED_IDLE || state > HFP_STATE_CONNECTED_ACTIVE)
                appHfpExitConnected(instance, source);
            break;
        case HFP_STATE_CONNECTED_OUTGOING:
            appHfpExitConnectedOutgoing(instance, source);
            if (state < HFP_STATE_CONNECTED_IDLE || state > HFP_STATE_CONNECTED_ACTIVE)
                appHfpExitConnected(instance, source);
            break;
        case HFP_STATE_DISCONNECTING:
            appHfpExitDisconnecting(instance, source);
            break;
        case HFP_STATE_DISCONNECTED:
            appHfpExitDisconnected(instance, source);
            break;
        default:
            break;
    }

    /* Set new state */
    instance->state = state;

    /* Handle state entry functions */
    switch (state)
    {
        case HFP_STATE_CONNECTING_LOCAL:
            appHfpEnterConnectingLocal(instance, source);
            break;
        case HFP_STATE_CONNECTING_REMOTE:
            appHfpEnterConnectingRemote(instance, source);
            break;
        case HFP_STATE_CONNECTED_IDLE:
            if (old_state < HFP_STATE_CONNECTED_IDLE || old_state > HFP_STATE_CONNECTED_ACTIVE)
                appHfpEnterConnected(instance, source);
            appHfpEnterConnectedIdle(instance, source);
            break;
        case HFP_STATE_CONNECTED_ACTIVE:
            if (old_state < HFP_STATE_CONNECTED_IDLE || old_state > HFP_STATE_CONNECTED_ACTIVE)
                appHfpEnterConnected(instance, source);
            appHfpEnterConnectedActive(instance, source);
            break;
        case HFP_STATE_CONNECTED_INCOMING:
            if (old_state < HFP_STATE_CONNECTED_IDLE || old_state > HFP_STATE_CONNECTED_ACTIVE)
                appHfpEnterConnected(instance, source);
            appHfpEnterConnectedIncoming(instance, source);
            break;
        case HFP_STATE_CONNECTED_OUTGOING:
            if (old_state < HFP_STATE_CONNECTED_IDLE || old_state > HFP_STATE_CONNECTED_ACTIVE)
                appHfpEnterConnected(instance, source);
            appHfpEnterConnectedOutgoing(instance, source);
            break;
        case HFP_STATE_DISCONNECTING:
            appHfpEnterDisconnecting(instance, source);
            break;
        case HFP_STATE_DISCONNECTED:
            appHfpEnterDisconnected(instance, source);
            break;
        default:
            break;
    }

    Focus_GetVoiceSourceForContext(ui_provider_telephony, &focused_source);
    if (focused_source == source)
    {
        Ui_InformContextChange(ui_provider_telephony, VoiceSources_GetSourceContext(source));
    }
    else
    {
        DEBUG_LOG_VERBOSE("appHfpSetState didn't push context for unfocused enum:voice_source_t:%d", source);
    }

    /* Update link policy following change in state */
    appLinkPolicyUpdatePowerTable(HfpProfile_GetHandsetBdAddr(instance));
}

hfpState appHfpGetState(hfpInstanceTaskData* instance)
{
    PanicNull(instance);
    return instance->state;
}
