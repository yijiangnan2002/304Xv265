/****************************************************************************
Copyright (c) 2004 - 2020 Qualcomm Technologies International, Ltd.

FILE NAME
    audio_plugin_common.c
    
DESCRIPTION
    Implementation of audio plugin common library.
*/

#include <stream.h>
#include <source.h>
#include <micbias.h>
#include <pio.h>
#include <pio_common.h>
#include <stdlib.h>
#include <logging.h>

#include "audio_config.h"
#include "audio_plugin_if.h"
#include "audio_plugin_common.h"

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_TYPE(audio_plugin_interface_message_type_t)
LOGGING_PRESERVE_MESSAGE_TYPE(aov_message_type_t)

#define RAW_INPUT_GAIN_0DB  (0x8020)

unsigned (* MicBiasVoltageCallback)(mic_bias_id id);

/****************************************************************************
DESCRIPTION
    Set up of a couple of common microphone levels
*/
const T_mic_gain MIC_MUTE = {0,8,0};  /* -45db, -24db */
const T_mic_gain MIC_DEFAULT_GAIN = {0,0x1,0xa}; /* +3db for digital and analog */

static uint8 bias_config_refcount[BIAS_CONFIG_PIO+1] = { 0 };

/****************************************************************************
DESCRIPTION
    Get hardware instance
*/
static audio_instance audioPluginGetInstance(audio_instance instance)
{
    if (instance == 0) return AUDIO_INSTANCE_0;
    if (instance == 1) return AUDIO_INSTANCE_1;
    if (instance == 2) return AUDIO_INSTANCE_2;

    /* Trying to use unsupported audio_instance */
    Panic();
    return AUDIO_INSTANCE_0; /* Satisfy compiler, should never get here */
}

/****************************************************************************
DESCRIPTION
    Get hardware instance from mic parameters
*/
audio_instance AudioPluginGetMicInstance(const audio_mic_params audio_mic)
{
    return audioPluginGetInstance(audio_mic.instance);
}
/****************************************************************************
DESCRIPTION
    Get hardware instance from analogue input parameters
*/
audio_instance AudioPluginGetAnalogueInputInstance(const analogue_input_params analogue_in)
{
    return audioPluginGetInstance(analogue_in.instance);
}

/****************************************************************************
DESCRIPTION
    Get audio source
*/
static Source getAudioSource(audio_instance instance, audio_channel channel, bool digital)
{
    return StreamAudioSource(digital ? AUDIO_HARDWARE_DIGITAL_MIC : AUDIO_HARDWARE_CODEC, instance, channel);
}

/****************************************************************************
DESCRIPTION
    Get mic source
*/
Source AudioPluginGetMicSource(const audio_mic_params audio_mic, audio_channel channel)
{
    return getAudioSource(audioPluginGetInstance(audio_mic.instance), channel, audio_mic.is_digital);
}

/****************************************************************************
DESCRIPTION
    Get analogue input source
*/
Source AudioPluginGetAnalogueInputSource(const analogue_input_params analogue_input, audio_channel channel)
{
    return getAudioSource(audioPluginGetInstance(analogue_input.instance), channel, FALSE);
}

/****************************************************************************
DESCRIPTION
    Configure Mic channel
*/
void AudioPluginSetMicRate(Source mic_source, bool digital, uint32 adc_rate)
{
    PanicFalse(SourceConfigure(mic_source, digital ? STREAM_DIGITAL_MIC_INPUT_RATE : STREAM_CODEC_INPUT_RATE, adc_rate));
}


/****************************************************************************
DESCRIPTION
    Configure analogue input channel
*/
static void AudioPluginSetAnalogueInputRate(Source mic_source, uint32 adc_rate)
{
    PanicFalse(SourceConfigure(mic_source, STREAM_CODEC_INPUT_RATE, adc_rate));
}

/****************************************************************************
DESCRIPTION
    Set mic gain
*/
void AudioPluginSetMicGain(Source mic_source, bool digital, uint16 gain)
{
    if (digital)
    {
        SourceConfigure(mic_source, STREAM_DIGITAL_MIC_INPUT_GAIN, gain);
    }
    else
    {
        SourceConfigure(mic_source, STREAM_CODEC_INPUT_GAIN, gain);
        SourceConfigure(mic_source, STREAM_CODEC_RAW_INPUT_GAIN, RAW_INPUT_GAIN_0DB);
    }
}

/****************************************************************************
DESCRIPTION
    Set analogue input gain
*/
static void AudioPluginSetAnalogueInputGain(Source mic_source, uint16 gain, bool preamp)
{
    SourceConfigure(mic_source, STREAM_CODEC_INPUT_GAIN, gain);
    SourceConfigure(mic_source, STREAM_CODEC_MIC_INPUT_GAIN_ENABLE, preamp);
}

/****************************************************************************
DESCRIPTION
    Helper to set bias drive PIO
*/
static bool setPioDrivenBias(unsigned pio, bool set)
{
    pio_common_allbits mask;

    PioCommonBitsInit(&mask);
    PioCommonBitsSetBit(&mask, pio);
    PioCommonSetMap(&mask, &mask);
 
    return PioCommonSetPio(pio, pio_drive, set);
}
/****************************************************************************
DESCRIPTION
    API for allowing application to register its function for getting Microphone
    Bias Voltage.
*/
void AudioPluginCommonRegisterMicBiasVoltageCallback(unsigned (*callback)(mic_bias_id id))
{
    MicBiasVoltageCallback=callback;
}

/****************************************************************************
DESCRIPTION
    Get the microphone bias voltage
*/

static unsigned getMicrophoneBiasVoltage(mic_bias_id id)
{
    unsigned bias_voltage =0;

    if(MicBiasVoltageCallback)
    {
        bias_voltage=MicBiasVoltageCallback(id);
    }
    else
    {
        /*Application has not registerd for MicBiasVoltage*/
        Panic();
    }
    return bias_voltage;
}

/****************************************************************************
DESCRIPTION
    Helper to set bias drive mic bias
*/
static bool setMicBiasDrivenBias(unsigned bias_config, bool set)
{
    mic_bias_id id = (bias_config == BIAS_CONFIG_MIC_BIAS_0 ?
                                                        MIC_BIAS_0 :
                                                        MIC_BIAS_1);
    uint16 value = (set ? MIC_BIAS_FORCE_ON : MIC_BIAS_OFF);

    MicbiasConfigure(id, MIC_BIAS_VOLTAGE, getMicrophoneBiasVoltage(id));

    return MicbiasConfigure(id, MIC_BIAS_ENABLE, value);

}

/****************************************************************************
DESCRIPTION
    Helper to configure the microphones bias drive
*/
static bool configureBiasDrive(unsigned bias_config, unsigned pio, bool set)
{
    if(bias_config != BIAS_CONFIG_DISABLE)
    {
        if(bias_config == BIAS_CONFIG_PIO)
        {
            return setPioDrivenBias(pio, set);
        }
        else
        {
            return setMicBiasDrivenBias(bias_config, set);
        }
    }
    /* Nothing to do, success! */
    return TRUE;
}

/****************************************************************************
DESCRIPTION
    Configure the state of the microphone bias drive
*/
void AudioPluginSetMicBiasDrive(const audio_mic_params params, bool set)
{
    PanicFalse(configureBiasDrive(params.bias_config, params.pio, set));
}


/****************************************************************************
DESCRIPTION
    Set mic bias or digital mic PIO to default state (off)
*/
void AudioPluginInitMicBiasDrive(const audio_mic_params audio_mic)
{
    /* Failure here most likely indicates "already off" so ignore it */
    (void)configureBiasDrive(audio_mic.bias_config, audio_mic.pio, FALSE);
}

/****************************************************************************
DESCRIPTION
    Apply mic configuration and set mic PIO
*/
Source AudioPluginMicSetup(audio_channel channel, const audio_mic_params audio_mic, uint32 rate)
{
    Source mic_source = AudioPluginGetMicSource(audio_mic, channel);
    if(rate)
    {
        AudioPluginSetMicRate(mic_source, audio_mic.is_digital, rate);
    }
    AudioPluginSetMicGain(mic_source, audio_mic.is_digital, audio_mic.gain);
    if(bias_config_refcount[audio_mic.bias_config]++ == 0)
    {
        AudioPluginSetMicBiasDrive(audio_mic, TRUE);
    }
    return mic_source;
}

/****************************************************************************
DESCRIPTION
    Apply analogue input configuration and set line in PIO
*/
Source AudioPluginAnalogueInputSetup(audio_channel channel, const analogue_input_params analogue_input, uint32 rate)
{
    Source mic_source = AudioPluginGetAnalogueInputSource(analogue_input, channel);
    if(rate)
    {
        AudioPluginSetAnalogueInputRate(mic_source, rate);
    }
    AudioPluginSetAnalogueInputGain(mic_source, analogue_input.gain, analogue_input.pre_amp);
    return mic_source;
}

/****************************************************************************
DESCRIPTION
    Apply shutdown to a mic previously setup 
*/
void AudioPluginMicShutdown(audio_channel channel, const audio_mic_params * params, bool close_mic)
{
    Source mic_source = AudioPluginGetMicSource(*params, channel);
    if (close_mic)
    {
        StreamDisconnect(mic_source, NULL);
        SourceClose(mic_source);
    }
    PanicFalse(bias_config_refcount[params->bias_config]>0);
    if(--bias_config_refcount[params->bias_config] == 0)
    {
        AudioPluginSetMicBiasDrive(*params, FALSE);
    }
}
