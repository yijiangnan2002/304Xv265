/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file   usb_source_hid.c
\brief  USB source - HID control commands
*/

#include "usb_source.h"
#include "usb_source_hid.h"
#include "logging.h"

static usb_source_hid_interface_t *usb_source_hid_interface = NULL;

void UsbSource_RegisterHid(usb_source_hid_interface_t *hid_interface)
{
    usb_source_hid_interface = hid_interface;
}

void UsbSource_UnregisterHid(void)
{
    usb_source_hid_interface = NULL;
}

bool UsbSource_SendEvent(usb_source_control_event_t event)
{
    if ((UsbSource_IsAudioSupported() || UsbSource_IsVoiceSupported()) &&
            usb_source_hid_interface && usb_source_hid_interface->send_event)
    {
        return usb_source_hid_interface->send_event(event) == USB_RESULT_OK;
    }
    return FALSE;
}

bool UsbSource_SendReport(const uint8 *report, uint16 size)
{
    if (usb_source_hid_interface &&
            usb_source_hid_interface->send_report)
    {
        return usb_source_hid_interface->send_report(report, size) == USB_RESULT_OK;
    }
    return FALSE;
}

void UsbSource_Play(audio_source_t source)
{
    if (source == audio_source_usb)
    {
        UsbSource_SendEvent(USB_SOURCE_PLAY);
    }
}

void UsbSource_Pause(audio_source_t source)
{
    if (source == audio_source_usb)
    {
        UsbSource_SendEvent(USB_SOURCE_PAUSE);
    }
}

void UsbSource_PlayPause(audio_source_t source)
{
    if (source == audio_source_usb)
    {
        UsbSource_SendEvent(USB_SOURCE_PLAY_PAUSE);
    }
}

void UsbSource_Stop(audio_source_t source)
{
    if (source == audio_source_usb)
    {
        UsbSource_SendEvent(USB_SOURCE_STOP);
    }
}

void UsbSource_Forward(audio_source_t source)
{
    if (source == audio_source_usb)
    {
        UsbSource_SendEvent(USB_SOURCE_NEXT_TRACK);
    }
}

void UsbSource_Back(audio_source_t source)
{
    if (source == audio_source_usb)
    {
        UsbSource_SendEvent(USB_SOURCE_PREVIOUS_TRACK);
    }
}

void UsbSource_FastForward(audio_source_t source, bool state)
{
    if (source == audio_source_usb)
    {
        UsbSource_SendEvent(state ?
                                USB_SOURCE_FFWD_ON:
                                USB_SOURCE_FFWD_OFF);
    }
}

void UsbSource_FastRewind(audio_source_t source, bool state)
{
    if (source == audio_source_usb)
    {
        UsbSource_SendEvent(state ?
                                USB_SOURCE_REW_ON:
                                USB_SOURCE_REW_OFF);
    }
}

void UsbSource_AudioVolumeUp(audio_source_t source)
{
    if (source == audio_source_usb)
    {
        UsbSource_SendEvent(USB_SOURCE_VOL_UP);
    }
}

void UsbSource_AudioVolumeDown(audio_source_t source)
{
    if (source == audio_source_usb)
    {
        UsbSource_SendEvent(USB_SOURCE_VOL_DOWN);
    }
}

void UsbSource_AudioSpeakerMute(audio_source_t source, mute_state_t state)
{
    UNUSED(state);
    if (source == audio_source_usb)
    {
        UsbSource_SendEvent(USB_SOURCE_MUTE);
    }
}

void UsbSource_AudioVolumeSetAbsolute(audio_source_t source, volume_t volume)
{
    UNUSED(source);
    UNUSED(volume);

    DEBUG_LOG_WARN("UsbSource::SetAbsolute is not supported");
}

void UsbSource_IncomingCallAccept(voice_source_t source)
{
    if (source == voice_source_usb)
    {
        /* This implementation shall not work with hosts which
         *  does not support HOOK SWITCH usage of USB HID. */
        UsbSource_SendEvent(USB_SOURCE_HOOK_SWITCH_ANSWER);
    }
}

void UsbSource_IncomingCallReject(voice_source_t source)
{
    if (source == voice_source_usb)
    {
        /* Version 4.0 of the "Microsoft Teams Devices General Specification" specifies
         * "Button 1" for Teams compatibility. The Jabra developer documentation shows that
         * a Button is required for correct operation for a call reject. */
        UsbSource_SendEvent(USB_SOURCE_BUTTON_ONE);
    }
}

void UsbSource_OngoingCallTerminate(voice_source_t source)
{
    if (source == voice_source_usb)
    {
        /* This implementation shall not work with hosts which
         *  does not support HOOK SWITCH usage of USB HID. */
        UsbSource_SendEvent(USB_SOURCE_HOOK_SWITCH_TERMINATE);
    }
}

void UsbSource_ToggleMicrophoneMute(voice_source_t source)
{
    if (source == voice_source_usb)
    {
        UsbSource_SendEvent(USB_SOURCE_PHONE_MUTE);
    }
}

void UsbSource_VoiceVolumeUp(voice_source_t source)
{
    if (source == voice_source_usb)
    {
        UsbSource_SendEvent(USB_SOURCE_VOL_UP);
    }
}

void UsbSource_VoiceVolumeDown(voice_source_t source)
{
    if (source == voice_source_usb)
    {
        UsbSource_SendEvent(USB_SOURCE_VOL_DOWN);
    }
}

void UsbSource_VoiceSpeakerMute(voice_source_t source, mute_state_t state)
{
    UNUSED(state);
    if (source == voice_source_usb)
    {
        UsbSource_SendEvent(USB_SOURCE_MUTE);
    }
}
