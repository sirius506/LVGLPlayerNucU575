#ifndef BTAPI_H_
#define BTAPI_H_
#include "btstack_util.h"

#define	BT_CONN_HID	1
#define	BT_CONN_A2DP	2

#define USE_NEW_BTAPI

typedef enum {
 BTREQ_START_SCAN = 1,
 BTREQ_STOP_SCAN,
 BTREQ_DISCONNECT,
 BTREQ_POWER_OFF,
 BTREQ_SHUTDOWN,
 BTREQ_SEND_REPORT,
 BTREQ_GET_STATUS,
 BTREQ_GET_INFO,
 BTREQ_AVRCP_PLAY,
 BTREQ_AVRCP_PAUSE,
 BTREQ_AVRCP_NEXT,
 BTREQ_AVRCP_LAST,
 BTREQ_START_A2DP,
} BTREQUEST;

typedef struct {
  BTREQUEST  code;
  uint32_t   argval;
  void       *argptr;
  uint16_t   report_code;
  uint16_t   report_length;
} BTREQ_PARAM;

#define	BT_STATE_INIT	0
#define	BT_STATE_SCAN	1
#define	BT_STATE_HID_CONNECT	2
#define	BT_STATE_HID_CLOSING	4
#define	BT_STATE_HID_MASK	0x06
#define	BT_STATE_A2DP_CONNECT	0x08
#define	BT_STATE_A2DP_CLOSING	0x10
#define	BT_STATE_A2DP_MASK	0x18

#define	COD_GAMEPAD	0x002508
#define	COD_AUDIO	0x200400

typedef struct {
  bd_addr_t  bdaddr;
  uint32_t   CoD;
} PEER_DEVICE;

typedef struct {
  uint16_t      state;
  uint16_t      avrcp_cid;
  uint16_t      a2dp_cid;
  uint16_t      hid_host_cid;
  int         deviceCount;
  PEER_DEVICE hidDevice;
  PEER_DEVICE a2dpHost;
} BTSTACK_INFO;

void btapi_avrcp_play();
void btapi_avrcp_pause();
void btapi_avrcp_prev();
void btapi_avrcp_next();
void btapi_start_scan();
void btapi_stop_scan();
void btapi_disconnect();
void btapi_power_off();
void btapi_send_report(uint8_t *ptr, int len);
void btapi_push_report();
void btapi_post_request(uint16_t code, uint16_t cid);
void btapi_shutdown();
void process_btapi_request(BTSTACK_INFO *info);
void btapi_setup();
void btapi_start_a2dp();

#endif
