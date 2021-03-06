/****************************************************************************
 * Copyright (c) 2011 - 2017 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 * \file  hydra_mmu_buff.c
 * \ingroup buffer
 *
 * Code for accessing MMU buffers.
 */

#include <string.h>
#include "buffer_private.h"
#include "hydra_cbuff.h"
#include "hal.h"
#include "hal_dm_sections.h"
#include "pl_timers/pl_timers.h"
#include "pl_assert.h"
#if defined(SUPPORTS_MULTI_CORE)
#include "ipc/ipc.h"
#endif


#ifndef BAC32
#error "We only do Hydra with BAC32"
#endif

#if CHIP_HAS_DEDICATED_APPS_QSPI_TRB_QUEUE
/* Remote destination block ID.
   This only applies when BAC_RW_PARAMS.RW_MAPPING_MODE is set to
   DIRECT_ACCESS_MAPPING. Generally this should be set to 0 but for
   accessing SQIF in Application subsystem this should be set to
   appropriate value which is 9 for StrePlus.*/
#define SET_BLOCK_ID_RW3_MAPPING()    hal_set_block_id_rw3_mapping(9)
#else
#define SET_BLOCK_ID_RW3_MAPPING()    ((void)0)
#endif /* CHIP_HAS_DEDICATED_APPS_QSPI_TRB_QUEUE */

/**
 *  Function declarations and globals
 */

/* mmu.h promises that we will define this. */
mmu_handle MMU_HANDLE_NULL = {0, MMU_INDEX_NULL, 0};

/* NULL buffer handle structure */
audio_buf_handle_struc BUFF_HANDLE_NULL = {0,0,0};

/* This is the array we use to hold our buffer handles. Nothing should be looking at this except us! */
/* The audio buffer handles are shared between P0 and P1. P1 access it through ipc table
 * TODO: The write access must be controlled via a mutex when there is a requirement for P1 write.
 */
#define NUM_MMU_BUFFER_HANDLES 128
#if defined(SUPPORTS_MULTI_CORE)
static DM_SHARED_ZI audio_buf_handle_struc mmu_buffer_handle_list[NUM_MMU_BUFFER_HANDLES];
static audio_buf_handle_struc *mmu_buffer_handles;
#else
static DM_SHARED_ZI audio_buf_handle_struc mmu_buffer_handles[NUM_MMU_BUFFER_HANDLES];
#endif

/**
 * Select a buffer size large enough for data
 */
static mmu_buffer_size mmu_choose_buffer_size(unsigned num_octets)
{
    patch_fn_shared(mmu_buffer_create);

    /* Number of pages we need (rounded up, by necessity) */
    unsigned int npages = (num_octets - 1) / MMU_PAGE_BYTES + 1;

    /* We can only allocate buffers that are 2^n pages big, so the following
     * loop works out the actual buffer size we have to use.
     */
    mmu_buffer_size size = MMU_BUFFER_SIZE_64;
    unsigned int capacity = MMU_PAGE_TABLE_PAGES(size);

    while (capacity < npages)
    {
        if (size == MAX_MMU_BUFFER_SIZE)
        {
            panic_diatribe(PANIC_AUDIO_MMU_TOO_LARGE, num_octets);
        }
        size = (mmu_buffer_size) (size + 1);
        capacity <<= 1;
    }

    /* A suitable buffer size has been found.
     * The worst-case scenario here is something like: the user
     * wanted 288 octets, which requires 5 pages (320 octets).
     * That then gets rounded up to 8 pages (512 octets), wasting almost
     * half the space in the buffer.
     * We therefore require users to choose their buffer sizes carefully!
     */
    return size;
}

/* Public functions */

/**
 * Initialise the MMU buffer hardware
 */
void mmu_buffer_initialise(void)
{
    patch_fn_shared(mmu_buffer_create);

#if defined(SUPPORTS_MULTI_CORE)
    if (PROC_SECONDARY_CONTEXT())
    {
        if(ipc_get_lookup_addr(IPC_LUT_ID_MMU_SHARED_ADDR,
                              (uintptr_t*)&mmu_buffer_handles) != IPC_SUCCESS)
        {
            panic_diatribe(PANIC_IPC_LUT_CORRUPTED, IPC_LUT_ID_MMU_SHARED_ADDR);
        }
    }
    else
    {
        mmu_buffer_handles = &mmu_buffer_handle_list[0];
        /* populate the MMU handle shared to the IPC LUT */
        if(ipc_set_lookup_addr(IPC_LUT_ID_MMU_SHARED_ADDR,
                           (uintptr_t)mmu_buffer_handles) != IPC_SUCCESS )
        {
            panic_diatribe(PANIC_IPC_LUT_CORRUPTED, IPC_LUT_ID_MMU_SHARED_ADDR);
        }
    }
#endif /* defined(SUPPORTS_MULTI_CORE) */



   /* Program the start of the buffer handle memory into BAC hardware.
    * No need to initialise the buffer handle array as it's in the ZI region of DM. */
   hal_set_reg_bac_buffer_handle_block_offset(DM1_TO_EXTERNAL_ADDRESS(mmu_buffer_handles));

#ifdef CHIP_BASE_HYDRA
   /* Now we do some initialisation of the buffer windows. Most of these settings never
    * change, so for speed we want to avoid setting them every time we perform
    * a read or write. */

    /* Reads are always performed via window 0 */
    /* Default mapping is to our own null buffer; that means we'll cause a
     * VM_NULL_BUFFER_REQ_ERROR if anything tries to access it unexpectedly.
     * This should result in an OUTBOUND_ACCESS_ERROR_EVENT interrupt, which
     * we don't specifically handle (thus resulting in a
     * PANIC_AUDIO_UNHANDLED_INTERRUPT). */
    hal_set_subsystem_id_rw0_mapping(hydra_get_local_ssid());
    hal_set_handle_id_rw0_mapping(0);
    hal_set_rw_mapping_mode_rw0_params(BUFFER_ACCESS_MAPPING);
    hal_set_rw_data_width_rw0_params(SIXTEEN_BIT); /* Very occasionally we put the window into 8-bit mode */
    hal_set_rw_access_permission_rw0_params(READ_ONLY);
    hal_set_rw_data_endianness_rw0_params(LITTLE_ENDIAN);
    hal_set_reg_bac_rw0_offset(0x000000);

    /* Writes always use window 1 */
    /* Default mapping is to our own null buffer, as above. */
    hal_set_subsystem_id_rw1_mapping(hydra_get_local_ssid());
    hal_set_handle_id_rw1_mapping(0);
    hal_set_rw_mapping_mode_rw1_params(BUFFER_ACCESS_MAPPING);
    hal_set_rw_data_width_rw1_params(EIGHT_BIT);

    hal_set_rw_access_permission_rw1_params(READ_ONLY); /* Will be temporarily modified each time we write */

    hal_set_rw_data_endianness_rw1_params(LITTLE_ENDIAN);
    hal_set_reg_bac_rw1_offset(0x000000);

    /* Wallclock reads are always performed via window 2.
     * We use a dedicated window for the wallclock since we'd like
     * it to be permanently mapped in. That way we can guarantee that it will
     * be visible in a coredump.
     * Map in our own null buffer to start off with, so we have a known
     * starting-point. */
    hal_set_subsystem_id_rw2_mapping(hydra_get_local_ssid());
    hal_set_handle_id_rw2_mapping(0);
    hal_set_rw_mapping_mode_rw2_params(BUFFER_ACCESS_MAPPING);

    hal_set_rw_data_width_rw2_params(THIRTY_TWO_BIT);

    hal_set_rw_access_permission_rw2_params(READ_ONLY);
    hal_set_rw_data_endianness_rw2_params(LITTLE_ENDIAN);
    hal_set_reg_bac_rw2_offset(0x000000);

    /* Accessing a BAC window (which is in the DM1 half of the address
     * space) immediately after writing a register (in DM2) to change
     * its configuration could go wrong, as one access overtakes the
     * other; see B-178412.
     * (It's very unlikely this would happen here, so this nop() is
     * as much for documentation as anything else.) */
    nop();
#endif /* CHIP_BASE_HYDRA */
}

/**
 * Create an MMU buffer.
 */
int *mmu_buffer_create(unsigned *buffer_size, mmu_handle *handlep, unsigned config_flags, unsigned preference)
{
    /* Find the smallest buffer size we can support which holds at
     * least *buffer_size octets of data */
    mmu_buffer_size size_code = mmu_choose_buffer_size(*buffer_size);
    int *buf_mem = NULL;
    int size_octets;

    patch_fn_shared(mmu_buffer_create);

    /* SAMPLE_SIZE_24BIT will not be supported as that requires
     * the stack to understand B' mode 1
     */
    PL_ASSERT(MMU_BUF_SAMP_SZ_GET(config_flags) != SAMPLE_SIZE_24BIT);

    /* Work out the capacity in octets of the buffer we'll actually
     * end up with (which might be bigger than requested). */
    size_octets = (MMU_PAGE_TABLE_PAGES(size_code) * MMU_PAGE_BYTES);

    buf_mem = xzppmalloc(BAC_OFFSET_TO_CHARS(size_octets, config_flags), preference);

    if(buf_mem == NULL)
    {
        /* Return gracefully here. We don't want to panic if the malloc above
         * failed, because we're probably asking for a pretty big buffer here.
         * Failure to allocate the buffer doesn't mean the rest of the system
         * is at risk of falling over if we continue.
         */
        return NULL;
    }

    handlep->index = mmu_buffer_claim_handle(buf_mem, size_code, config_flags);

    if (mmu_index_is_null(handlep->index))
    {
        panic_diatribe(PANIC_AUDIO_BUFFER_HANDLES_EXHAUSTED, (DIATRIBE_TYPE)((uintptr_t)handlep));
    }

    handlep->ssid = hydra_get_local_ssid();

    /* Check whether the handle base address matches the buffer base.
     * If not, assume the buffer is in DM2 and make a note of that in the handle structure.
     */
    if (IS_DM1_ADDRESS(buf_mem))
    {
        handlep->flags = MMU_BUFF_DM1;
    }
    else
    {
        handlep->flags = MMU_BUFF_DM2;
    }

    *buffer_size = mmu_buffer_get_size(*handlep);
    return buf_mem;
}


/**
 * Close a buffer, freeing associated local resources.
 */
void mmu_buffer_destroy(mmu_handle handle)
{
    if (!mmu_handle_is_null(handle) && mmu_handle_is_local(handle))
    {
        unsigned start_p = mmu_buffer_release_handle(handle.index);
        void *buff_ptr;

        if (0 == start_p)
        {
            /* nothing to do, seems like the handle has already been released */
            return;
        }

        /* Adjust the address if in DM2 */
        if (handle.flags == MMU_BUFF_DM2)
        {
            start_p += DM2_RAM_BASE;
        }
        else if (handle.flags == MMU_HANDLE_UNKNOWN_FLAGS)
        {
            /* 'destroy' method should really get the handle created at 'create' time;
               this one seems to have been obtained at a later time,
               (possibly using mmu_buffer_get_handle_from_ptr) or has been corrupted */
            panic_diatribe(PANIC_AUDIO_INVALID_MMU_HANDLE, 0);
        }
        buff_ptr = (void *)(uintptr_t)start_p;
        pfree(buff_ptr);
    }
}

/**
 * Read data into an array from an MMU buffer.
 * We don't do much sanity checking here, but we could still spot an error,
 * hence the bool return value.
 *
 * Note that it *is* possible to read an odd number of octets, but we assume
 * that both the first two octets should be placed into the first destination
 * word (i.e. it's not possible to change the alignment of the octets read).
 */
bool mmu_buffer_read(mmu_handle handle, unsigned int src_buf_offset,
                     unsigned int len_octets, void *dest, bool byte_swap)
{
    patch_fn_shared(mmu_buffer_readwrite);

    if (mmu_handle_is_null(handle))
    {
        panic_diatribe(PANIC_AUDIO_INVALID_MMU_HANDLE, 0);
    }

    unsigned char *dest_buf = (unsigned char *)dest;

    if (mmu_handle_is_local(handle))
    {
        int src_buf_size; /* src buffer *is* circular, so need to be able to wrap it */
        unsigned size_octets;

        /* Work out the size of the source buffer */
        src_buf_size = mmu_buffer_handles[handle.index].buffer_size;
        src_buf_size = src_buf_size & MMU_BUF_SIZE_MASK;  /* Mask out the config bits */
        src_buf_size = MMU_PAGE_BYTES * MMU_PAGE_TABLE_PAGES(src_buf_size); /* size in octets */

        /* Sanity check: have we been asked to read more data than there is in the buffer? */
        if (len_octets > src_buf_size)
        {
            return FALSE;
        }

        /* We don't support the byte-swap parameter for local buffers.
         * Local buffers can be permanently configued via
         * mmu_buffer_set_flags. */
        if (byte_swap)
        {
            fault_diatribe(FAULT_AUDIO_UNSUPPORTED, mmu_port_pack_buffer_handle(handle));
            return FALSE;
        }

        size_octets = mmu_buffer_get_size(handle);
        /* Packing should ideally be configurable. We assume packed so assert when not packed */
        PL_ASSERT(mmu_buffer_handles[handle.index].buffer_size & MMU_BUF_PKG_EN_MASK);
        /* Crescendo does not support wrap, presently, TODO */
        PL_ASSERT((src_buf_offset + len_octets) <= size_octets);

        memcpy(
            dest_buf,
            (unsigned char *)(uintptr_t)(EXTERNAL_ADDRESS_TO_DM1(mmu_buffer_handles[handle.index].buffer_start_p) + src_buf_offset),
            len_octets);
    }
#ifdef CHIP_BASE_HYDRA
    else
    {
        /* mmu_handle is NOT local */
        const unsigned char* src_ptr;
        int rw0_params, rw0_mapping;

        /* Save the state of the remote window before we mess with it
         * (to avoid the need to block interrupts for the whole read).
         * No need to read individual fields here: get the whole register at
         * once, so we don't have to block interrupts. */
        rw0_params = hal_get_reg_bac_rw0_params();
        rw0_mapping = hal_get_reg_bac_rw0_mapping();

        PL_ASSERT(!byte_swap);
        hal_set_rw_data_width_rw0_params(THIRTY_TWO_BIT);
        hal_set_subsystem_id_rw0_mapping(handle.ssid);
        hal_set_handle_id_rw0_mapping(handle.index);
        hal_set_rw_data_endianness_rw0_params(LITTLE_ENDIAN);
        /* Accessing a BAC window (which is in the DM1 half of the address
         * space) immediately after writing a register (in DM2) to change
         * its configuration could go wrong, as one access overtakes the
         * other; see B-178412. A 'nop' instruction is sufficient to
         * defuse this. */
        nop();
        src_ptr = (const unsigned char *)(uintptr_t)(BAC_START_REMOTE_WINDOW_0 + src_buf_offset);
        memcpy(dest_buf, src_ptr, len_octets);

        /* Restore the state of the remote window */
        hal_set_reg_bac_rw0_params(rw0_params);
        hal_set_reg_bac_rw0_mapping(rw0_mapping);
        nop();
    }
#endif /* CHIP_BASE_HYDRA */
    return TRUE;
}

#ifdef INSTALL_DIRECT_FLASH_ACCESS
/**
 * \brief Map BAC window 3 into the Apps address space.
 *
 * \param file_address File address in the remote filesystem
 * \param offset Offset within the file
 * \param remote_win_ptr Pointer to the start of BAC window 3
 */
void map_remote_flash_window(unsigned int file_address, unsigned int offset, unsigned char **remote_win_ptr)
{
    SET_BLOCK_ID_RW3_MAPPING();

    hal_set_reg_bac_rw3_offset(file_address + offset);
    hal_set_subsystem_id_rw3_mapping(SYSTEM_BUS_APPS_SYS);
    hal_set_handle_id_rw3_mapping(0);
    hal_set_rw_mapping_mode_rw3_params(DIRECT_ACCESS_MAPPING);
    hal_set_rw_data_width_rw3_params(THIRTY_TWO_BIT);
    hal_set_rw_access_permission_rw3_params(READ_ONLY);
    hal_set_rw_data_endianness_rw3_params(LITTLE_ENDIAN);

    *remote_win_ptr = (unsigned char *)BAC_START_REMOTE_WINDOW_3;
}
#endif /* INSTALL_DIRECT_FLASH_ACCESS */


#ifdef CHIP_BASE_HYDRA
/** The number of attempts made at reading the wallclock before we give up. This
 * should not take more than 1 or 2 attempts. An arbitrarily high number is
 * chosen to ensure that the retry count isn't accidentally hit. As the loop
 * does not take long to execute the spinning time before panic is still short.
 */
#define MAX_WALLCLOCK_RETRIES    25
/** Size of each wallclock in the wallclock buffer (words) */
#define WALLCLOCK_WORD_SIZE  4
/**
 * Read Wallclock data into an array from an MMU buffer.
 * This is a very streamlined version of mmu_buffer_read for wallclock use. That
 * saves the overhead of using mmu_buffer_read multiple times.
 */

bool mmu_buffer_read_wallclock(mmu_handle handle, unsigned src_buf_offset,
                   uint16 *dest_buf)
{
    uint16 init_tear_counter, end_tear_counter;
    unsigned retry_cnt;
    /* The contents of the wallclock buffer is a) volatile and b) read-only
     * However, we pass it to memcpy below, and can't cast away the volatile qualifier
     * if we add it here. So declare it as not-volatile, and cast to a volatile pointer
     * for the accesses that matter below.
     */
    const uint16* src_ptr;

    patch_fn_shared(mmu_buffer_readwrite);

    if (mmu_handle_is_null(handle))
    {
        panic_diatribe(PANIC_AUDIO_INVALID_MMU_HANDLE, 0);
    }

    /* Block interrupts while the buffer is mapped in */
    interrupt_block();
    /* Map the buffer into our special 'wallclock read' BAC window. */
    hal_set_subsystem_id_rw2_mapping(handle.ssid);
    hal_set_handle_id_rw2_mapping(handle.index);
    /* Work around B-178412; see comment in mmu_buffer_read(). */
    nop();
    /* src_buf_offset in octets needs to be converted to uint16s */
    src_ptr = (const uint16 *)(uintptr_t)(BAC_START_REMOTE_WINDOW_2) + (src_buf_offset >> 1);
    init_tear_counter = end_tear_counter = 0;

    do
    {
        /* If we manage to read during an update make sure we have the full
         * number of retries before we try again. */
        retry_cnt = 0;

        /* read the anti-tear counter until bit 1 is '0' indicating the
         * wallclock is not being updated. BT blocks interrupts here so we are
         * safe to sit in a loop. If we get stuck in the loop something bizarre
         * has happened so panic. */
        do
        {
            init_tear_counter = *(volatile const uint16 *)src_ptr;
        } while ((0 != (init_tear_counter & 0x0001)) && (retry_cnt++ < MAX_WALLCLOCK_RETRIES));

        /* Check that we didn't exit the loop because of a timeout */
        if (retry_cnt > MAX_WALLCLOCK_RETRIES)
        {
            panic_diatribe(PANIC_AUDIO_WALLCLOCK_UPDATE_TIMEOUT, handle.index);
        }

        /* Read the wallclock (skip src_ptr[0]; we don't want the tear_counter) */
        memcpy(dest_buf, &src_ptr[1], (WALLCLOCK_WORD_SIZE * sizeof(uint16)));

        /* Check that the tear_counter hasn't changed */
        end_tear_counter = *(volatile const uint16 *)src_ptr;

        /* If the tear counter changed while we performed the read, the wallclock was
         * updated so we need to read it again. */
    } while (init_tear_counter != end_tear_counter);

    /* Leave the buffer mapped in, since it could aid in coredump diagnosis to
     * know what we were looking at. */

    interrupt_unblock();

    return TRUE;
}
#endif /* CHIP_BASE_HYDRA */


/**
 * Write data to an MMU buffer from an array.
 * It is assumed that users will ensure that dest_buf_offset and len_octets
 * will always be even; these assumptions simplify the copy functions.
 * We don't do much sanity checking here, but we could still spot an error,
 * hence the bool return value.
 */
bool mmu_buffer_write(mmu_handle handle, unsigned dest_buf_offset,
                      unsigned len_octets, const void *src)
{
    patch_fn_shared(mmu_buffer_readwrite);

    const unsigned char *src_buf = (const unsigned char *)src;

    if (mmu_handle_is_null(handle))
    {
        panic_diatribe(PANIC_AUDIO_INVALID_MMU_HANDLE, 0);
    }

    if(mmu_handle_is_local(handle))
    {
        int dest_buf_size;  /* dest buffer *is* circular, so need to be able to wrap it */
        unsigned size_octets;

        /* Work out the size of the destination buffer */
        dest_buf_size = mmu_buffer_handles[handle.index].buffer_size;
        dest_buf_size = dest_buf_size & MMU_BUF_SIZE_MASK;  /* Mask out the config bits */
        dest_buf_size = MMU_PAGE_BYTES * MMU_PAGE_TABLE_PAGES(dest_buf_size); /* size in octets */

        /* Sanity check: have we been asked to write more data than there is space? */
        if (len_octets > dest_buf_size)
        {
            return FALSE;
        }

        size_octets = mmu_buffer_get_size(handle);
        /* Packing should ideally be configurable. We assume packed so assert when not packed */
        PL_ASSERT(mmu_buffer_handles[handle.index].buffer_size & MMU_BUF_PKG_EN_MASK);
        /* Crescendo does not support wrap, presently, TODO */
        PL_ASSERT((dest_buf_offset + len_octets) <= size_octets);

        memcpy(
            (unsigned char *)(uintptr_t)(EXTERNAL_ADDRESS_TO_DM1(mmu_buffer_handles[handle.index].buffer_start_p) + dest_buf_offset),
            src_buf, len_octets);
    }
#ifdef CHIP_BASE_HYDRA
    else
    {
        /* mmu_handle is NOT local */
        int* dest_ptr;
        int rw1_params, rw1_mapping;

        /* Save the state of the remote window before we mess with it
         * (to avoid the need to block interrupts for the whole write).
         * No need to read individual fields here: get whole registers,
         * for speed. The mapping register is write-sensitive but all
         * that happens is that the result of the previous VML is invalidated.
         * So it should be fine to not block interrupts around this. */
        rw1_params = hal_get_reg_bac_rw1_params();
        rw1_mapping = hal_get_reg_bac_rw1_mapping();

        /* Map the buffer into our write window. */
        hal_set_rw_data_endianness_rw1_params(LITTLE_ENDIAN); /* Could have been modified by write_circular */
        hal_set_rw_access_permission_rw1_params(READ_WRITE);
        hal_set_subsystem_id_rw1_mapping(handle.ssid);
        hal_set_handle_id_rw1_mapping(handle.index);
        /* Work around B-178412; see comment in mmu_buffer_read(). */
        nop();

        /* Work out start address - Assuming buffer can fit into remote window
         * at least twice so no need to worry about wrapping on the write side.
         */
#ifdef BROKEN_SUBWORD_WRITE_IN_BAC_WINDOW
        /* Setting data width of the remote window to THIRTY_TWO_BIT and using a memcpy
         * is not useful because of B-168854.
         */
        /*
         * Remote sub-system sees coninguous memory.
         * We see 1 byte every 4 bytes.
         */
        dest_ptr = (int *)(uintptr_t)(BAC_START_REMOTE_WINDOW_1 +
                (dest_buf_offset * 4));
        unsigned int i = 0;
        for (i = 0; i < len_octets; i++)
        {
            dest_ptr[i] = src_buf[i];
        }
#else /* BROKEN_SUBWORD_WRITE_IN_BAC_WINDOW */
#error "FW to leverage sub-word BAC writes is missing."
#endif /* BROKEN_SUBWORD_WRITE_IN_BAC_WINDOW */

        /* Restore the state of the remote window */
        hal_set_reg_bac_rw1_params(rw1_params);
        hal_set_reg_bac_rw1_mapping(rw1_mapping);
        nop();
    }
#endif /* CHIP_BASE_HYDRA */

    return TRUE;
}


/**
 * Read an offset from an MMU handle.
 */
unsigned mmu_buffer_get_handle_offset(mmu_handle handle)
{
    unsigned offset = 0; /* octets */

    if (mmu_handle_is_null(handle))
    {
        panic_diatribe(PANIC_AUDIO_INVALID_MMU_HANDLE, 0);
    }

    if(mmu_handle_is_local(handle))
    {
        offset = mmu_buffer_handles[handle.index].buffer_offset;
    }
#ifdef CHIP_BASE_HYDRA
    else
    {
        patch_fn_shared(mmu_buffer_readwrite);

        /* Block interrupts to make sure nothing else
         * changes these registers until we've finished
         */
        interrupt_block();

        /* If access block is busy when get here we have a design problem! */
        if( !hal_get_outbound_vml_active() )
        {
            /* Tell the hardware which remote handle we want to play with */
            hal_set_handle_id_buffer_handle(handle.index);
            hal_set_subsystem_id_buffer_handle(handle.ssid);

            /* Start the read and spin. The macro below monitors read duration and
             * takes action if the read is too long. Macro takes 2 params -
             *   start_rd - action to start/trigger the read
             *   has_rd_finished - condition to detect read request has finished
             * Note the '!' in use for the second parameter since the macro
             * looks for the negative
             */
            HYDRA_TXBUS_RD( hal_set_reg_bac_outbound_vml_trig_vml(TRIG_VML_READ),
                    /*lint --e{734}*/ !hal_get_outbound_vml_active() );

            /* Note the offset of the remote handle */
            offset = hal_get_reg_bac_outbound_vml_read_offset();
        }
        else
        {
            panic_diatribe(PANIC_HYDRA_MYSTERY, handle.index);
        }
        interrupt_unblock();
    }
#endif /* CHIP_BASE_HYDRA */
    return offset;
}

/**
 * Set the offset stored in an MMU handle.
 */
void mmu_buffer_set_handle_offset(mmu_handle handle, unsigned offset_octets)
{
    if (mmu_handle_is_null(handle))
    {
        panic_diatribe(PANIC_AUDIO_INVALID_MMU_HANDLE, 0);
    }

    if (mmu_handle_is_local(handle))
    {
        /* Validate the offset, since it's cheap */
        if (offset_octets > mmu_buffer_get_size(handle))
        {
            fault_diatribe(FAULT_AUDIO_BUFFER_OFFSET_TOO_BIG, offset_octets);
        }
        mmu_buffer_handles[handle.index].buffer_offset = offset_octets;
    }
#ifdef CHIP_BASE_HYDRA
    else
    {
        patch_fn_shared(mmu_buffer_readwrite);

        /* Block interrupts to make sure nothing else
         * changes these registers until we've finished
         */
        interrupt_block();
        /* If access block is busy when get here we have a design problem! */
        if( !hal_get_outbound_vml_active() )
        {
            /* Tell the hardware which remote handle we want to play with */
            hal_set_handle_id_buffer_handle(handle.index);
            hal_set_subsystem_id_buffer_handle(handle.ssid);

            /* Load new offset */
            hal_set_writeback_offset(offset_octets);

            /* Trigger the write back */
            hal_set_reg_bac_outbound_vml_trig_vml(TRIG_VML_WRITE);

            /*
             * Problems with write are signalled asynchronously (interrupt)
             * so we are done here.
             */
        }
        else
        {
            panic_diatribe(PANIC_HYDRA_MYSTERY, handle.index);
        }

        interrupt_unblock();
    }
#endif /* CHIP_BASE_HYDRA */
}

/**
 * Returns the MMU buffer size in octets
 */
unsigned mmu_buffer_get_size(mmu_handle handle)
{
    unsigned size_octets;

    if (mmu_handle_is_local(handle))
    {
        unsigned buffer_size = mmu_buffer_handles[handle.index].buffer_size;
        buffer_size &= MMU_BUF_SIZE_MASK;  /* Mask out the config bits */

        /* If B'mode == 0, capacity is <page size> * 2^buffer_size[3:0] octets */
        /* If B'mode == 1, capacity is <page size> * buffer_size[11:0] octets */
        /* Here, as elsewhere, we assume B'mode is 0. */
        size_octets = MMU_PAGE_BYTES * MMU_PAGE_TABLE_PAGES(buffer_size);
    }
    else
    {
        /* We don't currently have a way of finding out the size
         * of a remote buffer */
        fault_diatribe(FAULT_AUDIO_UNSUPPORTED, 0);
        size_octets = 0;
    }
    return size_octets;
}

/*
 * This crude sketch may help to understand the buffer handle array and the heinous crimes we commit below.
 *
 *  |----------------------|
 *  |        index         |
 *  |----------------------|   <- buf_handle_ptr  (= &mmu_buffer_handles[index])
 *  ...                   ...
 *  |----------------------|
 *  |          1           |   } audio_buf_handle_struc (sizeof(audio_buf_handle_struc) bytes big)
 *  |----------------------|
 *  |          0           |
 *  |----------------------|   <- mmu_buffer_handles
*/

/**
 * Given a pointer to somewhere in the mmu_buffer_handles array, find its index
 */

/* These functions use the size of a audio_buf_handle_struc in bytes, where
 * 'bytes' has its usual definition of 'smallest addressable unit'.
 * We need this so that we can normalise the result of the pointer arithmetic;
 * ptr_a - ptr_b will also be measured in bytes.
 */

mmu_index mmu_buffer_get_handle_idx_from_ptr(audio_buf_handle_struc* buf_handle_ptr)
{
    return (mmu_index)(buf_handle_ptr - mmu_buffer_handles);
}

/* This is a private function returning a newly created mmu_handle.
 * Caution: The mmu_handle->flags is undefined; this information is not stored
 * in the mmu_buffer_handles tables (because the BAC32 doesn't know about it).
 */
mmu_handle mmu_buffer_get_handle_from_ptr(audio_buf_handle_struc* buf_handle_ptr)
{
    mmu_handle ret;
    ret.ssid = hydra_get_local_ssid();
    ret.index = (mmu_index)(buf_handle_ptr - mmu_buffer_handles);
    ret.flags = MMU_HANDLE_UNKNOWN_FLAGS;
    return ret;
}

/**
 * Given an index into mmu_buffer_handles, return a pointer to the associated handle.
 */
audio_buf_handle_struc *mmu_buffer_get_handle_ptr_from_idx(mmu_index index)
{
    return &mmu_buffer_handles[index];
}

/**
 * Create an mmu_handle structure to wrap an mmu_index.
 */
mmu_handle mmu_handle_from_idx(mmu_index index)
{
    mmu_handle ret;
    ret.ssid = hydra_get_local_ssid();
    ret.index = index;
    ret.flags = MMU_HANDLE_UNKNOWN_FLAGS;
    return ret;
}

/**
 * Take a pre-existing mmu_handle, claim a new one and copy the buffer parameters across.
 */
mmu_handle mmu_buffer_clone_handle(mmu_handle handle)
{
    mmu_handle ret = MMU_HANDLE_NULL;
    mmu_index idx = 1; /* Need to keep index 0 free, it is the 'null' buffer handle. */
    mmu_index idx_in = handle.index;

    patch_fn_shared(mmu_buffer_create);

    /* same mechanism as for claim handle - look for the first free buffer handle */
    while ( idx < NUM_MMU_BUFFER_HANDLES )
    {
        /* start_p zero means handle free */
        if (mmu_buffer_handles[idx].buffer_start_p == 0)
        {
            interrupt_block();
            /* atomic, check again to make sure it wasn't changed under your nose */
            if (mmu_buffer_handles[idx].buffer_start_p == 0)
            {
                /* according to the loose criteria to find the next free handle above,
                    this should be enough, re-enable interrupts as early as you can */
                mmu_buffer_handles[idx].buffer_start_p = mmu_buffer_handles[idx_in].buffer_start_p;
                interrupt_unblock();
                mmu_buffer_handles[idx].buffer_offset = mmu_buffer_handles[idx_in].buffer_offset;
                mmu_buffer_handles[idx].buffer_size = mmu_buffer_handles[idx_in].buffer_size;
                ret.ssid = hydra_get_local_ssid();
                ret.index = idx;
                break;
            }
            else
            {
                /* oups! what are the chances, a higher priority task
                   has taken this handle in the meantime, keep searching */
                interrupt_unblock();
                ++idx;
            }
        }
        else
        {
            ++idx;
        }
    }
    return ret; /* Returns null handle if no valid index available */
}

/**
 * Take a pre-existing buffer and give it to the MMU by allocating a buffer handle.
 */
mmu_index mmu_buffer_claim_handle(void* buf_mem, mmu_buffer_size buffer_size, unsigned config_flags)
{
    mmu_index idx = 1, ret = 0;

    patch_fn_shared(mmu_buffer_create);
    /* Need to keep index 0 free, it is the 'null' buffer handle.
     * So we iterate through the array starting at index 1, and return 0 on failure. */
    while ( idx < NUM_MMU_BUFFER_HANDLES )
    {
        /* start_p zero means handle free */
        if (mmu_buffer_handles[idx].buffer_start_p == 0)
        {
            interrupt_block();
            /* atomic, check again to make sure it wasn't changed under your nose */
            if (mmu_buffer_handles[idx].buffer_start_p == 0)
            {
                /* according to the loose criteria to find the next free handle above,
                    this should be enough, re-enable interrupts as early as you can */
            mmu_buffer_handles[idx].buffer_start_p = DM1_TO_EXTERNAL_ADDRESS(ptr_to_octet(buf_mem));
                interrupt_unblock();
                mmu_buffer_handles[idx].buffer_offset = 0;
                /* TODO - we could do more to validate the flags here, but for now assume they're good. */
                mmu_buffer_handles[idx].buffer_size = buffer_size | (config_flags & MMU_BUF_ALL_FLAGS_MASK);
                ret = idx;
                break;
            }
            else
            {
                /* oups! what are the chances, a higher priority task
                   has taken this handle in the meantime, keep searching */
                interrupt_unblock();
                ++idx;
            }
        }
        else
        {
            ++idx;
        }
    }
    return ret; /* Returns 0 if no valid handle available */
}

/**
 * Remove a local buffer from MMU control by freeing its buffer handle.
 * Does NOT free the buffer memory itself.
 */
unsigned mmu_buffer_release_handle(mmu_index index)
{
    unsigned start_p = 0;

    patch_fn_shared(mmu_buffer_create);

    if ( mmu_index_is_null(index) || (index >= NUM_MMU_BUFFER_HANDLES) )
    {
        /* The caller did a bad thing */
        panic_diatribe(PANIC_AUDIO_INVALID_MMU_HANDLE, 0);
    }
    else
    {
        start_p = octet_to_ptr(mmu_buffer_handles[index].buffer_start_p);
        mmu_buffer_handles[index].buffer_start_p = 0;
        mmu_buffer_handles[index].buffer_offset = 0;
        mmu_buffer_handles[index].buffer_size = 0;
    }

    return start_p;
}

/**
 * Read the config flags for an MMU buffer.
 */
unsigned mmu_buffer_get_flags(mmu_handle handle)
{
    unsigned flags;

    patch_fn_shared(mmu_buffer_create);

    if (!mmu_handle_is_local( handle ))
    {
        /* The caller supplied a remote buffer handle */
        panic_diatribe(PANIC_AUDIO_INVALID_MMU_HANDLE, mmu_handle_pack(handle));
    }

    flags = mmu_buffer_handles[handle.index].buffer_size;
    flags &= MMU_BUF_ALL_FLAGS_MASK;    /* Mask out the buffer size itself, to leave just the flags. */
    return flags;
}

/**
 * Set the config flags for an MMU buffer.
 */
void mmu_buffer_set_flags(mmu_handle handle, unsigned config_flags)
{
    unsigned buffer_size;

    patch_fn_shared(mmu_buffer_create);

    if (!mmu_handle_is_local( handle ))
    {
        /* The caller supplied a remote buffer handle */
        panic_diatribe(PANIC_AUDIO_INVALID_MMU_HANDLE, mmu_handle_pack(handle));
    }

    buffer_size = mmu_buffer_handles[handle.index].buffer_size;
    buffer_size &= MMU_BUF_SIZE_MASK;    /* Clear any flags that are already set */
    /* TODO - we could do more to validate the flags here, but for now assume they're good. */
    buffer_size |= (config_flags & MMU_BUF_ALL_FLAGS_MASK);
    mmu_buffer_handles[handle.index].buffer_size = buffer_size;
}


/**
 *
 * mmu_release_handle - release read or write and aux handle
 *
 * Input argument:
 *      cbuffer   - pointer to a cbuffer structure (that encapsulates the mmu buffer) to release
 *
 * Return value
 *      pointer to buffer handle to free
 */
void hydra_mmu_release_handle(tCbuffer *cbuffer)
{
    patch_fn_shared(mmu_buffer_create);

    if (BUF_DESC_RD_PTR_TYPE_MMU(cbuffer->descriptor))
    {
        (void)mmu_buffer_release_handle( mmu_buffer_get_handle_idx_from_ptr((audio_buf_handle_struc *)cbuffer->read_ptr) );
    }

    if (BUF_DESC_WR_PTR_TYPE_MMU(cbuffer->descriptor))
    {
        (void)mmu_buffer_release_handle( mmu_buffer_get_handle_idx_from_ptr((audio_buf_handle_struc *)cbuffer->write_ptr) );
    }

    /* If we have an aux handle, free that too.  */
    if (BUF_DESC_AUX_PTR_PRESENT(cbuffer->descriptor))
    {
        (void)mmu_buffer_release_handle( mmu_buffer_get_handle_idx_from_ptr((audio_buf_handle_struc *)cbuffer->aux_ptr) );
    }
}
