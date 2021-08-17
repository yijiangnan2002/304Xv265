/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\brief      Kymera private header for output chain APIs.
*/

#ifndef KYMERA_OUTPUT_H
#define KYMERA_OUTPUT_H


/*! \brief Load downloadable capabilities for the output chain in advance.
    \param chain_type Chain type to use when loading the capabilities.
*/
void KymeraOutput_LoadDownloadableCaps(output_chain_t chain_type);

/*! \brief Undo KymeraOutput_LoadDownloadableCaps.
    \param chain_type Should be the same type used in KymeraOutput_LoadDownloadableCaps
*/
void KymeraOutput_UnloadDownloadableCaps(output_chain_t chain_type);

#endif /* KYMERA_OUTPUT_H */
