/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation of common prompts specific testing functions.
*/

#include "prompt_test.h"
#include "ui_prompts.h"

#ifdef GC_SECTIONS
/* Move all functions in KEEP_PM section to ensure they are not removed during
 * garbage collection */
#pragma unitcodesection KEEP_PM
#endif

void PromptTest_Play(MessageId event)
{
    MessageSend(UiPrompts_GetUiPromptsTask(), event, NULL);
}
