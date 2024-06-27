#ifndef QSPI_H
#define QSPI_H
#include "main.h"
#include "bsp.h"

#define CMD_READ_JEDEC_ID       0x9F
  
#define JEDEC_SR_WEL    0x02
#define	JEDEC_SR_WIP	0x01

#define CMD_READ_STATUS1        0x05
#define CMD_READ_STATUS2        0x35
#define CMD_READ_STATUS3        0x15
  
#define CMD_WRITE_STATUS1       0x01
#define CMD_WRITE_STATUS2       0x31
#define CMD_WRITE_STATUS3       0x11
  
#define	CMD_FAST_READ_QUAD	0x6B
#define	CMD_FAST_READ		0x0B

#define CMD_ENTER_QPI           0x38	// Enter QPI mode
#define CMD_EXIT_QPI            0xFF	// Exit QPI mode

#define	CMD_ENTER_ADDRESS4	0xB7	// Etner 4-Byte address mode

#define	CMD_ENABLE_RESET	0x66
#define	CMD_RESET		0x99

#define	CMD_ERASE_64K		0xD8

#define	CMD_WRITE_ENABLE_SR	0x50
#define	CMD_WRITE_ENABLE	0x06

#define	CMD_PAGE_PROGRAM	0x02

#define	W25Q_PAGE_SIZE		256

/**
 *  APS6404L-3SQN PSRAM Commands
 */
#define	PSRAM_FAST_READ		0x0B
#define	PSRAM_FAST_QUAD_READ	0xEB
#define	PSRAM_QUAD_WRITE	0x38
#define	PSRAM_ENTER_QPI		0x35
#define	PSRAM_EXIT_QPI		0xF5
  
#define QSPI_ERROR      -1    
#define QSPI_OK         0


int qspi_write_enable_sr(DOOM_OSPI_Handle *qspi);
int qspi_enter_4bytemode(DOOM_OSPI_Handle *qspi);
int qspi_qpi_enable(DOOM_OSPI_Handle *qspi);
int qspi_enter_qpi(DOOM_OSPI_Handle *qspi);
int qspi_exit_qpi(DOOM_OSPI_Handle *qspi);
int qspi_reset(DOOM_OSPI_Handle *qspi);
int qspi_enable_mappedmode(DOOM_OSPI_Handle *qspi);
int qspi_disable_mappedmode(DOOM_OSPI_Handle *qspi);
int qspi_read_status(DOOM_OSPI_Handle *qspi, uint8_t cmd);
int qspi_write_status3(DOOM_OSPI_Handle *qspi, uint8_t val);
int qspi_erase_block(DOOM_OSPI_Handle *qspi, uint8_t *dest);
int qspi_flash_write(DOOM_OSPI_Handle *qspi, uint8_t *src, uint8_t *dest, int len);

#endif
