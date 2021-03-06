/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       dfu_config.h
\brief      Configuration related definitions for dfu domain.
*/

#ifndef DFU_CONFIG_H_
#define DFU_CONFIG_H_

#ifdef INCLUDE_DFU

#include <upgrade.h>

#ifdef INCLUDE_DFU_PEER

/*! Select the default upgrade scheme for peer upgrade-  serialized or concurrent/serialized
  */
#define Dfu_ConfigUpgradePeerScheme()              (upgrade_peer_scheme_concurrent)

#endif /* INCLUDE_DFU_PEER */

#endif /* INCLUDE_DFU */

#endif /* DFU_CONFIG_H_ */

