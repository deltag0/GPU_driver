/*
 * pi_gpu_test.c - Test driver for driver features
 *
 * This module registers a platform device driver for a node with the
 * compatible string "pi_gpu" as defined in your overlay.
 */
#include "asm-generic/errno-base.h"

#include "drm/drm_device.h"
#include "drm/drm_drv.h"
#include "drm/drm_file.h"
#include "drm/drm_gem.h"
#include "drm/drm_gem_framebuffer_helper.h"
#include "drm/drm_gem_shmem_helper.h"
#include "drm/drm_mode_config.h"
#include "drm/drm_probe_helper.h"

#include "linux/err.h"
#include "linux/export.h"
#include "linux/of_address.h"
#include "linux/of_reserved_mem.h"
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#define PITCH 7680

static struct file_operations test_fops = {
    .owner = THIS_MODULE,
    .open = drm_open,
    .release = drm_release,
    .unlocked_ioctl = drm_ioctl,
    .compat_ioctl = drm_compat_ioctl,
    .poll = drm_poll,
    .read = drm_read,
    .llseek = noop_llseek,
    .mmap = drm_gem_mmap,
};

void unload(struct drm_device *drm);

struct pi_gpu {
  void __iomem *vram;
  struct drm_device drm;
};

static int testing_create(struct drm_file *file, struct drm_device *dev,
                   struct drm_mode_create_dumb *args) {
  dev_info(dev->dev, "CREATING FUNCTION CALLED");
  return drm_gem_shmem_dumb_create(file, dev, args);
}

static const struct drm_driver driver = {
    .driver_features = DRIVER_MODESET | DRIVER_GEM,
    .name = "pi_gpu",
    .desc = "PI GPU Controller",
    .date = "20240319",
    .dumb_create = testing_create,
    .gem_prime_import_sg_table = drm_gem_shmem_prime_import_sg_table,
    .fops = &test_fops,
};

static const struct drm_mode_config_funcs modecfg_funcs = {
    .fb_create = drm_gem_fb_create,
};

static int pi_gpu_probe(struct platform_device *pdev) {
  int ret;
  struct resource res;
  struct pi_gpu *gpu;
  struct device_node *node;
  struct drm_device *drm;

  dev_info(&pdev->dev, "Entered probe function");

  /* Allocate driver private structure */
  gpu = devm_drm_dev_alloc(&pdev->dev, &driver, struct pi_gpu, drm);

  if (!gpu)
    return -ENOMEM;

  if (ret)
    return ret;
  drm = &gpu->drm;

  /* Initialize reserved memory for this device.
   * This call reads the memory-region property from the DT and sets up
   * the reserved memory.
   */
  dev_info(&pdev->dev, "Initializing reserved memory...\n");
  ret = of_reserved_mem_device_init(&pdev->dev);
  if (ret) {
    dev_err(&pdev->dev, "Failed to init reserved memory: %d\n", ret);
    if (ret != -ENODEV)
      return ret;
  }

  dev_info(&pdev->dev, "Looking for memory region...\n");
  node = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);
  if (!node) {
    dev_err(&pdev->dev, "No memory-region specified\n");
    return -EINVAL;
  }
  if (IS_ERR(node)) {
    ret = PTR_ERR(node);
    dev_err(&pdev->dev, "Invalid memory-region: %d\n", ret);
    return ret;
  }

  dev_info(&pdev->dev, "Converting address to resource...\n");
  ret = of_address_to_resource(node, 0, &res);
  of_node_put(node);
  if (ret) {
    dev_err(&pdev->dev, "Failed to get memory resource: %d\n", ret);
    return ret;
  }

  dev_info(&pdev->dev, "Memory resource: %pR\n", &res);
  gpu->vram = devm_ioremap_resource(&pdev->dev, &res);
  if (IS_ERR(gpu->vram)) {
    ret = PTR_ERR(gpu->vram);
    dev_err(&pdev->dev, "Failed to map memory resource: %d\n", ret);
    return ret;
  }
  dev_info(&pdev->dev, "VRAM mapped at %p\n", gpu->vram);

  /* Map the reserved memory region.
   * Note: It uses the base address and size provided in the reserved memory.
   */
  dev_info(&pdev->dev, "pi_gpu: VRAM mapped at %p\n", gpu->vram);


  ret = drmm_mode_config_init(&gpu->drm);
  drm->mode_config.min_height = 0;
  drm->mode_config.min_width = 0;
  drm->mode_config.max_height = 1080;
  drm->mode_config.max_width = 1920;
  drm->mode_config.funcs = &modecfg_funcs;
  drm_mode_config_reset(&gpu->drm);

  platform_set_drvdata(pdev, &gpu->drm);

  // Testing iowrite without actual hardware (emulating it)
  iowrite32(0x0F, gpu->vram);
  u32 first_word = ioread32(gpu->vram);
  dev_info(&pdev->dev, "First word in VRAM: 0x%x\n", first_word);

  ret = drm_dev_register(&gpu->drm, 0);
  if (ret) {
    dev_info(&pdev->dev, "Could Not register device");
    unload(&gpu->drm);
    return ret;
  }

  dev_info(&pdev->dev, "Registered device");

  return 0;
}

void unload(struct drm_device *drm) {
  of_reserved_mem_device_release(drm->dev);
}

static int pi_gpu_remove(struct platform_device *pdev) {
  struct drm_device *drm = platform_get_drvdata(pdev);
  drm_dev_unregister(drm);
  // drm_atomic_helper_shutdown(drm);
  /* Clean up reserved memory resources */
  unload(drm);
  dev_info(&pdev->dev, "pi_gpu: removed\n");

  return 0;
}

/* Device Tree match table for this driver */
static const struct of_device_id pi_gpu_of_match[] = {
    {
        .compatible = "pi_gpu",
    },
    {/* sentinel */}};
MODULE_DEVICE_TABLE(of, pi_gpu_of_match);

/* Platform driver structure */
static struct platform_driver pi_gpu_driver = {
    .driver =
        {
            .name = "pi_gpu",
            .of_match_table = pi_gpu_of_match,
        },
    .probe = pi_gpu_probe,
    .remove = pi_gpu_remove,
};

module_platform_driver(pi_gpu_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Test driver for reserved memory via DT overlay");
