/* Copyright (c) 2020 Qualcomm Technologies International, Ltd. */
/*   %%version */
#ifndef IPC_SIGNAL_H_
#define IPC_SIGNAL_H_

#include "ipc/ipc_prim.h"


/**
 * The autogenerated signals are numbered by the corresponding
 * trap ID, which contains the trapset index in the upper word,
 * and trapsets are numbered from 1. Hence this is the lowest
 * possible number for an autogenerated signal.
 */
#define IPC_SIGNAL_AUTOGEN_BASE (0x10000)

/**
 * Bit position in a signal ID indicating that it is a response.
 *
 * @warning Only applicable to autogenerated signals.
 */
#define IPC_SIGNAL_IS_RESPONSE_BIT (15)

/**
 * Mask for determining if a signal ID is a response.
 *
 * @warning Only applicable to autogenerated signals.
 */
#define IPC_SIGNAL_IS_RESPONSE_MASK (1 << IPC_SIGNAL_IS_RESPONSE_BIT)

/**
 * @brief Determine whether this is a P1 to P0 signal that blocks the current
 * task until the response is received.
 *
 * @param [in] id  The signal to test.
 *
 * @return TRUE if it is a blocking request, FALSE otherwise.
 */
bool ipc_signal_is_blocking_request(IPC_SIGNAL_ID id);

/**
 * @brief Determine whether this is a P0 to P1 signal sent in response to a
 * blocking signal.
 *
 * @param [in] id  The signal to test.
 *
 * @return TRUE if it is a blocking response, FALSE otherwise.
 */
bool ipc_signal_is_blocking_response(IPC_SIGNAL_ID id);

/**
 * @brief Determine whether this signal is a blocking request / response or not.
 *
 * @param [in] id  The signal to test.
 *
 * @return TRUE if it is a blocking request or response, FALSE otherwise.
 */
bool ipc_signal_is_blocking(IPC_SIGNAL_ID id);

/**
 * @brief Determine whether this signal is the P0 to P1 response for a blocking
 * SQIF write or erase operation.
 *
 * @param [in] id  The signal to test.
 *
 * @return TRUE if it is a response for a blocking SQIF modify, FALSE otherwise.
 */
bool ipc_signal_is_response_for_blocking_sqif_modify(IPC_SIGNAL_ID id);

/**
 * @brief Determine whether this signal is executed on the Audio subsystem.
 *
 * @param [in] id  The signal to test.
 *
 * @return TRUE if it is executed on the Audio subsystem, FALSE otherwise.
 */
bool ipc_signal_is_executed_on_audio(IPC_SIGNAL_ID id);

/**
 * @brief Determine whether this signal is an autogenerated response.
 *
 * @param [in] _id  The signal to test
 *
 * @return TRUE if it is an autogenerated response, FALSE otherwise.
 */
#define ipc_signal_is_autogen_response(_id) \
    (ipc_signal_is_autogen(_id) && ((_id) & IPC_SIGNAL_IS_RESPONSE_MASK))

/**
 * @brief Determine whether this signal is autogenerated.
 *
 * @param [in] _id  The signal to test
 *
 * @return TRUE if it is an autogenerated signal, FALSE otherwise.
 */
#define ipc_signal_is_autogen(_id) ((_id) >= IPC_SIGNAL_AUTOGEN_BASE)

#endif /* IPC_SIGNAL_H_ */
