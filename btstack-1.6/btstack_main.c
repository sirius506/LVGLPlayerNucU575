#include "DoomPlayer.h"
#include "btstack_config.h"
#include "btstack.h"
#include "btapi.h"

BTSTACK_INFO BtStackInfo;

static btstack_data_source_t appapi_data_source;

extern void hid_host_setup(void);

static void appapi_process(btstack_data_source_t *ds, btstack_data_source_callback_type_t callback_type) {
    switch (callback_type){
        case DATA_SOURCE_CALLBACK_POLL:
            process_btapi_request(&BtStackInfo);
            break;
        default:
            break;
    }
}

int btstack_main(int argc, HAL_DEVICE *haldev)
{
  (void)argc;

  btstack_run_loop_set_data_source_handler(&appapi_data_source, &appapi_process);
  btstack_run_loop_enable_data_source_callbacks(&appapi_data_source, DATA_SOURCE_CALLBACK_POLL);

  btstack_run_loop_add_data_source(&appapi_data_source);

  btapi_setup();

  if (haldev->boot_mode == BOOTM_A2DP)
  {
    btstack_audio_main(0, haldev);
    return 0;
  }

  hid_host_setup();

  // Turn on the device 
  hci_power_control(HCI_POWER_ON);
  return 0;
}
