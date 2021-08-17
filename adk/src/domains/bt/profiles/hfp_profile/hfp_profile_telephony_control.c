/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   hfp_profile
\brief      The voice source telephony control interface implementation for HFP sources
*/

#include "hfp_profile_telephony_control.h"

#include <hfp.h>
#include <logging.h>
#include <message.h>
#include <panic.h>
#include <stdlib.h>
#include <ui.h>

#include "hfp_profile.h"
#include "hfp_profile_instance.h"
#include "hfp_profile_private.h"
#include "hfp_profile_sm.h"
#include "hfp_profile_voice_source_link_prio_mapping.h"
#include "voice_sources_telephony_control_interface.h"
#include "telephony_messages.h"

static void hfpProfile_IncomingCallAccept(voice_source_t source);
static void hfpProfile_IncomingCallReject(voice_source_t source);
static void hfpProfile_OngoingCallTerminate(voice_source_t source);
static void hfpProfile_OngoingCallTransferAudio(voice_source_t source, voice_source_audio_transfer_direction_t direction);
static void hfpProfile_InitiateCallUsingNumber(voice_source_t source, phone_number_t number);
static void hfpProfile_InitiateVoiceDial(voice_source_t source);
static void hfpProfile_CallLastDialed(voice_source_t source);
static void hfpProfile_ToggleMicMute(voice_source_t source);
static unsigned hfpProfile_GetCurrentContext(voice_source_t source);

static const voice_source_telephony_control_interface_t hfp_telephony_interface =
{
    .IncomingCallAccept = hfpProfile_IncomingCallAccept,
    .IncomingCallReject = hfpProfile_IncomingCallReject,
    .OngoingCallTerminate = hfpProfile_OngoingCallTerminate,
    .OngoingCallTransferAudio = hfpProfile_OngoingCallTransferAudio,
    .InitiateCallUsingNumber = hfpProfile_InitiateCallUsingNumber,
    .InitiateVoiceDial = hfpProfile_InitiateVoiceDial,
    .InitiateCallLastDialled = hfpProfile_CallLastDialed,
    .ToggleMicrophoneMute = hfpProfile_ToggleMicMute,
    .GetUiProviderContext = hfpProfile_GetCurrentContext
};

static void hfpProfile_IncomingCallAccept(voice_source_t source)
{
    hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForSource(source);

    DEBUG_LOG("hfpProfile_IncomingCallAccept(%p) enum:voice_source_t:%d", instance, source);

    PanicNull(instance);

    switch(instance->state)
    {
        case HFP_STATE_DISCONNECTED:
        {
            /* Connect SLC */
            if (!HfpProfile_ConnectHandset())
            {
                Telephony_NotifyError(source);
                break;
            }
        }
        /* Fall through */

        case HFP_STATE_CONNECTED_INCOMING:
        {
            Telephony_NotifyCallAnswered(source);

            /* Send message into HFP state machine */
            MAKE_HFP_MESSAGE(HFP_INTERNAL_HFP_CALL_ACCEPT_REQ);
            message->instance = instance;
            MessageSendConditionally(HfpProfile_GetInstanceTask(instance), HFP_INTERNAL_HFP_CALL_ACCEPT_REQ,
                                     message, HfpProfileInstance_GetLock(instance));
        }
        break;

        default:
            break;
    }
}

static void hfpProfile_IncomingCallReject(voice_source_t source)
{
    hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForSource(source);

    DEBUG_LOG("hfpProfile_IncomingCallReject(%p) enum:voice_source_t:%d", instance, source);

    PanicNull(instance);

    switch (instance->state)
    {
        case HFP_STATE_DISCONNECTED:
        {
            /* Connect SLC */
            if (!HfpProfile_ConnectHandset())
            {
                /* Play error tone to indicate we don't have a valid address */
                Telephony_NotifyError(source);
                break;
            }
        }
        /* Fall through */

        case HFP_STATE_CONNECTED_INCOMING:
        {
            Telephony_NotifyCallRejected(source);

            MAKE_HFP_MESSAGE(HFP_INTERNAL_HFP_CALL_REJECT_REQ);
            message->instance = instance;
            /* Send message into HFP state machine */
            MessageSendConditionally(HfpProfile_GetInstanceTask(instance), HFP_INTERNAL_HFP_CALL_REJECT_REQ,
                                     message, HfpProfileInstance_GetLock(instance));
        }
        break;

        default:
            break;
    }
}

static void hfpProfile_OngoingCallTerminate(voice_source_t source)
{
    hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForSource(source);

    DEBUG_LOG("hfpProfile_OngoingCallTerminate(%p) enum:voice_source_t:%d", instance, source);

    PanicNull(instance);

    switch (instance->state)
    {
        case HFP_STATE_DISCONNECTED:
        {
            /* Connect SLC */
            if (!HfpProfile_ConnectHandset())
            {
                Telephony_NotifyError(source);
                break;
            }
        }
        /* Fall through */

        case HFP_STATE_CONNECTED_INCOMING:
        case HFP_STATE_CONNECTED_OUTGOING:
        case HFP_STATE_CONNECTED_ACTIVE:
        {
            Telephony_NotifyCallTerminated(source);

            MAKE_HFP_MESSAGE(HFP_INTERNAL_HFP_CALL_HANGUP_REQ);
            message->instance = instance;
            /* Send message into HFP state machine */
            MessageSendConditionally(HfpProfile_GetInstanceTask(instance), HFP_INTERNAL_HFP_CALL_HANGUP_REQ,
                                     message, HfpProfileInstance_GetLock(instance));
        }
        break;

        default:
            break;
    }
}

static void hfpProfile_TransferCallAudio(voice_source_t source, voice_source_audio_transfer_direction_t direction)
{
    hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForSource(source);

    DEBUG_LOG("hfpProfile_TransferCallAudio(%p) enum:voice_source_t:%d", instance, source);

    PanicNull(instance);

    switch (instance->state)
    {
        case HFP_STATE_CONNECTED_OUTGOING:
        case HFP_STATE_CONNECTED_INCOMING:
        case HFP_STATE_CONNECTED_ACTIVE:
        {
            HFP_INTERNAL_HFP_TRANSFER_REQ_T * message = (HFP_INTERNAL_HFP_TRANSFER_REQ_T*)PanicUnlessNew(HFP_INTERNAL_HFP_TRANSFER_REQ_T);

            message->direction = direction;
            message->source = source;
            MessageSendConditionally(HfpProfile_GetInstanceTask(instance), HFP_INTERNAL_HFP_TRANSFER_REQ,
                                     message, HfpProfileInstance_GetLock(instance));

            Telephony_NotifyCallAudioTransferred(source);
        }
        break;

        default:
            break;
    }
}

static void hfpProfile_OngoingCallTransferAudio(voice_source_t source, voice_source_audio_transfer_direction_t direction)
{
    hfpProfile_TransferCallAudio(source, direction);
}

static void hfpProfile_InitiateCallUsingNumber(voice_source_t source, phone_number_t number)
{
    hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForSource(source);

    DEBUG_LOG("hfpProfile_InitiateCallUsingNumber(%p) enum:voice_source_t:%d", instance, source);

    PanicNull(instance);

    switch(instance->state)
    {
        case HFP_STATE_DISCONNECTED:
        {
            /* Connect SLC */
            if (!HfpProfile_ConnectHandset())
            {
                /* Play error tone to indicate we don't have a valid address */
                Telephony_NotifyError(source);
                break;
            }
        }
        /* Fall through */
        case HFP_STATE_CONNECTED_IDLE:
        {
            HFP_INTERNAL_NUMBER_DIAL_REQ_T * message= (HFP_INTERNAL_NUMBER_DIAL_REQ_T *)PanicNull(calloc(1,sizeof(HFP_INTERNAL_NUMBER_DIAL_REQ_T)+number.number_of_digits-1));
            message->length = number.number_of_digits;
            message->instance = instance;
            memmove(message->number, number.digits, number.number_of_digits);
            MessageSendConditionally(HfpProfile_GetInstanceTask(instance), HFP_INTERNAL_NUMBER_DIAL_REQ,
                                     message, HfpProfileInstance_GetLock(instance));

            Telephony_NotifyCallInitiatedUsingNumber(source);
        }
        break;
        default:
            break;
    }

}

static void hfpProfile_InitiateVoiceDial(voice_source_t source)
{
    hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForSource(source);

    DEBUG_LOG("hfpProfile_InitiateVoiceDial(%p) enum:voice_source_t:%d", instance, source);

    PanicNull(instance);

    switch (instance->state)
    {
        case HFP_STATE_DISCONNECTED:
        {
            if(!HfpProfile_ConnectHandset())
            {
                Telephony_NotifyError(HfpProfileInstance_GetVoiceSourceForInstance(instance));
                break;
            }
        }

        case HFP_STATE_CONNECTING_LOCAL:
        case HFP_STATE_CONNECTING_REMOTE:
        case HFP_STATE_CONNECTED_IDLE:
        {
            MAKE_HFP_MESSAGE(HFP_INTERNAL_HFP_VOICE_DIAL_REQ);
            message->instance = instance;
            /* Send message into HFP state machine */
            MessageSendConditionally(HfpProfile_GetInstanceTask(instance), HFP_INTERNAL_HFP_VOICE_DIAL_REQ,
                                     message, HfpProfileInstance_GetLock(instance));
        }
        break;

        default:
            break;
    }

}

/*! \brief Attempt last number redial

    Initiate last number redial, attempt to connect SLC first if not currently
    connected.
*/
static void hfpProfile_CallLastDialed(voice_source_t source)
{
    hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForSource(source);

    DEBUG_LOG("hfpProfile_CallLastDialed(%p) enum:voice_source_t:%d", instance, source);

    PanicNull(instance);

    switch (appHfpGetState(instance))
    {
        case HFP_STATE_DISCONNECTED:
        {
            /* Connect SLC */
            if (!HfpProfile_ConnectHandset())
            {
                Telephony_NotifyError(HfpProfileInstance_GetVoiceSourceForInstance(instance));
                break;
            }
        }
        /* Fall through */

        case HFP_STATE_CONNECTING_LOCAL:
        case HFP_STATE_CONNECTING_REMOTE:
        case HFP_STATE_CONNECTED_IDLE:
        {
            MAKE_HFP_MESSAGE(HFP_INTERNAL_HFP_LAST_NUMBER_REDIAL_REQ);
            message->instance = instance;
            /* Send message into HFP state machine */
            MessageSendConditionally(HfpProfile_GetInstanceTask(instance), HFP_INTERNAL_HFP_LAST_NUMBER_REDIAL_REQ,
                                     message, HfpProfileInstance_GetLock(instance));
        }
        break;

        default:
            break;
    }
}

static void hfpProfile_ToggleMicMute(voice_source_t source)
{
    hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForSource(source);

    DEBUG_LOG("hfpProfile_ToggleMicMute(%p) enum:voice_source_t:%d", instance, source);

    PanicNull(instance);

    switch (appHfpGetState(instance))
    {
        case HFP_STATE_CONNECTED_ACTIVE:
        {
            MAKE_HFP_MESSAGE(HFP_INTERNAL_HFP_MUTE_REQ);

            /* Send message into HFP state machine */
            message->mute = !HfpProfile_IsMicrophoneMuted(instance);
            message->instance = instance;
            MessageSendConditionally(HfpProfile_GetInstanceTask(instance), HFP_INTERNAL_HFP_MUTE_REQ,
                                     message, HfpProfileInstance_GetLock(instance));
        }
        break;

        default:
            break;
    }
}

/*! \brief provides hfp (telephony) current context to ui module

    \param[in]  void

    \return     current context of hfp module.
*/
static unsigned hfpProfile_GetCurrentContext(voice_source_t source)
{
    voice_source_provider_context_t context = BAD_CONTEXT;

    hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForSource(source);

    if (instance == NULL)
        return context_voice_disconnected;

    switch(appHfpGetState(instance))
    {
    case HFP_STATE_NULL:
        break;
    case HFP_STATE_DISCONNECTING:
    case HFP_STATE_DISCONNECTED:
    case HFP_STATE_CONNECTING_LOCAL:
    case HFP_STATE_CONNECTING_REMOTE:
        context = context_voice_disconnected;
        break;
    case HFP_STATE_CONNECTED_IDLE:
        context = context_voice_connected;
        break;
    case HFP_STATE_CONNECTED_OUTGOING:
        context = context_voice_ringing_outgoing;
        break;
    case HFP_STATE_CONNECTED_INCOMING:
        context = context_voice_ringing_incoming;
        break;
    case HFP_STATE_CONNECTED_ACTIVE:
        context = context_voice_in_call;
        break;
    default:
        break;
    }
    return (unsigned)context;
}

const voice_source_telephony_control_interface_t * HfpProfile_GetTelephonyControlInterface(void)
{
    return &hfp_telephony_interface;
}
