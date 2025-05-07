#ifndef EXECBUFFER_H
#define EXECBUFFER_H

#include "asm-generic/int-ll64.h"
#include <linux/platform_device.h>
#include "asm-generic/int-ll64.h"
#include "drm/drm_atomic_helper.h"
#include "drm/drm_crtc.h"
#include "drm/drm_device.h"
#include "drm/drm_drv.h"
#include "drm/drm_format_helper.h"
#include "drm/drm_fourcc.h"
#include "drm/drm_framebuffer.h"
#include "drm/drm_gem_framebuffer_helper.h"
#include "drm/drm_ioctl.h"
#include "drm/drm_mode_config.h"
#include <linux/platform_device.h>


#define DRM_IOCTL_EXC_BUFFER 0x00

#define FRM_BUFFER_OFFSET 0x1000
#define FRM_BUFFER_LEN_OFFSET 0x1002
#define INS_BUFFER_OFFSET 0x0000
#define INS_BUFFER_START_OFFSET 0x0002
#define INS_BUFFER_LEN_OFFSET 0x0003


// Forward declarations
struct pi_gpu;


typedef struct pair {
  void *first;
  void *second;
} Pair;


struct pi_exec_buffer {
  __u64 buffers;     // pointer to buffer objects of type &pi_exec_buffer_obj
  __u32 num_buffers; // number of buffer objects;
                     // NOTE: Right now, only 2 buffers are supported. An
                     // instruction buffer and frame buffer.

  /* Offset from where we start execution from the instruction buffer (one of
   * the submitted buffers). Usually 0.
   */
  __u32 instr_start_offset;

  /* Length of how many instructions we want to execute from the instruction
   * buffer from instr_start_offset. If specified as 0, the entire length of the
   * buffer is executed.
   */
  __u32 instr_len;
};


/*
 * Need to make sure these fields are aligned by 4 and 8 bytes
 */
struct pi_exec_buffer_obj {
  /*
   * GEM handle.
   */
  __u32 handle;

  /*
   * Flags for type of buffer.
   */
  __u8 flag;

  __u8 padding[3]; // just in case
};


int process_gem_exec_obj(unsigned long addr, size_t size, u8 flag,
                          struct pi_gpu *gpu, struct pi_exec_buffer *buffer);

int gpu_render_ioctl(struct drm_device *dev, void *data,
                     struct drm_file *file);

#endif
