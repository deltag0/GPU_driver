#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "drm.h"
#include "drm/drm_fourcc.h"
#include "drm_mode.h"

#include "SDL_surface.h"
#include "SDL_timer.h"
#include "SDL_video.h"
#include <SDL2/SDL.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

#define MAX_PLANES 4

int main() {
  int fd = open("/dev/dri/card2", O_RDWR);

  if (fd < 0) {
    perror("open");
    printf("Could not a get a handle for the GPU\n");
    return 1;
  }

  struct drm_mode_create_dumb created_scanout;
  struct drm_mode_destroy_dumb destroy_scanout;
  struct drm_mode_map_dumb mapping;
  void *map;

  uint32_t fb_id;
  int ret;

  memset(&created_scanout, 0, sizeof(created_scanout));
  memset(&destroy_scanout, 0, sizeof(destroy_scanout));

  created_scanout.height = 1080;
  created_scanout.width = 1920;
  created_scanout.bpp = 32;
  ret = drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &created_scanout);
  if (ret) {
    printf("Creating BO failed\n");
    return 1;
  }
  destroy_scanout.handle = created_scanout.handle;
  uint32_t handles[MAX_PLANES] = {created_scanout.handle, 0, 0, 0};
  uint32_t pitches[MAX_PLANES] = {created_scanout.pitch, 0, 0, 0};
  uint32_t offsets[MAX_PLANES] = {0, 0, 0, 0};

  ret = drmModeAddFB2(fd, created_scanout.width, created_scanout.height,
                      DRM_FORMAT_RGBX8888, handles, pitches, offsets, &fb_id,
                      DRM_MODE_SCALE_NONE);
  if (ret) {
    printf("Adding FB failed\n");
    return 1;
  }

  memset(&mapping, 0, sizeof(mapping));
  mapping.handle = created_scanout.handle;

  ret = drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &mapping);
  if (ret) {
    printf("Creating dump mapping failed\n");
    return 1;
  }

  /*
   * From DRM, we get a fake offset, so we can access data directly from map
   */
  map = mmap(0, created_scanout.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
             mapping.offset);

  if (map == MAP_FAILED) {
    printf("Mapping Failed\n");
    return 1;
  }

  SDL_Window *window = SDL_CreateWindow(
      "Drm Buffer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
      created_scanout.width, created_scanout.height, SDL_WINDOW_SHOWN);

  SDL_Surface *screen_surface = SDL_GetWindowSurface(window);
  SDL_Surface *fb_surface = SDL_CreateRGBSurfaceFrom(
      map, created_scanout.width, created_scanout.height, 32, 1920 * 4,
      0x00FF0000, // R mask
      0x0000FF00, // G mask
      0x000000FF, // B mask
      0xFF000000  // A mask (or 0 if RGBX)
  );

  uint32_t *pixels = map;
  for (int j = 0; j < 100; j++) {
    for (int i = 0; i < (int)created_scanout.width; i++) {
      pixels[created_scanout.width * j + i] = 0xff0000ff;
    }
  }

  SDL_BlitSurface(fb_surface, NULL, screen_surface, NULL);
  SDL_UpdateWindowSurface(window);
  SDL_Delay(3000);
  SDL_FreeSurface(fb_surface);
  SDL_DestroyWindow(window);
  SDL_Quit();

  munmap(map, created_scanout.size);
  ret = drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_scanout);
  close(fd);
}
