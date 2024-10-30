#include "DoomPlayer.h"
#include "bsp.h"
#include "hal_uart_dma.h"

static UART_HandleTypeDef *btuart;

// handlers
static void dummy_handler(void);
static void (*rx_done_handler)(void) = &dummy_handler;
static void (*tx_done_handler)(void) = &dummy_handler;

static void dummy_handler(void)
{
}

void ErrorCallback(UART_HandleTypeDef *huart)
{
  debug_printf("Error: %d\n", huart->ErrorCode);
}
  
void RxCompCallback(UART_HandleTypeDef *huart)
{
  UNUSED(huart);
  rx_done_handler();
}

void TxCompCallback(UART_HandleTypeDef *huart)
{
  UNUSED(huart);
  tx_done_handler();
}

void hal_uart_dma_init(HAL_DEVICE *haldev)
{
  HAL_StatusTypeDef st;
  uint8_t wbuffer[2];

  btuart = haldev->bt_uart->huart;

  HAL_UART_RegisterCallback(btuart, HAL_UART_TX_COMPLETE_CB_ID, TxCompCallback);
  HAL_UART_RegisterCallback(btuart, HAL_UART_RX_COMPLETE_CB_ID, RxCompCallback);

  HAL_UART_RegisterCallback(btuart, HAL_UART_ERROR_CB_ID, ErrorCallback);

#if 0
  HAL_GPIO_WritePin(BT_RESET_GPIO_Port, BT_RESET_Pin, GPIO_PIN_SET);
  osDelay(80);

  do {
    st = HAL_UART_Receive(btuart, wbuffer, 1, 4);
  } while (st == HAL_OK);

  osDelay(50);
#endif
  HAL_GPIO_WritePin(BT_RESET_GPIO_Port, BT_RESET_Pin, GPIO_PIN_RESET);
  osDelay(10);
  HAL_GPIO_WritePin(BT_RESET_GPIO_Port, BT_RESET_Pin, GPIO_PIN_SET);

  /* Tyry to flush any garbage output */
  do {
    st = HAL_UART_Receive(btuart, wbuffer, 1, 4);
  } while (st == HAL_OK);
}

int hal_uart_dma_set_baud(uint32_t baud)
{
  debug_printf("%s: %d\n", __FUNCTION__, baud);
  btuart->Init.BaudRate = baud;
  UART_SetConfig(btuart);
  return 0;
}

void hal_uart_dma_set_block_received( void (*the_block_handler)(void))
{
  rx_done_handler = the_block_handler;
}   
    
void hal_uart_dma_set_block_sent( void (*the_block_handler)(void))
{
  tx_done_handler = the_block_handler;
}

void hal_uart_dma_send_block(const uint8_t *data, uint16_t size){
  HAL_UART_Transmit_DMA(btuart, data, size);
}

void hal_uart_dma_receive_block(uint8_t *data, uint16_t size){
  HAL_UART_Receive_DMA(btuart, data, size);
}
