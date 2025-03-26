#ifndef TAGET_H
#define TAGET_H
#define	LCD_CMD_ADDR	(uint16_t *)0x60000000	/* LCD Located at FMC bank 1 */
#define	LCD_DATA_ADDR	(uint16_t *)0x60080000

/*#define  USE_LISSAJOUS*/

#define	QSPI_FLASH_ADDR	0x90000000
#define	QSPI_PSRAM_ADDR	0x70000000
#define	QSPI_PSRAM_SIZE	(8*1024*1024)

#define	DISP_HOR_RES	480
#define	DISP_VER_RES	320

#define	SND_CACHESIZE	(2 * 1024 * 1024)	/* DOOM sound cache size */

#define RTOS_HEAP_ADDR	0x20098000		/* FreeRTOS Heap starting address */
#define	RTOS_HEAP_SIZE	0x00008000		/* Size of FreeRTOS Heap space */
#define LV_HEAP_ADDR	0x200A0000		/* LVGL Heap starging address */
#define	LV_HEAP_SIZE	(128*1024)		/* Size of LVGL Heap space */
#define	DRLIB_HEAP_SIZE	(40*1024)

#define	SECTION_DTCMRAM
#define	SECTION_AHBSRAM
#define SECTION_SRDSRAM __attribute__((section(".SRDSRAMSection")))
#define SECTION_BKPSRAM __attribute__((section(".BKPSRAMSection")))
#endif
