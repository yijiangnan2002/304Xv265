/* Copyright (c) 2020 Qualcomm Technologies International, Ltd. */
/*   %%version */

/* Link script for Aura Plus P1 */
/* Based on CS-407343-DC */

ENTRY($reset)

$DATA_RAM_SIZE_BYTES    = 40 * 1024;
/*See CS-333748-DD for memory layout */
$DATA_ROM_WINDOW        = 0x78000000;
$DATA_RAM_START_ADDRESS = 0x00010000;

INCLUDE link_script_chip_common.ld  /* common to P0 and P1 for Aura Plus */
INCLUDE link_script_common.ld           /* common across chips */
