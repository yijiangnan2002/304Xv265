/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   adk_test_common_test
\ingroup    common
\brief      Interface for common testing functions.
*/

#ifndef COMMON_TEST_H
#define COMMON_TEST_H

/*!
 * \brief Check if requested Handset is connected over QHS or not.
 * 
 * \param handset_bd_addr BT address of the device.
 * 
 * \return TRUE if requested Handset is connected over QHS.
 *         FALSE if it is not connected over QHS or NULL BT address supplied.
 */
bool appTestIsHandsetQhsConnectedAddr(const bdaddr* handset_bd_addr);

/*!
 * \brief Check if requested Handset's SCO is active or not.
 * 
 * \param handset_bd_addr BT address of the device.
 * 
 * \return TRUE if requested Handset's SCO is active.
 *         FALSE if its SCO is not active or NULL BT address supplied.
 */
bool appTestIsHandsetHfpScoActiveAddr(const bdaddr* handset_bd_addr);

/*!
 * \brief Check if requested Handset is connected.
 *
 * \param handset_bd_addr BT address of the device.
 *
 * \return TRUE if the requested Handset has at least one profile connected.
 *         FALSE if it has no connected profiles.
 */
bool appTestIsHandsetAddrConnected(const bdaddr* handset_bd_addr);

/*!
 * \brief Enable the common output chain feature if it's been compiled in
 */
void appTestEnableCommonChain(void);

/*!
 * \brief Disable the common output chain feature if it's been compiled in
 */
void appTestDisableCommonChain(void);

#endif /* COMMON_TEST_H */
