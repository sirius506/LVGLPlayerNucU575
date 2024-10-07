#include "DoomPlayer.h"
#include "btstack_config.h"
#include "btstack.h"
#include "btapi.h"

#define	INQUIRY_INTERVAL 5
#define	BTREQ_DEPTH	3

static uint8_t btreqBuffer[BTREQ_DEPTH * sizeof(BTREQ_PARAM)];
MESSAGEQ_DEF(btreqq, btreqBuffer, sizeof(btreqBuffer))

static osMessageQueueId_t btreqqId;

void btapi_setup()
{
  btreqqId = osMessageQueueNew(BTREQ_DEPTH, sizeof(BTREQ_PARAM), &attributes_btreqq);
}

static const avrcp_media_attribute_id_t media_attributes[4] = {
  AVRCP_MEDIA_ATTR_TITLE,
  AVRCP_MEDIA_ATTR_ARTIST,
  AVRCP_MEDIA_ATTR_DEFAULT_COVER_ART,
  AVRCP_MEDIA_ATTR_SONG_LENGTH_MS,
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
      info->state = BTSTACK_STATE_ACTIVE;
      break;
    case BTREQ_STOP_SCAN:
      gap_inquiry_stop();
      info->state = BTSTACK_STATE_INIT;
      break;
    case BTREQ_DISCONNECT:
    case BTREQ_SHUTDOWN:
      if (info->state == BTSTACK_STATE_CONNECT) 
      {
debug_printf("disc: %d, %d\n", info->hid_host_cid, info->a2dp_cid);
        if (info->hid_host_cid != 0)
        {
          hid_host_disconnect(info->hid_host_cid);
        }
        if (info->a2dp_cid != 0)
          a2dp_sink_disconnect(info->a2dp_cid);
        {
        }
        if (req.code == BTREQ_SHUTDOWN)
          info->state = BTSTACK_STATE_CLOSING;
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
        if (st) debug_printf("get_info: %d\n", st);
      }
      break;
    case BTREQ_GET_STATUS:
      info->avrcp_cid = req.argval;
      if (info->avrcp_cid)
      {
        st = avrcp_controller_get_play_status(info->avrcp_cid);
        if (st) debug_printf("get_status: %d\n", st);
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

void btapi_disconnect()
{
  BTREQ_PARAM req;

  req.code = BTREQ_DISCONNECT;
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

