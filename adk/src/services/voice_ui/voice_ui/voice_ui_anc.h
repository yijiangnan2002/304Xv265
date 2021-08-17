/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       voice_ui_ANC.h
\brief      API of the voice UI ANC handling.
*/

#ifndef VOICE_UI_ANC_H_
#define VOICE_UI_ANC_H_


/*! \brief Initialisation of Voice UI ANC handling
*/
void VoiceUi_AncInit(void);

/*! \brief Gets the status of ANC
    \return TRUE if ANC is enabled, FALSE otherwise
*/
bool VoiceUi_AncGetEnabled(void);

/*! \brief Sets the status of ANC
    \return TRUE if ANC is to be enabled, FALSE if ANC is to be disabled
*/
void VoiceUi_AncSetEnabled(bool enabled);


#endif  /* VOICE_UI_ANC_H_ */
