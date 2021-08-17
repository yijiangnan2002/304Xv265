/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       kymera_adaptive_anc.h
\brief      Private header to connect/manage to Adaptive ANC chain
*/

#ifndef KYMERA_ADAPTIVE_ANC_H_
#define KYMERA_ADAPTIVE_ANC_H_

#include "kymera_private.h"
#include <operators.h>
#include <anc.h>

/*!Structure that defines inputs for SCO Tx path */
typedef struct
{
    Sink cVc_in1;
    Sink cVc_in2;
    Sink cVc_ref_in;
}adaptive_anc_sco_send_t;

/*! Enum to define AANC use case */
typedef enum
{
    aanc_usecase_default,
    aanc_usecase_standalone,
    aanc_usecase_sco_concurrency
} aanc_usecase_t ;

/*! \brief Registers AANC callbacks in the mic interface layer
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptiveAnc_Init(void);
#else
#define KymeraAdaptiveAnc_Init() ((void)(0))
#endif

#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptiveAnc_EnableGentleMute(void);
#else
#define KymeraAdaptiveAnc_EnableGentleMute() ((void)(0))
#endif

#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptiveAnc_SetUcid(anc_mode_t mode);
#else
#define KymeraAdaptiveAnc_SetUcid(mode) ((void) mode)
#endif


#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptiveAnc_ApplyModeChange(anc_mode_t new_mode, audio_anc_path_id feedforward_anc_path, adaptive_anc_hw_channel_t hw_channel);
#else
#define KymeraAdaptiveAnc_ApplyModeChange(new_mode, feedforward_anc_path, hw_channel) ((void)new_mode, (void)feedforward_anc_path, (void)hw_channel)
#endif


/*! \brief Enable Adaptive ANC capability
    \param msg - Adaptive ANC connection param \ref KYMERA_INTERNAL_AANC_ENABLE_T
    \return void
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptiveAnc_Enable(const KYMERA_INTERNAL_AANC_ENABLE_T* msg);
#else
#define KymeraAdaptiveAnc_Enable(msg) ((void)(0))
#endif

/*! \brief Disable Adaptive ANC
    \param void
    \return void
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptiveAnc_Disable(void);
#else
#define KymeraAdaptiveAnc_Disable() ((void)(0))
#endif

/*! \brief Update Adaptive ANC that earbud is in-ear
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptiveAnc_UpdateInEarStatus(void);
#else
#define KymeraAdaptiveAnc_UpdateInEarStatus() ((void)(0))
#endif

/*! \brief Update Adaptive ANC that earbud is out-of ear
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptiveAnc_UpdateOutOfEarStatus(void);
#else
#define KymeraAdaptiveAnc_UpdateOutOfEarStatus() ((void)(0))
#endif

/*! \brief Enables the adaptivity sub-module in the Adaptive ANC capability
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptiveAnc_EnableAdaptivity(void);
#else
#define KymeraAdaptiveAnc_EnableAdaptivity() ((void)(0))
#endif

/*! \brief Disables the adaptivity sub-module in the Adaptive ANC capability
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptiveAnc_DisableAdaptivity(void);
#else
#define KymeraAdaptiveAnc_DisableAdaptivity() ((void)(0))
#endif

/*! \brief Update Kymera state to AANC Standalone
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptiveAnc_UpdateKymeraStateToStandalone(void);
#else
#define KymeraAdaptiveAnc_UpdateKymeraStateToStandalone() ((void)(0))
#endif

/*! \brief Used for concurrency cases to stop AANC chain and disconnect AEC chain
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptiveAnc_StopAancChainAndDisconnectAec(void);
#else
#define KymeraAdaptiveAnc_StopAancChainAndDisconnectAec() ((void)(0))
#endif

/*! \brief Get the Feed Forward gain
    \param gain - pointer to get the value
    \return void
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptiveAnc_GetFFGain(uint8* gain);
#else
#define KymeraAdaptiveAnc_GetFFGain(gain) ((void)(0))
#endif

/*! \brief Set the Mantissa & Exponent values
    \param gain - pointer to get the value
    \return void
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptiveAnc_SetGainValues(uint32 mantissa, uint32 exponent);
#else
#define KymeraAdaptiveAnc_SetGainValues(mantissa, exponent) ((void)(0))
#endif

#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptiveAnc_EnableQuietMode(void);
#else
#define KymeraAdaptiveAnc_EnableQuietMode() ((void)(0))
#endif

#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptiveAnc_DisableQuietMode(void);
#else
#define KymeraAdaptiveAnc_DisableQuietMode() ((void)(0))
#endif

/*! \brief Obtain Current Adaptive ANC mode from AANC operator
    \param aanc_mode - pointer to get the mode value
    \return TRUE if current mode is stored in aanc_mode, else FALSE
*/
#ifdef ENABLE_ADAPTIVE_ANC
bool KymeraAdaptiveAnc_ObtainCurrentAancMode(adaptive_anc_mode_t *aanc_mode);
#else
#define KymeraAdaptiveAnc_ObtainCurrentAancMode(aanc_mode) (FALSE)
#endif

/*! \brief Identify if noise level is below Quiet Mode threshold
    \param void
    \return TRUE if noise level is below threshold, otherwise FALSE
*/
#ifdef ENABLE_ADAPTIVE_ANC
bool KymeraAdapativeAnc_IsNoiseLevelBelowQmThreshold(void);
#else
#define KymeraAdapativeAnc_IsNoiseLevelBelowQmThreshold() (FALSE)
#endif

#if 0
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptiveAnc_DisconnectAancFromStandalone(void);
#else
#define KymeraAdaptiveAnc_DisconnectAancFromStandalone() ((void)(0))
#endif
#endif

#if 0
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptiveAnc_AddAancConcurrencyChainForMusic(void);
#else
#define KymeraAdaptiveAnc_AddAancConcurrencyChainForMusic() ((void)(0))
#endif
#endif

#if 0
#ifdef ENABLE_ADAPTIVE_ANC
void KymerAdaptiveAnc_TearDownAancConcurrencyChainForMusic(void);
#else
#define KymerAdaptiveAnc_TearDownAancConcurrencyChainForMusic() ((void)(0))
#endif
#endif

/*! \brief Switch from AANC concurrency audio graph to Standlaone AANC audio graph.
    \param void
    \return void
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptive_ConcurrencyToStandalone(void);
#else
#define KymeraAdaptive_ConcurrencyToStandalone() ((void)(0))
#endif

/*! \brief Switch from Standlaone AANC audio graph to AANC concurrency audio graph.
    \param void
    \return void
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptive_StandaloneToConcurrency(void);
#else
#define KymeraAdaptive_StandaloneToConcurrency() ((void)(0))
#endif

/*! \brief Start Adaptive Anc tuning procedure.
           Note that Device has to be enumerated as USB audio device before calling this API.
    \param usb_rate the input sample rate of audio data
    \return void
*/
#ifdef ENABLE_ADAPTIVE_ANC
void kymeraAdaptiveAnc_EnterAdaptiveAncTuning(uint16 usb_rate);
#else
#define kymeraAdaptiveAnc_EnterAdaptiveAncTuning(x) ((void)(0*x))
#endif

/*! \brief Stop Adaptive ANC tuning procedure.
    \param void
    \return void
*/
#ifdef ENABLE_ADAPTIVE_ANC
void kymeraAdaptiveAnc_ExitAdaptiveAncTuning(void);
#else
#define kymeraAdaptiveAnc_ExitAdaptiveAncTuning() ((void)(0))
#endif

#endif /* KYMERA_ADAPTIVE_ANC_H_ */
