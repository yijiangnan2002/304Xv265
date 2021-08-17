/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file   usb_source_context.c
\brief  USB source context is returned via Media Control interface.
        It is up to the USB audio function driver to update the current context
        when it changes.
*/

#include "usb_source.h"
#include "ui.h"

static audio_source_provider_context_t usb_source_audio_ctx = context_audio_disconnected;
static voice_source_provider_context_t usb_source_voice_ctx = context_voice_disconnected;

void UsbSource_SetAudioCtx(audio_source_provider_context_t ctx)
{
    usb_source_audio_ctx = ctx;
}

unsigned UsbSource_GetAudioCtx(audio_source_t source)
{
    return (unsigned)((source == audio_source_usb) ?
                          usb_source_audio_ctx :
                          BAD_CONTEXT);
}

bool UsbSource_IsAudioSupported(void)
{
    return usb_source_audio_ctx != context_audio_disconnected;
}

void UsbSource_SetVoiceCtx(voice_source_provider_context_t ctx)
{
    usb_source_voice_ctx = ctx;
}

unsigned UsbSource_GetVoiceCtx(voice_source_t source)
{
    return (unsigned)((source == voice_source_usb) ?
                          usb_source_voice_ctx :
                          BAD_CONTEXT);
}

bool UsbSource_IsVoiceSupported(void)
{
    return usb_source_voice_ctx != context_voice_disconnected;
}
