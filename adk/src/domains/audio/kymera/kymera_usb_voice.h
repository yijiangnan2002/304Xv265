/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief     The Kymera USB Voice API

*/

#ifndef KYMERA_USB_VOICE_H
#define KYMERA_USB_VOICE_H

#include "kymera_private.h"

/*! \brief Create and start USB Voice chain.
    \param voice_params Parameters for USB voice chain connect.
*/
#ifdef INCLUDE_USB_DEVICE
void KymeraUsbVoice_Start(KYMERA_INTERNAL_USB_VOICE_START_T *voice_params);
#else
#define KymeraUsbVoice_Start(x) UNUSED(x)
#endif
/*! \brief Stop and destroy USB Voice chain.
    \param voice_params Parameters for USB voice chain disconnect.
*/
#ifdef INCLUDE_USB_DEVICE
void KymeraUsbVoice_Stop(KYMERA_INTERNAL_USB_VOICE_STOP_T *voice_params);
#else
#define KymeraUsbVoice_Stop(x) UNUSED(x)
#endif

/*! \brief Set USB Voice volume.

    \param[in] volume_in_db.
 */
#ifdef INCLUDE_USB_DEVICE
void KymeraUsbVoice_SetVolume(int16 volume_in_db);
#else
#define KymeraUsbVoice_SetVolume(x) UNUSED(x)
#endif

/*! \brief Enable or disable MIC muting.

    \param[in] mute TRUE to mute MIC, FALSE to unmute MIC.
*/
#ifdef INCLUDE_USB_DEVICE
void KymeraUsbVoice_MicMute(bool mute);
#else
#define KymeraUsbVoice_MicMute(x) UNUSED(x)
#endif

/*! \brief Init USB voice component
*/
#ifdef INCLUDE_USB_DEVICE
void KymeraUsbVoice_Init(void);
#else
#define KymeraUsbVoice_Init(x) ((void)(0))
#endif

#endif /* KYMERA_USB_VOICE_H */
