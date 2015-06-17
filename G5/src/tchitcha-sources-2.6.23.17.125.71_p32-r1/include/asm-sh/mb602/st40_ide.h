/*
 * include/asm-sh/mb602/st40_ide.h
 *
 * This file contains all the definitions to support
 * the IDE interface on mb602 board.
 * IDE is mapped into EMI bank3.
 */

/* STb5202 only has one hw interface */
#undef MAX_HWIFS
#define MAX_HWIFS       1

/* The ATA base address. This is the base of EMI bank 3. */
#define ATA_ADDRESS           0xA2800000

/* The ATA data base address offset. It is used to map
 * all the ide registers on the hwif interface
 * starting from this offset.
 */
#define ATA_DATA_OFFS_ADDRESS	0x00200000

/* The ide registers offset. Registers will be stored into
 * the hwif interface with a costant offset starting from
 * the ATA data register address.
 */
#define REG_OFFSET		0x20000

/* Only the control register has a different offset. */
#define CTRL_REG_OFFSET		-0x40000

/* ATA IRQ number */
#define ATA_IRQ			IRL1_IRQ

/* ide_enable does nothing because there is no external hardware to configure */
#define ide_enable()

/* hddi_reset does nothing for stb7100, because it does not have a HDDI interface. */
#define hddi_reset()

/* hddi_set_pio_timings does nothing for mb411, because it has not an HDDI
 * interface and PIO timings are setted into the EMI bank3 (PIO4).
 */
#define hddi_set_pio_timings(rate)

/* ide_ack_intr returns 1 when invoked.
 * It is needed by the ide_intr function.
 */
#define ide_ack_intr(hwif)	1

/* DMA in not supported for mb411 board, so hddi_setup_dma does nothing. */
#define hddi_setup_dma(interface, address, offset)
