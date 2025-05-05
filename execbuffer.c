#include "asm-generic/errno-base.h"
#include "asm-generic/int-ll64.h"
#include "drm/drm_atomic_helper.h"
#include "drm/drm_crtc.h"
#include "drm/drm_device.h"
#include "drm/drm_drv.h"
#include "drm/drm_format_helper.h"
#include "drm/drm_fourcc.h"
#include "drm/drm_framebuffer.h"
#include "drm/drm_gem.h"
#include "drm/drm_gem_framebuffer_helper.h"
#include "drm/drm_gem_shmem_helper.h"
#include "drm/drm_ioctl.h"
#include "drm/drm_mode_config.h"
#include "linux/err.h"
#include "linux/gfp_types.h"
#include "linux/iosys-map.h"
#include "linux/kernel.h"
#include "linux/printk.h"
#include "linux/slab.h"
#include "linux/uaccess.h"
#include <linux/platform_device.h>

#include "driver.h"
#include "execbuffer.h"

#define INS_OBJ 0x00
#define FRM_OBJ 0x01

void process_gem_exec_obj(unsigned long addr, size_t size, u8 flag,
                          struct pi_gpu *gpu, struct pi_exec_buffer *buffer) {
  switch (flag) {
  case INS_OBJ:
    *(gpu->vram + INS_BUFFER_OFFSET) = get_64_lo(addr);
    *(gpu->vram + INS_BUFFER_OFFSET + 1) = get_64_hi(addr);
    *(gpu->vram + INS_BUFFER_LEN_OFFSET) = buffer->instr_len == 0 ? size : buffer->instr_len;
    *(gpu->vram + INS_BUFFER_START_OFFSET) = buffer->instr_start_offset;
    break;
  case FRM_OBJ:
    *(gpu->vram + FRM_BUFFER_OFFSET) = get_64_lo(addr);
    *(gpu->vram + FRM_BUFFER_OFFSET + 1) = get_64_hi(addr);
    *(gpu->vram + FRM_BUFFER_LEN_OFFSET) = get_64_lo(size);
    *(gpu->vram + FRM_BUFFER_LEN_OFFSET + 1) = get_64_hi(size);
    break;
  }
}

/*
 * data argument is a pointer that the kernel already converted for us into the
 kernel address space
 * (by doing copy_from_user). So we can cast it to a struct we defined in
 user-space (that's also defined)
 * in the kernel module.
 * e.g.
 *struct drm_my_ioctl_args {
    __u32 some_cool_argument;
  };

  This function transforms data into a &pi_exec_buffer structure. That structure
 contains a list of pointers to shmem GEM buffer objects. This is similar to:
 &struct drm_i915_execbuffer2 defined in linux/include/uapi/drm/i915_drm.h An
 example of using the struct can be found in
 linux/drivers/gpu/drm/i915/gem/i915_gem_execbuffer.c
 */
int gpu_render_ioctl(struct drm_device *dev, void *data,
                     struct drm_file *file) {
  struct pi_gpu *gpu = to_gpu(dev);

  if (IS_ERR(gpu))
    return -ENODEV;

  struct pi_exec_buffer *args = data;
  struct page **pages;
  struct sg_table *table;
  struct drm_gem_shmem_object *shmem_obj;

  struct iosys_map *bo_va;
  size_t bo_size = 0;
  int ret;
  u32 handle;

  u8 *va;
  unsigned long addr;

  size_t buffer_ptrs_size =
      args->num_buffers * sizeof(struct pi_exec_buffer_obj);
  struct pi_exec_buffer_obj *bo_ptr = kmalloc(buffer_ptrs_size, GFP_KERNEL);

  if (IS_ERR(bo_ptr))
    return -EINVAL;

  ret =
      copy_from_user(bo_ptr, u64_to_user_ptr(args->buffers), buffer_ptrs_size);

  if (ret)
    goto release;

  for (int i = 0; i < args->num_buffers; i++) {
    handle = (bo_ptr + i)->handle;
    struct drm_gem_object *obj = drm_gem_object_lookup(file, handle);

    if (IS_ERR(obj))
      return -EINVAL;

    shmem_obj = to_drm_gem_shmem_obj(obj);

    if (IS_ERR(shmem_obj))
      return -EINVAL;

    bo_size = shmem_obj->base.size;
    // This pins the pages into memory
    ret = drm_gem_shmem_vmap(shmem_obj, bo_va);

    if (ret)
      return ret;

    if (bo_va->is_iomem) {
      printk(KERN_CRIT "Not supposed to be io mem\n");
      goto unmap_release;
    }

    printk(KERN_INFO "Correctly mapped Buffer Object\n");

    if (obj->dev != dev) {
      ret = -ENODEV;
      goto unmap_release;
    }

    va = bo_va->vaddr;
    addr = (unsigned long)va;

    process_gem_exec_obj(addr, bo_size, bo_ptr->flag, gpu, args);
  }

  // execute_bfr();
unmap_release:
  drm_gem_shmem_vunmap(shmem_obj, bo_va);
release:
  kfree(bo_ptr);

  return ret;
}
