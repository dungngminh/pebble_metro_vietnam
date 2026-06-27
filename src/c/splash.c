#include "splash.h"

static Window *s_window;
static Layer *s_layer;
static GBitmap *s_train;
static Animation *s_anim;
static SplashDoneHandler s_done;
static int16_t s_x;      // train x position (animated)
static int16_t s_w;      // cached width for the run

static void update_proc(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  int16_t ty = b.size.h / 2 + 16;

  // Rail the train runs along.
  graphics_context_set_fill_color(ctx, GColorDarkGray);
  graphics_fill_rect(ctx, GRect(0, ty, b.size.w, 2), 0, GCornerNone);

  // Title + tagline.
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, "Metro VN", fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK),
                     GRect(0, b.size.h / 2 - 54, b.size.w, 38), GTextOverflowModeFill,
                     GTextAlignmentCenter, NULL);
  graphics_context_set_text_color(ctx, GColorLightGray);
  graphics_draw_text(ctx, "Vietnam metro tracker", fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(0, b.size.h / 2 - 18, b.size.w, 18), GTextOverflowModeFill,
                     GTextAlignmentCenter, NULL);

  // The running train.
  if (s_train) {
    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    graphics_draw_bitmap_in_rect(ctx, s_train, GRect(s_x, ty - 19, 25, 25));
  }
}

static void anim_update(Animation *anim, const AnimationProgress progress) {
  s_x = (int16_t)(-30 + (((int32_t)(s_w + 60)) * progress) / ANIMATION_NORMALIZED_MAX);
  if (s_layer) layer_mark_dirty(s_layer);
}

static const AnimationImplementation s_impl = { .update = anim_update };

// Deferred so we leave the window outside the animation's own teardown.
static void do_finish(void *ctx) {
  SplashDoneHandler d = s_done;
  if (s_window) window_stack_remove(s_window, false);
  if (d) d();
}

static void anim_stopped(Animation *anim, bool finished, void *ctx) {
  app_timer_register(0, do_finish, NULL);
}

static void window_load(Window *window) {
  window_set_background_color(window, GColorBlack);
  Layer *root = window_get_root_layer(window);
  GRect b = layer_get_bounds(root);
  s_w = b.size.w;
  s_x = -30;

  s_train = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_TRAIN);
  s_layer = layer_create(b);
  layer_set_update_proc(s_layer, update_proc);
  layer_add_child(root, s_layer);

  s_anim = animation_create();
  animation_set_implementation(s_anim, &s_impl);
  animation_set_duration(s_anim, 1300);
  animation_set_curve(s_anim, AnimationCurveEaseInOut);
  animation_set_handlers(s_anim, (AnimationHandlers){ .stopped = anim_stopped }, NULL);
  animation_schedule(s_anim);
}

static void window_unload(Window *window) {
  if (s_train) { gbitmap_destroy(s_train); s_train = NULL; }
  layer_destroy(s_layer);
  s_layer = NULL;
  window_destroy(s_window);
  s_window = NULL;
}

void splash_push(SplashDoneHandler done) {
  s_done = done;
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, false);
}
