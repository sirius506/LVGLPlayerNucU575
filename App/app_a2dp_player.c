/**
 * @file app_a2dp_player.c
 * @brief A2DP player GUI
 */
#include "DoomPlayer.h"
#include "app_music.h"
#include "a2dp_player.h"

/*********************
 *      DEFINES
 *********************/

extern lv_obj_t * spectrum_obj;
static lv_obj_t * play_obj;
static lv_obj_t * slider_obj;
static lv_obj_t * time_obj;

static void prev_click_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED) {
        //_lv_demo_music_album_next(false);
      btapi_avrcp_prev();
    }
}

static void next_click_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED) {
        //_lv_demo_music_album_next(true);
      btapi_avrcp_next();
    }
}

static void play_click_event_cb(lv_event_t * e)
{
    lv_obj_t * obj = lv_event_get_target(e);
    if(lv_obj_has_state(obj, LV_STATE_CHECKED))
    {
        btapi_avrcp_play();
    }
    else
    {
        btapi_avrcp_pause();
    }
}

static void slider_event_cb(lv_event_t * e)
{
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *obj = lv_event_get_target(e);

  /*Provide some extra space for the value*/
  if(code == LV_EVENT_REFR_EXT_DRAW_SIZE) {
    lv_event_set_ext_draw_size(e, 50);
  }
  else if (code == LV_EVENT_DRAW_MAIN_END)
  {
    int ctime;
    lv_slider_t *slider = (lv_slider_t *)obj;
    lv_area_t knob_area;
    char buf[16];

    ctime = lv_slider_get_value(obj);
    lv_snprintf(buf, sizeof(buf), "%d:%02d", ctime/60, ctime%60);

    lv_point_t label_size;
    lv_txt_get_size(&label_size, buf, LV_FONT_DEFAULT, 0, 0, LV_COORD_MAX, 0);
    lv_area_t label_area;
    label_area.x1 = 0;
    label_area.x2 = label_size.x - 1;
    label_area.y1 = 0;
    label_area.y2 = label_size.y - 1;

    knob_area = slider->right_knob_area;
    lv_area_align(&knob_area, &label_area, LV_ALIGN_OUT_TOP_RIGHT, 0, 13);

    lv_draw_label_dsc_t label_draw_dsc;
    lv_draw_label_dsc_init(&label_draw_dsc);
    label_draw_dsc.color = lv_color_hex(0x8a86b8);
    label_draw_dsc.text = buf;
    label_draw_dsc.text_local = true;
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_draw_label(layer, &label_draw_dsc, &label_area);
  }
}

static lv_obj_t * create_title_box(A2DP_SCREEN *a2dps, lv_obj_t * parent)
{
    FS_DIRENT *ttf;

    /*Create the titles*/
    lv_obj_t * cont = lv_obj_create(parent);
    lv_obj_remove_style_all(cont);
    lv_obj_set_height(cont, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    ttf = find_flash_file(TTF_FONT_NAME);
    a2dps->title_font = lv_tiny_ttf_create_data((const void *)(QSPI_FLASH_ADDR + ttf->foffset), ttf->fsize, 20);
    a2dps->artist_font = lv_tiny_ttf_create_data((const void *)(QSPI_FLASH_ADDR + ttf->foffset), ttf->fsize, 15);

    a2dps->title_label = lv_label_create(cont);
    lv_obj_set_style_text_font(a2dps->title_label, a2dps->title_font, 0);
    lv_obj_set_style_text_color(a2dps->title_label, lv_color_hex(0x504d6d), 0);

    set_scroll_labeltext(a2dps->title_label, "Waiting for Media Info.. ", a2dps->title_font);
    lv_obj_set_height(a2dps->title_label, lv_font_get_line_height(a2dps->title_font) * 3 / 2);

    a2dps->artist_label = lv_label_create(cont);
    lv_obj_set_style_text_font(a2dps->artist_label, a2dps->artist_font, 0);
    lv_obj_set_style_text_color(a2dps->artist_label, lv_color_hex(0x504d6d), 0);
    set_scroll_labeltext(a2dps->artist_label, "Please connect..", a2dps->artist_font);

    return cont;
}

static lv_obj_t *ctrl_buttons[5];

void show_a2dp_buttons()
{
    for (int i = 0; i < 5; i++)
    {
      lv_obj_clear_flag(ctrl_buttons[i], LV_OBJ_FLAG_HIDDEN);
    }
}

void hide_a2dp_buttons(A2DP_SCREEN *a2dps)
{
    for (int i = 0; i < 5; i++)
    {
      lv_obj_add_flag(ctrl_buttons[i], LV_OBJ_FLAG_HIDDEN);
    }
    if (a2dps)
    {
      set_scroll_labeltext(a2dps->title_label, "Waiting for Media Info.. ", a2dps->title_font);
      set_scroll_labeltext(a2dps->artist_label, "Please connect..", a2dps->artist_font);
    }
}

static lv_obj_t * create_ctrl_box(A2DP_SCREEN *a2dps, lv_obj_t * parent, lv_group_t *g)
{
    static lv_style_t style_focus;

    lv_style_init(&style_focus);
    lv_style_set_img_recolor_opa(&style_focus, LV_OPA_10);
    lv_style_set_img_recolor(&style_focus, lv_color_black());

    /*Create the control box*/
    lv_obj_t * cont = lv_obj_create(parent);
    lv_obj_remove_style_all(cont);
    lv_obj_set_height(cont, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_bottom(cont, 17, 0);
    static const lv_coord_t grid_col[] = {LV_GRID_FR(2), LV_GRID_FR(3), LV_GRID_FR(5), LV_GRID_FR(5), LV_GRID_FR(5), LV_GRID_FR(3), LV_GRID_FR(2), LV_GRID_TEMPLATE_LAST};

    static const lv_coord_t grid_row[] = {LV_GRID_CONTENT, 50, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(cont, grid_col, grid_row);

    LV_IMG_DECLARE(img_lv_demo_music_btn_loop);
    LV_IMG_DECLARE(img_lv_demo_music_btn_rnd);
    LV_IMG_DECLARE(img_lv_demo_music_btn_next);
    LV_IMG_DECLARE(img_lv_demo_music_btn_prev);
    LV_IMG_DECLARE(img_lv_demo_music_btn_play);
    LV_IMG_DECLARE(img_lv_demo_music_btn_pause);

    lv_obj_t **cbutton = ctrl_buttons;

    *cbutton = lv_image_create(cont);
    lv_image_set_src(*cbutton, &img_lv_demo_music_btn_rnd);
    lv_obj_set_grid_cell(*cbutton, LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_CENTER, 0, 1);

    cbutton++;
    *cbutton = lv_image_create(cont);
    lv_image_set_src(*cbutton, &img_lv_demo_music_btn_loop);
    lv_obj_set_grid_cell(*cbutton, LV_GRID_ALIGN_END, 5, 1, LV_GRID_ALIGN_CENTER, 0, 1);

    cbutton++;
    *cbutton = lv_image_create(cont);
    lv_image_set_src(*cbutton, &img_lv_demo_music_btn_prev);
    lv_obj_set_grid_cell(*cbutton, LV_GRID_ALIGN_CENTER, 2, 1, LV_GRID_ALIGN_CENTER, 0, 1);
    lv_obj_add_event_cb(*cbutton, prev_click_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(*cbutton, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_style(*cbutton, &style_focus, LV_STATE_FOCUS_KEY);
    lv_group_add_obj(g, *cbutton);

    cbutton++;
    play_obj = *cbutton = lv_imagebutton_create(cont);
    lv_imagebutton_set_src(play_obj, LV_IMAGEBUTTON_STATE_RELEASED, NULL, &img_lv_demo_music_btn_play, NULL);
    lv_imagebutton_set_src(play_obj, LV_IMAGEBUTTON_STATE_CHECKED_RELEASED, NULL, &img_lv_demo_music_btn_pause, NULL);
    lv_obj_add_style(play_obj, &style_focus, LV_STATE_FOCUS_KEY);
    //lv_image_set_zoom(play_obj, LV_IMG_ZOOM_NONE);
    lv_obj_add_flag(play_obj, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_set_grid_cell(play_obj, LV_GRID_ALIGN_CENTER, 3, 1, LV_GRID_ALIGN_CENTER, 0, 1);

    lv_obj_add_event_cb(play_obj, play_click_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(play_obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_width(play_obj, img_lv_demo_music_btn_play.header.w);
    lv_group_add_obj(g, play_obj);

    cbutton++;
    *cbutton = lv_image_create(cont);
    lv_image_set_src(*cbutton, &img_lv_demo_music_btn_next);
    lv_obj_set_grid_cell(*cbutton, LV_GRID_ALIGN_CENTER, 4, 1, LV_GRID_ALIGN_CENTER, 0, 1);
    lv_obj_add_event_cb(*cbutton, next_click_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(*cbutton, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_style(*cbutton, &style_focus, LV_STATE_FOCUS_KEY);
    lv_group_add_obj(g, *cbutton);

    lv_group_focus_obj(play_obj);	// Set focus on Play button

    LV_IMG_DECLARE(img_lv_demo_music_slider_knob);
    slider_obj = lv_slider_create(cont);
    lv_obj_set_style_anim_time(slider_obj, 100, 0);
    lv_obj_remove_flag(slider_obj, LV_OBJ_FLAG_CLICKABLE); /*No input from the slider*/
    lv_obj_add_event_cb(slider_obj, slider_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_refresh_ext_draw_size(slider_obj);

    lv_obj_set_height(slider_obj, 6);
    lv_obj_set_grid_cell(slider_obj, LV_GRID_ALIGN_STRETCH, 1, 4, LV_GRID_ALIGN_CENTER, 1, 1);

    lv_obj_set_style_bg_image_src(slider_obj, &img_lv_demo_music_slider_knob, LV_PART_KNOB);
    lv_obj_set_style_bg_opa(slider_obj, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider_obj, 20, LV_PART_KNOB);
    lv_obj_set_style_bg_grad_dir(slider_obj, LV_GRAD_DIR_HOR, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider_obj, lv_color_hex(0x569af8), LV_PART_INDICATOR);
    lv_obj_set_style_bg_grad_color(slider_obj, lv_color_hex(0xa666f1), LV_PART_INDICATOR);
    lv_obj_set_style_outline_width(slider_obj, 0, 0);

    time_obj = lv_label_create(cont);
    lv_obj_set_style_text_font(time_obj, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(time_obj, lv_color_hex(0x8a86b8), 0);
    lv_label_set_text(time_obj, "0:00");
    lv_obj_set_grid_cell(time_obj, LV_GRID_ALIGN_END, 5, 1, LV_GRID_ALIGN_CENTER, 1, 1);

    hide_a2dp_buttons(a2dps);

    return cont;
}

void set_playbtn_state(lv_imagebutton_state_t new_state)
{
    lv_imagebutton_set_state(play_obj, new_state);
}

extern lv_obj_t * create_spectrum_obj(lv_obj_t * parent);

lv_obj_t * a2dp_player_create(A2DP_SCREEN *a2dps, lv_obj_t * parent, lv_group_t *g)
{
  a2dps->cover_count = register_cover_file();
  a2dps->cur_cover = 0;

  //create_wave_images(parent);
  lv_obj_t * title_box = create_title_box(a2dps, parent);
  lv_obj_t * ctrl_box = create_ctrl_box(a2dps, parent, g);
  spectrum_obj = create_spectrum_obj(parent);

    /*Arrange the content into a grid*/
    static const lv_coord_t grid_cols[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static const lv_coord_t grid_rows[] = {LV_DEMO_MUSIC_HANDLE_SIZE,     /*Spacing*/
                                           LV_GRID_FR(1),   /*Spacer*/
                                           LV_GRID_CONTENT, /*Title box*/
                                           LV_GRID_FR(1),   /*Spacer*/
                                           LV_GRID_CONTENT, /*Icon box*/
                                           LV_GRID_FR(3),   /*Spacer*/
                                           LV_GRID_CONTENT, /*Control box*/
                                           LV_GRID_FR(1),   /*Spacer*/
                                           LV_GRID_CONTENT, /*Handle box*/
                                           LV_GRID_FR(1),   /*Spacer*/
                                           LV_DEMO_MUSIC_HANDLE_SIZE,     /*Spacing*/
                                           LV_GRID_TEMPLATE_LAST
                                          };

    lv_obj_set_grid_dsc_array(parent, grid_cols, grid_rows);
    lv_obj_set_style_grid_row_align(parent, LV_GRID_ALIGN_SPACE_BETWEEN, 0);
    lv_obj_set_grid_cell(title_box, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_CENTER, 2, 1);
    lv_obj_set_grid_cell(ctrl_box, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_CENTER, 6, 1);
    lv_obj_set_grid_cell(spectrum_obj, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_CENTER, 1, 9);

    return parent;
}

static lv_image_dsc_t imgdesc;

void change_track_cover(A2DP_SCREEN *a2dps)
{
  COVER_INFO *cfp;
  lv_obj_t * img;

  a2dps->cur_cover++;
  if (a2dps->cur_cover >= a2dps->cover_count)
    a2dps->cur_cover = 0;

  img = lv_image_create(spectrum_obj);

  cfp = track_cover(a2dps->cur_cover);
  if (cfp)
  {
      lv_image_dsc_t *desc = (lv_image_dsc_t *)(cfp->faddr);

      imgdesc = *desc;
      imgdesc.data_size = desc->header.w * desc->header.h * 2;
      imgdesc.data = cfp->faddr + 12;
      lv_image_set_src(img, &imgdesc);
  }
  lv_image_set_antialias(img, false);
  lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);
#if 0
  lv_obj_add_event_cb(img, album_gesture_event_cb, LV_EVENT_GESTURE, NULL);
  lv_obj_remove_flag(img, LV_OBJ_FLAG_GESTURE_BUBBLE);
  lv_obj_add_flag(img, LV_OBJ_FLAG_CLICKABLE);
#endif

  //return img;
}

void app_ppos_update(MIX_INFO *mixInfo)
{
    unsigned long slen = mixInfo->song_len / 44100;
    lv_slider_set_range(slider_obj, 0, slen);
    lv_slider_set_value(slider_obj, mixInfo->psec, LV_ANIM_ON);
    lv_label_set_text_fmt(time_obj, "%"LV_PRIu32":%02"LV_PRIu32, slen / 60, slen % 60);
}
