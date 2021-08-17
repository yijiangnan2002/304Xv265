/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Interface to voice_sources_telephony_control - provides an interface that can be used to
            access the telephony features of a voice source.

            This interface is required, but only the call to get a context is mandatory, all others are optional
*/

#ifndef VOICE_SOURCES_TELEPHONY_CONTROL_INTERFACE_H_
#define VOICE_SOURCES_TELEPHONY_CONTROL_INTERFACE_H_

#include "voice_sources_list.h"

typedef struct
{
    uint8 * digits;
    unsigned number_of_digits;
} phone_number_t;

/*! \brief Transfer direction for voice call audio. */
typedef enum
{
    /*! Transfer the audio to the HFP device.*/
    voice_source_audio_transfer_to_hfp,
    /*! Transfer the audio to the audio gateway.*/
    voice_source_audio_transfer_to_ag,
    /*! Toggle the location at which the call audio is rendered*/
    voice_source_audio_transfer_toggle,
}voice_source_audio_transfer_direction_t;

typedef struct
{
    void (*IncomingCallAccept)(voice_source_t source);
    void (*IncomingCallReject)(voice_source_t source);
    void (*OngoingCallTerminate)(voice_source_t source);
    void (*OngoingCallTransferAudio)(voice_source_t source, voice_source_audio_transfer_direction_t direction);
    void (*InitiateCallUsingNumber)(voice_source_t source, phone_number_t number);
    void (*InitiateVoiceDial)(voice_source_t source);
    void (*InitiateCallLastDialled)(voice_source_t source);
    void (*ToggleMicrophoneMute)(voice_source_t source);
    unsigned (*GetUiProviderContext)(voice_source_t source);
} voice_source_telephony_control_interface_t;

#endif /* VOICE_SOURCES_TELEPHONY_CONTROL_INTERFACE_H_ */
