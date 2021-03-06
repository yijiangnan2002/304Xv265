/****************************************************************************
* Copyright (c) 2017 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 * \file stream_delegate_rate_adjust.h
 * \ingroup stream
 *
 * Public APIs function for sending messages to a standalone rate adjust operator
 */
#ifndef STREAM_DELEGATE_RATE_ADJUST_H
#define STREAM_DELEGATE_RATE_ADJUST_H

#include "opmgr/opmgr_common.h"

/**
 * \brief send message to a stanalone rate adjust op to set the target rate
 *
 * \param opid operator id for the standalone rate adjust operator
 * \param target_rate target rate
 */
extern void stream_delegate_rate_adjust_set_target_rate(EXT_OP_ID opid,
                                                        unsigned target_rate);

/**
 * \brief sends message to a standalone rate adjust op to enable/disable
 *        passthrough mode
 *
 * \param opid operator id for the standalone rate adjust operator
 * \param enable if TRUE enables pass-through mode else disables it
 */
extern void stream_delegate_rate_adjust_set_passthrough_mode(EXT_OP_ID opid,
                                                             bool enable);

/**
 * \brief send message to a standalone rate adjust op to set the current
 *             rate of the rate adjust operator
 *
 * \param opid operator id for the standalone rate adjust operator
 * \param rate the current rate to be used
 *
 * Note: This message when delivered will directly set the current sra rate,
 *       suitable for TTP-type rate adjustment.
 */
extern void stream_delegate_rate_adjust_set_current_rate(EXT_OP_ID opid,
                                                         unsigned rate);

#endif /* STREAM_DELEGATE_RATE_ADJUST_H */
