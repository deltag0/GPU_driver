/*
 * Description:
 * This file contains the implementation of the fake kernel GPU
 *
 *
 
 */
#include "linux/init.h"
#include "linux/module.h"
#include "linux/platform_device.h"
#include <drm/drm_device.h>
#include <drm/drm_simple_kms_helper.h>

MODULE_LICENSE("GPL");

struct FakeGpu {
  struct platform_device *pdev;
} fake_gpu;

// #ifdef TEST_GPU

// __init functions place the function of we're writing it with somewhere
// special in memory So, since we're writing LKM, when the module is loaded,
// then the init functions run once, and then memory is freed for them.
//
// __exit cleans it up once the module is unloaded
static int __init fake_gpu_init(void) {
  fake_gpu.pdev = platform_device_alloc("fake_gpu", -1);
  struct platform_device *pdev = fake_gpu.pdev;

  if (!fake_gpu.pdev) {
    return -ENOMEM;
  }

  // Adds platform device to the device hierarchy (kind of registrates
  // the device)
  return platform_device_add(fake_gpu.pdev);
}

static void __exit fake_gpu_exit(void) {
  // we can only call unregister after we've added (registered a device )
  // a device
  platform_device_unregister(fake_gpu.pdev);
}


module_init(fake_gpu_init);
module_exit(fake_gpu_exit);

// #endif
