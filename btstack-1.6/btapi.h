#ifndef BTAPI_H_
#define BTAPI_H_

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
} BTREQUEST;

typedef struct {
  BTREQUEST  code;
  uint32_t   argval;
  void       *argptr;
  uint16_t   report_code;
  uint16_t   report_length;
} BTREQ_PARAM;

typedef enum {
  BTSTACK_STATE_INIT = 0,
  BTSTACK_STATE_ACTIVE,
  BTSTACK_STATE_CONNECT,
  BTSTACK_STATE_CLOSING,
} BTSTACK_STATE;

typedef struct {
  BTSTACK_STATE state;
  uint16_t      avrcp_cid;
  uint16_t      hid_host_cid;
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

#endif
