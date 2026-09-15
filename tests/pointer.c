#include <wayland-client.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "virtual-pointer.h"
static struct zwlr_virtual_pointer_manager_v1 *manager;
static void global(void *data, struct wl_registry *r, uint32_t name, const char *iface, uint32_t version) {
  if (!strcmp(iface, zwlr_virtual_pointer_manager_v1_interface.name))
    manager=wl_registry_bind(r,name,&zwlr_virtual_pointer_manager_v1_interface,1);
}
static void removed(void *data, struct wl_registry *r, uint32_t name) {}
static const struct wl_registry_listener listener={global,removed};
static uint32_t now_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return t.tv_sec*1000+t.tv_nsec/1000000; }
int main(int argc,char **argv) {
  if(argc!=6) return 2;
  struct wl_display *display=wl_display_connect(argv[1]);
  if(!display) return 3;
  struct wl_registry *registry=wl_display_get_registry(display);
  wl_registry_add_listener(registry,&listener,NULL);
  wl_display_roundtrip(display);
  if(!manager) return 4;
  struct zwlr_virtual_pointer_v1 *pointer=zwlr_virtual_pointer_manager_v1_create_virtual_pointer(manager,NULL);
  wl_display_roundtrip(display);
  zwlr_virtual_pointer_v1_motion_absolute(pointer,now_ms(),atoi(argv[2]),atoi(argv[3]),atoi(argv[4]),atoi(argv[5]));
  zwlr_virtual_pointer_v1_frame(pointer);
  wl_display_roundtrip(display);
  usleep(100000);
  zwlr_virtual_pointer_v1_button(pointer,now_ms(),272,WL_POINTER_BUTTON_STATE_PRESSED);
  zwlr_virtual_pointer_v1_frame(pointer);
  wl_display_roundtrip(display);
  usleep(100000);
  zwlr_virtual_pointer_v1_button(pointer,now_ms(),272,WL_POINTER_BUTTON_STATE_RELEASED);
  zwlr_virtual_pointer_v1_frame(pointer);
  wl_display_roundtrip(display);
  zwlr_virtual_pointer_v1_destroy(pointer);
  zwlr_virtual_pointer_manager_v1_destroy(manager);
  wl_registry_destroy(registry);
  wl_display_disconnect(display);
  return 0;
}
