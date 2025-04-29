/*
 * pi_gpu_test.c - Test driver for reserved memory via DT overlay
 *
 * This module registers a platform device driver for a node with the
 * compatible string "pi_gpu" as defined in your overlay. It uses the
 * reserved-memory API to map the memory region specified by the device tree.
 */

#include "asm-generic/errno-base.h"
#include "linux/err.h"
#include "linux/hdmi.h"
#include "linux/of_address.h"
#include "linux/of_reserved_mem.h"
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

struct pi_gpu {
  void __iomem *vram;
};

static int pi_gpu_probe(struct platform_device *pdev) {
  int ret;
  struct resource res;
  struct pi_gpu *gpu;
  struct device_node *node;

  dev_info(&pdev->dev, "pi_gpu: probe() called\n");

  /* Allocate driver private structure */
  gpu = devm_kzalloc(&pdev->dev, sizeof(*gpu), GFP_KERNEL);
  if (!gpu)
    return -ENOMEM;
  platform_set_drvdata(pdev, gpu);

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
    // Continue anyway if just ENODEV
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

  iowrite32(0x0F, gpu->vram);
  u32 first_word = ioread32(gpu->vram);
  dev_info(&pdev->dev, "First word in VRAM: 0x%x\n", first_word);

  return 0;
}

static int pi_gpu_remove(struct platform_device *pdev) {
  /* Clean up reserved memory resources */
  of_reserved_mem_device_release(&pdev->dev);
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
