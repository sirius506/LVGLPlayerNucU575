#ifndef A2DP_LAYER_H
#define A2DP_LAYER_H
#include "btapi.h"

void app_ppos_update(MIX_INFO *mixInfo);
lv_obj_t * a2dp_player_create(A2DP_SCREEN *a2dps, lv_obj_t * parent, lv_group_t *g);
void show_a2dp_buttons();
void hide_a2dp_buttons(A2DP_SCREEN *a2dps);
void set_playbtn_state(lv_imagebutton_state_t new_state);
void set_scroll_labeltext(lv_obj_t *label, const char *str, const lv_font_t *font);
#endif
