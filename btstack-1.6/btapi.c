#include "DoomPlayer.h"
#include "btstack_config.h"
#include "btstack.h"
#include "btapi.h"

#define	INQUIRY_INTERVAL 5
#define	BTREQ_DEPTH	3

static uint8_t btreqBuffer[BTREQ_DEPTH * sizeof(BTREQ_PARAM)];
MESSAGEQ_DEF(btreqq, btreqBuffer, sizeof(btreqBuffer))

extern void btstack_start_a2dp_sink();
extern void btstack_run_loop_freertos_trigger();

static osMessageQueueId_t btreqqId;

void btapi_setup()
{
  btreqqId = osMessageQueueNew(BTREQ_DEPTH, sizeof(BTREQ_PARAM), &attributes_btreqq);
}

static const avrcp_media_attribute_id_t media_attributes[3] = {
  AVRCP_MEDIA_ATTR_TITLE,
  AVRCP_MEDIA_ATTR_ARTIST,
  AVRCP_MEDIA_ATTR_SONG_LENGTH_MS,
};

static const avrcp_media_attribute_id_t cover_attributes[1] = {
  AVRCP_MEDIA_ATTR_DEFAULT_COVER_ART,
};

void process_btapi_request(BTSTACK_INFO *info)
{
  BTREQ_PARAM req;
  int8_t st;

  if (osMessageQueueGet(btreqqId, &req, 0, 0) == osOK)
  {
    switch (req.code)
    {
    case BTREQ_START_SCAN:
      gap_inquiry_start(INQUIRY_INTERVAL);
      info->state |= BT_STATE_SCAN;
      break;
    case BTREQ_STOP_SCAN:
      gap_inquiry_stop();
      info->state &= ~BT_STATE_SCAN;
      break;
    case BTREQ_DISC_HID:
debug_printf("BTREQ_DISC_HID: state = %x, cid = %x\n", info->state, info->hid_host_cid);
      if ((info->state & BT_STATE_HID_CONNECT) && (info->hid_host_cid != 0))
      {
          hid_host_disconnect(info->hid_host_cid);
          info->state &= ~BT_STATE_HID_CONNECT;
          info->state |= BT_STATE_HID_CLOSING;
      }
      break;
    case BTREQ_DISC_A2DP:
      if ((info->state & BT_STATE_A2DP_CONNECT) && (info->a2dp_cid != 0)) 
      {
          a2dp_sink_disconnect(info->a2dp_cid);
          info->state &= ~BT_STATE_A2DP_CONNECT;
          info->state |= BT_STATE_A2DP_CLOSING;
      }
      break;
    case BTREQ_SHUTDOWN:
      if (info->state & (BT_STATE_HID_CONNECT|BT_STATE_A2DP_CONNECT)) 
      {
debug_printf("shutdown: %d, %d\n", info->hid_host_cid, info->a2dp_cid);
        if (info->hid_host_cid != 0)
        {
          hid_host_disconnect(info->hid_host_cid);
        }
        if (info->a2dp_cid != 0)
          a2dp_sink_disconnect(info->a2dp_cid);
        {
        }
        info->state &= ~BT_STATE_HID_CONNECT;
        info->state |= BT_STATE_HID_CLOSING;
      }
      break;
    case BTREQ_SEND_REPORT:
      hid_host_send_report(info->hid_host_cid, req.report_code, req.argptr, req.report_length);
      break;
    case BTREQ_GET_INFO:
      info->avrcp_cid = req.argval;
      if (info->avrcp_cid)
      {
        st = avrcp_controller_get_element_attributes(info->avrcp_cid, 3, media_attributes);
        (void)st;
      }
      break;
    case BTREQ_GET_COVER:
      info->avrcp_cid = req.argval;
      if (info->avrcp_cid)
      {
        st = avrcp_controller_get_element_attributes(info->avrcp_cid, 1, cover_attributes);
#ifdef BTAPI_DEBUG
        if (st) debug_printf("get_cover: %d\n", st);
#else
        (void)st;
#endif
      }
      break;
    case BTREQ_GET_STATUS:
      info->avrcp_cid = req.argval;
      if (info->avrcp_cid)
      {
        st = avrcp_controller_get_play_status(info->avrcp_cid);
#ifdef BTAPI_DEBUG
        if (st) debug_printf("get_status: %d\n", st);
#else
        (void)st;
#endif
      }
      break;
    case BTREQ_POWER_OFF:
      hci_power_control(HCI_POWER_OFF);
      break;
    case BTREQ_AVRCP_PLAY:
      if (info->avrcp_cid)
      {
        avrcp_controller_play(info->avrcp_cid);
      }
      break;
    case BTREQ_AVRCP_PAUSE:
      if (info->avrcp_cid)
      {
        avrcp_controller_pause(info->avrcp_cid);
      }
      break;
    case BTREQ_AVRCP_NEXT:
      if (info->avrcp_cid)
      {
        avrcp_controller_forward(info->avrcp_cid);
      }
      break;
    case BTREQ_AVRCP_LAST:
      if (info->avrcp_cid)
      {
        avrcp_controller_backward(info->avrcp_cid);
      }
      break;
    case BTREQ_START_A2DP:
#ifdef BTAPI_DEBUG
      debug_printf("Got BTREQ_START_A2DP\n");
#endif
      btstack_start_a2dp_sink();
      break;
    default:
      break;
    }
  }
}

void btapi_post_request(uint16_t code, uint16_t cid)
{
  BTREQ_PARAM req;
  osStatus_t st;

  req.code = code;
  req.argval = cid;
  st = osMessageQueuePut(btreqqId, &req, 0, 3);
  if (st != osOK)
  {
    debug_printf("%s failed.\n");
  }
  btstack_run_loop_freertos_trigger();
}

void btapi_avrcp_play()
{
  BTREQ_PARAM req;

  req.code = BTREQ_AVRCP_PLAY;
  osMessageQueuePut(btreqqId, &req, 0, 0);
  btstack_run_loop_freertos_trigger();
}

void btapi_avrcp_pause()
{
  BTREQ_PARAM req;

  req.code = BTREQ_AVRCP_PAUSE;
  osMessageQueuePut(btreqqId, &req, 0, 0);
  btstack_run_loop_freertos_trigger();
}

void btapi_avrcp_next()
{
  BTREQ_PARAM req;

  req.code = BTREQ_AVRCP_NEXT;
  osMessageQueuePut(btreqqId, &req, 0, 0);
  btstack_run_loop_freertos_trigger();
}

void btapi_avrcp_prev()
{
  BTREQ_PARAM req;

  req.code = BTREQ_AVRCP_LAST;
  osMessageQueuePut(btreqqId, &req, 0, 0);
  btstack_run_loop_freertos_trigger();
}

void btapi_start_scan()
{
  BTREQ_PARAM req;

  req.code = BTREQ_START_SCAN;
  osMessageQueuePut(btreqqId, &req, 0, 0);
  btstack_run_loop_freertos_trigger();
}

void btapi_stop_scan()
{
  BTREQ_PARAM req;

  req.code = BTREQ_STOP_SCAN;
  osMessageQueuePut(btreqqId, &req, 0, 0);
  btstack_run_loop_freertos_trigger();
}

void btapi_hid_disconnect()
{
  BTREQ_PARAM req;

  req.code = BTREQ_DISC_HID;
  osMessageQueuePut(btreqqId, &req, 0, 0);
  btstack_run_loop_freertos_trigger();
}

void btapi_a2dp_disconnect()
{
  BTREQ_PARAM req;

  req.code = BTREQ_DISC_A2DP;
  osMessageQueuePut(btreqqId, &req, 0, 0);
  btstack_run_loop_freertos_trigger();
}

void btapi_shutdown()
{
  BTREQ_PARAM req;

  req.code = BTREQ_SHUTDOWN;
  osMessageQueuePut(btreqqId, &req, 0, 0);
}

void btapi_power_off()
{
  BTREQ_PARAM req;

  req.code = BTREQ_POWER_OFF;
  osMessageQueuePut(btreqqId, &req, 0, 0);
}

void btapi_send_report(uint8_t *ptr, int len)
{
  BTREQ_PARAM request;

  request.code = BTREQ_SEND_REPORT;
  request.report_code = *ptr;
  request.argptr = ptr + 1;
  request.report_length = len - 1;
  osMessageQueuePut(btreqqId, &request, 0, 0);
}

void btapi_start_a2dp()
{
  BTREQ_PARAM req;

  req.code = BTREQ_START_A2DP;
  osMessageQueuePut(btreqqId, &req, 0, 0);
  btstack_run_loop_poll_data_sources_from_irq();
}
