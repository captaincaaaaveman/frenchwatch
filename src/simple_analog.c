#include "simple_analog.h"
#include <math.h>
#include "pebble.h"
#define KEY_TEMPERATURE 0
#define KEY_CONDITIONS 1

static Window *s_window;
static Layer *s_simple_bg_layer, *s_date_layer, *s_hands_layer;
static TextLayer *s_day_label, *s_num_label;

static TextLayer *s_stock_layer;

// Store incoming information
static char temperature_buffer[8];
static char conditions_buffer[32];
static char weather_layer_buffer[32];

static GPath *s_tick_paths[NUM_CLOCK_TICKS];
static GPath *s_minute_arrow, *s_hour_arrow;
static char s_num_buffer[6], s_day_buffer[7];

static BitmapLayer *s_background_layer;
static GBitmap *s_background_bitmap;

static void bg_update_proc(Layer *layer, GContext *ctx) {
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
  graphics_context_set_fill_color(ctx, GColorWhite);
  for (int i = 0; i < NUM_CLOCK_TICKS; ++i) {
    const int x_offset = PBL_IF_ROUND_ELSE(18, 0);
    const int y_offset = PBL_IF_ROUND_ELSE(6, 0);
    gpath_move_to(s_tick_paths[i], GPoint(x_offset, y_offset));
    gpath_draw_filled(ctx, s_tick_paths[i]);
  }
}

void get_decimal_time(tm *t) {
  unsigned int decimal_time;
  int decimal_hour;
  int decimal_minute;
  int decimal_seconds;

  time_t now = time(NULL);
  struct tm *archaic_time = localtime(&now);
  decimal_time = floor(((archaic_time->tm_hour * 3600) + (archaic_time->tm_min * 60) + archaic_time->tm_sec) / .864);

  decimal_hour = floor(decimal_time / 10000);
  decimal_time = decimal_time - (decimal_hour * 10000);

  decimal_minute = floor(decimal_time / 100);
  decimal_time = decimal_time - (decimal_minute * 100);
  
  decimal_seconds = decimal_time;

  t->tm_sec = decimal_seconds;
  t->tm_min = decimal_minute;
  t->tm_hour = decimal_hour;
  t->tm_mday = archaic_time->tm_mday;
  t->tm_mon = archaic_time->tm_mon;
  t->tm_year = archaic_time->tm_year;
  t->tm_wday = archaic_time->tm_wday;
  t->tm_yday = archaic_time->tm_yday;
  t->tm_isdst = archaic_time->tm_isdst;
}

static void hands_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GPoint center = grect_center_point(&bounds);

  const int16_t second_hand_length = PBL_IF_ROUND_ELSE((bounds.size.w / 2) - 19, bounds.size.w / 2);

  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  
  get_decimal_time(t);
  
  int32_t second_angle = TRIG_MAX_ANGLE * ( t->tm_sec / 100.0f) ;
  
//   printf( "ratio: %f", (seconds_through_the_day*100) / seconds_in_the_day );
  GPoint second_hand = {
    .x = (int16_t)(sin_lookup(second_angle) * (int32_t)second_hand_length / TRIG_MAX_RATIO) + center.x,
    .y = (int16_t)(-cos_lookup(second_angle) * (int32_t)second_hand_length / TRIG_MAX_RATIO) + center.y,
  };

  // second hand
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_draw_line(ctx, second_hand, center);

  // minute/hour hand
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_context_set_stroke_color(ctx, GColorBlack);

  gpath_rotate_to(s_minute_arrow, TRIG_MAX_ANGLE * t->tm_min / 100);
  gpath_draw_filled(ctx, s_minute_arrow);
  gpath_draw_outline(ctx, s_minute_arrow);

//  printf( "hour: %d", t->tm_hour );
//  printf( "min: %d", t->tm_min );
//  printf( "total: %f", (((t->tm_hour % 10) * 100.0f) + t->tm_min) );
  
  gpath_rotate_to(s_hour_arrow, (TRIG_MAX_ANGLE * (((t->tm_hour % 10) * 100.0f) + t->tm_min)) / (1000.0f));
  gpath_draw_filled(ctx, s_hour_arrow);
  gpath_draw_outline(ctx, s_hour_arrow);

  // dot in the middle
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(bounds.size.w / 2 - 1, bounds.size.h / 2 - 1, 3, 3), 0, GCornerNone);
}

static void date_update_proc(Layer *layer, GContext *ctx) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);

  strftime(s_day_buffer, sizeof(s_day_buffer), "(%H%M)", t);
  text_layer_set_text(s_num_label, s_day_buffer);

  get_decimal_time(t);
  
  strftime(s_num_buffer, sizeof(s_num_buffer), "%H%M", t);
  text_layer_set_text(s_day_label, s_num_buffer);
}

static void handle_second_tick(struct tm *tick_time, TimeUnits units_changed) {
  layer_mark_dirty(window_get_root_layer(s_window));

// Get weather update every 5 minutes
if(tick_time->tm_min % 5 == 0) {
  // Begin dictionary
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);

  // Add a key-value pair
  dict_write_uint8(iter, 0, 0);

  // Send the message!
  app_message_outbox_send();
}
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_simple_bg_layer = layer_create(bounds);
  layer_set_update_proc(s_simple_bg_layer, bg_update_proc); 
  layer_add_child(window_layer, s_simple_bg_layer);

  s_date_layer = layer_create(bounds);
  layer_set_update_proc(s_date_layer, date_update_proc);



  // Create GBitmap
  s_background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_BACKGROUND_CLOCKFACE);

  // Create BitmapLayer to display the GBitmap
  s_background_layer = bitmap_layer_create(bounds);

  // Set the bitmap onto the layer and add to the window
  bitmap_layer_set_bitmap(s_background_layer, s_background_bitmap);
  layer_add_child(window_layer, bitmap_layer_get_layer(s_background_layer));
  layer_add_child(window_layer, s_date_layer);

  
  // Create temperature Layer
s_stock_layer = text_layer_create(
    GRect(0, PBL_IF_ROUND_ELSE(35, 35), bounds.size.w, 25));

// Style the text
text_layer_set_background_color(s_stock_layer, GColorClear);
text_layer_set_text_color(s_stock_layer, GColorWhite);
text_layer_set_font(s_stock_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
text_layer_set_text_alignment(s_stock_layer, GTextAlignmentCenter);
text_layer_set_text(s_stock_layer, "Loading...");

layer_add_child(window_layer, text_layer_get_layer(s_stock_layer));
  
  
  s_day_label = text_layer_create(PBL_IF_ROUND_ELSE(
    GRect(53, 114, 67, 20),
    GRect(36, 114, 67, 20)));

//  text_layer_set_text(s_day_label, s_day_buffer);
  text_layer_set_background_color(s_day_label, GColorBlack);
  text_layer_set_text_color(s_day_label, GColorWhite);
  text_layer_set_font(s_day_label, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  layer_add_child(s_date_layer, text_layer_get_layer(s_day_label) );

  s_hands_layer = layer_create(bounds);
  layer_set_update_proc(s_hands_layer, hands_update_proc);
  layer_add_child(window_layer, s_hands_layer);

//   layer_add_child(s_date_layer, text_layer_get_layer(s_day_label));

   s_num_label = text_layer_create(PBL_IF_ROUND_ELSE(
     GRect(90, 114, 45, 20),
     GRect(73, 114, 45, 20)));
  text_layer_set_text(s_num_label, s_num_buffer);
  text_layer_set_background_color(s_num_label, GColorBlack);
  text_layer_set_text_color(s_num_label, GColorWhite);
  text_layer_set_font(s_num_label, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));

  layer_add_child(s_date_layer, text_layer_get_layer(s_num_label));


}

static void window_unload(Window *window) {
  layer_destroy(s_simple_bg_layer);
  layer_destroy(s_date_layer);

  text_layer_destroy(s_day_label);
  text_layer_destroy(s_num_label);
  text_layer_destroy(s_stock_layer);

  layer_destroy(s_hands_layer);
  
  // Destroy GBitmap
  gbitmap_destroy(s_background_bitmap);

  // Destroy BitmapLayer
  bitmap_layer_destroy(s_background_layer);

}

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  
  // Read tuples for data
Tuple *temp_tuple = dict_find(iterator, KEY_TEMPERATURE);
Tuple *conditions_tuple = dict_find(iterator, KEY_CONDITIONS);

// If all data is available, use it
if(temp_tuple && conditions_tuple) {
  snprintf(temperature_buffer, sizeof(temperature_buffer), "%s", temp_tuple->value->cstring);
  snprintf(conditions_buffer, sizeof(conditions_buffer), "%s", conditions_tuple->value->cstring);
  
  // Assemble full string and display
  snprintf(weather_layer_buffer, sizeof(weather_layer_buffer), "%s, %s", temperature_buffer, conditions_buffer);
  text_layer_set_text(s_stock_layer, weather_layer_buffer);
//   text_layer_set_text(s_stock_layer, conditions_buffer);
}


}
static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}



static void init() {
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  s_day_buffer[0] = '\0';
  s_num_buffer[0] = '\0';

  // init hand paths
  s_minute_arrow = gpath_create(&MINUTE_HAND_POINTS);
  s_hour_arrow = gpath_create(&HOUR_HAND_POINTS);

  Layer *window_layer = window_get_root_layer(s_window);
  GRect bounds = layer_get_bounds(window_layer);
  GPoint center = grect_center_point(&bounds);
  gpath_move_to(s_minute_arrow, center);
  gpath_move_to(s_hour_arrow, center);

  for (int i = 0; i < NUM_CLOCK_TICKS; ++i) {
    s_tick_paths[i] = gpath_create(&ANALOG_BG_POINTS[i]);
  }

  tick_timer_service_subscribe(SECOND_UNIT, handle_second_tick);

  // Register callbacks
  app_message_register_inbox_received(inbox_received_callback);
  
  // Open AppMessage
  const int inbox_size = 128;
  const int outbox_size = 128;
  app_message_open(inbox_size, outbox_size);

  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);
}


static void deinit() {
  gpath_destroy(s_minute_arrow);
  gpath_destroy(s_hour_arrow);

  for (int i = 0; i < NUM_CLOCK_TICKS; ++i) {
    gpath_destroy(s_tick_paths[i]);
  }

  tick_timer_service_unsubscribe();
  window_destroy(s_window);
}

int main() {
  init();
  app_event_loop();
  deinit();
}
