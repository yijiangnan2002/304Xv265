/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       voice_ui_anc.c
\brief      Implementation of the voice UI ANC handling.
*/

#include <logging.h>
#include "voice_ui_anc.h"
// #include "anc_monitor.h"
#include "anc_state_manager.h"
#include "voice_ui_container.h"
#include "ui.h"

typedef struct
{
    TaskData task;
} voiceui_anc_task;

static voiceui_anc_task voiceui_anc_data;

static void voiceui_AncMessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);

    DEBUG_LOG("voiceui_AncMessageHandler: enum:anc_msg_t:%d", id);
    switch(id)
    {
        case ANC_UPDATE_STATE_ENABLE_IND:
        {
            voice_ui_handle_t* handle = VoiceUi_GetActiveVa();
            if (handle && handle->voice_assistant && handle->voice_assistant->AncEnabledUpdate)
            {
                handle->voice_assistant->AncEnabledUpdate(TRUE);
            }
            break;
        }
        case ANC_UPDATE_STATE_DISABLE_IND:
        {
            voice_ui_handle_t* handle = VoiceUi_GetActiveVa();
            if (handle && handle->voice_assistant && handle->voice_assistant->AncEnabledUpdate)
            {
                handle->voice_assistant->AncEnabledUpdate(FALSE);
            }
            break;
        }
        default:
            break;
    }
}

void VoiceUi_AncInit(void)
{
    DEBUG_LOG("VoiceUi_AncInit");
    voiceui_anc_data.task.handler= voiceui_AncMessageHandler;
    AncStateManager_ClientRegister(&voiceui_anc_data.task);
}

bool VoiceUi_AncGetEnabled(void)
{
    bool enabled = AncStateManager_IsEnabled();
    DEBUG_LOG("VoiceUi_AncGetEnabled: %d", enabled);
    return enabled;
}

void VoiceUi_AncSetEnabled(bool enabled)
{
    DEBUG_LOG("VoiceUi_AncSetEnabled: %d", enabled);

    if (enabled)
    {
        Ui_InjectUiInput(ui_input_anc_on);
    }
    else
    {
        Ui_InjectUiInput(ui_input_anc_off);
    }
}
