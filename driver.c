#include "asm-generic/errno-base.h"
#include "asm-generic/int-ll64.h"
#include "drm/drm_atomic.h"
#include "drm/drm_atomic_helper.h"
#include "drm/drm_crtc.h"
#include "drm/drm_damage_helper.h"
#include "drm/drm_device.h"
#include "drm/drm_drv.h"
#include "drm/drm_encoder.h"
#include "drm/drm_format_helper.h"
#include "drm/drm_fourcc.h"
#include "drm/drm_framebuffer.h"
#include "drm/drm_gem.h"
#include "drm/drm_gem_atomic_helper.h"
#include "drm/drm_gem_framebuffer_helper.h"
#include "drm/drm_gem_shmem_helper.h"
#include "drm/drm_ioctl.h"
#include "drm/drm_mode_config.h"
#include "drm/drm_modeset_helper_vtables.h"
#include "drm/drm_plane.h"
#include "drm/drm_simple_kms_helper.h"
#include "linux/clk.h"
#include "linux/container_of.h"
#include "linux/device.h"
#include "linux/device/class.h"
#include "linux/dma-mapping.h"
#include "linux/err.h"
#include "linux/gfp_types.h"
#include "linux/ioport.h"
#include "linux/iosys-map.h"
#include "linux/kernel.h"
#include "linux/of.h"
#include "linux/of_address.h"
#include "linux/of_graph.h"
#include "linux/of_reserved_mem.h"
#include "linux/printk.h"
#include "linux/slab.h"
#include "linux/uaccess.h"
#include "linux/workqueue.h"
#include <linux/platform_device.h>
#include <stddef.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Victor");
MODULE_DESCRIPTION("GPU driver for the rasberry pi");

#define GPU_ID 0x0000 // temporary offset for the ID register for now
#define BUFFER_OFFSET 0x1000

#define PI_MAX_PITCH                                                           \
  (0x1FF << 3) /* (4096 - 1) & ~111b bytes                                     \
                = 4088. It's made sure that it's a multiple of 8 for easier    \
                and consistent memory access.                                  \
                */
#define PI_MAX_VRAM                                                            \
  (4 * 1024 * 1024) /* 4MB since 1024 bytes is a KB and 1024 KB is a MB */

struct gpu_render_args {
  /*
   * We're using __u32 instead of u32 because __u32 is compatible (and signifies
   * it interacts with user-space)
   *
   * Not a realistic structure, but want to keep it simple for this driver
   */
  __u32 x;
  __u32 y;
  __u32 width;
  __u32 height;
  __u32 color;
  __u32 handle;
};

#define DRM_IOCTL_EXC_BUFFER 0x00

// NOTE: We only need to define our cutom iocts like this.
// So all the ioctls in the .iocts field in drm_driver are custom ones, WE
// created Non-custom ioctls aren't added there. For example dumb_create is a
// callback from an ioctl which help us create GEM object (and returns a GEM
// object handler)
#define DRM_IOCTL_EXC_BUFFER_IOCTL                                             \
  DRM_IOWR(DRM_COMMAND_BASE + DRM_IOCTL_EXC_BUFFER, struct gpu_render_args)

struct pi_gpu;
static int probe_fake_gpu(struct platform_device *);
static int remove_fake_gpu(struct platform_device *);
static void pi_format_set(struct pi_gpu *gpu,
                          const struct drm_format_info *format);
int gpu_render_ioctl(struct drm_device *dev, void *data, struct drm_file *file);

static const struct drm_ioctl_desc ioctl_funcs[] = {
    DRM_IOCTL_DEF_DRV(EXC_BUFFER_IOCTL, gpu_render_ioctl, DRM_RENDER_ALLOW),
    // TODO: more if needed
};

static const struct drm_driver pi_gpu_driver = {
    .driver_features = DRIVER_GEM | DRIVER_MODESET | DRIVER_RENDER,
    .name = "pi_gpu",
    .desc = "PI GPU Controller",
    .date = "20240319",
    .major = 1,
    .minor = 0,
    .patchlevel = 0,
    .ioctls = ioctl_funcs,
    .num_ioctls = ARRAY_SIZE(ioctl_funcs),

    /* SHMEM isshared memory, I think it's for RAM
     * This function returns the handle of the GEM object created
     *
     * shmem isn't guaranteed to be continuous, it's not allocated by the CMA
     */
    .dumb_create = drm_gem_shmem_dumb_create,
    // some more things down below I need to learn about
};

static struct platform_driver pi_connection_driver = {
    // the platform_driver is composed of driver, and the only way to link it
    // with the
    // platform device is through the name of the device. Hence, we give it the
    // same name
    .driver =
        {
            .name = "fake_gpu",
        },
    .probe = probe_fake_gpu,
    .remove = remove_fake_gpu,

};

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
  u64 *vram;
  size_t vram_size;
  struct clk *clk;

  struct drm_plane primary_plane;
  struct drm_connector connector;
  struct drm_encoder encoder;
  struct drm_crtc crtc;
};

#define to_pi(_dev) container_of(_dev, struct pi_gpu, drm_device)
/*
 * Just like the pi_gpu, we're going to create our own version of a plane state
 * for this driver it's going to embed a drm_shadow_plane_state which itself
 * embeds a drm_plane_state
 *
 * A shadow plane is a struct that has a plane_state, but also a copy of it,
 * which can be modified before actually updating the plane
 */
struct pi_primary_plane_state {
  struct drm_shadow_plane_state base;

  // These seem to be helper fields for rendering or
  // different things about the interactions wth the frambuffer (NOT SURE YET)
  const struct drm_format_info *format;

  // The pitch is NOTE: the number of bytes between the start of one line of
  // pixels and the start of the next line
  unsigned int pitch;
};

// TODO: STEP AFTER PROBE
// Seems like we'll need to call the predefined function device_register() (in
// base/probe.c) to register our device Our device is discovered by some bus
// driver, and it should initialize these fields: -parent -name -bus_id
//  -bus
static int register_pi_gpu(struct device *pi_gpu) { return 0; }

static int remove_fake_gpu(struct platform_device *pdev) { return 0; }

/* ---------------------------------------------------
Emulating a GPU with QEMU yourself is actually very hard.
So, we'll be intercepting GPU calls ourselves. :( Not a real GPU I know
------------------------------------------------------
*/

static u8 reg_read8_ioctl(struct file *file, unsigned int cmd, u8 data,
                          u64 offset) {
  return 0;
}

static u16 reg_read16_ioctl(u16 data, u64 offset) { return 0; }

static u32 reg_read32_ioctl(u32 data, u64 offset) { return 0; }

static void reg_write8_ioctl(u8 data, u64 offset) { ; }

static void reg_write16_ioctl(u16 data, u64 offset) { ; }

static void reg_write32_ioctl(u32 data, u64 offset) { ; }

struct pi_gpu *to_gpu(struct drm_device *drm) {
  return container_of_const(drm, struct pi_gpu, drm_device);
}

/*
 * data argument is a pointer that the kernel already converted for us into the
 kernel address space
 * (by doing copy_from_user). So we can cast it to a struct we defined in
 user-space (that's also defined)
 * in the kernel module.
 * e.g.
 *struct drm_my_ioctl_args {
    __u32 handle;
    __u32 width;
    __u32 height;
  };
 *
 * And we do what we need with that
 *
 * Probably won't touch the drm_file
 */
int gpu_render_ioctl(struct drm_device *dev, void *data,
                     struct drm_file *file) {
  struct pi_gpu *gpu = to_gpu(dev);
  struct gpu_render_args *render_args = data;
  struct page **pages;
  struct sg_table *table;

  struct iosys_map *bo_va;
  size_t bo_size = 0;
  int ret;

  u32 handle = render_args->handle;

  struct drm_gem_object *obj = drm_gem_object_lookup(file, handle);

  if (IS_ERR(obj))
    return -EINVAL;

  struct drm_gem_shmem_object *shmem_obj = to_drm_gem_shmem_obj(obj);

  if (IS_ERR(shmem_obj))
    return -EINVAL;

  bo_size = shmem_obj->base.size;
  // This pins the pages into memory
  ret = drm_gem_shmem_vmap(shmem_obj, bo_va);

  if (ret)
    return ret;

  if (bo_va->is_iomem) {
    printk(KERN_CRIT "Not supposed to be io mem\n");
    goto release;
  }
  else {
    printk(KERN_INFO "Correctly mapped Buffer Object\n");
  }

  if (obj->dev != dev) {
    ret = -ENODEV;
    goto release;
  }

  for (int i = 0; i < bo_size; i++) {
    u8 *va = bo_va->vaddr;
    unsigned long addr = (unsigned long)(va + i);
    *(gpu->vram + BUFFER_OFFSET + i) = addr; 
  }


release:
  drm_gem_shmem_vunmap(shmem_obj, bo_va);

  return ret;
}

/*
 * Returns a handle to the GEM BO created
 *
 */
int gpu_gem_create_ioctl(struct drm_device *dev, void *data,
                         struct drm_file *file) {
  struct pi_gpu *gpu = to_gpu(dev);
  u64 *starting_mr = gpu->vram;
}

// -------------------------------------------------
static const struct drm_mode_config_funcs fake_gpu_modecfg_funcs = {
    /*
     * frm_gem_fb create is a helper function to create a framebuffer for
     * the driver. It gets an object called: drm_mode_fb_cmd2
     * The object comes from user space (makes sense since the thing in user
     * space determine the size that our frame buffers need to be). Called
     * through ioctl.
     *
     * Also needs an object called: drm_file
     * Needs this object
     *
     * For example, when a userspace app wants to use a framebuffer:
     * - It allocates a GEM object and gets a handle.
     * - It passes that handle to DRM_IOCTL_MODE_ADDFB2 (inside
     * drm_mode_fb_cmd2).
     * - The kernel looks up the GEM object using drm_file.
     * - Object can map the map hardware memory using mmap to manipulate it
     *   NOTE: Userspace can directly access location memories like RAM or
     * shared memory but not VRAM, it would do that through MMIOs like in the
     * NES
     *
     */
    .fb_create = drm_gem_fb_create,

    /*
     * Basically checks if transitioning to the new state is legal.
     * If it's not, we throw an error.
     *
     * The state passed in the atomic checks is the new state (for the checks
     * ofc)
     */
    .atomic_check = drm_atomic_helper_check,

    /*
     * If the check is successful, I believe this just applies the change
     */
    .atomic_commit = drm_atomic_helper_commit,
};

static inline struct pi_primary_plane_state *
to_pi_primary_plane(struct drm_plane_state *state) {
  return container_of(state, struct pi_primary_plane_state, base.base);
}

static void pi_primary_destroy_state(struct drm_plane *plane,
                                     struct drm_plane_state *state) {
  struct pi_primary_plane_state *pp_state = to_pi_primary_plane(state);

  // Removes all memory associated with the shadow plane
  // A shadow plane (or plane) has a reference count to a framebuffer
  // if the reference count of the framebuffer goes to 0 as the plane_state is
  // destroyed, the framebuffer is also freed. So, NOTE: plane states manage fb
  __drm_gem_destroy_shadow_plane_state(&pp_state->base);
  kfree(pp_state);
}

static void pi_primary_reset_plane(struct drm_plane *plane) {
  struct pi_primary_plane_state *pp_state;

  if (plane->state) {
    pi_primary_destroy_state(plane, plane->state);
    plane->state = NULL;
  }

  pp_state = kzalloc(sizeof(*pp_state), GFP_KERNEL);
  if (!pp_state) {
    return;
  }

  __drm_gem_reset_shadow_plane(plane, &pp_state->base);
}

static struct drm_plane_state *
pi_primary_duplicate_state(struct drm_plane *plane) {
  if (!plane->state) {
    return NULL;
  }

  struct pi_primary_plane_state *pp_state = to_pi_primary_plane(plane->state);

  struct pi_primary_plane_state *pp_state_new;
  struct drm_shadow_plane_state *shadow_state_new;
  pp_state_new = kzalloc(sizeof(*pp_state_new), GFP_KERNEL);

  if (!pp_state_new) {
    return NULL;
  }
  shadow_state_new = &pp_state_new->base;
  // In the function definition of this, the plane state gets casted to a shadow
  // plane state I checked
  __drm_gem_duplicate_shadow_plane_state(plane, shadow_state_new);
  pp_state_new->base = pp_state->base;
  pp_state_new->pitch = pp_state->pitch;

  return &pp_state_new->base.base;
}

/**
 * pi_convert_format - tries to conver the format of the framebuffer plane to a
 * more compact one if the size of the pitch exceeds the maximum pitch
 * @fb: framebuffer whose plane we're attempting to make fit with the maximum
 * pitch size
 *
 * This function is an attempt to convert the pitch into a valid format, but
 * does not guarantee it. The caller must ensure that the pitch is valid.
 *
 * Returns:
 * NULL on success, and ERR_PTR on failure
 */
static const struct drm_format_info *
pi_convert_format(const struct drm_framebuffer *fb) {
  if (fb->format->format == DRM_FORMAT_XRGB8888 &&
      fb->pitches[0] > PI_MAX_PITCH) {
    if (fb->width * 3 <= PI_MAX_PITCH) {
      return drm_format_info(DRM_FORMAT_RGB888);
    } else {
      return drm_format_info(DRM_FORMAT_RGB565);
    }
  }
  return NULL;
}

static int pi_pitch(struct drm_framebuffer *fb) {
  const struct drm_format_info *format = pi_convert_format(fb);

  if (format) {
    // Finds the minimum valid pitch
    // Since we're using simple formats, it would just be
    // width * bytes per pixel
    return drm_format_info_min_pitch(format, 0, fb->width);
  }
  return fb->pitches[0];
}

// Function to actually get a converted format from the original format in the
// frame buffer
static const struct drm_format_info *
pi_format(const struct drm_framebuffer *fb) {
  const struct drm_format_info *format = pi_convert_format(fb);

  if (format)
    return format;
  return fb->format;
}

/*
 * drm_atomic_state -> can contain multiple drm_plane_state structs
 * since 1 commit can affect multiple planes.
 *
 * Can also affect multiple crtcs
 *
 * NOTE: This is the atomic check pipeline:
 * First the modeset atomic check gets called whichdoes some checks, then we
 * check the planes In the plane atomic check, the check for CRTC is done
 * (crtc_atomic_check is called at trhe very end) So the CRTC knows all planes
 * are individually correct, so it can check between plane compatibility, and
 * and can check resources common to all planes.
 *
 *
 *
 */
static int
pi_primary_plane_helper_atomic_check(struct drm_plane *plane,
                                     struct drm_atomic_state *state) {
  // This is the plane state that we want to commit
  struct drm_plane_state *new_plane_state =
      drm_atomic_get_new_plane_state(state, plane);

  // Still the plane state we want to commit but with our struct
  struct pi_primary_plane_state *pp_state =
      to_pi_primary_plane(new_plane_state);

  struct drm_framebuffer *fb = new_plane_state->fb;
  struct drm_crtc *new_crtc = new_plane_state->crtc;
  struct drm_crtc_state *new_crtc_state = NULL;
  int ret = 0;
  unsigned int pitch;

  if (new_crtc) {
    /*
     * The changes aren't live yet, so if we use the new_crtc->state, we would
     * just get what we actually have since we add our changes to a "staging
     * area", so they're not visible. These are the changes we want to commit
     */
    new_crtc_state = drm_atomic_get_new_crtc_state(state, new_crtc);
  }

  ret = drm_atomic_helper_check_plane_state(new_plane_state, new_crtc_state,
                                            DRM_PLANE_NO_SCALING,
                                            DRM_PLANE_NO_SCALING, false, false);

  if (ret) {
    return ret;
  } else if (!new_plane_state->visible) {
    // The plane is disabled, so it won't be shown on screen, so it can just
    // be returned as 0, basically a no-op
    return 0;
  }

  pitch = pi_pitch(fb);
  if (pitch > PI_MAX_PITCH) {
    return -EINVAL;
  } else if (pitch * fb->height > PI_MAX_VRAM) {
    return -EINVAL;
  }

  // In this case, we know that the pitch was already fine, or the pitch was
  // successfully converted, so we modify the state from the actual commit to
  // make sure that the correct one is selected
  pp_state->format = pi_format(fb);
  pp_state->pitch = pitch;
  return 0;
}

static void
pi_primary_plane_helper_atomic_update(struct drm_plane *plane,
                                      struct drm_atomic_state *state) {
  struct pi_gpu *gpu = to_pi(plane->dev);
  struct drm_plane_state *plane_state =
      drm_atomic_get_new_plane_state(state, plane);
  struct pi_primary_plane_state *pp_state = to_pi_primary_plane(plane_state);
  // I think this should just be the same as doing pp_state->base
  struct drm_shadow_plane_state *shadow_state =
      to_drm_shadow_plane_state(plane_state);
  struct drm_framebuffer *fb = plane_state->fb;

  const struct drm_format_info *format = pp_state->format;
  unsigned int pitch = pp_state->pitch;

  // Get the old state using helper functions. Can't just do plane->state cuz
  // the state might have been modified along the way.
  struct drm_plane_state *old_state =
      drm_atomic_get_old_plane_state(state, plane);
  struct pi_primary_plane_state *old_pp_state = to_pi_primary_plane(old_state);
  // inits an iosys_map struct, specifying that we have system memory, no I/O
  // memory
  struct iosys_map vaddr = IOSYS_MAP_INIT_VADDR(gpu->vram);

  /*
   * In graphics rendering, damage is the parts of a framebuffer that have been
   * modified since the last refresh (update). So we just re-render those parts
   * instead of re-rendering everything available
   *
   * It keeps track of the parts it needs to re-render in rectangles
   */
  struct drm_atomic_helper_damage_iter iter;
  struct drm_rect damage;
  int idx = 0;

  if (!fb) {
    return;
  }

  // This checks if the device is still alive and hasn't just been unplugged or
  // something to avoid a nasty error like that
  if (!drm_dev_enter(&gpu->drm_device, &idx)) {
    return;
  }

  if (old_pp_state->format != format) {
    pi_format_set(gpu, format);
  }
  if (old_pp_state->pitch != pitch) {
  }

  drm_atomic_helper_damage_iter_init(&iter, old_state, plane_state);
  drm_atomic_for_each_plane_damage(&iter, &damage) {
    // Calculate where the start of the dirty area is. Need the pitch for the
    // width bytes needed, format for bytes/pixel and of course where the area
    // is.
    unsigned int offset = drm_fb_clip_offset(pitch, format, &damage);
    struct iosys_map dst = IOSYS_MAP_INIT_OFFSET(&vaddr, offset);

    // TODO: fb_blit only actually writes to the dispaly memory which I'm not
    // sure what it is exactly
    drm_fb_blit(&dst, &pitch, format->format, shadow_state->data, fb, &damage);
  }
  drm_dev_exit(idx);
}

/*-------------------------------------------------------------------------------
 * Most device specific features
 *
 *
 *-------------------------------------------------------------------------------
 */

#define REG_FORMAT 0x04
#define REG_PITCH 0x08

enum {
  PIX_FMT_RGB565 = 0,
  PIX_FMT_RGB888 = 1,
  PIX_FMT_XRGB8888 = 2,
};

static void pi_format_set(struct pi_gpu *gpu,
                          const struct drm_format_info *format) {
  u8 fmt;

  switch (format->format) {
  case DRM_FORMAT_RGB565:
    fmt = PIX_FMT_RGB565;
    break;
  case DRM_FORMAT_RGB888:
    fmt = PIX_FMT_RGB888;
    break;
  case DRM_FORMAT_XRGB8888:
    fmt = PIX_FMT_XRGB8888;
    break;
  default:
    WARN(1, "Unsupported format: %08x\n", format->format);
    return;
  }

  // TODO: important, I think we'll actually have to make our GPU with QEMU,
  // otherwise, it's going to be hard as hell
  iowrite8(fmt, gpu->registers + REG_FORMAT);
}

static void pi_pitch_set(struct pi_gpu *gpu, unsigned int pitch) {
  u16 reg_pitch = pitch;

  iowrite16(reg_pitch, gpu->registers + REG_PITCH);
}

/*
 * These are all the mandatory functions I need for my plane
 * Since I'm using the atomic thing
 *
 */
static const struct drm_plane_funcs pi_primary_plane_funcs = {
    .disable_plane = drm_atomic_helper_disable_plane,
    .update_plane = drm_atomic_helper_update_plane,
    .destroy = drm_plane_cleanup,
    .reset = pi_primary_reset_plane,
    .atomic_duplicate_state = pi_primary_duplicate_state,
    .atomic_destroy_state = pi_primary_destroy_state,
};

static const struct drm_plane_helper_funcs pi_primary_plane_helper_funcs = {
    // Macro that initializes some helper functions
    // Apparently they're tedious, and shouldn't be written by me
    //
    // NOTE: Shadow plane states only map the frame buffers to the kernel
    // address space
    // so the CPU can access them. There's no copy made or anything
    // The only time changes to the framebuffer really occur are in the
    // atomic_update()
    // The above is done by begin_fb_access()
    // And I think the end_fb_access() relesases the memory or someeee
    //
    // NOTE: The driver never scans the framebuffer for "dirt" (changed regions
    // in the framebuffer)
    // Those are supplied by the user space (and we call
    // drm_plane_enable_fb_damage_clips) to receive them
    //
    // There's no need to define prepare_fb or cleanup_fb because if they're not
    // defined,
    // begin_access_fb() implements them, and that's what happens in
    // DRM_GEM_SHADOW_PLANE_HELPER_FUNCS
    DRM_GEM_SHADOW_PLANE_HELPER_FUNCS,
    .atomic_check = pi_primary_plane_helper_atomic_check,
    .atomic_update = pi_primary_plane_helper_atomic_update,
};

static struct drm_crtc_helper_funcs pi_crtc_helper_funcs = {};

static const uint32_t pi_primary_plane_formats[] = {
    DRM_FORMAT_XRGB8888, DRM_FORMAT_RGB888, DRM_FORMAT_RGB565};

static const uint64_t pi_primary_plane_modifiers[] = {
    DRM_FORMAT_MOD_LINEAR,
};

static const struct drm_crtc_funcs pi_crtc_funcs = {
    .reset = drm_atomic_helper_crtc_reset,
    .destroy = drm_crtc_cleanup,
    .set_config = drm_atomic_helper_set_config,
    .page_flip = drm_atomic_helper_page_flip,
    .atomic_duplicate_state = drm_atomic_helper_crtc_duplicate_state,
    .atomic_destroy_state = drm_atomic_helper_crtc_destroy_state,
};

static int pi_pipe_init(struct pi_gpu *gpu) {
  struct drm_device *drm = &gpu->drm_device;
  struct drm_crtc *crtc = NULL;
  struct drm_encoder *enconder = NULL;
  struct drm_connector *connector = NULL;
  int ret = 0;

  /*
   * NOTE:  DRM state structures are allocated, managed and freed by the DRIVER
   *
   * NOTE: DRM state structs are owned by the corresponding DRM object
   *
   * NOTE: Before a commit is finised, the state is owned by the atomic_commit
   * state After it's finished, the state is owned by the object (and memory by
   * the driver)
   *
   * If the atomic state owns a state, it can free it if it fails.
   * Usually 2 copies during an atomic operation
   */

  /*
   * Initializing the primary plane
   *
   * NOTE: We're not assigning a framebuffer to the plane yet.
   * This just initializes a plane
   *
   * 0 -> possible crts FIXME: not sure why it's 0 yet.
   *
   * Plane funcs -> Functions that define what happens when a plane is
   * enabled, disabled or updated (those are the mandatory behaviors for a
   * func)
   *
   * Plane primary formats -> Type of pixel data the frame can display NOTE:
   * (we'll only do RGB), I think Right after the formates is just how many
   * formats there are.
   *
   * Plane modifiers -> change how the GPU's pixel data is stored/organized in
   * memory NOTE: We'll probably only do linear for this one.
   *
   * NOTE: Planes are owned by the device, so it still exists if it has no
   * framebuffers or are inactive
   */
  ret = drm_universal_plane_init(
      drm, &gpu->primary_plane, 0, &pi_primary_plane_funcs,
      pi_primary_plane_formats, ARRAY_SIZE(pi_primary_plane_formats),
      pi_primary_plane_modifiers, DRM_PLANE_TYPE_PRIMARY, NULL);
  if (ret)
    return ret;

  drm_plane_helper_add(&gpu->primary_plane, &pi_primary_plane_helper_funcs);

  /*
   * This enables tracking the damage rectangles for updating the output
   */
  drm_plane_enable_fb_damage_clips(&gpu->primary_plane);

  /*
   * The first NULL is for the cursor plane
   *
   * NOTE: CRTC is owned by the device, so still exists even if there's no
   * planes pointing to it
   */
  ret = drmm_crtc_init_with_planes(drm, &gpu->crtc, &gpu->primary_plane, NULL,
                                   &pi_crtc_funcs, NULL);

  if (ret) {
    return ret;
  }
  drm_crtc_helper_add(crtc, &pi_crtc_helper_funcs);

  return 0;
}

static int fake_gpu_load(struct pi_gpu *gpu) {
  // cool way to get back the platform device from the device without having
  // to pass it in the function. to_platform_device is a wrapper for
  // container_of(), so it used the field, and struct to determine this
  struct platform_device *pdev = to_platform_device(gpu->drm_device.dev);
  struct drm_device *drm = &gpu->drm_device;
  struct device_node *res_node;
  struct resource *res;
  struct resource vram;
  // this address is passed to our device which can write to it
  // since we're emulating, we can't really use it, because it's meant for a
  // device to write to the actual physical mapping (unless there's more
  // hardware that does some mapping (but still in hardware))
  dma_addr_t dma_handle_vram;

  int ret = 0;

  // TODO:We'll look at this next time
  struct device_node *encoder_node = NULL, *enpointnode = NULL;

  /* devm_clk_get gets a clock for the device. Aside: Power-aware drivers only
   * update the clocks when the device they manage is in actve use
   * (https://www.chiark.greenend.org.uk/doc/linux-doc-3.16/html/kernel-api/clk.html)
   *
   * when the kernel tries to get a clock, the clock is actually supposed to
   * be on the device tree, so for our fake GPU, we won't have this (at least
   * for now). I believe it does device/driver binding similarly
   */

  /*
   *  drmm_mode_config_init sets up internal things for GPU used for display
   * and output No look up in device tree, sets up interals for drm device,
   * but we will have to actually do things like adding components things
   * related to the mode setting?
   *
   * NOTE: The resolution actually defines the possible ranges of size that
   * the framebuffer can have. The resolution is basically the size of the
   * framebuffer, which makes sense
   */

  ret = drmm_mode_config_init(drm);
  if (ret) {
    return ret;
  }

  /* set the resolution for the GPU
   * These are for the frambuffers, I think
   * These are the available resolutions available to select from for the mode
   * setting
   */
  drm->mode_config.min_height = 0;
  drm->mode_config.min_width = 0;
  drm->mode_config.max_height = 1080;
  drm->mode_config.max_width = 1920;
  drm->mode_config.funcs = &fake_gpu_modecfg_funcs;
  // TODO: something with drm_mode_config_init() ?

  /*
   * resources have a start address and a size
   * IORESOURCE_MEM gets an MMIO resource
   * For the NES, for example, that would be like starting
   * from 0x2000 and going to 0x2007.
   *
   * NOTE: It knows the size (resource struct has a start and end) from device
   * tree, as usual.
   *
   * platform_get_resource returns a PHYSICAL address, so we can't use it
   * because in code we're using virtual addresses, so we'll have no idea what
   * to do. using devm_ioremp_resource, we get back virtual addresses pointing
   * to the physical addresses
   */
  res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

  /*
   * After mapping the address, we get a void pointer, however we can use
   * read/write functions from the kernal (ioread32 or iowrite32) to use the
   * void pointers.
   */
  gpu->registers = devm_ioremap_resource(&pdev->dev, res);
  if (IS_ERR(gpu->registers)) {
    return PTR_ERR(gpu->registers);
  }

  // Keeping this for now, but our platform device doesn't actually have vram,
  // so we don't do this res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
  // gpu->vram = devm_ioremap_resource(&pdev->dev, res);
  // if (IS_ERR(gpu->vram)) {
  //   return PTR_ERR(gpu->vram);
  // }

  /*
   * dev_info() is just a logging mechanism for the kernel, so kind of like
   * the logging module in python It logs at an info level, and has the device
   * name as a prefix to the message. Modern GPUs define offsets for their
   * Registers and the OS picks an area of memory where the MMIO exists. So if
   * a register has an offset of 0, and the MMIO is mapped at 0xFC00, then
   * 0xFC00 is just how we access that register
   *
   * NOTE: We'll have to look at the GPU_ID offset later when we make the
   * emulated GPU to determine what's an actual good offset
   */
  dev_info(drm->dev, "loading fake GPU device\n");

  /*
   * NOTE: Reserved memory is a preallocated area of RAM used by the GPU
   * for things sicha as framebuffers, DMA pools, etc (not used by registers
   * though) Knows how much memory to allocate from device tree
   *
   * For the emulated GPU, this area will be our VRAM since we won't actually
   * have any VRAM.
   */
  // initializing GPU VRAM
  /*
   * If we want the CPU to access the memory, either memory needs to be
   * coherent DMA or not DMA at all.
   *
   * DMA addresses allow the GPU to access te addresses directly without the
   * CPU
   *
   * DMA memory regions don't need to be copied and pasted by the CPU, it's
   * done in parallel to CPU execution be DMA, so it won't need the CPU to
   * copy and paste data into other places.
   */
  ret = of_reserved_mem_device_init(drm->dev);
  if (ret && ret != -ENODEV)
    return ret;
  if (dma_set_mask_and_coherent(drm->dev, DMA_BIT_MASK(8)))
    return -ENODEV;

  // memory-region will be the second resource
  // phandle (short for pointer handle)
  /*
   * The devie tree has nodes, and phandles can be treated as references to
   * other nodes or extend the features of something else
   *
   * The .of_node is set using "compatible" field, so that's how we get it
   */
  res_node = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);

  if (IS_ERR(res_node)) {
    return PTR_ERR(res_node);
  }

  ret = of_address_to_resource(res_node, 0, &vram);

  if (ret) {
    return -ENODEV;
  }

  /*
   * The function directly returns a virtual address, sets the dma address by
   * passing the address of a dma address.
   *
   */

  resource_size_t vram_size = resource_size(&vram);

  // SIZE_MAX is dependent on platforms. On 64bit platforms, it's 64bits, on
  // 32bit plats, it's 32.
  if (vram_size > SIZE_MAX) {
    return -EINVAL;
  }

  gpu->vram = dma_alloc_coherent(drm->dev, (size_t)vram_size, &dma_handle_vram,
                                 GFP_KERNEL);
  gpu->vram_size = (size_t)vram_size;

  if (IS_ERR(gpu->vram)) {
    return PTR_ERR(gpu->vram);
  }

  /*
   * Gets the first endpoint from the device tree. The second param (where we
   * pass in NULL) is the previous endpoint. Since we passed NULL, we would
   * get the first endpoint from the function.
   *
   * Since each endpoint from our device points to another endpoint to a path
   * to another device (HDMI). For GPUs and what I'm focusing on it can be
   * like HDMI or VGA.
   */
  // endpoint_node = of_graph_get_next_endpoint(pdev->dev.of_node, NULL);
  // if (endpoint_node) {
  /*
   * Getting the remote port, so what device (path to monitor) the endpoint
   * points to We also don't need to create our own connector. We can create a
   * bridge using the encoder (bridges and encoders are kind of the same
   * thing, I will treat them as the same, so it does the format conversion to
   * HDMI). The bridge has an connector attached to it. That's where the
   * connector comes from.
   */
  // encoder_node = of_graph_get_remote_port_parent(endpoint_node);
  // of_node_put(encoder_node);
  // }

  ret = pi_pipe_init(gpu);
  return 0;
}

static int probe_fake_gpu(struct platform_device *device) {
  /* We need this function because it's part of the platform_driver
   * to determine if the device exists and to set up resources for it
   *
   * This function is called by the kernel when it tries to match drivers with
   * devices So the platform device will already have things like bus
   * information initialized
   *
   * We must add to our driver the name field. This is necessary because if
   * the kernel finds a device with that name, they're matched and the probe
   * function is called
   */
  printk("Just checking we've reached the probe function\n");
  struct pi_gpu *pi_device = NULL;
  int ret = 0;

  // Initialize main device we'll be using.
  // Its lifespan depends on device->dev (so basically device, our platform
  // device) which is the actual existing hardware device
  pi_device = devm_drm_dev_alloc(&(device->dev), &pi_gpu_driver, struct pi_gpu,
                                 drm_device);
  ret = fake_gpu_load(pi_device);

  // IS_ERR returns true if the pointer passed is an error pointer
  // Since devm_dr_devl_alloc returns an error pointer, this is good use
  if (IS_ERR(pi_device)) {
    // PTR_ERR extracts the error from an error pointer
    return PTR_ERR(pi_device);
  }

  struct resource *res = NULL;
  void __iomem *reg_base;
  res = platform_get_resource(device, IORESOURCE_MEM, 0);
  if (!res) {
    return -EFAULT;
  }
  return 0;
}

module_platform_driver(pi_connection_driver);

/*
 * One of the usecases of Loadable Kernel Modules is for device drivers. LKM's
 are loaded during runtime in the kernel
 * They communicate directly with the base kernel
 *
 * LKM's are not part of the user space. They are part of the kernel space.
 *
 * When we, in the user space do something that interacts with the hardware,
 like requesting to load a page using internet
 * And that requires some hardware usage, we use APIs exposed to the kernel
 which can effectively get what we need using the kernel
 * back to the user space.
 *
 * What we did with our first test using printk with init and exit was using
 LMK. We loaded the LMK into the kernel which is how that worked.
 *
 *
 *
 * To develop a GPU driver, we will probably need to look at the KMS mode
 setting pipeline:
 *
 * 1. GEM Buffer object 1 -> Let me allocate an area of memory where I can
 write pixels
 * 2. Frambuffer -> Something the GPU knows how to draw TODO: gotta look at
 what this is more
 *
 * 3. Primary plane -> 2 different physical parts of memory can contain
 something that make a sensible image
 *
 *
 *
 *
 *
 *
 *
 * 5. CRTC -> If the image is something like 800x600, and the monitor is
 320x240, the CRTC decides how to scale
 * the monitor
 *
 *
 * Could be useful for some documentation
 * https://stackoverflow.com/questions/22961714/what-is-platform-get-resource-in-linux-driver
 *
 * definition of Resources:
 *
                struct resource {
                        resource_size_t start;
                        resource_size_t end;
                        const char *name;
                        unsigned long flags;
                        unsigned long desc;
                        struct resource *parent, *sibling, *child;
                };

                System memory != RAM
                Ram is part of that memory
                Registers are just in another range of that memory, and I was
 under the belief that things like I/O and MMIO were in RAM
 *
 * Obviously C doens't have classes and inheritance like C++ does. However,
 inheritance can be usually
 * be achieved by composition which is what the driver developpers usually do.
 *
 * We see this with the structures like device, drm_device, and
 platform_device. The latter 2 both have
 * a device struct embedded inside.
 *
 * There's often the use of preprocessor macros
 *
 * The macr container_of gets a pointer to the "parent" struct of a field
 inside of that struct (given we have
 * a pointer to that field)
 *
 * According to documentation, embdding struct drm_device in our device is
 reccomended, as is done in the example.
 * FIXME: (not a fixme, but importat) Functions with a prefix of devm manage
 their own memor based on devres framework
 * When we create a device with devres, the RESOURCES are managed by devres.
 Each device has resources associated to it stored in a linked list. And the
 linked list is properly cleared when the device is detached.
 *
 * dev_kmalloc -> automatically frees the memory used, but should be used for
 the resources of the device I THINK
 * dev_kzmalloc -> same as above, but initializes with 0 before (like calloc)
 * devres only automatically manages memory with some specific functions, so I
 should watch out with that. (usually a function to initialize the device)
 *
 * devres worksa lil something like this I believe:
 * 1. pass in the size of memory we want to occupy, device as pointer of
 course and other things
 * 2. Mainly make some kind of devres object including information about
 memory allocated (starting point and size)
 * 3. The devres obj is initialized with some kind of releasing function
 *
 * For devm_drm_dev_alloc, the drm_device acts as a resource for the parent
 device, just like other devres functions.
 * BUG: ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ Is kind of wrong
 * devm_drm_dev_alloc creates a new device which points to the parent device.
 However, the parent device is not the user device we created. The parent
 devie is an actua device structure from which the drm_device inherits from.
 The device in the arc example is found in the platfor device. The function
 can return our user defined device, meaning it initializes it, so it only
 really manages the resources for drm.
 * > we have pointers to the resources in the struct. So of course, the actual
 content is on the heap (imagining we use malloc). Since drm is managed by
 devres, the memory on the "heap" is freed. But not for the other fields.
 * The fields (pointers) are freed because we free the structure automatically
 using devres. Just like in C++ tho, ifwe have fields allocated on the heap,
 then in the destructor, we must also free them.
 *
 * __iomem -> annotation added to pointer types. Marks that the pointer refers
 to I/O memory regions.
 * mapped to the kernel space means -> something with a physical address that
 can be accessed with a virtual address
 *
 * functions marked static might also only be used in the kernel space since
 they cannot be imported/exported
 *
 * NOTE: DEVICE DRIVER DESIGN PATTERNS
 * State container -> this, I think is kind of like what we did with our
 pi_gpu struct using that as a state container
 * container_of -> get address of struct container member using address of
 member
 *
 * tyepdef struct some_struct -> now we don't need to add struct before using
 some_struct
 *
 * ==========================================================================
 * OBJECTS of the Linux Graphicds Stack
 *
 * 3D model -> stored in vertex-buffer object = geometry
 * texture -> stored in a texture-buffer object = sampling during shading?
 * Output image -> stored in a framebuffer object -> framebuffer = where we
 write our pixels
 *
 * ioctl interface managing buffer objects is GEM
 * GEM is implemented according to the actual hardware
 *
 * GEM interface allows mapping a buffer object memory pages to
 user-space/kernel-space
 * GEM objects = buffers
 *
 *
 * ioctl -> stands for input / output control
 * used to send custom commands from user space to device drivers
 * Apis like vulkan or OpenGL have a standard set of ioctl instructions the
 drivers need to implement so they
 * can be used
 *
 * However there's also things called Vulkan Drivers which work with the
 specific gpus, so that's the things
 * able to use custom ioctl functions
 *
 * NOTE: I think this is important
 * Minimum stages for a drvier to get pixel data on its way to the monitor:
 * - Framebuffer
 * - Plane
 * - CRTC
 * - Encoder
 * - Connector
 *
 * Framebuffer -> buffer object storing on-screen image, color, format, size
 * Getting pixel data buffer object -> scanout buffer
 * RGB groups all pixel data in a single (scanout) buffer
 * scanout buffer is part of the framebuffer
 *
 * Plane -> with the scanout buffer we got, the plane sets the location of the
 buffer,
 * the orientation and the scaling factors. Contain information on how to
 blend
 *
 * Planes feed pixel output in the CRTC (reads pixel data from memory and
"formats" it to provide it to display hardware). NOTE: The pixel data from
memory is what a framebuffer represents
 *
 * CRTC -> controls everything related to display-mode settings.
 * display mode setting is the sepcific resolution, refresh rate and other
 parameters
 * planes are assigned to a CRTC I think
 *
 * NOTE: The crtc is the display pipeline. This display pipeline (CRTC) must be
connected to a connector
 *
 * compositors manage the planes and shit
 *
 * Rendering (started by compositors) uses vulkan/openGL to start compute
 image data
 * Displaying actually done by compositors passing the info to the drivers
 *
 * Encoder -> represents output. encodes raw pixel data into something the
 output type (HDMI or VGA, etc)
  * understands
 *
 * Connector -> represents output (screen, it represents a SCREEN). Represents
physical monitor. Also provides resolution color space and the like.
 *
 * Pipeline: Naive implementation -> Program display modein CRTC, upload
 buffer objects in graphics memory, set u framebuffers and planes, enable
 encoder and connector
 *
 * To change displayed image in the next frame, we only need to replace
 frambuffer with a new one. THIS is PAGE FLIPPING
 *
 * Driver can configure CRTC through EDID (display info thing) from connector
 *
 *
 *
 * With limited device memory, if 1 step in the pipeline fails, screen might
 be still off or in a distorted state
 *
 * Atomic mode setting solves problem to some extent.
 * It tracks complete state of all elements of all elements of the pipeline in
 a data structure called: drm_atomic_state
 * Also tracks a sub-state for each stage in the pipeline. Basically atomic
 makes it strong guarantee, so we only apply changes
 *
 * Every DRM drvier requires a memory manager which must be initialized at
 load time. This will probably be GEM
 *
 * TODO: (marked as TODO, but who knows when) We'll need to define a .fops
 field for the drm driver, so that's going to be for te userspace API.
 *
 * For
 *
 * cirrus
 *
 * This is a C fact for macros that cool/useful:
 * In C macros, we can add ... for a variable number of arguments after
 (basically like *args in python)
  *
  * C macros are kind of similar to inline functions, but useful when we can
 work with any type (like a
 * function accepting any type)
  *
  * Modern GPUs define offsets for their registers because the OS decides
 where the MMIO region starts at
  * Kernel can only access memory visible by the CPU btw
  *
  * Driver should never crash from userland code, always has to protect itself
  * NOTE: Failing probe or returning something like -ENODEV is NOT a crash.
 Still expected behaviour

 Calling kmalloc with GFP_KERNEL
 means that the allocation is performed by a process running in the kernel
 space (which is what we're doing) Need to be careful calling kmalloc in an
 atomic context

 3 important memory zones -> DMA, Normal, and high memory

 DMA -> talked about where we allocate the DMA memory
 Normal -> Normal? Just something like RAM
 High -> TODO: (something about being able to access large amount of memory)
 *
 *
 * Cache coherency -> 2 clients (e.g. CPU and GPU)
  * CPU write to its cache, then
  * without cache coherency, if the GPU would read from the shared memory, it
 would see un-updated data
  * with cache coherency, if the GPU would read from the shared memory, it
 sees the correctly updated data
  *
  * For now, we'll limit the ways the GPU can access RAM from the CPU to only
 DMA (there might be other things, but I'll only consider this for now)
  * It's slower and it does it through the PCIe.
  *
  * The CPU has multiple caches with levels:
  *
  * L1: Fast
  *
  * L2: Faster
  *
  * L3: Slow
  *
  * DRAM: Much slower
  *
  * Frequently used data goes into the caches. Writes to the RAM are delayed
 because they're much slower
  * So, a CPU would first write to its caches and then to the RAM.
  *
  * Caches are usually SRAM, RAM that's fast and the data doesn't decay
  * DRAM is usually the RAM where most data is stored where data decays, but
 it's always refreshed



In the atomic framework, every KMS object related to the pipeline has a state
(CRTC, Plane, Connector). During an atomic commit, they're owned by an
atomic_state as we can see in the struct of the states. Once the atomic commit
is done, it's ownership is released because it's no longer owned. Old state gets
swapped with new state.


NOTE: https://www.kernel.org/doc/html/v4.14/gpu/drm-uapi.html | This is how
drivers interact with the user space. They litewally have different ioctl
functions for DRM WHICH I HAD THE OPPORTUNITY TO FIGURE THAT OUT FOR A
WHILEEEEEEEE


# NOTE: GPU rendering is actually triggered by IOCTL calls (or sys things calls,
but usually ioctl) and that's how a GPU renders. When a GPU renders, what it
does is it modifies the frame buffer that the driver owns. There's other things
for device specific things IOCTL calls are used for, but this is really what we
mean by rendering.


#NOTE: in fact, here's a quote from a linux documentation for a driver:
 "The Intel GPU family is a family of integrated GPU's using Unified Memory
Access. For having the GPU "do work", user space will feed the GPU batch buffers
via one of the ioctls DRM_IOCTL_I915_GEM_EXECBUFFER2 or
DRM_IOCTL_I915_GEM_EXECBUFFER2_WR"


#NOTE: This is pretty important about the atomic framework and something I had
mistaken

The atomic framework doesn't copy any frame buffers, or anything. It does NOT
create any atomic STATE. This is left to the user space. In fact, there is a
"strategy" called double buffering which is pretty simple. The principle is that
there's 2 BO, one used for the current display, what the user sees, and one that
gets updated. During VBlank, usually (or other suitable time? if there is any,
not sure), the userspace calls an atomic commit with the second buffer. If they
want to modify other things than the framebuffers or planes, FIXME: I believe
that requires a full modeset

Moreover, a Buffer Object (BO) is (kind of obviously) different from a frame
buffer or the such. The BO, is really just the memory allocated for it, and the
framebuffer contains important metadata about the pixels such as the format, the
width, pitch, etc.


NOTE: PINNING PAGES
First, the kernel allocated memory in pages, as we know and studied in our
google doc. So, let's say we allocate some memory, a page is created for it and
on hardware and the kernel might move the data from that page to another page
which would cause problems. Pinning basically stops that from happening and the
data in the page is forced to stay there.


 *https://www.kernel.org/doc/html/v5.7/driver-api/driver-model/index.html
 *https://docs.kernel.org/5.17/gpu/drm-internals.html#c.devm_drm_dev_alloc
 *https://www.kernel.org/doc/html/v5.7/driver-api/driver-model/devres.html

 Probably very useful: https://www.kernel.org/doc/html/latest/gpu/index.html
                             //
 book on drivers: https://lwn.net/Kernel/LDD3/
                        //
  https://bootlin.com/pub/conferences/2023/eoss/kocialkowski-current-overview-drm-kms-driver-side-apis/kocialkowski-current-overview-drm-kms-driver-side-apis.pdf
 */
