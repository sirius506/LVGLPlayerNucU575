/**
 * @file app_music_control.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "DoomPlayer.h"
#include <stdio.h>
#include "app_music.h"
#include "audio_output.h"

/*********************
 *      DEFINES
 *********************/
#define INTRO_TIME          200
#define BAR_COLOR1          lv_color_hex(0xe9dbfc)
#define BAR_COLOR2          lv_color_hex(0x6f8af6)
#define BAR_COLOR3          lv_color_hex(0xffffff)
#if LV_DEMO_MUSIC_LARGE
    #define BAR_COLOR1_STOP     160
    #define BAR_COLOR2_STOP     200
    #define BAR_REST_RADIUS     128
#else
    #define BAR_COLOR1_STOP     80
    #define BAR_COLOR2_STOP     100
    #define BAR_REST_RADIUS     62
#endif
#define BAR_COLOR3_STOP     (LV_MAX(LV_HOR_RES, LV_VER_RES) / 3)
#define BAR_CNT             20
#define DEG_STEP            (180/BAR_CNT)
#define BAND_CNT            4
#define BAR_PER_BAND_CNT    (BAR_CNT / BAND_CNT)

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static lv_obj_t * create_cont(lv_obj_t * parent);
static void create_wave_images(lv_obj_t * parent);
static lv_obj_t * create_title_box(lv_obj_t * parent);
static lv_obj_t * create_icon_box(lv_obj_t * parent);
lv_obj_t * create_spectrum_obj(lv_obj_t * parent);
static lv_obj_t * create_ctrl_box(lv_obj_t * parent, lv_group_t *g);
static lv_obj_t * create_handle(lv_obj_t * parent);

#if 0
static void spectrum_anim_cb(void * a, int32_t v);
#endif
static void spectrum_draw_event_cb(lv_event_t * e);
static lv_obj_t * album_image_create(lv_obj_t * parent);
static void album_gesture_event_cb(lv_event_t * e);
static void play_event_click_cb(lv_event_t * e);
static void prev_click_event_cb(lv_event_t * e);
static void next_click_event_cb(lv_event_t * e);
static void menu_click_event_cb(lv_event_t * e);
static void track_load(uint32_t id);
#ifdef DO_INTRO_ANIM
static void start_anim_cb(void * var, int32_t v);
static void stop_start_anim(lv_timer_t * t);
#endif
static void album_fade_anim_cb(void * var, int32_t v);
static int32_t get_cos(int32_t deg, int32_t a);
static int32_t get_sin(int32_t deg, int32_t a);

lv_obj_t * spectrum_obj;

/**********************
 *  STATIC VARIABLES
 **********************/
static lv_obj_t * main_cont;
static lv_obj_t * title_label;
static lv_obj_t * artist_label;
static lv_obj_t * time_obj;
static lv_obj_t * album_image_obj;
static lv_obj_t * slider_obj;
static uint32_t spectrum_i = 0;
static uint32_t spectrum_i_pause = 0;
static uint32_t bar_ofs = 0;
static uint32_t bar_rot = 0;
static lv_timer_t  * inter_pause_timer;
static int32_t track_id;
static Mix_Music *curMusic;
static bool playing;
static bool start_anim;
static lv_coord_t start_anim_values[40];
static lv_obj_t * play_obj;
#if 0
static const uint16_t (* spectrum)[4];
static uint32_t spectrum_len;
#endif
static const uint16_t rnd_array[30] = {994, 285, 553, 11, 792, 707, 966, 641, 852, 827, 44, 352, 146, 581, 490, 80, 729, 58, 695, 940, 724, 561, 124, 653, 27, 292, 557, 506, 382, 199};

static void spectrum_anim_update(int v);

extern lv_obj_t *getMusicList();

extern const GUI_LAYOUT GuiLayout;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/*
 * Callback adapter function to convert parameter types to avoid compile-time
 * warning.
 */
static void _image_set_scale_anim_cb(void * obj, int32_t scale)
{           
    lv_image_set_scale((lv_obj_t *)obj, (uint16_t)scale);
}

/*
 * Callback adapter function to convert parameter types to avoid compile-time
 * warning.
 */
static void _obj_set_x_anim_cb(void * obj, int32_t x)
{
    lv_obj_set_x((lv_obj_t *)obj, (lv_coord_t)x);
}

lv_obj_t * _lv_demo_music_main_create(lv_obj_t * parent, lv_group_t *g, lv_style_t *btn_style)
{
  /*Create the content of the music player*/
  lv_obj_t * cont = create_cont(parent);
  create_wave_images(cont);
  lv_obj_t * title_box = create_title_box(cont);
  lv_obj_t * icon_box = create_icon_box(cont);
  lv_obj_t * ctrl_box = create_ctrl_box(cont, g);
  spectrum_obj = create_spectrum_obj(cont);
  lv_obj_t * handle_box = create_handle(cont);

  lv_obj_t *btn_home, *label;

  btn_home = lv_btn_create(cont);
  lv_obj_set_size(btn_home, W_PERCENT(18), W_PERCENT(18));
  lv_obj_align(btn_home, LV_ALIGN_TOP_RIGHT, W_PERCENT(9), -W_PERCENT(9) + LV_DEMO_MUSIC_HANDLE_SIZE);
  lv_obj_set_style_radius(btn_home, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(btn_home, lv_palette_main(LV_PALETTE_ORANGE), 0);
  lv_obj_add_event_cb(btn_home, menu_click_event_cb, LV_EVENT_CLICKED, 0);
  lv_obj_add_style(btn_home, btn_style, LV_STATE_FOCUS_KEY);

  label = lv_label_create(btn_home);
  lv_label_set_text(label, LV_SYMBOL_HOME);
  lv_obj_align(label, LV_ALIGN_BOTTOM_LEFT, 2, -5);

  lv_obj_add_flag(btn_home, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_group_add_obj(g, btn_home);

    track_id = -1;

#if LV_DEMO_MUSIC_ROUND
    lv_obj_set_style_pad_hor(cont, LV_HOR_RES / 6, 0);
#endif

    /*Arrange the content into a grid*/
#if LV_DEMO_MUSIC_SQUARE || LV_DEMO_MUSIC_ROUND
    static const lv_coord_t grid_cols[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t grid_rows[] = {LV_DEMO_MUSIC_HANDLE_SIZE,     /*Spacing*/
                                     0,   /*Spectrum obj, set later*/
                                     LV_GRID_CONTENT, /*Title box*/
                                     LV_GRID_FR(3),   /*Spacer*/
                                     LV_GRID_CONTENT, /*Icon box*/
                                     LV_GRID_FR(3),   /*Spacer*/
                                     LV_GRID_CONTENT, /*Control box*/
                                     LV_GRID_FR(3),   /*Spacer*/
                                     LV_GRID_CONTENT, /*Handle box*/
                                     LV_DEMO_MUSIC_HANDLE_SIZE,     /*Spacing*/
                                     LV_GRID_TEMPLATE_LAST
                                    };

    grid_rows[1] = LV_VER_RES;

    lv_obj_set_grid_dsc_array(cont, grid_cols, grid_rows);
    lv_obj_set_style_grid_row_align(cont, LV_GRID_ALIGN_SPACE_BETWEEN, 0);
    lv_obj_set_grid_cell(spectrum_obj, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_CENTER, 1, 1);
    lv_obj_set_grid_cell(title_box, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_CENTER, 2, 1);
    lv_obj_set_grid_cell(icon_box, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_CENTER, 4, 1);
    lv_obj_set_grid_cell(ctrl_box, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_CENTER, 6, 1);
    lv_obj_set_grid_cell(handle_box, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_CENTER, 8, 1);
#elif LV_DEMO_MUSIC_LANDSCAPE == 0
    static const lv_coord_t grid_cols[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static const lv_coord_t grid_rows[] = {LV_DEMO_MUSIC_HANDLE_SIZE,     /*Spacing*/
                                           LV_GRID_FR(1),   /*Spacer*/
                                           LV_GRID_CONTENT, /*Title box*/
                                           LV_GRID_FR(3),   /*Spacer*/
                                           LV_GRID_CONTENT, /*Icon box*/
                                           LV_GRID_FR(3),   /*Spacer*/
# if LV_DEMO_MUSIC_LARGE == 0
                                           250,    /*Spectrum obj*/
# else
                                           480,   /*Spectrum obj*/
# endif
                                           LV_GRID_FR(3),   /*Spacer*/
                                           LV_GRID_CONTENT, /*Control box*/
                                           LV_GRID_FR(3),   /*Spacer*/
                                           LV_GRID_CONTENT, /*Handle box*/
                                           LV_GRID_FR(1),   /*Spacer*/
                                           LV_DEMO_MUSIC_HANDLE_SIZE,     /*Spacing*/
                                           LV_GRID_TEMPLATE_LAST
                                          };

    lv_obj_set_grid_dsc_array(cont, grid_cols, grid_rows);
    lv_obj_set_style_grid_row_align(cont, LV_GRID_ALIGN_SPACE_BETWEEN, 0);
    lv_obj_set_grid_cell(title_box, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_CENTER, 2, 1);
    lv_obj_set_grid_cell(icon_box, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_CENTER, 4, 1);
    lv_obj_set_grid_cell(spectrum_obj, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_CENTER, 6, 1);
    lv_obj_set_grid_cell(ctrl_box, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_CENTER, 8, 1);
    lv_obj_set_grid_cell(handle_box, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_CENTER, 10, 1);
#else
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

    lv_obj_set_grid_dsc_array(cont, grid_cols, grid_rows);
    lv_obj_set_style_grid_row_align(cont, LV_GRID_ALIGN_SPACE_BETWEEN, 0);
    lv_obj_set_grid_cell(title_box, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_CENTER, 2, 1);
    lv_obj_set_grid_cell(icon_box, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_CENTER, 4, 1);
    lv_obj_set_grid_cell(ctrl_box, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_CENTER, 6, 1);
    lv_obj_set_grid_cell(handle_box, LV_GRID_ALIGN_STRETCH, 0, 2, LV_GRID_ALIGN_CENTER, 8, 1);
    lv_obj_set_grid_cell(spectrum_obj, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_CENTER, 1, 9);
#endif

#ifdef DO_INTRO_ANIM
    /*Animate in the content after the intro time*/
    lv_anim_t a;

    start_anim = true;

    lv_timer_t * timer = lv_timer_create(stop_start_anim, INTRO_TIME + 6000, NULL);
    lv_timer_set_repeat_count(timer, 1);

    lv_anim_init(&a);

    uint32_t i;
    lv_anim_set_exec_cb(&a, start_anim_cb);
    lv_anim_set_values(&a, LV_MAX(LV_HOR_RES, LV_VER_RES) / 2, 0);
    lv_anim_set_path_cb(&a, lv_anim_path_bounce);
    for(i = 0; i < BAR_CNT; i++) {
        lv_anim_set_delay(&a, INTRO_TIME - 200 + (rnd_array[i] % 500));
        lv_anim_set_duration(&a, 2500 + rnd_array[i] % 500);
        lv_anim_set_var(&a, &start_anim_values[i]);
        lv_anim_start(&a);
    }

    lv_obj_fade_in(title_box, 1000, INTRO_TIME + 1000);
    lv_obj_fade_in(icon_box, 1000, INTRO_TIME + 1000);
    lv_obj_fade_in(ctrl_box, 1000, INTRO_TIME + 1000);
    lv_obj_fade_in(handle_box, 1000, INTRO_TIME + 1000);
    lv_obj_fade_in(album_image_obj, 800, INTRO_TIME + 1000);
    lv_obj_fade_in(spectrum_obj, 0, INTRO_TIME);

    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_var(&a, album_image_obj);
    lv_anim_set_time(&a, 1000);
    lv_anim_set_delay(&a, INTRO_TIME + 1000);
    lv_anim_set_values(&a, 1, LV_SCALE_NONE);
    lv_anim_set_exec_cb(&a, _image_set_scale_anim_cb);
    lv_anim_set_completed_cb(&a, NULL);
    lv_anim_start(&a);
#endif

    lv_obj_update_layout(main_cont);
    track_load(0);

    return main_cont;
}

void _lv_demo_music_album_next(bool next)
{
    uint32_t id = track_id;

    if (inter_pause_timer)
    {
      lv_timer_del(inter_pause_timer);
      inter_pause_timer = NULL;
    }

    if(next) {
        id++;
        if(id >= _lv_demo_music_get_track_count()) id = 0;
    }
    else {
        if(id == 0) {
            id = _lv_demo_music_get_track_count() - 1;
        }
        else {
            id--;
        }
    }

    if(playing) {
        _lv_demo_music_play(id);
    }
    else {
        Mix_HaltMusic();
        track_load(id);
    }
}

static void inter_pause_timeout(lv_timer_t *t)
{
    lv_timer_del(t);
    inter_pause_timer = NULL;
    _lv_demo_music_album_next(true);
}

void _lv_demo_inter_pause_start()
{
    inter_pause_timer = lv_timer_create(inter_pause_timeout, 800, NULL);
}

void _lv_demo_music_play(uint32_t id)
{
    if (playing)
    {
      _lv_demo_music_pause();
      Mix_HaltMusic();
    }
    track_load(id);

    _lv_demo_music_resume();
    Mix_PlayMusic(curMusic, 0);
}

void _lv_demo_music_resume(void)
{
    uint32_t track_len;

    playing = true;
    spectrum_i = spectrum_i_pause;
#if 0
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_values(&a, spectrum_i, spectrum_len - 1);
    lv_anim_set_exec_cb(&a, spectrum_anim_cb);
    lv_anim_set_var(&a, spectrum_obj);
    lv_anim_set_time(&a, ((spectrum_len - spectrum_i) * 1000) / 30);
    lv_anim_set_playback_time(&a, 0);
    lv_anim_set_ready_cb(&a, spectrum_end_cb);
    lv_anim_start(&a);
#endif

    track_len = _lv_demo_music_get_track_length(track_id);
    lv_slider_set_range(slider_obj, 0, track_len);

    lv_obj_add_state(play_obj, LV_STATE_CHECKED);

    lv_obj_invalidate(spectrum_obj);
}

void _lv_demo_music_pause(void)
{
    playing = false;
    spectrum_i_pause = spectrum_i;
    spectrum_i = 0;
    lv_obj_invalidate(spectrum_obj);
#if 0
    lv_anim_del(spectrum_obj, spectrum_anim_cb);
    lv_obj_invalidate(spectrum_obj);
#endif
    lv_image_set_scale(album_image_obj, LV_SCALE_NONE);
    lv_obj_clear_state(play_obj, LV_STATE_CHECKED);
}

void app_spectrum_update(int v)
{
    spectrum_anim_update(v);
    lv_obj_invalidate(spectrum_obj);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static lv_obj_t * create_cont(lv_obj_t * parent)
{
    /*A transparent container in which the player section will be scrolled*/
    main_cont = lv_obj_create(parent);
    lv_obj_remove_flag(main_cont, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(main_cont, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_remove_style_all(main_cont);                            /*Make it transparent*/
    lv_obj_set_size(main_cont, lv_pct(100), lv_pct(100));
    lv_obj_set_scroll_snap_y(main_cont, LV_SCROLL_SNAP_CENTER);    /*Snap the children to the center*/

    /*Create a container for the player*/
    lv_obj_t * player = lv_obj_create(main_cont);
    lv_obj_set_y(player, - LV_DEMO_MUSIC_HANDLE_SIZE);
#if LV_DEMO_MUSIC_SQUARE || LV_DEMO_MUSIC_ROUND
    lv_obj_set_size(player, LV_HOR_RES, 2 * LV_VER_RES + LV_DEMO_MUSIC_HANDLE_SIZE * 2);
#else
    lv_obj_set_size(player, LV_HOR_RES, LV_VER_RES + LV_DEMO_MUSIC_HANDLE_SIZE * 2);
#endif
    lv_obj_remove_flag(player, LV_OBJ_FLAG_SNAPPABLE);

    lv_obj_set_style_bg_color(player, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_border_width(player, 0, 0);
    lv_obj_set_style_pad_all(player, 0, 0);
    lv_obj_set_scroll_dir(player, LV_DIR_VER);

    /* Transparent placeholders below the player container
     * It is used only to snap it to center.*/
    lv_obj_t * placeholder1 = lv_obj_create(main_cont);
    lv_obj_remove_style_all(placeholder1);
    lv_obj_remove_flag(placeholder1, LV_OBJ_FLAG_CLICKABLE);
#if 0
        lv_obj_set_style_bg_color(placeholder1, lv_color_hex(0xff0000), 0);
        lv_obj_set_style_bg_opa(placeholder1, LV_OPA_50, 0);
    lv_obj_add_flag(placeholder1, LV_OBJ_FLAG_GESTURE_BUBBLE);
#endif

    lv_obj_t * placeholder2 = lv_obj_create(main_cont);
    lv_obj_remove_style_all(placeholder2);
    lv_obj_remove_flag(placeholder2, LV_OBJ_FLAG_CLICKABLE);
#if 0
        lv_obj_set_style_bg_color(placeholder2, lv_color_hex(0x00ff00), 0);
        lv_obj_set_style_bg_opa(placeholder2, LV_OPA_50, 0);
#endif

#if LV_DEMO_MUSIC_SQUARE || LV_DEMO_MUSIC_ROUND
    lv_obj_t * placeholder3 = lv_obj_create(main_cont);
    lv_obj_remove_style_all(placeholder3);
    lv_obj_remove_flag(placeholder3, LV_OBJ_FLAG_CLICKABLE);
    //    lv_obj_set_style_bg_color(placeholder3, lv_color_hex(0x0000ff), 0);
    //    lv_obj_set_style_bg_opa(placeholder3, LV_OPA_20, 0);

    lv_obj_set_size(placeholder1, lv_pct(100), LV_VER_RES);
    lv_obj_set_y(placeholder1, 0);

    lv_obj_set_size(placeholder2, lv_pct(100), LV_VER_RES);
    lv_obj_set_y(placeholder2, LV_VER_RES);

    lv_obj_set_size(placeholder3, lv_pct(100),  LV_VER_RES - 2 * LV_DEMO_MUSIC_HANDLE_SIZE);
    lv_obj_set_y(placeholder3, 2 * LV_VER_RES + LV_DEMO_MUSIC_HANDLE_SIZE);
#else
    lv_obj_set_size(placeholder1, lv_pct(100), LV_VER_RES);
    lv_obj_set_y(placeholder1, 0);

    lv_obj_set_size(placeholder2, lv_pct(100),  LV_VER_RES - 2 * LV_DEMO_MUSIC_HANDLE_SIZE);
    lv_obj_set_y(placeholder2, LV_VER_RES + LV_DEMO_MUSIC_HANDLE_SIZE);
#endif

    lv_obj_update_layout(main_cont);

    return player;
}

static void create_wave_images(lv_obj_t * parent)
{
    LV_IMG_DECLARE(img_lv_demo_music_wave_top);
    LV_IMG_DECLARE(img_lv_demo_music_wave_bottom);
    lv_obj_t * wave_top = lv_image_create(parent);
    lv_image_set_src(wave_top, &img_lv_demo_music_wave_top);
    lv_image_set_inner_align(wave_top, LV_IMAGE_ALIGN_TILE);
    lv_obj_set_width(wave_top, LV_HOR_RES);
    lv_obj_align(wave_top, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_add_flag(wave_top, LV_OBJ_FLAG_IGNORE_LAYOUT);

    lv_obj_t * wave_bottom = lv_image_create(parent);
    lv_image_set_src(wave_bottom, &img_lv_demo_music_wave_bottom);
    lv_image_set_inner_align(wave_bottom, LV_IMAGE_ALIGN_TILE);
    lv_obj_set_width(wave_bottom, LV_HOR_RES);
    lv_obj_align(wave_bottom, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(wave_bottom, LV_OBJ_FLAG_IGNORE_LAYOUT);

    LV_IMG_DECLARE(img_lv_demo_music_corner_left);
    LV_IMG_DECLARE(img_lv_demo_music_corner_right);
    lv_obj_t * wave_corner = lv_image_create(parent);
    lv_image_set_src(wave_corner, &img_lv_demo_music_corner_left);
#if LV_DEMO_MUSIC_ROUND == 0
    lv_obj_align(wave_corner, LV_ALIGN_BOTTOM_LEFT, 0, 0);
#else
    lv_obj_align(wave_corner, LV_ALIGN_BOTTOM_LEFT, -LV_HOR_RES / 6, 0);
#endif
    lv_obj_add_flag(wave_corner, LV_OBJ_FLAG_IGNORE_LAYOUT);

    wave_corner = lv_image_create(parent);
    lv_image_set_src(wave_corner, &img_lv_demo_music_corner_right);
#if LV_DEMO_MUSIC_ROUND == 0
    lv_obj_align(wave_corner, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
#else
    lv_obj_align(wave_corner, LV_ALIGN_BOTTOM_RIGHT, LV_HOR_RES / 6, 0);
#endif
    lv_obj_add_flag(wave_corner, LV_OBJ_FLAG_IGNORE_LAYOUT);
}

static lv_obj_t * create_title_box(lv_obj_t * parent)
{
    const GUI_LAYOUT *layout = &GuiLayout;

    /*Create the titles*/
    lv_obj_t * cont = lv_obj_create(parent);
    lv_obj_remove_style_all(cont);
    lv_obj_set_height(cont, LV_SIZE_CONTENT);
    lv_obj_remove_flag(cont, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    title_label = lv_label_create(cont);
    lv_obj_set_style_text_font(title_label, layout->font_large, 0);
    lv_obj_set_style_text_color(title_label, lv_color_hex(0x504d6d), 0);

    lv_label_set_text(title_label, _lv_demo_music_get_title(track_id));
    lv_obj_set_height(title_label, lv_font_get_line_height(layout->font_large) * 3 / 2);

    artist_label = lv_label_create(cont);
    lv_obj_set_style_text_font(artist_label, layout->font_small, 0);
    lv_obj_set_style_text_color(artist_label, lv_color_hex(0x504d6d), 0);
    lv_label_set_text(artist_label, _lv_demo_music_get_artist(track_id));

#if 0
    genre_label = lv_label_create(cont);
    lv_obj_set_style_text_font(genre_label, font_small, 0);
    lv_obj_set_style_text_color(genre_label, lv_color_hex(0x8a86b8), 0);
    lv_label_set_text(genre_label, _lv_demo_music_get_genre(track_id));
#endif

    return cont;
}

static lv_obj_t * create_icon_box(lv_obj_t * parent)
{

    lv_obj_t * cont = lv_obj_create(parent);
    lv_obj_remove_style_all(cont);
    lv_obj_set_height(cont, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * icon;
    LV_IMG_DECLARE(img_lv_demo_music_icon_1);
    LV_IMG_DECLARE(img_lv_demo_music_icon_2);
    LV_IMG_DECLARE(img_lv_demo_music_icon_3);
    LV_IMG_DECLARE(img_lv_demo_music_icon_4);
    icon = lv_image_create(cont);
    lv_image_set_src(icon, &img_lv_demo_music_icon_1);
    icon = lv_image_create(cont);
    lv_image_set_src(icon, &img_lv_demo_music_icon_2);
    icon = lv_image_create(cont);
    lv_image_set_src(icon, &img_lv_demo_music_icon_3);
    icon = lv_image_create(cont);
    lv_image_set_src(icon, &img_lv_demo_music_icon_4);
#if 0
    lv_obj_add_event_cb(icon, menu_click_event_cb, LV_EVENT_CLICKED, NULL);
#endif
    lv_obj_add_flag(icon, LV_OBJ_FLAG_CLICKABLE);

    return cont;
}

lv_obj_t * create_spectrum_obj(lv_obj_t * parent)
{
    /*Create the spectrum visualizer*/
    lv_obj_t * obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
#if LV_DEMO_MUSIC_LARGE
      lv_obj_set_height(obj, 500);
#else
      lv_obj_set_height(obj, 250);
#endif
      lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_add_event_cb(obj, spectrum_draw_event_cb, LV_EVENT_ALL, NULL);
      lv_obj_refresh_ext_draw_size(obj);

      if (album_image_obj == NULL)
      {
        album_image_obj = album_image_create(obj);
      }
    return obj;
}

static void slider_event_cb(lv_event_t *e)
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

    knob_area = slider->right_knob_area;

    ctime = lv_slider_get_value(obj);
    lv_snprintf(buf, sizeof(buf), "%d:%02d", ctime/60, ctime%60);

    lv_point_t label_size;
    lv_txt_get_size(&label_size, buf, LV_FONT_DEFAULT, 0, 0, LV_COORD_MAX, 0);

    lv_area_t label_area;
    label_area.x1 = 0;
    label_area.x2 = label_size.x - 1;
    label_area.y1 = 0;
    label_area.y2 = label_size.y - 1;

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

static lv_obj_t * create_ctrl_box(lv_obj_t * parent, lv_group_t *g)
{
    const GUI_LAYOUT *layout = &GuiLayout;
    static lv_style_t style_focus;

    lv_style_init(&style_focus);
    lv_style_set_img_recolor_opa(&style_focus, LV_OPA_10);
    lv_style_set_img_recolor(&style_focus, lv_color_black());

    /*Create the control box*/
    lv_obj_t * cont = lv_obj_create(parent);
    lv_obj_remove_style_all(cont);
    lv_obj_set_height(cont, LV_SIZE_CONTENT);
#if LV_DEMO_MUSIC_LARGE
    lv_obj_set_style_pad_bottom(cont, 17, 0);
#else
    lv_obj_set_style_pad_bottom(cont, 8, 0);
#endif
    static const lv_coord_t grid_col[] = {LV_GRID_FR(2), LV_GRID_FR(3), LV_GRID_FR(5), LV_GRID_FR(5), LV_GRID_FR(5), LV_GRID_FR(3), LV_GRID_FR(2), LV_GRID_TEMPLATE_LAST};

#if 0
    static const lv_coord_t grid_row[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
#else
    static const lv_coord_t grid_row[] = {LV_GRID_CONTENT, 50, LV_GRID_TEMPLATE_LAST};
#endif
    lv_obj_set_grid_dsc_array(cont, grid_col, grid_row);

    LV_IMG_DECLARE(img_lv_demo_music_btn_loop);
    LV_IMG_DECLARE(img_lv_demo_music_btn_rnd);
    LV_IMG_DECLARE(img_lv_demo_music_btn_next);
    LV_IMG_DECLARE(img_lv_demo_music_btn_prev);
    LV_IMG_DECLARE(img_lv_demo_music_btn_play);
    LV_IMG_DECLARE(img_lv_demo_music_btn_pause);

    lv_obj_t * icon;
    icon = lv_image_create(cont);
    lv_image_set_src(icon, &img_lv_demo_music_btn_rnd);
    lv_obj_set_grid_cell(icon, LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_CENTER, 0, 1);

    icon = lv_image_create(cont);
    lv_image_set_src(icon, &img_lv_demo_music_btn_loop);
    lv_obj_set_grid_cell(icon, LV_GRID_ALIGN_END, 5, 1, LV_GRID_ALIGN_CENTER, 0, 1);

    icon = lv_image_create(cont);
    lv_image_set_src(icon, &img_lv_demo_music_btn_prev);
    lv_obj_set_grid_cell(icon, LV_GRID_ALIGN_CENTER, 2, 1, LV_GRID_ALIGN_CENTER, 0, 1);
    lv_obj_add_event_cb(icon, prev_click_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(icon, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_style(icon, &style_focus, LV_STATE_FOCUS_KEY);
    lv_group_add_obj(g, icon);

    play_obj = lv_imagebutton_create(cont);
    lv_imagebutton_set_src(play_obj, LV_IMAGEBUTTON_STATE_RELEASED, NULL, &img_lv_demo_music_btn_play, NULL);
    lv_imagebutton_set_src(play_obj, LV_IMAGEBUTTON_STATE_CHECKED_RELEASED, NULL, &img_lv_demo_music_btn_pause, NULL);
    lv_obj_add_style(play_obj, &style_focus, LV_STATE_FOCUS_KEY);
    //lv_image_set_zoom(play_obj, LV_IMG_ZOOM_NONE);
    lv_obj_add_flag(play_obj, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_set_grid_cell(play_obj, LV_GRID_ALIGN_CENTER, 3, 1, LV_GRID_ALIGN_CENTER, 0, 1);

    lv_obj_add_event_cb(play_obj, play_event_click_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(play_obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_width(play_obj, img_lv_demo_music_btn_play.header.w);
    lv_group_add_obj(g, play_obj);

    icon = lv_image_create(cont);
    lv_image_set_src(icon, &img_lv_demo_music_btn_next);
    lv_obj_set_grid_cell(icon, LV_GRID_ALIGN_CENTER, 4, 1, LV_GRID_ALIGN_CENTER, 0, 1);
    lv_obj_add_event_cb(icon, next_click_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(icon, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_style(icon, &style_focus, LV_STATE_FOCUS_KEY);
    lv_group_add_obj(g, icon);

    lv_group_focus_obj(play_obj);	// Set focus on Play button

    LV_IMG_DECLARE(img_lv_demo_music_slider_knob);
    slider_obj = lv_slider_create(cont);
    lv_obj_set_style_anim_time(slider_obj, 100, 0);
    lv_obj_add_flag(slider_obj, LV_OBJ_FLAG_CLICKABLE); /*No input from the slider*/
    lv_obj_remove_flag(slider_obj, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_add_event_cb(slider_obj, slider_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_refresh_ext_draw_size(slider_obj);

#if LV_DEMO_MUSIC_LARGE == 0
    lv_obj_set_height(slider_obj, 3);
#else
    lv_obj_set_height(slider_obj, 6);
#endif
    lv_obj_set_grid_cell(slider_obj, LV_GRID_ALIGN_STRETCH, 1, 4, LV_GRID_ALIGN_CENTER, 1, 1);

    lv_obj_set_style_bg_image_src(slider_obj, &img_lv_demo_music_slider_knob, LV_PART_KNOB);
    lv_obj_set_style_bg_opa(slider_obj, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider_obj, 20, LV_PART_KNOB);
    lv_obj_set_style_bg_grad_dir(slider_obj, LV_GRAD_DIR_HOR, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider_obj, lv_color_hex(0x569af8), LV_PART_INDICATOR);
    lv_obj_set_style_bg_grad_color(slider_obj, lv_color_hex(0xa666f1), LV_PART_INDICATOR);
    lv_obj_set_style_outline_width(slider_obj, 0, 0);

    time_obj = lv_label_create(cont);
    lv_obj_set_style_text_font(time_obj, layout->font_small, 0);
    lv_obj_set_style_text_color(time_obj, lv_color_hex(0x8a86b8), 0);
    lv_label_set_text(time_obj, "0:00");
    lv_obj_set_grid_cell(time_obj, LV_GRID_ALIGN_END, 5, 1, LV_GRID_ALIGN_CENTER, 1, 1);

    return cont;
}

static lv_obj_t * create_handle(lv_obj_t * parent)
{
    const GUI_LAYOUT *layout = &GuiLayout;
    lv_obj_t * cont = lv_obj_create(parent);
    lv_obj_remove_style_all(cont);

    lv_obj_set_size(cont, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(cont, 8, 0);

    /*A handle to scroll to the track list*/
    lv_obj_t * handle_label = lv_label_create(cont);
    lv_label_set_text_static(handle_label, "ALL TRACKS");
    lv_obj_set_style_text_font(handle_label, layout->font_small, 0);
    lv_obj_set_style_text_color(handle_label, lv_color_hex(0x8a86b8), 0);

    lv_obj_t * handle_rect = lv_obj_create(cont);
#if LV_DEMO_MUSIC_LARGE
    lv_obj_set_size(handle_rect, 40, 3);
#else
    lv_obj_set_size(handle_rect, 20, 2);
#endif

    lv_obj_set_style_bg_color(handle_rect, lv_color_hex(0x8a86b8), 0);
    lv_obj_set_style_border_width(handle_rect, 0, 0);

    return cont;
}

/*
 * If title and/or artist string is long enough, set LV_LABEL_LONG_SCROLL_CIRCULAR mode.
 */
void set_scroll_labeltext(lv_obj_t *label, const char *str, const lv_font_t *font)
{
    lv_point_t size;
    lv_draw_label_dsc_t label_draw_dsc;

    lv_draw_label_dsc_init(&label_draw_dsc);
    lv_txt_get_size(&size, str, font,
       label_draw_dsc.letter_space, label_draw_dsc.line_space, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    if (size.x > DISP_HOR_RES/2 - 20)
    {
       /* To make scroll mode effective, we need to set proper object width. */
       lv_obj_set_style_width(label, LV_PART_MAIN, DISP_HOR_RES/2 - 20);
       lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
       lv_obj_set_width(label, DISP_HOR_RES/2 -20);
    }
    else
    {
       /* Use content width. */
       lv_obj_set_width(label, LV_SIZE_CONTENT);
       lv_obj_set_style_width(label, LV_PART_MAIN, LV_SIZE_CONTENT);
    }

    lv_label_set_text(label, str);
}

static void track_load(uint32_t id)
{
    const GUI_LAYOUT *layout = &GuiLayout;
    const char *title_str;
    const char *artist_str;
    uint32_t track_len;

    spectrum_i = 0;
    spectrum_i_pause = 0;
    lv_slider_set_value(slider_obj, 0, LV_ANIM_OFF);
    lv_label_set_text(time_obj, "0:00");

    if(id == track_id) return;
    bool next = false;

    if (track_id >= 0)
      _lv_demo_music_list_btn_check(getMusicList(), track_id, false);

    track_id = id;

    _lv_demo_music_list_btn_check(getMusicList(), id, true);

    title_str =  _lv_demo_music_get_title(track_id);
    set_scroll_labeltext(title_label, title_str, layout->font_large);

    artist_str =  _lv_demo_music_get_artist(track_id);
    set_scroll_labeltext(artist_label, artist_str, layout->font_small);

    track_len = _lv_demo_music_get_track_length(track_id);
    lv_label_set_text_fmt(time_obj, "%"LV_PRIu32":%02"LV_PRIu32, track_len / 60, track_len % 60);

#if 0
    lv_label_set_text(genre_label, _lv_demo_music_get_genre(track_id));
#endif

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, album_image_obj);
    lv_anim_set_values(&a, lv_obj_get_style_img_opa(album_image_obj, 0), LV_OPA_TRANSP);
    lv_anim_set_exec_cb(&a, album_fade_anim_cb);
    lv_anim_set_time(&a, 500);
    lv_anim_start(&a);

    lv_anim_init(&a);
    lv_anim_set_var(&a, album_image_obj);
    lv_anim_set_time(&a, 500);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
#if LV_DEMO_MUSIC_LANDSCAPE
    if(next) {
        lv_anim_set_values(&a, 0, - LV_HOR_RES / 7);
    }
    else {
        lv_anim_set_values(&a, 0, LV_HOR_RES / 7);
    }
#else
    if(next) {
        lv_anim_set_values(&a, 0, - LV_HOR_RES / 2);
    }
    else {
        lv_anim_set_values(&a, 0, LV_HOR_RES / 2);
    }
#endif
    lv_anim_set_exec_cb(&a, _obj_set_x_anim_cb);
    lv_anim_set_completed_cb(&a, lv_obj_delete_anim_completed_cb);
    lv_anim_start(&a);

    lv_anim_set_path_cb(&a, lv_anim_path_linear);
    lv_anim_set_var(&a, album_image_obj);
    lv_anim_set_time(&a, 500);
    lv_anim_set_values(&a, LV_SCALE_NONE, LV_SCALE_NONE / 2);
    lv_anim_set_exec_cb(&a, _image_set_scale_anim_cb);
    lv_anim_set_completed_cb(&a, NULL);
    lv_anim_start(&a);

    album_image_obj = album_image_create(spectrum_obj);

    lv_anim_set_path_cb(&a, lv_anim_path_overshoot);
    lv_anim_set_var(&a, album_image_obj);
    lv_anim_set_time(&a, 500);
    lv_anim_set_delay(&a, 100);
    lv_anim_set_values(&a, LV_SCALE_NONE / 4, LV_SCALE_NONE);
    lv_anim_set_exec_cb(&a, _image_set_scale_anim_cb);
    lv_anim_set_completed_cb(&a, NULL);
    lv_anim_start(&a);

    lv_anim_init(&a);
    lv_anim_set_var(&a, album_image_obj);
    lv_anim_set_values(&a, 0, LV_OPA_COVER);
    lv_anim_set_exec_cb(&a, album_fade_anim_cb);
    lv_anim_set_time(&a, 500);
    lv_anim_set_delay(&a, 100);
    lv_anim_start(&a);

    if (curMusic) free(curMusic);
    curMusic = Mix_LoadMUS(_lv_demo_music_get_path(track_id));
}

int32_t get_cos(int32_t deg, int32_t a)
{
    int32_t r = (lv_trigo_cos(deg) * a);

    r += LV_TRIGO_SIN_MAX / 2;
    return r >> LV_TRIGO_SHIFT;
}

int32_t get_sin(int32_t deg, int32_t a)
{
    int32_t r = lv_trigo_sin(deg) * a;

    r += LV_TRIGO_SIN_MAX / 2;
    return r >> LV_TRIGO_SHIFT;

}

extern int binvals[];

static void spectrum_draw_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_REFR_EXT_DRAW_SIZE) {
#if LV_DEMO_MUSIC_LANDSCAPE
        lv_event_set_ext_draw_size(e, LV_HOR_RES);
#else
        lv_event_set_ext_draw_size(e, LV_VER_RES);
#endif
    }
    else if(code == LV_EVENT_COVER_CHECK) {
        lv_event_set_cover_res(e, LV_COVER_RES_NOT_COVER);
    }
    else if(code == LV_EVENT_DRAW_MAIN_BEGIN) {
        lv_obj_t * obj = lv_event_get_target(e);
        lv_layer_t * layer = lv_event_get_layer(e);

        lv_opa_t opa = lv_obj_get_style_opa_recursive(obj, LV_PART_MAIN);
        if(opa < LV_OPA_MIN) return;

        lv_point_t center;
        lv_area_t obj_coords;
        lv_obj_get_coords(obj, &obj_coords);
        center.x = obj_coords.x1 + lv_obj_get_width(obj) / 2;
        center.y = obj_coords.y1 + lv_obj_get_height(obj) / 2;

        lv_draw_triangle_dsc_t draw_dsc;
        lv_draw_triangle_dsc_init(&draw_dsc);
        draw_dsc.bg_opa = LV_OPA_COVER;

        uint16_t r[64];
        uint32_t i;

        for(i = 0; i < BAR_CNT; i++) {
            r[i] = BAR_REST_RADIUS;
        }

        uint32_t s;
        for(s = 0; s < 4; s++) {
            uint32_t f;
            uint32_t band_w = 0;    /*Real number of bars in this band.*/
            switch(s) {
                case 0:
                    band_w = 20;
                    break;
                case 1:
                    band_w = 8;
                    break;
                case 2:
                    band_w = 4;
                    break;
                case 3:
                    band_w = 2;
                    break;
            }

            /* Add "side bars" with cosine characteristic.*/
            for(f = 0; f < band_w; f++) {
#if 0
                uint32_t ampl_main = spectrum[spectrum_i][s];
#else
                uint32_t ampl_main = fft_getband(s);
#endif
                int32_t ampl_mod = get_cos(f * 360 / band_w + 180, 180) + 180;
                int32_t t = BAR_PER_BAND_CNT * s - band_w / 2 + f;
                if(t < 0) t = BAR_CNT + t;
                if(t >= BAR_CNT) t = t - BAR_CNT;
                r[t] += (ampl_main * ampl_mod) >> 9;
            }
        }

        uint32_t amax = 20;
        int32_t animv = spectrum_i - 0;
        if(animv > amax) animv = amax;
        for(i = 0; i < BAR_CNT; i++) {
            uint32_t deg_space = 1;
            uint32_t deg = i * DEG_STEP + 90;
            uint32_t j = (i + bar_rot + rnd_array[bar_ofs % 10]) % BAR_CNT;
            uint32_t k = (i + bar_rot + rnd_array[(bar_ofs + 1) % 10]) % BAR_CNT;

            uint32_t v;
            if (start_anim) {
                v = BAR_REST_RADIUS + start_anim_values[i];
            }
            else {
                v = (r[k] * animv + r[j] * (amax - animv)) / amax;
            }

            if(v < BAR_COLOR1_STOP) draw_dsc.bg_color = BAR_COLOR1;
            else if(v > BAR_COLOR3_STOP) draw_dsc.bg_color = BAR_COLOR3;
            else if(v > BAR_COLOR2_STOP) draw_dsc.bg_color = lv_color_mix(BAR_COLOR3, BAR_COLOR2,
                                                                              ((v - BAR_COLOR2_STOP) * 255) / (BAR_COLOR3_STOP - BAR_COLOR2_STOP));
            else draw_dsc.bg_color = lv_color_mix(BAR_COLOR2, BAR_COLOR1,
                                                      ((v - BAR_COLOR1_STOP) * 255) / (BAR_COLOR2_STOP - BAR_COLOR1_STOP));

            uint32_t di = deg + deg_space;

            int32_t x1_out = get_cos(di, v);
            draw_dsc.p[0].x = center.x + x1_out;
            draw_dsc.p[0].y = center.y + get_sin(di, v);

            di += DEG_STEP - deg_space * 2;

            int32_t x2_out = get_cos(di, v);
            draw_dsc.p[1].x = center.x + x2_out;
            draw_dsc.p[1].y = center.y + get_sin(di, v);

            int32_t x2_in = get_cos(di, 0);
            draw_dsc.p[2].x = center.x + x2_in;
            draw_dsc.p[2].y = center.y + get_sin(di, 0);
            lv_draw_triangle(layer, &draw_dsc);

            draw_dsc.p[0].x = center.x - x1_out;
            draw_dsc.p[1].x = center.x - x2_out;
            draw_dsc.p[2].x = center.x - x2_in;
            lv_draw_triangle(layer, &draw_dsc);
        }
    }
#if 0
    else if(code == LV_EVENT_DELETE) {
        lv_anim_delete(NULL, start_anim_cb);
        lv_anim_delete(NULL, spectrum_anim_cb);
        if(start_anim && stop_start_anim_timer) lv_timer_delete(stop_start_anim_timer);
    }
#endif
}

void spectrum_anim_update(int v)
{
    int b0;
#if 0
    lv_obj_t * obj = a;
    if(start_anim) {
        lv_obj_invalidate(obj);
        return;
    }
#endif

    spectrum_i = v;
#if 0
    lv_obj_invalidate(obj);
#endif

    static uint32_t bass_cnt = 0;
    static int32_t last_bass = -1000;
    static int32_t dir = 1;
    if(fft_getband(0) > 12) {
        if(spectrum_i - last_bass > 5) {
            bass_cnt++;
            last_bass = spectrum_i;
            if(bass_cnt >= 2) {
                bass_cnt = 0;
                bar_ofs++;
            }
        }
    }
    b0 = fft_getband(0);

    if (b0 < 4) bar_rot += dir;

    if (b0 > 5)
      lv_image_set_scale(album_image_obj, LV_SCALE_NONE + 10);
    else
      lv_image_set_scale(album_image_obj, LV_SCALE_NONE);
}

#ifdef DO_INTRO_ANIM
static void start_anim_cb(void * var, int32_t v)
{
    lv_coord_t * av = var;
    *av = v;
    lv_obj_invalidate(spectrum_obj);
}
#endif

static lv_image_dsc_t imgdesc;

static lv_obj_t * album_image_create(lv_obj_t * parent)
{

    int id;
    COVER_INFO *cfp;

    id = track_id;
    if (id < 0) id = 0;

    lv_obj_t * img;

    img = lv_image_create(parent);

    cfp = track_cover(id);
    if (cfp)
    {
      lv_image_dsc_t *desc = (lv_image_dsc_t *)(cfp->faddr);

      imgdesc = *desc;
      imgdesc.data_size = desc->header.w * desc->header.h * 3;
      imgdesc.data = cfp->faddr + 12;
      imgdesc.header.flags = 0;

      lv_image_set_src(img, &imgdesc);
    }
    lv_image_set_antialias(img, false);
    lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(img, album_gesture_event_cb, LV_EVENT_GESTURE, NULL);
    lv_obj_remove_flag(img, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_flag(img, LV_OBJ_FLAG_CLICKABLE);

    return img;

}

static void album_gesture_event_cb(lv_event_t * e)
{
    UNUSED(e);
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if(dir == LV_DIR_LEFT) _lv_demo_music_album_next(true);
    if(dir == LV_DIR_RIGHT) _lv_demo_music_album_next(false);
}

static void play_event_click_cb(lv_event_t * e)
{
    lv_obj_t * obj = lv_event_get_target(e);
    if(lv_obj_has_state(obj, LV_STATE_CHECKED)) {
        _lv_demo_music_resume();
        Mix_ResumeMusic(curMusic);
    }
    else {
        _lv_demo_music_pause();
        Mix_PauseMusic(curMusic);
    }
}

static void menu_click_event_cb(lv_event_t * e)
{
    UNUSED(e);
#if 0
  if (playing)
  {
      _lv_demo_music_pause();
  }
  Mix_HaltMusic();
#endif

  postGuiEventMessage(GUIEV_MPLAYER_DONE, 0, NULL, NULL);
}

static void prev_click_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED) {
        _lv_demo_music_album_next(false);
    }
}

static void next_click_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED) {
        _lv_demo_music_album_next(true);
    }
}

void app_psec_update(int tv)
{
    if (slider_obj)
      lv_slider_set_value(slider_obj, tv, LV_ANIM_ON);
}

#ifdef DO_INTRO_ANIM
static void stop_start_anim(lv_timer_t * t)
{
    LV_UNUSED(t);
    start_anim = false;
    lv_obj_refresh_ext_draw_size(spectrum_obj);
}
#endif

static void album_fade_anim_cb(void * var, int32_t v)
{
    lv_obj_set_style_img_opa(var, v, 0);
}
