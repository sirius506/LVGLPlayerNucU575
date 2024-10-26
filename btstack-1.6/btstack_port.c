#include "debug.h"
#include "bsp.h"
#include "btstack.h"
#include "btstack_run_loop_freertos.h"
#include "btstack_tlv.h"
#include "btstack_tlv_flash_bank.h"
#include "btstack_uart.h"
#include "classic/btstack_link_key_db_tlv.h"
#include "hci_transport_h4.h"
#include "hci_dump_segger_rtt_stdout.h"
#include "hci_if.h"
#include "app_gui.h"
#include "btstack_chipset_bcm.h"
#include "../chocolate-doom/m_misc.h"
#include "btapi.h"

#include "hal_flash_bank_memory.h"

static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_tlv_flash_bank_t btstack_tlv_flash_bank_context;

static hci_transport_config_uart_t transport_config = { 
    HCI_TRANSPORT_CONFIG_UART,
    115200,
    2000000,  // main baudrate
    1,       // flow control
    NULL,
    BTSTACK_UART_PARITY_OFF, // parity
};
static btstack_uart_config_t uart_config;

static hal_flash_bank_memory_t hal_flash_bank_context;

#define	STORAGE_SIZE	(1024*2)
SECTION_BKPSRAM static uint8_t tlvStorage_p[STORAGE_SIZE];

extern int btstack_audio_main(int argc, HAL_DEVICE *haldev);
extern int btstack_main(int argc, HAL_DEVICE *haldev);

#include "hal_time_ms.h"
uint32_t hal_time_ms(void)
{
#if 1
  return HAL_GetTick();
#else
  return osKernelGetTickCount();
#endif
}

static btstack_packet_callback_registration_t hci_event_callback_registration;

static void setup_dbs(void){

    const hal_flash_bank_t * hal_flash_bank_impl = hal_flash_bank_memory_init_instance(
            &hal_flash_bank_context,
            tlvStorage_p,
            STORAGE_SIZE);

    const btstack_tlv_t * btstack_tlv_impl = btstack_tlv_flash_bank_init_instance(
            &btstack_tlv_flash_bank_context,
            hal_flash_bank_impl,
            &hal_flash_bank_context);

    // setup global TLV
    btstack_tlv_set_instance(btstack_tlv_impl, &btstack_tlv_flash_bank_context);

    hci_set_link_key_db(btstack_link_key_db_tlv_get_instance(btstack_tlv_impl, &btstack_tlv_flash_bank_context));
}

static void local_version_information_handler(uint8_t * packet){
    debug_printf("Local version information:\n");
#if 0
    uint16_t hci_version    = packet[6];
    uint16_t hci_revision   = little_endian_read_16(packet, 7);
    uint16_t lmp_version    = packet[9];
    uint16_t manufacturer   = little_endian_read_16(packet, 10);
    uint16_t lmp_subversion = little_endian_read_16(packet, 12);
    debug_printf("- HCI Version    0x%04x\n", hci_version);
    debug_printf("- HCI Revision   0x%04x\n", hci_revision);
    debug_printf("- LMP Version    0x%04x\n", lmp_version);
    debug_printf("- LMP Subversion 0x%04x\n", lmp_subversion);
    debug_printf("- Manufacturer 0x%04x\n", manufacturer);
#else
    uint16_t manufacturer   = little_endian_read_16(packet, 10);
#endif

    if (manufacturer == BLUETOOTH_COMPANY_ID_BROADCOM_CORPORATION)
    {
        debug_printf("Broadcom/Cypress - using BCM driver.\n");
        hci_set_chipset(btstack_chipset_bcm_instance());
    }
}

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    UNUSED(size);
    UNUSED(channel);
    bd_addr_t local_addr;

    if (packet_type != HCI_EVENT_PACKET) return;
    switch(hci_event_packet_get_type(packet)){
        case HCI_EVENT_TRANSPORT_READY:
            setup_dbs();
            break;
        case BTSTACK_EVENT_STATE:
            if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING) return;
            gap_local_bd_addr(local_addr);
            debug_printf("BTstack up and running on %s.\n", bd_addr_to_str(local_addr));
            setup_dbs();
            app_btstack_ready();
            break;
        case HCI_EVENT_COMMAND_COMPLETE:
             if (hci_event_command_complete_get_command_opcode(packet) == HCI_OPCODE_HCI_READ_LOCAL_NAME){
                if (hci_event_command_complete_get_return_parameters(packet)[0]) break;
                // terminate, name 248 chars
                packet[6+248] = 0;
                debug_printf("Local name: %s\n", &packet[6]);
            }
            if (hci_event_command_complete_get_command_opcode(packet) == HCI_OPCODE_HCI_READ_LOCAL_VERSION_INFORMATION){
                local_version_information_handler(packet);
            }
            break;
        default:
            break;
    }
}

void btstack_assert_failed(const char * file, uint16_t line_nr)
{
    debug_printf("ASSERT in %s, line %u failed - HALT\n", file, line_nr);
    while(1) osDelay(100);
}

void StartBtstackTask(void *arg)
{
  HAL_DEVICE *haldev = (HAL_DEVICE *) arg;

  //hci_dump_init(hci_dump_segger_rtt_stdout_get_instance());

  // start with BTstack init - especially configure HCI Transport
  btstack_memory_init();
  btstack_run_loop_init(btstack_run_loop_freertos_get_instance());

  // setup UART driver
  const btstack_uart_block_t * uart_driver = btstack_uart_block_freertos_instance();
 
  // extract UART config from transport config
  uart_config.baudrate    = transport_config.baudrate_init;
  uart_config.flowcontrol = transport_config.flowcontrol;
  uart_config.device_name = transport_config.device_name;
  uart_driver->init(&uart_config);

  // setup HCI
  const hci_transport_t *transport = hci_transport_h4_instance(uart_driver);

  // init HCI
  hci_init(transport, (void *)&transport_config);

  // inform about BTstack state
  hci_event_callback_registration.callback = &packet_handler;
  hci_add_event_handler(&hci_event_callback_registration);

  btstack_main(0, haldev);

  // go
  btstack_run_loop_execute();
}

void postHCIEvent(uint8_t ptype, uint8_t *pkt, uint16_t size)
{
  HCIEVT evtdata;

  if (HciIfInfo.hcievqId)
  {
    evtdata.packet_type = ptype;
    evtdata.packet = pkt;
    evtdata.size = size;
    if (osMessageQueuePut(HciIfInfo.hcievqId, &evtdata, 0, osWaitForever) != osOK)
      debug_printf("hci event post failed.\n");
    btstack_run_loop_poll_data_sources_from_irq();
  }
}
