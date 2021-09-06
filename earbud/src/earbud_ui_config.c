/*!
\copyright  Copyright (c) 2019 - 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       earbud_ui_config.c
\brief      ui configuration table

    This file contains ui configuration table which maps different logical inputs to
    corresponding ui inputs based upon ui provider contexts.
*/

#include "earbud_ui_config.h"
#include "ui.h"
#include "earbud_buttons.h"

/* Needed for UI contexts - transitional; when table is code generated these can be anonymised
 * unsigned ints and these includes can be removed. */
#include "media_player.h"
#include "hfp_profile.h"
#include "bt_device.h"
#include "earbud_sm.h"
#include "power_manager.h"
#include "voice_ui.h"
#include "audio_curation.h"
#include <focus_select.h>
#include <macros.h>

const focus_select_audio_tie_break_t audio_source_focus_tie_break_order[] =
{
    FOCUS_SELECT_AUDIO_LINE_IN,
    FOCUS_SELECT_AUDIO_USB,
    FOCUS_SELECT_AUDIO_A2DP,
    FOCUS_SELECT_AUDIO_LEA_UNICAST,
    FOCUS_SELECT_AUDIO_LEA_BROADCAST,
};

const focus_select_audio_tie_break_t voice_source_focus_tie_break_order[] =
{
    FOCUS_SELECT_VOICE_HFP,
    FOCUS_SELECT_VOICE_USB,
    FOCUS_SELECT_VOICE_LEA_UNICAST,
};

#ifdef INCLUDE_CAPSENSE
const touch_event_config_t touch_event_table [] =
{
#ifdef TOUCH_SENSOR_V2
    {
        SLIDE_UP,
        CAP_SENSE_SLIDE_UP
    },
    {
        SLIDE_DOWN,
        CAP_SENSE_SLIDE_DOWN
    },
    /* map touch double press to single click event, 
       single click on touch is normally user unintentionally touch the capsense*/
    {
        SINGLE_PRESS,
        APP_MFB_BUTTON_SINGLE_CLICK
    },
    {
        DOUBLE_PRESS,
        APP_ANC_SET_NEXT_MODE
    },
    {
        TRIPLE_PRESS,
        CAP_SENSE_TRIPLE_PRESS
    },
    /* Convert tap-and-swipe into appropriate track skip */
    {
        TAP_SLIDE_UP,
        APP_BUTTON_FORWARD
    },
    {
        TAP_SLIDE_DOWN,
        APP_BUTTON_BACKWARD
    },

/* */
#if !(defined INCLUDE_GAA || defined INCLUDE_AMA)
    {
        TOUCH_ONE_SECOND_PRESS,
        APP_MFB_BUTTON_HELD_1SEC
    },
    {
        TOUCH_ONE_SECOND_PRESS_RELEASE,
        APP_MFB_BUTTON_HELD_RELEASE_1SEC
    },
    {
        TOUCH_THREE_SECOND_PRESS,
        APP_MFB_BUTTON_HELD_3SEC
    },
    {
        TOUCH_THREE_SECOND_PRESS_RELEASE,
        APP_MFB_BUTTON_HELD_RELEASE_3SEC
    },

    {
        TOUCH_SIX_SECOND_PRESS,
        APP_MFB_BUTTON_HELD_6SEC
    },
    {
        TOUCH_SIX_SECOND_PRESS_RELEASE,
        APP_MFB_BUTTON_HELD_RELEASE_6SEC
    },
#ifdef HAVE_RDP_UI
    {
        TOUCH_EIGHT_SECOND_PRESS,
        APP_MFB_BUTTON_HELD_8SEC
    },
    {
        TOUCH_EIGHT_SECOND_PRESS_RELEASE,
        APP_MFB_BUTTON_HELD_RELEASE_8SEC
    },
#endif
    {
        TOUCH_TWELVE_SECOND_PRESS,
        APP_BUTTON_HELD_DFU
    },
    {
        TOUCH_TWELVE_SECOND_PRESS_RELEASE,
        APP_BUTTON_DFU
    },
    {
        TOUCH_FIFTEEN_SECOND_PRESS,
        APP_BUTTON_HELD_FACTORY_RESET
    },
    {
        TOUCH_FIFTEEN_SECOND_PRESS_RELEASE,
        APP_BUTTON_FACTORY_RESET
    },
    {
        TOUCH_THIRTY_SECOND_PRESS_RELEASE,
        APP_BUTTON_FORCE_RESET
    },
    {
        TOUCH_DOUBLE_ONE_SECOND_PRESS,
        CAP_SENSE_DOUBLE_PRESS_HOLD
    },
    {
        TOUCH_DOUBLE_ONE_SECOND_PRESS_RELEASE,
        CAP_SENSE_DOUBLE_PRESS_HOLD_RELEASE
    }
#else
    /* These are the mapping when VA is enabled - be it GAA or AMA */
    /* The long press and hold 1s and above are to be used for VA */
    {
        HAND_COVER,
        APP_VA_BUTTON_DOWN
    },
    {
        TOUCH_ONE_SECOND_PRESS,
        APP_VA_BUTTON_HELD_1SEC
    },
    {
        HAND_COVER_RELEASE,
        APP_VA_BUTTON_RELEASE
    },

    /* The double press and hold events are for standard UI menu */
    {
        TOUCH_DOUBLE_ONE_SECOND_PRESS,
        APP_MFB_BUTTON_HELD_1SEC
    },
    {
        TOUCH_DOUBLE_ONE_SECOND_PRESS_RELEASE,
        APP_MFB_BUTTON_HELD_RELEASE_1SEC
    },
    {
        TOUCH_DOUBLE_THREE_SECOND_PRESS,
        APP_MFB_BUTTON_HELD_3SEC
    },
    {
        TOUCH_DOUBLE_THREE_SECOND_PRESS_RELEASE,
        APP_MFB_BUTTON_HELD_RELEASE_3SEC
    },
#ifdef HAVE_RDP_UI
    {
        TOUCH_DOUBLE_EIGHT_SECOND_PRESS,
        APP_MFB_BUTTON_HELD_8SEC
    },
    {
        TOUCH_DOUBLE_EIGHT_SECOND_PRESS_RELEASE,
        APP_MFB_BUTTON_HELD_RELEASE_8SEC
    },
#endif
#ifdef HAVE_RDP_UI_V2
    {
        TOUCH_DOUBLE_SIX_SECOND_PRESS,
        APP_MFB_BUTTON_HELD_6SEC
    },
    {
        TOUCH_DOUBLE_SIX_SECOND_PRESS_RELEASE,
        APP_BUTTON_FORCE_RESET
    },
    {
        TOUCH_DOUBLE_NINE_SECOND_PRESS,
        APP_BUTTON_HELD_FACTORY_RESET
    },
    {
        TOUCH_DOUBLE_NINE_SECOND_PRESS_RELEASE,
        APP_BUTTON_FACTORY_RESET
    },
    {
        TOUCH_DOUBLE_TWELVE_SECOND_PRESS,
        APP_BUTTON_HELD_FACTORY_RESET_CANCEL
    },
    {
        TOUCH_DOUBLE_TWELVE_SECOND_PRESS_RELEASE,
        APP_MESSAGE_IGNORE
    },
#else /* ifdef HAVE_RDP_UI_V2 */
    {
        TOUCH_DOUBLE_SIX_SECOND_PRESS,
        APP_MFB_BUTTON_HELD_6SEC
    },
    {
        TOUCH_DOUBLE_SIX_SECOND_PRESS_RELEASE,
        APP_MFB_BUTTON_HELD_RELEASE_6SEC
    },
    {
        TOUCH_DOUBLE_TWELVE_SECOND_PRESS,
        APP_BUTTON_HELD_DFU
    },
    {
        TOUCH_DOUBLE_TWELVE_SECOND_PRESS_RELEASE,
        APP_BUTTON_DFU
    },
    {
        TOUCH_DOUBLE_FIFTEEN_SECOND_PRESS,
        APP_BUTTON_HELD_FACTORY_RESET
    },
    {
        TOUCH_DOUBLE_FIFTEEN_SECOND_PRESS_RELEASE,
        APP_BUTTON_FACTORY_RESET
    },
    {
        TOUCH_DOUBLE_THIRTY_SECOND_PRESS_RELEASE,
        APP_BUTTON_FORCE_RESET
    },
#endif // HAVE_RDP_UI_V2
#endif

#else
    {
        SINGLE_PRESS,
        CAP_SENSE_SINGLE_PRESS
    },
    {
        DOUBLE_PRESS,
        CAP_SENSE_DOUBLE_PRESS
    },    
    {
        DOUBLE_PRESS_HOLD,
        CAP_SENSE_DOUBLE_PRESS_HOLD
    },
    {
        TRIPLE_PRESS,
        CAP_SENSE_TRIPLE_PRESS
    },    
    {
        TRIPLE_PRESS_HOLD,
        CAP_SENSE_TRIPLE_PRESS_HOLD
    },  
    {
        FOUR_PRESS_HOLD,
        CAP_SENSE_FOUR_PRESS_HOLD
    },
    {
        FIVE_PRESS,
        CAP_SENSE_FIVE_PRESS
    },
    {
        FIVE_PRESS_HOLD,
        CAP_SENSE_FIVE_PRESS_HOLD
    },
    {
        SIX_PRESS,
        CAP_SENSE_SIX_PRESS
    },
    {
        SEVEN_PRESS,
        CAP_SENSE_SEVEN_PRESS
    },
    {
        EIGHT_PRESS,
        CAP_SENSE_EIGHT_PRESS
    },
    {
        NINE_PRESS,
        CAP_SENSE_NINE_PRESS
    },
    {
        LONG_PRESS,
        CAP_SENSE_LONG_PRESS
    },
    {
        VERY_LONG_PRESS,
        CAP_SENSE_VERY_LONG_PRESS
    },
    {
        VERY_VERY_LONG_PRESS,
        CAP_SENSE_VERY_VERY_LONG_PRESS
    },
    {
        VERY_VERY_VERY_LONG_PRESS,
        CAP_SENSE_VERY_VERY_VERY_LONG_PRESS
    },
    {
        SLIDE_UP,
        CAP_SENSE_SLIDE_UP
    },
    {
        SLIDE_DOWN,
        CAP_SENSE_SLIDE_DOWN
    },
    {
        SLIDE_LEFT,
        CAP_SENSE_SLIDE_UP
    },
    {
        SLIDE_RIGHT,
        CAP_SENSE_SLIDE_DOWN
    },
    {
        HAND_COVER,
        CAP_SENSE_HAND_COVER
    },
    {
        HAND_COVER_RELEASE,
        CAP_SENSE_HAND_COVER_RELEASE
    },
#endif
};
#endif

/*! \ingroup configurable

   \brief Button configuration table

   This is an ordered table that associates logical inputs and contexts with actions.
*/
const ui_config_table_content_t earbud_ui_config_table[] =
{
#if 1
    {APP_MFB_BUTTON_DOUBLE_CLICK,      ui_provider_telephony,       context_voice_in_call,                ui_input_voice_call_hang_up                   },//harry 0825 99999999
    {APP_MFB_BUTTON_DOUBLE_CLICK,      ui_provider_telephony,       context_voice_ringing_outgoing,       ui_input_voice_call_hang_up                   }, //harry 0825 99999999
    {APP_MFB_BUTTON_DOUBLE_CLICK,      ui_provider_telephony,       context_voice_ringing_incoming,       ui_input_voice_call_accept                    },	//harry 0825 888888
    {APP_MFB_BUTTON_DOUBLE_CLICK,      ui_provider_media_player,    context_media_player_streaming,       ui_input_toggle_play_pause                    },   //harry 0825 77777777
    {APP_MFB_BUTTON_DOUBLE_CLICK,      ui_provider_media_player,    context_media_player_idle,            ui_input_toggle_play_pause                    },
    {APP_MFB_BUTTON_LEFT_DOUBLE_CLICK,      ui_provider_telephony,    context_voice_connected,            ui_input_voice_dial                    },  //harry 0825 131313131313
    {APP_MFB_BUTTON_HELD_1SEC,      ui_provider_telephony,       context_voice_ringing_incoming,       ui_input_voice_call_reject                    },	//harry 0825 101010101010
    {APP_MFB_BUTTON_TRIPLE_CLICK,              ui_provider_media_player,    context_media_player_streaming,       ui_input_av_forward                           },  //harry 0825 1212121212
    {APP_MFB_BUTTON_LEFT_TRIPLE_CLICK,              ui_provider_media_player,    context_media_player_streaming,       ui_input_av_backward                           },//harry 0825 1111111111


    {APP_PIO3_BUTTON_SINGLE_CLICK, ui_provider_phy_state,       context_phy_state_out_of_case,        ui_input_sm_pair_handset                      },
    {APP_PIO3_BUTTON_DOUBLE_CLICK,      ui_provider_device,          context_handset_not_connected,        ui_input_connect_handset                      },
    {APP_PIO3_BUTTON_TRIPLE_CLICK, ui_provider_phy_state,       context_phy_state_out_of_case,        ui_input_sm_delete_handsets                   },

#else
    {APP_LEAKTHROUGH_TOGGLE_ON_OFF,    ui_provider_audio_curation,  context_leakthrough_disabled,         ui_input_leakthrough_toggle_on_off            },
    {APP_LEAKTHROUGH_TOGGLE_ON_OFF,    ui_provider_audio_curation,  context_leakthrough_enabled,          ui_input_leakthrough_toggle_on_off            },
    {APP_ANC_ENABLE,                   ui_provider_audio_curation,  context_anc_disabled,                 ui_input_anc_on                               },
    {APP_ANC_DISABLE,                  ui_provider_audio_curation,  context_anc_enabled,                  ui_input_anc_off                              },
    {APP_ANC_TOGGLE_ON_OFF,            ui_provider_audio_curation,  context_anc_disabled,                 ui_input_anc_toggle_on_off                    },
    {APP_ANC_TOGGLE_ON_OFF,            ui_provider_audio_curation,  context_anc_enabled,                  ui_input_anc_toggle_on_off                    },
    {APP_ANC_SET_NEXT_MODE,            ui_provider_audio_curation,  context_anc_disabled,                 ui_input_anc_toggle_way                       },
    {APP_ANC_SET_NEXT_MODE,            ui_provider_audio_curation,  context_anc_enabled,                  ui_input_anc_toggle_way                       },
#ifdef CORVUS_YD300 /* This is only for corvus board */
    {APP_ANC_DELETE_PDL,               ui_provider_phy_state,       context_phy_state_out_of_case,        ui_input_sm_delete_handsets                   },
#endif
#ifdef HAVE_RDP_UI_V2
    {APP_MFB_BUTTON_SINGLE_CLICK,      ui_provider_telephony,       context_voice_in_call,                ui_input_mic_mute_toggle                      },
    {APP_MFB_BUTTON_HELD_1SEC,         ui_provider_telephony,       context_voice_in_call,                ui_input_voice_call_hang_up                    },
    {APP_MFB_BUTTON_HELD_1SEC,         ui_provider_telephony,       context_voice_ringing_outgoing,       ui_input_voice_call_hang_up                    },
    {APP_MFB_BUTTON_SINGLE_CLICK,      ui_provider_telephony,       context_voice_ringing_incoming,       ui_input_voice_call_reject                    },
#else
    {APP_MFB_BUTTON_SINGLE_CLICK,      ui_provider_telephony,       context_voice_in_call,                ui_input_voice_call_hang_up                   },
    {APP_MFB_BUTTON_SINGLE_CLICK,      ui_provider_telephony,       context_voice_ringing_outgoing,       ui_input_voice_call_hang_up                   },
    {APP_MFB_BUTTON_SINGLE_CLICK,      ui_provider_telephony,       context_voice_ringing_incoming,       ui_input_voice_call_accept                    },
    {APP_MFB_BUTTON_HELD_RELEASE_1SEC, ui_provider_telephony,       context_voice_in_call,                ui_input_voice_transfer                       },
#endif // HAVE_RDP_UI_V2
    {APP_MFB_BUTTON_SINGLE_CLICK,      ui_provider_media_player,    context_media_player_streaming,       ui_input_toggle_play_pause                    },
    {APP_MFB_BUTTON_SINGLE_CLICK,      ui_provider_media_player,    context_media_player_idle,            ui_input_toggle_play_pause                    },
    {APP_MFB_BUTTON_SINGLE_CLICK,      ui_provider_device,          context_handset_not_connected,        ui_input_connect_handset                      },

#ifdef HAVE_RDP_UI_V2
    {APP_MFB_BUTTON_HELD_1SEC,         ui_provider_telephony,       context_voice_ringing_incoming,       ui_input_voice_call_accept                    },
#else
    {APP_MFB_BUTTON_HELD_RELEASE_1SEC, ui_provider_telephony,       context_voice_ringing_incoming,       ui_input_voice_call_reject                    },
#endif
    {APP_MFB_BUTTON_HELD_RELEASE_1SEC, ui_provider_media_player,    context_media_player_streaming,       ui_input_stop_av_connection                   },

#ifdef PRODUCTION_TEST_MODE
    {APP_MFB_BUTTON_HELD_3SEC,         ui_provider_ptm_state,       context_ptm_state_ptm,                ui_input_production_test_mode                 },
    {APP_MFB_BUTTON_HELD_RELEASE_3SEC, ui_provider_ptm_state,       context_ptm_state_ptm,                ui_input_production_test_mode_request         },
#else
    {APP_MFB_BUTTON_HELD_RELEASE_3SEC, ui_provider_telephony,       context_voice_in_call,                ui_input_mic_mute_toggle                      },
#endif

    {APP_BUTTON_DFU,                   ui_provider_phy_state,       context_phy_state_out_of_case,        ui_input_dfu_active_when_in_case_request      },
    {APP_BUTTON_FACTORY_RESET,         ui_provider_phy_state,       context_phy_state_out_of_case,        ui_input_factory_reset_request                },

#if defined(QCC3020_FF_ENTRY_LEVEL_AA) || (defined HAVE_RDP_UI)
    {APP_BUTTON_FORCE_RESET,           ui_provider_phy_state,       context_phy_state_out_of_case,        ui_input_force_reset                          },
#endif
#ifdef HAVE_RDP_UI_V2
    {APP_MFB_BUTTON_HELD_RELEASE_3SEC, ui_provider_phy_state,       context_phy_state_out_of_case,        ui_input_sm_pair_handset                      },
#else
    {APP_MFB_BUTTON_HELD_RELEASE_6SEC, ui_provider_phy_state,       context_phy_state_out_of_case,        ui_input_sm_pair_handset                      },
#endif // HAVE_RDP_UI_V2
    {APP_MFB_BUTTON_HELD_RELEASE_8SEC, ui_provider_phy_state,       context_phy_state_out_of_case,        ui_input_sm_delete_handsets                   },

    {APP_VA_BUTTON_DOWN,              ui_provider_voice_ui,        context_voice_ui_default,              ui_input_va_1                                 },
    {APP_VA_BUTTON_SINGLE_CLICK,      ui_provider_voice_ui,        context_voice_ui_default,              ui_input_va_3                                 },
    {APP_VA_BUTTON_DOUBLE_CLICK,      ui_provider_voice_ui,        context_voice_ui_default,              ui_input_va_4                                 },
    {APP_VA_BUTTON_HELD_1SEC,         ui_provider_voice_ui,        context_voice_ui_default,              ui_input_va_5                                 },
    {APP_VA_BUTTON_RELEASE,           ui_provider_voice_ui,        context_voice_ui_default,              ui_input_va_6                                 },
    
#if defined(HAVE_4_BUTTONS) || defined(HAVE_6_BUTTONS) || defined(HAVE_7_BUTTONS) || defined(HAVE_9_BUTTONS)
    {APP_LEAKTHROUGH_ENABLE,          ui_provider_audio_curation,  context_leakthrough_disabled,         ui_input_leakthrough_on                       },
    {APP_LEAKTHROUGH_DISABLE,         ui_provider_audio_curation,  context_leakthrough_enabled,          ui_input_leakthrough_off                      },
    {APP_LEAKTHROUGH_SET_NEXT_MODE,   ui_provider_audio_curation,  context_leakthrough_enabled,          ui_input_leakthrough_set_next_mode            },

    {APP_BUTTON_VOLUME_DOWN,          ui_provider_telephony,       context_voice_in_call,                ui_input_voice_volume_down_start              },
    {APP_BUTTON_VOLUME_DOWN,          ui_provider_media_player,    context_media_player_streaming,       ui_input_audio_volume_down_start              },
    {APP_BUTTON_VOLUME_DOWN,          ui_provider_telephony,       context_voice_connected,              ui_input_voice_volume_down_start              },

    {APP_BUTTON_VOLUME_UP,            ui_provider_telephony,       context_voice_in_call,                ui_input_voice_volume_up_start                },
    {APP_BUTTON_VOLUME_UP,            ui_provider_media_player,    context_media_player_streaming,       ui_input_audio_volume_up_start                },
    {APP_BUTTON_VOLUME_UP,            ui_provider_telephony,       context_voice_connected,              ui_input_voice_volume_up_start                },

    {APP_BUTTON_VOLUME_DOWN_RELEASE,  ui_provider_telephony,       context_voice_in_call,                ui_input_voice_volume_stop                    },
    {APP_BUTTON_VOLUME_DOWN_RELEASE,  ui_provider_media_player,    context_media_player_streaming,       ui_input_audio_volume_down_stop               },
    {APP_BUTTON_VOLUME_DOWN_RELEASE,  ui_provider_telephony,       context_voice_connected,              ui_input_voice_volume_stop                    },

    {APP_BUTTON_VOLUME_UP_RELEASE,    ui_provider_telephony,       context_voice_in_call,                ui_input_voice_volume_stop                    },
    {APP_BUTTON_VOLUME_UP_RELEASE,    ui_provider_media_player,    context_media_player_streaming,       ui_input_audio_volume_up_stop                 },
    {APP_BUTTON_VOLUME_UP_RELEASE,    ui_provider_telephony,       context_voice_connected,              ui_input_voice_volume_stop                    },
#endif /* HAVE_4_BUTTONS || HAVE_6_BUTTONS || HAVE_7_BUTTONS || HAVE_9_BUTTONS */

#if defined(HAVE_6_BUTTONS) || defined(HAVE_7_BUTTONS) || defined(HAVE_9_BUTTONS)
    {APP_BUTTON_PLAY_PAUSE_TOGGLE,    ui_provider_media_player,    context_media_player_streaming,       ui_input_toggle_play_pause                    },
    {APP_BUTTON_PLAY_PAUSE_TOGGLE,    ui_provider_media_player,    context_media_player_idle,            ui_input_toggle_play_pause                    },
    {APP_BUTTON_FORWARD,              ui_provider_media_player,    context_media_player_streaming,       ui_input_av_forward                           },
    {APP_BUTTON_FORWARD_HELD,         ui_provider_media_player,    context_media_player_streaming,       ui_input_av_fast_forward_start                },
    {APP_BUTTON_FORWARD_HELD_RELEASE, ui_provider_media_player,    context_media_player_streaming,       ui_input_fast_forward_stop                    },
    {APP_BUTTON_BACKWARD,             ui_provider_media_player,    context_media_player_streaming,       ui_input_av_backward                          },
    {APP_BUTTON_BACKWARD_HELD,        ui_provider_media_player,    context_media_player_streaming,       ui_input_av_rewind_start                      },
    {APP_BUTTON_BACKWARD_HELD_RELEASE,ui_provider_media_player,    context_media_player_streaming,       ui_input_rewind_stop                          },
#endif /* HAVE_6_BUTTONS || HAVE_7_BUTTONS || HAVE_9_BUTTONS */
#if defined(HAVE_5_BUTTONS)
    {APP_BUTTON_VOLUME_DOWN,          ui_provider_telephony,       context_voice_in_call,                ui_input_voice_volume_down_start              },
    {APP_BUTTON_VOLUME_DOWN,          ui_provider_media_player,    context_media_player_streaming,       ui_input_audio_volume_down_start              },
    {APP_BUTTON_VOLUME_DOWN,          ui_provider_telephony,       context_voice_connected,              ui_input_voice_volume_down_start              },

    {APP_BUTTON_VOLUME_UP,            ui_provider_telephony,       context_voice_in_call,                ui_input_voice_volume_up_start                },
    {APP_BUTTON_VOLUME_UP,            ui_provider_media_player,    context_media_player_streaming,       ui_input_audio_volume_up_start                },
    {APP_BUTTON_VOLUME_UP,            ui_provider_telephony,       context_voice_connected,              ui_input_voice_volume_up_start                },
#endif /* HAVE_5_BUTTONS */

#ifdef INCLUDE_CAPSENSE
#ifdef TOUCH_SENSOR_V2
    {CAP_SENSE_SLIDE_DOWN,            ui_provider_telephony,       context_voice_in_call,                ui_input_voice_volume_down                    },
    {CAP_SENSE_SLIDE_DOWN,            ui_provider_media_player,    context_media_player_streaming,       ui_input_audio_volume_down                    },
    {CAP_SENSE_SLIDE_DOWN,            ui_provider_telephony,       context_voice_connected,              ui_input_voice_volume_down                    },
    
    {CAP_SENSE_SLIDE_UP,              ui_provider_telephony,       context_voice_in_call,                ui_input_voice_volume_up                      },
    {CAP_SENSE_SLIDE_UP,              ui_provider_media_player,    context_media_player_streaming,       ui_input_audio_volume_up                      },
    {CAP_SENSE_SLIDE_UP,              ui_provider_telephony,       context_voice_connected,              ui_input_voice_volume_up                      },
    {CAP_SENSE_TRIPLE_PRESS,          ui_provider_device,          context_handset_connected,            ui_input_gaming_mode_toggle                   },

    /* Doesn't seem to be the right place for this ... */
    {APP_BUTTON_FORWARD,              ui_provider_media_player,    context_media_player_streaming,       ui_input_av_forward                           },
    {APP_BUTTON_BACKWARD,             ui_provider_media_player,    context_media_player_streaming,       ui_input_av_backward                          },

#else
    {CAP_SENSE_DOUBLE_PRESS,          ui_provider_telephony,       context_voice_in_call,                ui_input_voice_call_hang_up                   },
    {CAP_SENSE_DOUBLE_PRESS,          ui_provider_telephony,       context_voice_ringing_outgoing,       ui_input_voice_call_hang_up                   },
    {CAP_SENSE_DOUBLE_PRESS,          ui_provider_telephony,       context_voice_ringing_incoming,       ui_input_voice_call_accept                    },
    {CAP_SENSE_DOUBLE_PRESS,          ui_provider_media_player,    context_media_player_streaming,       ui_input_toggle_play_pause                    },
    {CAP_SENSE_DOUBLE_PRESS,          ui_provider_media_player,    context_media_player_idle,            ui_input_toggle_play_pause                    },
    {CAP_SENSE_DOUBLE_PRESS,          ui_provider_device,          context_handset_not_connected,        ui_input_connect_handset                      },

    {CAP_SENSE_SLIDE_DOWN,            ui_provider_telephony,       context_voice_in_call,                ui_input_voice_volume_down                    },
    {CAP_SENSE_SLIDE_DOWN,            ui_provider_media_player,    context_media_player_streaming,       ui_input_audio_volume_down                    },
    {CAP_SENSE_SLIDE_DOWN,            ui_provider_telephony,       context_voice_connected,              ui_input_voice_volume_down                    },

    {CAP_SENSE_SLIDE_UP,              ui_provider_telephony,       context_voice_in_call,                ui_input_voice_volume_up                      },
    {CAP_SENSE_SLIDE_UP,              ui_provider_media_player,    context_media_player_streaming,       ui_input_audio_volume_up                      },
    {CAP_SENSE_SLIDE_UP,              ui_provider_telephony,       context_voice_connected,              ui_input_voice_volume_up                      },

    {CAP_SENSE_TRIPLE_PRESS,          ui_provider_device,          context_handset_connected,            ui_input_gaming_mode_toggle                   },

    {CAP_SENSE_FOUR_PRESS_HOLD,       ui_provider_phy_state,       context_phy_state_out_of_case,        ui_input_dfu_active_when_in_case_request      },
#ifdef ENABLE_ANC
    {CAP_SENSE_FIVE_PRESS,            ui_provider_audio_curation,  context_anc_disabled,                 ui_input_anc_toggle_on_off                    },
    {CAP_SENSE_FIVE_PRESS,            ui_provider_audio_curation,  context_anc_enabled,                  ui_input_anc_toggle_on_off                    },
    {CAP_SENSE_SEVEN_PRESS,           ui_provider_audio_curation,  context_anc_disabled,                 ui_input_anc_toggle_way                       },
    {CAP_SENSE_SEVEN_PRESS,           ui_provider_audio_curation,  context_anc_enabled,                  ui_input_anc_toggle_way                       },
#endif
    {CAP_SENSE_SIX_PRESS,             ui_provider_phy_state,       context_phy_state_out_of_case,        ui_input_sm_pair_handset                      },
    {CAP_SENSE_EIGHT_PRESS,           ui_provider_phy_state,       context_phy_state_out_of_case,        ui_input_sm_delete_handsets                   },

    {CAP_SENSE_VERY_VERY_VERY_LONG_PRESS,  ui_provider_phy_state,  context_phy_state_out_of_case,        ui_input_factory_reset_request                },
#endif
#endif
#endif
};



const ui_config_table_content_t* EarbudUi_GetConfigTable(unsigned* table_length)
{
    *table_length = ARRAY_DIM(earbud_ui_config_table);
    return earbud_ui_config_table;
}

#ifdef INCLUDE_CAPSENSE
const touch_event_config_t* EarbudUi_GetCapsenseEventTable(unsigned* table_length)
{
    *table_length = ARRAY_DIM(touch_event_table);
    return touch_event_table;
}
#endif

void EarbudUi_ConfigureFocusSelection(void)
{
    FocusSelect_ConfigureAudioSourceTieBreakOrder(audio_source_focus_tie_break_order);
    FocusSelect_ConfigureVoiceSourceTieBreakOrder(voice_source_focus_tie_break_order);
}
