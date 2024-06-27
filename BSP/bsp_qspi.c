#include <string.h>
#include "qspi.h"
#include "board_if.h"
#include "rtosdef.h"
#include "debug.h"

#if 0
static uint8_t ReadStatus(DOOM_OSPI_Handle *qspi)
{ 
  OSPI_RegularCmdTypeDef  sCommand = { 0};
  uint8_t bdata[2];
  
  /* Configure automatic polling mode to wait for memory ready */
  sCommand.OperationType      = HAL_OSPI_OPTYPE_COMMON_CFG;
  sCommand.FlashId            = HAL_OSPI_FLASH_ID_1;
  sCommand.Instruction        = CMD_READ_STATUS1;
  if (qspi->qspi_mode & QSPI_ACCESS_QPI)
  {
    sCommand.InstructionMode    = HAL_OSPI_INSTRUCTION_4_LINES;
    sCommand.DataMode           = HAL_OSPI_DATA_4_LINES;
  }
  else
  {
    sCommand.InstructionMode    = HAL_OSPI_INSTRUCTION_1_LINE;
    sCommand.DataMode           = HAL_OSPI_DATA_1_LINE;
  }
  sCommand.AddressMode        = HAL_OSPI_ADDRESS_NONE;
  sCommand.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;
  sCommand.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;
  sCommand.DQSMode            = HAL_OSPI_DQS_DISABLE;
  sCommand.SIOOMode           = HAL_OSPI_SIOO_INST_EVERY_CMD;
  sCommand.NbData             = 1;

  if (HAL_OSPI_Command(&qspi->hospi, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    debug_printf("%s: cmderror \n", __FUNCTION__);
    return QSPI_ERROR;
  }
  if (HAL_OSPI_Receive_IT(&qspi->hospi, bdata) != HAL_OK)
  {
    debug_printf("Receive error 1.\n");
    return QSPI_ERROR;
  }
  osSemaphoreAcquire(qspi->sem_read, osWaitForever);

  return bdata[0];
}
#endif

static uint8_t OSPI_AutoPolling(DOOM_OSPI_Handle *qspi, uint8_t match, uint8_t mask)
{ 
  OSPI_RegularCmdTypeDef  sCommand = { 0};
  OSPI_AutoPollingTypeDef sConfig = { 0};
  
  /* Configure automatic polling mode to wait for memory ready */
  sCommand.OperationType      = HAL_OSPI_OPTYPE_COMMON_CFG;
  sCommand.FlashId            = HAL_OSPI_FLASH_ID_1;
  sCommand.Instruction        = CMD_READ_STATUS1;
  if (qspi->qspi_mode & QSPI_ACCESS_QPI)
  {
    sCommand.InstructionMode    = HAL_OSPI_INSTRUCTION_4_LINES;
    sCommand.DataMode           = HAL_OSPI_DATA_4_LINES;
  }
  else
  {
    sCommand.InstructionMode    = HAL_OSPI_INSTRUCTION_1_LINE;
    sCommand.DataMode           = HAL_OSPI_DATA_1_LINE;
  }
  sCommand.AddressMode        = HAL_OSPI_ADDRESS_NONE;
  sCommand.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;
  sCommand.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;
  sCommand.DQSMode            = HAL_OSPI_DQS_DISABLE;
  sCommand.SIOOMode           = HAL_OSPI_SIOO_INST_EVERY_CMD;
  sCommand.NbData             = 1;

  sConfig.Match           = 0;
  sConfig.Mask            = JEDEC_SR_WIP;
  sConfig.MatchMode       = HAL_OSPI_MATCH_MODE_AND;
  sConfig.Interval        = 0x20;
  sConfig.AutomaticStop   = HAL_OSPI_AUTOMATIC_STOP_ENABLE;

  if (HAL_OSPI_Command(qspi->hospi, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    debug_printf("%s: cmderror cmd = %02x\n", __FUNCTION__, CMD_READ_STATUS1);
    return QSPI_ERROR;
  }

  if (HAL_OSPI_AutoPolling_IT(qspi->hospi, &sConfig) != HAL_OK)
  {
    debug_printf("auto poll error.\n");
    return QSPI_ERROR;
  }
  osSemaphoreAcquire(qspi->sem_status, osWaitForever);
  return QSPI_OK;
}

static int qspi_simple_command(DOOM_OSPI_Handle *qspi, uint8_t cmd)
{
  OSPI_RegularCmdTypeDef sCommand = { 0 };

  sCommand.OperationType      = HAL_OSPI_OPTYPE_COMMON_CFG;
  sCommand.FlashId            = HAL_OSPI_FLASH_ID_1;
  if (qspi->qspi_mode & QSPI_ACCESS_QPI)
  {
    sCommand.InstructionMode    = HAL_OSPI_INSTRUCTION_4_LINES;
  }
  else
  {
    sCommand.InstructionMode    = HAL_OSPI_INSTRUCTION_1_LINE;
  }
  sCommand.InstructionSize    = HAL_OSPI_INSTRUCTION_8_BITS;
  sCommand.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;
  sCommand.Instruction        = cmd;                                 // command
  sCommand.AddressMode        = HAL_OSPI_ADDRESS_NONE;
  sCommand.AddressSize        = 0;

  sCommand.DummyCycles        = 0;
  sCommand.Address            = 0;
  sCommand.AddressDtrMode     = HAL_OSPI_ADDRESS_DTR_DISABLE;
  sCommand.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;
  sCommand.DataMode           = HAL_OSPI_DATA_NONE;
  sCommand.DataDtrMode        = HAL_OSPI_DATA_DTR_DISABLE;
  sCommand.DQSMode            = HAL_OSPI_DQS_DISABLE;
  sCommand.SIOOMode           = HAL_OSPI_SIOO_INST_EVERY_CMD;

  /* Send the command */
  if (HAL_OSPI_Command(qspi->hospi, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
      return QSPI_ERROR;
  }
  return QSPI_OK;
}

int qspi_read_status(DOOM_OSPI_Handle *qspi, uint8_t cmd)
{
  uint8_t bdata[2];
  OSPI_RegularCmdTypeDef sCommand = { 0 };

  sCommand.OperationType      = HAL_OSPI_OPTYPE_COMMON_CFG;
  sCommand.FlashId            = HAL_OSPI_FLASH_ID_1;
  if (qspi->qspi_mode & QSPI_ACCESS_QPI)
  {
    sCommand.InstructionMode    = HAL_OSPI_INSTRUCTION_4_LINES;
    sCommand.DataMode           = HAL_OSPI_DATA_4_LINES;
  }
  else
  {
    sCommand.InstructionMode    = HAL_OSPI_INSTRUCTION_1_LINE;
    sCommand.DataMode           = HAL_OSPI_DATA_1_LINE;
  }
  sCommand.InstructionSize    = HAL_OSPI_INSTRUCTION_8_BITS;
  sCommand.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;
  sCommand.Instruction        = cmd;                                 // command
  sCommand.AddressMode        = HAL_OSPI_ADDRESS_NONE;
  sCommand.AddressSize        = 0;

  sCommand.DummyCycles        = 0;
  sCommand.Address            = 0;
  sCommand.AddressDtrMode     = HAL_OSPI_ADDRESS_DTR_DISABLE;
  sCommand.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;
  sCommand.NbData             = 1;
  sCommand.DataDtrMode        = HAL_OSPI_DATA_DTR_DISABLE;
  sCommand.DQSMode            = HAL_OSPI_DQS_DISABLE;
  sCommand.SIOOMode           = HAL_OSPI_SIOO_INST_EVERY_CMD;

  /* Send the command */
  if (HAL_OSPI_Command(qspi->hospi, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
      return QSPI_ERROR;
  }

  if (HAL_OSPI_Receive_IT(qspi->hospi, bdata) != HAL_OK)
  {
    debug_printf("Receive error 1.\n");
    return QSPI_ERROR;
  }
  osSemaphoreAcquire(qspi->sem_read, osWaitForever);
  return bdata[0] & 255;
}

/**
 * @brief Write enable for status register
 */
int qspi_write_enable_sr(DOOM_OSPI_Handle *qspi)
{   
  return qspi_simple_command(qspi, CMD_WRITE_ENABLE_SR);
}

int qspi_write_enable(DOOM_OSPI_Handle *qspi)
{
  int st;

  st = qspi_simple_command(qspi, CMD_WRITE_ENABLE);
  if (st == QSPI_OK)
    st = OSPI_AutoPolling(qspi, JEDEC_SR_WEL, JEDEC_SR_WEL);
  return st;
}

int qspi_write_status3(DOOM_OSPI_Handle *qspi, uint8_t val)
{
  uint8_t bdata[2];
  OSPI_RegularCmdTypeDef sCommand = { 0 };

  sCommand.OperationType      = HAL_OSPI_OPTYPE_COMMON_CFG;
  sCommand.FlashId            = HAL_OSPI_FLASH_ID_1;
  sCommand.InstructionMode    = HAL_OSPI_INSTRUCTION_1_LINE;
  sCommand.InstructionSize    = HAL_OSPI_INSTRUCTION_8_BITS;
  sCommand.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;
  sCommand.Instruction        = CMD_WRITE_STATUS3;
  sCommand.AddressMode        = HAL_OSPI_ADDRESS_NONE;
  sCommand.AddressSize        = 0;
  sCommand.DummyCycles        = 0;
  sCommand.Address            = 0;
  sCommand.AddressDtrMode     = HAL_OSPI_ADDRESS_DTR_DISABLE;
  sCommand.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;
  sCommand.DataMode           = HAL_OSPI_DATA_1_LINE;
  sCommand.NbData             = 1;
  sCommand.DataDtrMode        = HAL_OSPI_DATA_DTR_DISABLE;
  sCommand.DQSMode            = HAL_OSPI_DQS_DISABLE;
  sCommand.SIOOMode           = HAL_OSPI_SIOO_INST_EVERY_CMD;

  /* Send the command */
  if (HAL_OSPI_Command(qspi->hospi, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
      return -1;
  }

  bdata[0] = val;

  if (HAL_OSPI_Transmit_IT(qspi->hospi, bdata) != HAL_OK)
  {
    return -1;
  }
  osSemaphoreAcquire(qspi->sem_write, osWaitForever);
  return 0;
}

int qspi_qpi_enable(DOOM_OSPI_Handle *qspi)
{
  uint8_t bdata[2];
  OSPI_RegularCmdTypeDef sCommand = { 0 };

  if (qspi->qspi_mode & QSPI_ACCESS_QPI)
    return 0;

  sCommand.OperationType      = HAL_OSPI_OPTYPE_COMMON_CFG;
  sCommand.FlashId            = HAL_OSPI_FLASH_ID_1;
  sCommand.InstructionMode    = HAL_OSPI_INSTRUCTION_1_LINE;
  sCommand.InstructionSize    = HAL_OSPI_INSTRUCTION_8_BITS;
  sCommand.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;
  sCommand.Instruction        = CMD_WRITE_STATUS2;
  sCommand.AddressMode        = HAL_OSPI_ADDRESS_NONE;
  sCommand.AddressSize        = 0;
  sCommand.DummyCycles        = 0;
  sCommand.Address            = 0;
  sCommand.AddressDtrMode     = HAL_OSPI_ADDRESS_DTR_DISABLE;
  sCommand.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;
  sCommand.DataMode           = HAL_OSPI_DATA_1_LINE;
  sCommand.NbData             = 1;
  sCommand.DataDtrMode        = HAL_OSPI_DATA_DTR_DISABLE;
  sCommand.DQSMode            = HAL_OSPI_DQS_DISABLE;
  sCommand.SIOOMode           = HAL_OSPI_SIOO_INST_EVERY_CMD;

  /* Send the command */
  if (HAL_OSPI_Command(qspi->hospi, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
      return -1;
  }

  bdata[0] = 1 << 1;

  if (HAL_OSPI_Transmit_IT(qspi->hospi, bdata) != HAL_OK)
  {
    return -1;
  }
  osSemaphoreAcquire(qspi->sem_write, osWaitForever);
  return 0;
}

int qspi_enter_qpi(DOOM_OSPI_Handle *qspi)
{
  OSPI_RegularCmdTypeDef sCommand = { 0 };

  if (qspi->qspi_mode & QSPI_ACCESS_QPI)
    return QSPI_OK;

  sCommand.OperationType      = HAL_OSPI_OPTYPE_COMMON_CFG;
  sCommand.FlashId            = HAL_OSPI_FLASH_ID_1;
  sCommand.InstructionMode    = HAL_OSPI_INSTRUCTION_1_LINE;
  sCommand.InstructionSize    = HAL_OSPI_INSTRUCTION_8_BITS;
  sCommand.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;
  sCommand.Instruction        = (qspi->device_mode == DEV_MODE_FLASH)? CMD_ENTER_QPI : PSRAM_ENTER_QPI;
  sCommand.AddressMode        = HAL_OSPI_ADDRESS_NONE;
  sCommand.AddressSize        = 0;
  sCommand.DummyCycles        = 0;
  sCommand.Address            = 0;
  sCommand.AddressDtrMode     = HAL_OSPI_ADDRESS_DTR_DISABLE;
  sCommand.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;
  sCommand.DataMode           = HAL_OSPI_DATA_NONE;
  sCommand.NbData             = 0;
  sCommand.DataDtrMode        = HAL_OSPI_DATA_DTR_DISABLE;
  sCommand.DQSMode            = HAL_OSPI_DQS_DISABLE;
  sCommand.SIOOMode           = HAL_OSPI_SIOO_INST_EVERY_CMD;

  /* Send the command */
  if (HAL_OSPI_Command(qspi->hospi, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
      return QSPI_ERROR;
  }

  qspi->qspi_mode |= QSPI_ACCESS_QPI;
  return QSPI_OK;
}

int qspi_exit_qpi(DOOM_OSPI_Handle *qspi)
{
  OSPI_RegularCmdTypeDef sCommand = { 0 };

  sCommand.OperationType      = HAL_OSPI_OPTYPE_COMMON_CFG;
  sCommand.FlashId            = HAL_OSPI_FLASH_ID_1;
  sCommand.InstructionMode    = HAL_OSPI_INSTRUCTION_4_LINES;
  sCommand.InstructionSize    = HAL_OSPI_INSTRUCTION_8_BITS;
  sCommand.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;
  sCommand.Instruction        = (qspi->device_mode == DEV_MODE_FLASH)? CMD_EXIT_QPI : PSRAM_EXIT_QPI;
  sCommand.AddressMode        = HAL_OSPI_ADDRESS_NONE;
  sCommand.AddressSize        = 0;
  sCommand.DummyCycles        = 0;
  sCommand.Address            = 0;
  sCommand.AddressDtrMode     = HAL_OSPI_ADDRESS_DTR_DISABLE;
  sCommand.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;
  sCommand.DataMode           = HAL_OSPI_DATA_NONE;
  sCommand.NbData             = 0;
  sCommand.DataDtrMode        = HAL_OSPI_DATA_DTR_DISABLE;
  sCommand.DQSMode            = HAL_OSPI_DQS_DISABLE;
  sCommand.SIOOMode           = HAL_OSPI_SIOO_INST_EVERY_CMD;

  /* Send the command */
  if (HAL_OSPI_Command(qspi->hospi, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
      return QSPI_ERROR;
  }

  qspi->qspi_mode &= ~QSPI_ACCESS_QPI;
  return QSPI_OK;
}

int qspi_enter_4bytemode(DOOM_OSPI_Handle *qspi)
{
  return qspi_simple_command(qspi, CMD_ENTER_ADDRESS4);
}

static int qspi_readid_command(DOOM_OSPI_Handle *qspi, int nbdata)
{
  OSPI_RegularCmdTypeDef sCommand = { 0 };

  sCommand.OperationType      = HAL_OSPI_OPTYPE_COMMON_CFG;
  sCommand.FlashId            = HAL_OSPI_FLASH_ID_1;
  if (qspi->qspi_mode & QSPI_ACCESS_QPI)
  {
    sCommand.InstructionMode    = HAL_OSPI_INSTRUCTION_4_LINES;
    sCommand.DataMode           = HAL_OSPI_DATA_4_LINES;
  }
  else
  {
    sCommand.InstructionMode    = HAL_OSPI_INSTRUCTION_1_LINE;
    sCommand.DataMode           = HAL_OSPI_DATA_1_LINE;
  }
  sCommand.InstructionSize    = HAL_OSPI_INSTRUCTION_8_BITS;
  sCommand.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;
  sCommand.Instruction        = CMD_READ_JEDEC_ID;                                 // command
  sCommand.AddressMode        = HAL_OSPI_ADDRESS_NONE;
  sCommand.AddressSize        = 0;

  sCommand.DummyCycles        = (qspi->device_mode == DEV_MODE_FLASH)? 0 : 24;
  sCommand.Address            = 0;
  sCommand.AddressDtrMode     = HAL_OSPI_ADDRESS_DTR_DISABLE;
  sCommand.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;
  sCommand.NbData             = nbdata;
  sCommand.DataDtrMode        = HAL_OSPI_DATA_DTR_DISABLE;
  sCommand.DQSMode            = HAL_OSPI_DQS_DISABLE;
  sCommand.SIOOMode           = HAL_OSPI_SIOO_INST_EVERY_CMD;

  /* Send the command */
  if (HAL_OSPI_Command(qspi->hospi, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
      return -1;
  }
  return 0;
}

int qspi_reset(DOOM_OSPI_Handle *qspi)
{
  qspi_exit_qpi(qspi);
  osDelay(1);
  if (qspi->device_mode == DEV_MODE_FLASH)
  {
    qspi_simple_command(qspi, CMD_ENABLE_RESET);
    qspi_simple_command(qspi, CMD_RESET);
    osDelay(1);
  }
  return QSPI_OK;
}

int qspi_enable_mappedmode(DOOM_OSPI_Handle *qspi)
{
  OSPI_RegularCmdTypeDef sCommand = { 0 };
  OSPI_MemoryMappedTypeDef sConfig = { 0 };


  /* Initialize the read command */
  sCommand.OperationType      = HAL_OSPI_OPTYPE_READ_CFG;
  sCommand.FlashId            = HAL_OSPI_FLASH_ID_1;
  sCommand.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;
  sCommand.InstructionSize    = HAL_OSPI_INSTRUCTION_8_BITS;

  if (qspi->qspi_mode & QSPI_ACCESS_QPI)
  {
#ifdef USE_DTR
    qspi->qspi_mode |= QSPI_ACCESS_DTR;
#endif
    sCommand.InstructionMode = HAL_OSPI_INSTRUCTION_4_LINES;
    sCommand.AddressMode     = HAL_OSPI_ADDRESS_4_LINES;
    sCommand.DataMode        = HAL_OSPI_DATA_4_LINES;
  
    if (qspi->device_mode == DEV_MODE_FLASH)
    {
      sCommand.AddressSize = HAL_OSPI_ADDRESS_32_BITS;
      if (qspi->qspi_mode & QSPI_ACCESS_DTR)
      {
        sCommand.Instruction = 0x0D;
        sCommand.DummyCycles = 8;
      }
      else
      {
        sCommand.Instruction = CMD_FAST_READ;
        sCommand.DummyCycles = 2;
      }
    }
    else
    {
      sCommand.AddressSize = HAL_OSPI_ADDRESS_24_BITS;
      sCommand.Instruction = PSRAM_FAST_QUAD_READ;
      sCommand.DummyCycles = 6;
    }
  }
  else
  {
    sCommand.InstructionMode = HAL_OSPI_INSTRUCTION_1_LINE;
  
    if (qspi->device_mode == DEV_MODE_FLASH)
    {
      sCommand.AddressMode = HAL_OSPI_ADDRESS_1_LINE;
      sCommand.AddressSize = HAL_OSPI_ADDRESS_32_BITS;
      sCommand.Instruction = CMD_FAST_READ;
      sCommand.DummyCycles = 8;
      sCommand.DataMode    = HAL_OSPI_DATA_1_LINE;
    }
    else
    {
      sCommand.AddressMode = HAL_OSPI_ADDRESS_4_LINES;
      sCommand.AddressSize = HAL_OSPI_ADDRESS_24_BITS;
      sCommand.Instruction = PSRAM_FAST_QUAD_READ;
      sCommand.DummyCycles = 6;
      sCommand.DataMode    = HAL_OSPI_DATA_4_LINES;
    }
  }
  sCommand.AddressDtrMode     = HAL_OSPI_ADDRESS_DTR_DISABLE;
  sCommand.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;
  if (qspi->qspi_mode & QSPI_ACCESS_DTR)
    sCommand.DataDtrMode        = HAL_OSPI_DATA_DTR_ENABLE;
  else
    sCommand.DataDtrMode        = HAL_OSPI_DATA_DTR_DISABLE;
  sCommand.DQSMode            = HAL_OSPI_DQS_DISABLE;
  sCommand.SIOOMode           = HAL_OSPI_SIOO_INST_EVERY_CMD;
  sCommand.NbData             = 1;

  /* Send the read command */
  if (HAL_OSPI_Command(qspi->hospi, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    return QSPI_ERROR;
  }

  if (qspi->device_mode == DEV_MODE_FLASH)
  {
    /* Flash is READ ONLY. We'll skip WRITE_CFG */
    qspi->hospi->State = HAL_OSPI_STATE_CMD_CFG;
  }
  else
  {
    /* Initialize the program command */
    /* Although our QSPI PSRAM doesn't have DQS pin, we need to set DQSMode enabled,
     * because of silicon bug. Read Errata sheet for details.
     */
    sCommand.OperationType      = HAL_OSPI_OPTYPE_WRITE_CFG;
    sCommand.Instruction        = PSRAM_QUAD_WRITE;
    sCommand.DummyCycles        = 0U;
    sCommand.DQSMode            = HAL_OSPI_DQS_ENABLE;

    /* Send the write command */
    if (HAL_OSPI_Command(qspi->hospi, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
      return QSPI_ERROR;
    }
  }

  /* Configure the memory mapped mode */

  sConfig.TimeOutActivation = HAL_OSPI_TIMEOUT_COUNTER_DISABLE;

  if (HAL_OSPI_MemoryMapped(qspi->hospi, &sConfig) != HAL_OK)
  {
    return QSPI_ERROR;
  }

  qspi->qspi_mode |= QSPI_ACCESS_MMAP;
  return QSPI_OK;
}

int qspi_disable_mappedmode(DOOM_OSPI_Handle *qspi)
{
  if (qspi->qspi_mode & QSPI_ACCESS_MMAP)
  {
    HAL_OSPI_Abort(qspi->hospi);
    qspi->qspi_mode &= ~QSPI_ACCESS_MMAP;
    debug_printf("Aborted: %x\n", qspi->hospi->State);
  }
  return QSPI_ERROR;
}

int flash_read_id(DOOM_OSPI_Handle *qspi, uint8_t *bp, int idlen)
{
  qspi_readid_command(qspi, idlen);

  if (HAL_OSPI_Receive_IT(qspi->hospi, bp) != HAL_OK)
  {
    debug_printf("Receive error 1.\n");
    return -1;
  }
  osSemaphoreAcquire(qspi->sem_read, osWaitForever);

  debug_printf("flashid: %x, %x, %x\n", bp[0], bp[1], bp[2]);
  return 0;
}

int psram_read_id(DOOM_OSPI_Handle *qspi, uint8_t *psramid, int idlen)
{   
  qspi_readid_command(qspi, idlen);

  if (HAL_OSPI_Receive_IT(qspi->hospi, psramid) != HAL_OK)
  {
    debug_printf("Receive error 2.\n");
    return 0;
  }
  osSemaphoreAcquire(qspi->sem_read, osWaitForever);

  debug_printf("psramid: %x, %x, %x\n", psramid[0], psramid[1], psramid[2]);
  return 0;
}

#if 0
/**
 * @bief Exit from PSRAM QPI mode
 */
static void psram_qpi_exit(OSPI_HandleTypeDef *qspi)
{
  OSPI_RegularCmdTypeDef sCommand = { 0 };
  
  sCommand.InstructionMode   = HAL_OSPI_INSTRUCTION_4_LINES;
  sCommand.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;
  sCommand.Instruction       = 0xF5;
  sCommand.AddressMode       = HAL_OSPI_ADDRESS_NONE;
  sCommand.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;
  sCommand.DataMode          = HAL_OSPI_DATA_NONE;
  sCommand.DummyCycles       = 0;
  sCommand.NbData            = 0;
  sCommand.DQSMode           = HAL_OSPI_DQS_DISABLE;
  sCommand.SIOOMode          = HAL_OSPI_SIOO_INST_EVERY_CMD;
  
  if (HAL_OSPI_Command(qspi, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    debug_printf("qpi_exit error.\n");
  }
} 
#endif


#if 0
void psram_mem_test()
{
  uint8_t *bp;
  uint16_t *sp;
  uint32_t *wp;
  int i;

  bp = (uint8_t *)QSPI_PSRAM_ADDR;

  for (i = 0; i < 1024; i++)
  {
    *bp++ = i;
  }

bp++;
  sp = (uint16_t *) bp;
  debug_printf("16bit access @ %x\n", sp);
  for (i = 0; i < 1024; i++)
  {
    *sp++ = i;
  }

sp++;
  wp = (uint32_t *) sp;
  debug_printf("32bit access @ %x\n", wp);
  for (i = 0; i < 1024; i++)
  {
    *wp++ = i;
  }
}
#endif


int qspi_flash_write(DOOM_OSPI_Handle *qspi, uint8_t *src, uint8_t *dest, int len)
{
  OSPI_RegularCmdTypeDef     sCommand = { 0 };
  int st;

  if (qspi->device_mode != DEV_MODE_FLASH)
    return QSPI_ERROR;
  if (qspi->qspi_mode & QSPI_ACCESS_MMAP)
    qspi_disable_mappedmode(qspi);

  sCommand.OperationType      = HAL_OSPI_OPTYPE_COMMON_CFG;
  sCommand.FlashId            = HAL_OSPI_FLASH_ID_1;
  sCommand.Instruction        = CMD_PAGE_PROGRAM;

  if (qspi->qspi_mode & QSPI_ACCESS_QPI)
  {
    sCommand.InstructionMode    = HAL_OSPI_INSTRUCTION_4_LINES;
    sCommand.AddressMode        = HAL_OSPI_ADDRESS_4_LINES;
    sCommand.DataMode           = HAL_OSPI_DATA_4_LINES;
  }
  else
  {
    sCommand.InstructionMode    = HAL_OSPI_INSTRUCTION_1_LINE;
    sCommand.AddressMode        = HAL_OSPI_ADDRESS_1_LINE;
    sCommand.DataMode           = HAL_OSPI_DATA_1_LINE;
  }
  sCommand.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;
  sCommand.AddressDtrMode     = HAL_OSPI_ADDRESS_DTR_DISABLE;
  sCommand.AddressSize        = HAL_OSPI_ADDRESS_32_BITS;
  sCommand.Address            = (uint32_t)dest;
  sCommand.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;
  sCommand.DummyCycles        = 0;
  sCommand.DQSMode           = HAL_OSPI_DQS_DISABLE;
  sCommand.SIOOMode           = HAL_OSPI_SIOO_INST_EVERY_CMD;

  sCommand.NbData            = 0;

  while (len > 0)
  {
    /* Perform the write page by page */
    int wsize;

    wsize = W25Q_PAGE_SIZE - ((uint32_t)dest % W25Q_PAGE_SIZE);
    if (wsize > len)
      wsize = len;

    sCommand.Address            = (uint32_t)dest;
    sCommand.NbData = wsize;	// wsize

    st = qspi_write_enable(qspi);
    if (st != QSPI_OK)
    {
      debug_printf("%s: write enable error.\n", __FUNCTION__);
      return QSPI_ERROR;
    }

    if (HAL_OSPI_Command(qspi->hospi, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
      debug_printf("Prog failure..\n");
      return QSPI_ERROR;
    }
    /* Transmission of the data */
    st = HAL_OSPI_Transmit_IT(qspi->hospi, src);
    if (st != HAL_OK)
    {
      debug_printf("Prog TX failure.. (st = %d, len = %d\n", st, len);
      return QSPI_ERROR;
    }
    osSemaphoreAcquire(qspi->sem_write, osWaitForever);
    //debug_printf("Prog: %x -->%x, %d\n", src, dest, wsize);

    /* Configure automatic polling mode to wait for end of program */
    if (OSPI_AutoPolling(qspi, 0x00, JEDEC_SR_WIP) != QSPI_OK)
    {
      debug_printf("Prog Polling failure..\n");
      return QSPI_ERROR;
    }

    /* Update the address and size variables for next page programming */
    dest += wsize;
    src += wsize;
    len -= wsize;
  }
  // debug_printf("Write OK.\n");
  return QSPI_OK;
}

int qspi_erase_block(DOOM_OSPI_Handle *qspi, uint8_t *dest)
{
  OSPI_RegularCmdTypeDef sCommand = { 0 };
  int st;

  st = qspi_write_enable(qspi);
  if (st != QSPI_OK)
  {
      debug_printf("%s: write enable error.\n", __FUNCTION__);
      return QSPI_ERROR;
  }


  sCommand.OperationType      = HAL_OSPI_OPTYPE_COMMON_CFG;
  sCommand.FlashId            = HAL_OSPI_FLASH_ID_1;
  if (qspi->qspi_mode & QSPI_ACCESS_QPI)
  {
    sCommand.InstructionMode    = HAL_OSPI_INSTRUCTION_4_LINES;
    sCommand.AddressMode        = HAL_OSPI_ADDRESS_4_LINES;
  }
  else
  {
    sCommand.InstructionMode    = HAL_OSPI_INSTRUCTION_1_LINE;
    sCommand.AddressMode        = HAL_OSPI_ADDRESS_1_LINE;
  }
  sCommand.InstructionSize    = HAL_OSPI_INSTRUCTION_8_BITS;
  sCommand.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;
  sCommand.Instruction        = CMD_ERASE_64K;
  sCommand.AddressSize        = HAL_OSPI_ADDRESS_32_BITS;

  sCommand.DummyCycles        = 0;
  sCommand.Address            = (uint32_t) dest;
  sCommand.AddressDtrMode     = HAL_OSPI_ADDRESS_DTR_DISABLE;
  sCommand.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;
  sCommand.DataMode           = HAL_OSPI_DATA_NONE;
  sCommand.DataDtrMode        = HAL_OSPI_DATA_DTR_DISABLE;
  sCommand.DQSMode            = HAL_OSPI_DQS_DISABLE;
  sCommand.SIOOMode           = HAL_OSPI_SIOO_INST_EVERY_CMD;

  /* Send the command */
  if (HAL_OSPI_Command(qspi->hospi, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
      return QSPI_ERROR;
  }

  if (OSPI_AutoPolling(qspi, 0x00, JEDEC_SR_WIP) != QSPI_OK)
    return QSPI_ERROR;

  return QSPI_OK;
}


#ifdef USE_FLASH_TEST

static uint8_t tbuffer[512];

void Flash_Test(HAL_DEVICE *haldev)
{
  int i;
  uint8_t *bp;

  int bst;

  bst = HAL_GPIO_ReadPin(USER_BUTTON_GPIO_Port, USER_BUTTON_Pin);
  debug_printf("pin = %x\n", bst);

  if (bst == 0)
    Board_Flash_Init(haldev, 1);
  else
  {

    Board_Flash_Init(haldev, 0);

    qspi_erase_block(haldev->qspi_flash, (uint8_t *)0);
    debug_printf("Erase done.\n");

#if 1
    bp = tbuffer;
    for (i = 0; i < 256; i++)
      *bp++ = i;
    for (i = 0; i < 256; i++)
      *bp++ = 255 - i;
  
    qspi_flash_write(haldev->qspi_flash, tbuffer, (uint8_t *)0, 512);
    debug_printf("Write done.\n");
#endif
  }
}
#endif
