/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   USB source
\brief      USB audio/voice sources media and volume control interfaces
*/

#ifndef USB_SOURCE_H
#define USB_SOURCE_H

#include "audio_sources.h"
#include "voice_sources.h"
#include "usb_device.h"

/*! Return if USB audio is supported by the current configuration */
bool UsbSource_IsAudioSupported(void);

/*! Set USB audio context
 *
 * Used by USB audio function driver to update audio context reported
 * via Media Control interface. */
void UsbSource_SetAudioCtx(audio_source_provider_context_t cxt);

/*! Return USB audio context */
unsigned UsbSource_GetAudioCtx(audio_source_t source);

/*! Return if USB Voice is supported by the current configuration */
bool UsbSource_IsVoiceSupported(void);

/*! Set USB Voice context
 *
 * Used by USB audio function driver to update Voice context. */
void UsbSource_SetVoiceCtx(voice_source_provider_context_t cxt);

/*! Return USB Voice context */
unsigned UsbSource_GetVoiceCtx(voice_source_t source);

/*! HID control events */
typedef enum
{
    /*! Send a HID play/pause event over USB */
    USB_SOURCE_PLAY_PAUSE,
    /*! Send a HID stop event over USB */
    USB_SOURCE_STOP,
    /*! Send a HID next track event over USB */
    USB_SOURCE_NEXT_TRACK,
    /*! Send a HID previous track event over USB */
    USB_SOURCE_PREVIOUS_TRACK,
    /*! Send a HID play event over USB */
    USB_SOURCE_PLAY,
    /*! Send a HID pause event over USB */
    USB_SOURCE_PAUSE,
    /*! Send a HID Volume Up event over USB */
    USB_SOURCE_VOL_UP,
    /*! Send a HID Volume Down event over USB */
    USB_SOURCE_VOL_DOWN,
    /*! Send a HID Mute event over USB */
    USB_SOURCE_MUTE,
    /*! Send a HID Fast Forward ON event over USB */
    USB_SOURCE_FFWD_ON,
    /*! Send a HID Fast Forward OFF event over USB */
    USB_SOURCE_FFWD_OFF,
    /*! Send a HID consumer Rewind ON event over USB */
    USB_SOURCE_REW_ON,
    /*! Send a HID consumer Rewind OFF event over USB */
    USB_SOURCE_REW_OFF,

    /*! Send a HID Telephony Mute event over USB */
    USB_SOURCE_PHONE_MUTE,
    /*! Send a HID Telephony Call Answer event over USB */
    USB_SOURCE_HOOK_SWITCH_ANSWER,
    /*! Send a HID Telephony Call Terminate event over USB */
    USB_SOURCE_HOOK_SWITCH_TERMINATE,
    /*! Send a HID Telephony Flash event over USB */
    USB_SOURCE_FLASH,
    /*! Send a HID Telephony Programmable button 1 event over USB */
    USB_SOURCE_BUTTON_ONE,

    /*! Number of supported events */
    USB_SOURCE_EVT_COUNT
} usb_source_control_event_t;

/*! HID interface for USB source */
typedef struct
{
    /*! Callback to send HID event */
    usb_result_t (*send_event)(usb_source_control_event_t event);
    /*! Callback to send HID report */
    usb_result_t (*send_report)(const uint8 *report, uint16 size);
} usb_source_hid_interface_t;

/*! Register HID send event handler
 *
 * Used by USB HID Consumer Transport class driver to register a callback
 * for sending HID control events to the host. */
void UsbSource_RegisterHid(usb_source_hid_interface_t *hid_interface);

/*! Unregister HID send event handler
 *
 * Called when previously registered HID interfaces is being removed */
void UsbSource_UnregisterHid(void);

/*! Register Media Player and Volume Control interfaces for USB audio source */
void UsbSource_RegisterAudioControl(void);

/*! Deregister USB audio source controls */
void UsbSource_DeregisterAudioControl(void);

/*! Register Telephony control interfaces for USB Voice source */
void UsbSource_RegisterVoiceControl(void);

/*! Deregister USB Voice source control */
void UsbSource_DeregisterVoiceControl(void);

#endif /* USB_SOURCE_H */
