#ifndef DRIVER_H
#define DRIVER_H

#include "asm-generic/int-ll64.h"
#include "drm/drm_atomic_helper.h"
#include "drm/drm_crtc.h"
#include "drm/drm_device.h"
#include "drm/drm_encoder.h"
#include "drm/drm_format_helper.h"
#include "drm/drm_fourcc.h"
#include "drm/drm_framebuffer.h"
#include "drm/drm_gem_framebuffer_helper.h"
#include "drm/drm_ioctl.h"
#include "drm/drm_mode_config.h"
#include "drm/drm_plane.h"
#include "linux/clk.h"
#include <linux/platform_device.h>


#define get_64_lo(val) (val & 0xFFFFFFFF)

#define get_64_hi(val) ((val >> 32) & 0xFFFFFFFF)

#define GPU_ID 0x0000 // temporary offset for the ID register for now
#define NUM_PLANES 2


// This is the main device the driver will be for.
// I defined it like this, it seems like it's just the basics for now
// We'll see if we need to add any more things
//
// Our device definition wraps a drm_device which wraps a device
// We get that device from a platform device wrapping the device
//
struct pi_gpu {
  struct drm_device drm_device;
  void __iomem *registers;

  // TODO: I'm just pointing to the beginning of the allocated RAM memory (see
  // below on why it's RAM) so I'll have to add an offset when allocating the
  // RAM memory for the device
  u32 *vram;
  size_t vram_size;
  struct iosys_map render_addr;
  struct iosys_map display_addr;

  // plane[0] -> Primary plane
  // plane[1] -> Render plane
  struct drm_plane planes[2];
  struct drm_connector connector;
  struct drm_encoder encoder;
  struct drm_crtc crtc;
};

struct pi_gpu *to_gpu(struct drm_device *drm);

#endif
