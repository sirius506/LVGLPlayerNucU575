/*
 * Copyright (C) 2017 BlueKitchen GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL BLUEKITCHEN
 * GMBH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at 
 * contact@bluekitchen-gmbh.com
 *
 */

#define BTSTACK_FILE__ "hid_gamepad.c"

/*
 * hid_host_demo.c
 */

/* EXAMPLE_START(hid_host_demo): HID Host Classic
 *
 * @text This example implements a HID Host. For now, it connects to a fixed device.
 * It will connect in Report protocol mode if this mode is supported by the HID Device,
 * otherwise it will fall back to BOOT protocol mode. 
 */

#include <inttypes.h>
#include <stdio.h>
#include "DoomPlayer.h"
#include "btstack_config.h"
#include "btstack.h"
#include "gamepad.h"
#include "rtosdef.h"
#include "app_gui.h"
#include "btapi.h"
#include "debug.h"

extern BTSTACK_INFO BtStackInfo;

#define MAX_ATTRIBUTE_VALUE_SIZE 300

static bd_addr_t remote_addr;

static btstack_packet_callback_registration_t hci_event_callback_registration;

#define	MAX_DEVICES 20
enum DEVICE_STATE {
    REMOTE_NAME_INIT, REMOTE_NAME_REQUEST, REMOTE_NAME_INQUIRED, REMOTE_NAME_FETCHED
};

struct device {
    bd_addr_t address;
    uint8_t pageScanRepetitionMode;
    uint16_t clockOffset;
    uint32_t CoD;
    enum DEVICE_STATE state;
};

#define	INQUIRY_INTERVAL 5
static struct device devices[MAX_DEVICES];

// SDP
static uint8_t hid_descriptor_storage[MAX_ATTRIBUTE_VALUE_SIZE];

static bool     hid_host_descriptor_available = false;
static hid_protocol_mode_t hid_host_report_mode = HID_PROTOCOL_MODE_REPORT;

static const struct sGamePadDriver *padDriver;
static GAMEPAD_INFO *padInfo;

static uint16_t hid_vid, hid_pid;

/* @section Main application configuration
 *
 * @text In the application configuration, L2CAP and HID host are initialized, and the link policies 
 * are set to allow sniff mode and role change. 
 */

/* LISTING_START(PanuSetup): Panu setup */
static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

void hid_host_setup(void){

    // Initialize HID Host
    hid_host_init(hid_descriptor_storage, sizeof(hid_descriptor_storage));
    hid_host_register_packet_handler(packet_handler);

    // Allow sniff mode requests by HID device and support role switch
    gap_set_default_link_policy_settings(LM_LINK_POLICY_ENABLE_SNIFF_MODE | LM_LINK_POLICY_ENABLE_ROLE_SWITCH);

    // try to become master on incoming connections
    hci_set_master_slave_policy(HCI_ROLE_MASTER);

    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

}
/* LISTING_END */

static struct device *getDeviceForAddress(BTSTACK_INFO *pinfo, bd_addr_t addr) {
  int j;
  struct device *pdev = devices;

  for (j = 0; j < pinfo->deviceCount; j++) {
    if (bd_addr_cmp(addr, pdev->address) == 0) {
      return pdev;
    }
    pdev++;
  }
  return NULL;
}

static void add_device(BTSTACK_INFO *pinfo, bd_addr_t addr, uint32_t cod)
{
  struct device *pdev = devices + pinfo->deviceCount;

  memcpy(pdev->address, addr, 6);
  pdev->CoD = cod;
  pinfo->deviceCount++;

  if (cod == COD_GAMEPAD)
  {
    hid_vid = hid_pid = 0;
  }
}

static int has_more_remote_name_requests(BTSTACK_INFO *pinfo) {
  int i;
  for (i = 0; i < pinfo->deviceCount; i++) {
    if (devices[i].state == REMOTE_NAME_REQUEST)
      return 1;
  }
  return 0;
}

static void do_next_remote_name_request(BTSTACK_INFO *pinfo) {
  int i;
  for (i = 0; i < pinfo->deviceCount; i++) {
    // remote name request
    if (devices[i].state == REMOTE_NAME_REQUEST) {
      devices[i].state = REMOTE_NAME_INQUIRED;
      debug_printf("Get remote name of %s...\n", bd_addr_to_str(devices[i].address));
      gap_remote_name_request(devices[i].address,
                              devices[i].pageScanRepetitionMode,
                              devices[i].clockOffset | 0x8000);
      return;
    }
  }
}

static void continue_remote_names(BTSTACK_INFO *pinfo) {
  if (has_more_remote_name_requests(pinfo)) {
    do_next_remote_name_request(pinfo);
    return;
  }
  gap_inquiry_start(INQUIRY_INTERVAL);
}

static void handle_sdp_client_query_result(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
  UNUSED(packet_type);
  UNUSED(channel);
  UNUSED(size);
  int type, id;
  int offset;
  uint8_t bdata;
  uint16_t *vp;

  type = hci_event_packet_get_type(packet);

  switch (type)
  {
  case SDP_EVENT_QUERY_ATTRIBUTE_VALUE:
    id = sdp_event_query_attribute_byte_get_attribute_id(packet);

    switch (id)
    {
    case 0x0201:
      vp = &hid_vid;
      break;
    case 0x202:
      vp = &hid_pid;
      break;
    default:
      return;
      break;
    }

    offset = sdp_event_query_attribute_byte_get_data_offset(packet);
    bdata = sdp_event_query_attribute_byte_get_data(packet);

    switch (offset)
    {
    case 1:
          *vp = bdata << 8;
          break;
    case 2:
          *vp |= bdata;
          break;
    default:
          *vp = 0;
          break;
    }
    break;
  case SDP_EVENT_QUERY_COMPLETE:
    //debug_printf("Query complete %d, %d\n", hid_vid, hid_pid);
    break;
  default:
    break;
  }
}

/*
 * @section Packet Handler
 * 
 * @text The packet handler responds to various HID events.
 */

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
  UNUSED(channel);
  UNUSED(size);

  BTSTACK_INFO *pinfo = &BtStackInfo;
  uint8_t   event = 0;
  bd_addr_t event_addr;
  uint8_t   status;
  uint32_t  codval;
  uint16_t  chandle;
  struct device *pdev = devices;

  bd_addr_t addr;
  int i;

  switch (packet_type) {
  case HCI_EVENT_PACKET:
    event = hci_event_packet_get_type(packet);

    switch (event) {
    case GAP_EVENT_INQUIRY_RESULT:
      if (pinfo->deviceCount > MAX_DEVICES)
        break;  // already full

      gap_event_inquiry_result_get_bd_addr(packet, addr);
      pdev = getDeviceForAddress(pinfo, addr);
      if (pdev)
        break;	// already in our lisst

      /* Register newly found device. */
      pdev = devices + pinfo->deviceCount;
      memcpy(pdev->address, addr, 6);
      pdev->pageScanRepetitionMode = gap_event_inquiry_result_get_page_scan_repetition_mode(packet);
      pdev->clockOffset = gap_event_inquiry_result_get_clock_offset(packet);


      // print info
      codval = (unsigned int) gap_event_inquiry_result_get_class_of_device(packet);

      pdev->CoD = codval;
      debug_printf("Device found: %s, CoD = %x\n", bd_addr_to_str(addr), codval);

      if (gap_event_inquiry_result_get_name_available(packet)) {
        char name_buffer[240];
        int name_len = gap_event_inquiry_result_get_name_len(packet);

        memcpy(name_buffer, gap_event_inquiry_result_get_name(packet), name_len);
        name_buffer[name_len] = 0;
        debug_printf(", name '%s'", name_buffer);
        pdev->state = REMOTE_NAME_FETCHED;
        if ((strcmp(name_buffer, "Wireless Controller") == 0) && (pdev->CoD == COD_GAMEPAD))
        {
          debug_printf(" -- Gamepad has found!!\n");
        }
      }
      else if (pdev->CoD == COD_GAMEPAD) {
        pdev->state = REMOTE_NAME_REQUEST;
      }
      if (gap_event_inquiry_result_get_device_id_available(packet)) {
            hid_vid = gap_event_inquiry_result_get_device_id_vendor_id(packet);
            hid_pid = gap_event_inquiry_result_get_device_id_product_id(packet);
          debug_printf("vid, pid: %x, %x\n", hid_vid, hid_pid);
      }
      debug_printf("\n");
      pinfo->deviceCount++;
      break;

    case GAP_EVENT_INQUIRY_COMPLETE:
      pdev = devices;
      for (i = 0; i < pinfo->deviceCount; i++) {
        // retry remote name request
        if ((pdev->state == REMOTE_NAME_INQUIRED) && (pdev->CoD == COD_GAMEPAD))
        pdev->state = REMOTE_NAME_REQUEST;
      }
      continue_remote_names(pinfo);
      break;

    case HCI_EVENT_REMOTE_NAME_REQUEST_COMPLETE:
      reverse_bd_addr(&packet[3], addr);
      pdev = getDeviceForAddress(pinfo, addr);
      if (pdev) {
        if (packet[2] == 0) {
          debug_printf("Name: '%s'\n", &packet[9]);
          pdev->state = REMOTE_NAME_FETCHED;

          if ((strcmp((char *)&packet[9], "Wireless Controller") == 0) && (pdev->CoD == COD_GAMEPAD)) {
            debug_printf(" -- Gamepad has found!!\n");
            gap_inquiry_stop();
            memcpy(remote_addr, pdev->address, 6);
            memcpy(pinfo->hidDevice.bdaddr, remote_addr, 6);
            pinfo->hidDevice.CoD = pdev->CoD;

            sdp_client_query_uuid16(
                &handle_sdp_client_query_result, remote_addr,
                BLUETOOTH_SERVICE_CLASS_PNP_INFORMATION);
            debug_printf("Connect to HID dev (%s).\n", bd_addr_to_str(remote_addr));
            status = hid_host_connect(remote_addr, hid_host_report_mode, &pinfo->hid_host_cid);
            break;
          }
        }
        else {
          debug_printf("Failed to get name: pag timeout.\n");
        }
      }
      continue_remote_names(pinfo);
      break;
    case HCI_EVENT_CONNECTION_REQUEST:
      codval = hci_event_connection_request_get_class_of_device(packet);
      hci_event_connection_request_get_bd_addr(packet, addr);
      add_device(pinfo, addr, hci_event_connection_request_get_class_of_device(packet));
      break;
    case HCI_EVENT_CONNECTION_COMPLETE:
      if (hci_event_connection_complete_get_status(packet) == 0)
      {
        hci_event_connection_complete_get_bd_addr(packet, remote_addr);
        pdev = getDeviceForAddress(pinfo, remote_addr);
        if (pdev)
        {
          if (pdev->CoD == COD_GAMEPAD)
          {
            pinfo->state |= BT_STATE_HID_CONNECT;
            gap_inquiry_stop();

            memcpy(pinfo->hidDevice.bdaddr, &remote_addr, 6);
            pinfo->hidDevice.cHandle = hci_event_connection_complete_get_connection_handle(packet);
            debug_printf("HCI Connect (%s), handle = %x.\n", bd_addr_to_str(remote_addr), pinfo->hidDevice.cHandle);
            hid_vid = hid_pid = 0;
            sdp_client_query_uuid16(
                      &handle_sdp_client_query_result, remote_addr,
                      BLUETOOTH_SERVICE_CLASS_PNP_INFORMATION);
          }
          else
          {
            pinfo->state |= BT_STATE_A2DP_CONNECT;
          }
        }
      }
      break;
    case HCI_EVENT_PIN_CODE_REQUEST:
      // inform about pin code request
      debug_printf("Pin code request - using '0000'\n");
      hci_event_pin_code_request_get_bd_addr(packet, event_addr);
      gap_pin_code_response(event_addr, "0000");
      break;

    case HCI_EVENT_USER_CONFIRMATION_REQUEST:
      // inform about user confirmation request
      debug_printf("SSP User Confirmation Request with numeric value '%"PRIu32"'\n",
                                   little_endian_read_32(packet, 8));
      debug_printf("SSP User Confirmation Auto accept\n");
      break;

    case HCI_EVENT_HID_META:
      switch (hci_event_hid_meta_get_subevent_code(packet)) {
      case HID_SUBEVENT_INCOMING_CONNECTION:
        debug_printf("Incoming conn.\n");
        // There is an incoming connection: we can accept it or decline it.
        // The hid_host_report_mode in the hid_host_accept_connection function
        // allows the application to request a protocol mode.
        // For available protocol modes, see hid_protocol_mode_t in btstack_hid.h file.
        hid_host_accept_connection(hid_subevent_incoming_connection_get_hid_cid(packet), hid_host_report_mode);
        break;
                        
      case HID_SUBEVENT_CONNECTION_OPENED:
        // The status field of this event indicates if the control and interrupt
        // connections were opened successfully.
        status = hid_subevent_connection_opened_get_status(packet);
        if (status != ERROR_CODE_SUCCESS) {
          debug_printf("HID Connection failed, status 0x%x\n", status);
          pinfo->hid_host_cid = 0;
          pinfo->state ^= ~(BT_STATE_HID_CONNECT|BT_STATE_HID_CLOSING);
          return;
        }

        hid_host_descriptor_available = false;
        pinfo->hid_host_cid = hid_subevent_connection_opened_get_hid_cid(packet);
        pinfo->state |= BT_STATE_HID_CONNECT;
        debug_printf("HID Host connected %x (%04x, %04x).\n", pinfo->hid_host_cid, hid_vid, hid_pid);
        postGuiEventMessage(GUIEV_ICON_CHANGE, ICON_SET | ICON_BLUETOOTH, NULL, NULL);
        postGuiEventMessage(GUIEV_BTDEV_CONNECTED, BT_CONN_HID, pinfo, NULL);

        padInfo = IsSupportedGamePad(hid_vid, hid_pid);
        if (padInfo)
        {
          padDriver = padInfo->padDriver;
          postGuiEventMessage(GUIEV_GAMEPAD_READY, 1, padInfo, NULL);
        }
        break;
      case HID_SUBEVENT_DESCRIPTOR_AVAILABLE:
        if (padDriver)
              (padDriver->btSetup)(pinfo->hid_host_cid);
        break;
      case HID_SUBEVENT_REPORT:
        if (padDriver)
        {
          HID_REPORT report;

          report.ptr = (uint8_t *)hid_subevent_report_get_report(packet);
          report.len = hid_subevent_report_get_report_len(packet);
          report.hid_mode = padInfo->hid_mode;
          padDriver->DecodeInputReport(&report);
        }
        break;
      case HID_SUBEVENT_SET_REPORT_RESPONSE:
        debug_printf("report_response: %x\n",
           hid_subevent_set_report_response_get_handshake_status(packet));
        break;
      case HID_SUBEVENT_SET_PROTOCOL_RESPONSE:
        // For incoming connections, the library will set the protocol mode of the
        // HID Device as requested in the call to hid_host_accept_connection. The event
        // reports the result. For connections initiated by calling hid_host_connect,
        // this event will occur only if the established report mode is boot mode.
        status = hid_subevent_set_protocol_response_get_handshake_status(packet);
        if (status != HID_HANDSHAKE_PARAM_TYPE_SUCCESSFUL){
          debug_printf("Error set protocol, status 0x%02x\n", status);
                break;
        }
        switch ((hid_protocol_mode_t)hid_subevent_set_protocol_response_get_protocol_mode(packet)){
        case HID_PROTOCOL_MODE_BOOT:
          debug_printf("Protocol mode set: BOOT.\n");
          break;
        case HID_PROTOCOL_MODE_REPORT:
          debug_printf("Protocol mode set: REPORT.\n");
          break;
        default:
          debug_printf("Unknown protocol mode.\n");
          break;
        }
        break;
      case HID_SUBEVENT_CONNECTION_CLOSED:
        // The connection was closed.
        pinfo->state &= ~BT_STATE_HID_MASK;
        hid_host_descriptor_available = false;
        debug_printf("HID Host disconnected (%d, %d) --> %x.\n", pinfo->a2dp_cid, pinfo->avrcp_cid, pinfo->state);
        if ((pinfo->a2dp_cid == 0) && (pinfo->avrcp_cid == 0))
        {
          postGuiEventMessage(GUIEV_ICON_CHANGE, ICON_CLEAR | ICON_BLUETOOTH, NULL, NULL);
        }
        postGuiEventMessage(GUIEV_BTDEV_CONNECTED, BT_CONN_HID, pinfo, NULL);
        gap_disconnect(pinfo->hidDevice.cHandle);
        postGuiEventMessage(GUIEV_GAMEPAD_READY, 0, NULL, NULL);
        pinfo->hid_host_cid = 0;
        if (padDriver)
        {
          (padDriver->btDisconnect)();
          padDriver = NULL;
        }
        break;
      case HID_SUBEVENT_GET_REPORT_RESPONSE:
        status = hid_subevent_get_report_response_get_handshake_status(packet);
        if (status != HID_HANDSHAKE_PARAM_TYPE_SUCCESSFUL){
          printf("Error get report, status 0x%02x\n", status);
          break;
        }
        debug_printf("Received report[%d]: ", hid_subevent_get_report_response_get_report_len(packet));
        debug_printf("\n");
        if (padDriver && padDriver->btProcessGetReport)
                (padDriver->btProcessGetReport)(hid_subevent_get_report_response_get_report(packet), hid_subevent_get_report_response_get_report_len(packet));
        break;
      default:
        break;
      }	/* end of subevent */
      break;	/* end of HCI_EVENT_HID_META */
    case HCI_EVENT_DISCONNECTION_COMPLETE:
      chandle = hci_event_disconnection_complete_get_connection_handle(packet);
      debug_printf("Disconnect complete. %d (%d, %d)\n", chandle, pinfo->hid_host_cid, pinfo->a2dp_cid);
      break;
    }
  }
}

