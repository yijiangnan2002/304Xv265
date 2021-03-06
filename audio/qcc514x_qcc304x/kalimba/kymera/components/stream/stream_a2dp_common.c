/****************************************************************************
 * Copyright (c) 2015 - 2017 Qualcomm Technologies International, Ltd.
****************************************************************************/
#include "adaptor/adaptor.h"
#include "audio_log/audio_log.h"
#include "buffer.h"
#include "opmgr/opmgr.h"
#include "opmsg_prim.h"
#include "pl_fractional.h"
#include "platform/pl_intrinsics.h"
#include "stream.h"
#include "stream_audio_data_format.h"
#include "stream_endpoint.h"
#include "stream_endpoint_a2dp.h"
#include "stream_private.h"
#include "types.h"

/**
 * a2dp_set_data_format
 */
static bool a2dp_set_data_format(ENDPOINT *endpoint, AUDIO_DATA_FORMAT format)
{
    if (AUDIO_DATA_FORMAT_ENCODED_DATA == format)
    {
        return TRUE;
    }

    return FALSE;
}

static bool set_rm_enacting_cback(CONNECTION_LINK con_id,
                                  STATUS_KYMERA status,
                                  unsigned op_id,
                                  unsigned num_resp_params,
                                  unsigned *resp_params)
{
    return TRUE;
}

bool a2dp_configure(ENDPOINT *endpoint, unsigned key, uint32 value)
{
    /* A2DP specific endpoint configuration code to go here.
     *
     * Currently there is nothing about an a2dp endpoint that can be configured
     * so reject any attempt to configure it. The host has probably got the wrong
     * endpoint, so it needs to be told it's trying to do something impossible.
     *
     * There are some internal configuration commands that are supported however
     */
    switch(key)
    {
    case EP_DATA_FORMAT:
        return a2dp_set_data_format(endpoint, (AUDIO_DATA_FORMAT)value);
    case EP_RATEMATCH_ENACTING:
    {
        unsigned params[3];

        params[0] = OPMSG_COMMON_SET_RM_ENACTING;

        params[1] = (bool)value;
        params[2] = (unsigned)(uintptr_t)&endpoint->state.a2dp.rm_adjust_amount;

        opmgr_operator_message(OPMGR_ADAPTOR_OBPM, endpoint->connected_to->id,
                sizeof(params)/sizeof(unsigned), params, set_rm_enacting_cback);
        return TRUE;
    }
    case EP_RATEMATCH_ADJUSTMENT:
    {
        endpoint->state.a2dp.rm_adjust_amount = (unsigned)value;
        return TRUE;
    }
    default:
        return FALSE;
    }
}

bool a2dp_buffer_details(ENDPOINT *endpoint, BUFFER_DETAILS *details)
{
    if (endpoint == NULL || details == NULL)
    {
        return FALSE;
    }

#ifdef INSTALL_METADATA
    /* This endpoint type may produce/consume metadata. */
    endpoint_a2dp_state *a2dp = &endpoint->state.a2dp;

    details->supports_metadata = FALSE;
    if (SOURCE == endpoint->direction)
    {
        if (buff_has_metadata(a2dp->source_buf))
        {
            details->supports_metadata = TRUE;
        }
    }
    else
    {
        if (buff_has_metadata(a2dp->sink_buf))
        {
            details->supports_metadata = TRUE;
        }
    }
    details->metadata_buffer = NULL;
#endif /* INSTALL_METADATA */

    details->b.buff_params.size = A2DP_BUFFER_MIN_SIZE;
    details->b.buff_params.flags = BUF_DESC_SW_BUFFER;
    details->supplies_buffer = FALSE;
    details->runs_in_place = FALSE;
    details->wants_override = details->can_override = FALSE;
    return TRUE;
}

/**
 * \brief Connect to the endpoint.
 *
 * \param *endpoint pointer to the endpoint that is being connected
 * \param *Cbuffer_ptr pointer to the Cbuffer struct for the buffer that is being connected.
 * \param *ep_to_kick pointer to the endpoint which will be kicked after a successful
 *              run. Note: this can be different from the connected to endpoint when
 *              in-place running is enabled.
 * \param *start_on_connect return flag which indicates if the endpoint wants be started
 *              on connect. Note: The endpoint will only be started if the connected
 *              to endpoint wants to be started too.
 *
 * \return success or failure
 */
bool a2dp_connect(ENDPOINT *endpoint, tCbuffer *cbuffer_ptr, ENDPOINT *ep_to_kick, bool* start_on_connect)
{
    endpoint->ep_to_kick = ep_to_kick;

    cbuffer_set_usable_octets(cbuffer_ptr, A2DP_USABLE_OCTETS_PER_SAMPLE);

    if (SOURCE == endpoint->direction)
    {
        endpoint->state.a2dp.sink_buf = cbuffer_ptr;
    }
    else
    {
        endpoint->state.a2dp.source_buf = cbuffer_ptr;
    }
    *start_on_connect = FALSE;
    return TRUE;
}

void a2dp_source_kick_common(ENDPOINT *ep)
{
    patch_fn_shared(stream_a2dp_kick);
#ifdef TIMED_PLAYBACK_MODE
    /* No play flag is used when the timed playback mode is used. */
    ep->connected_to->functions->kick(ep->connected_to, STREAM_KICK_FORWARDS);
#endif /* TIMED_PLAYBACK_MODE */
}

bool a2dp_disconnect(ENDPOINT *endpoint)
{
    if (SOURCE == endpoint->direction)
    {
        endpoint->state.a2dp.sink_buf = NULL;
    }
    else
    {
        endpoint->state.a2dp.source_buf = NULL;
    }

    return TRUE;
}

bool a2dp_start_common(ENDPOINT *ep)
{
    patch_fn_shared(stream_a2dp);
    endpoint_a2dp_state *a2dp = &ep->state.a2dp;
    /* If we are already running then don't do anything */
    if(a2dp->running)
    {
        return FALSE;
    }
    /* Initialise internal state */
    a2dp->stalled = FALSE;

    return TRUE;
}

void a2dp_get_timing_common(ENDPOINT *ep, ENDPOINT_TIMING_INFORMATION *time_info)
{
    time_info->has_deadline = FALSE;
    time_info->is_volatile = TRUE;
    time_info->block_size = 1; /* This should go when the scheduler is finished */
    if (ep->direction == SINK)
    {
        /* To-air A2DP is just sent as and when we dictate so this end owns the
         * clock source. */
        time_info->locally_clocked = TRUE;
    }
    else
    {
#ifdef TIMED_PLAYBACK_MODE
        /* Disable rate matchin in timed playback mode */
        time_info->locally_clocked = TRUE;
#else
        time_info->locally_clocked = FALSE;
#endif
    }
}

#ifndef TIMED_PLAYBACK_MODE
/**
 * \brief Calculate the measured a2dp arrival rate seen relative to rate it is
 * being consumed.
 *
 * \param endpoint The a2dp endpoint to calculate the rate of.
 *
 * \return The measured rate in Q22 form.
 */
static int a2dp_rate(ENDPOINT *endpoint)
{
    int diff;
    patch_fn_shared(stream_a2dp);
    endpoint_a2dp_state *a2dp = &endpoint->state.a2dp;
    /* The sink will just go at whatever rate the dsp supplies data so just say
     * perfect.
     */
    if(endpoint->direction == SINK)
    {
        return RM_PERFECT_RATE;
    }

    /* Only calculate a new rate if the data isn't stalled. As nothing new/valid
     * is happening in this state.
     */
    if (a2dp->stalled)
    {
        return RM_PERFECT_RATE;
    }

    /* The rate missmatch is based upon the current average buffer level difference
     * from the target buffer level. 1000 words away from the target level equates
     * to rate missmatch which saturates at 0.5% warp in whichever direction.
     */
    diff = (unsigned)(endpoint->state.a2dp.average_level >> RM_A2DP_NET_SHIFT) - endpoint->state.a2dp.target_level;

    diff = FRACTIONAL_TO_QFORMAT(diff * FRACTIONAL(RM_CONSTANT), STREAM_RATEMATCHING_FIX_POINT_SHIFT);

    diff = RM_PERFECT_RATE + diff;

    /* Limit the maximum requested rate adjustment to +/- 0.5% */
    if (diff > (int)(1.005 * RM_PERFECT_RATE))
    {
        diff = (int)(1.005 * RM_PERFECT_RATE);
    }
    else if (diff < (int)(0.995 * RM_PERFECT_RATE))
    {
        diff = (int)(0.995 * RM_PERFECT_RATE);
    }

    return diff;
}
#endif

bool a2dp_get_config(ENDPOINT *endpoint, unsigned key, ENDPOINT_GET_CONFIG_RESULT* result)
{
    /* A2DP specific endpoint configuration code to go here.
     */
    switch(key)
    {
    case EP_DATA_FORMAT:
        result->u.value = AUDIO_DATA_FORMAT_ENCODED_DATA;
        return TRUE;
    case EP_RATEMATCH_ABILITY:
        result->u.value = (uint32)RATEMATCHING_SUPPORT_NONE;
        return TRUE;
    case EP_RATEMATCH_RATE:
#ifdef TIMED_PLAYBACK_MODE
        result->u.value = RM_PERFECT_RATE;
#else
        result->u.value = a2dp_rate(endpoint);
#endif
        return TRUE;
    case EP_RATEMATCH_MEASUREMENT:
#ifdef TIMED_PLAYBACK_MODE
        result->u.rm_meas.sp_deviation = 0;
#else
        result->u.rm_meas.sp_deviation =
                STREAM_RATEMATCHING_RATE_TO_FRAC(a2dp_rate(endpoint));
#endif
        result->u.rm_meas.measurement.valid = FALSE;
        return TRUE;
    default:
        return FALSE;
    }
}
