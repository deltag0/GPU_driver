#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/export-internal.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

#ifdef CONFIG_UNWINDER_ORC
#include <asm/orc_header.h>
ORC_HEADER;
#endif

BUILD_SALT;
BUILD_LTO_INFO;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif



static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0xfd69799a, "__platform_driver_register" },
	{ 0xfe344697, "__drm_gem_destroy_shadow_plane_state" },
	{ 0x37a0cba, "kfree" },
	{ 0x1a704aed, "platform_driver_unregister" },
	{ 0xdcb764ad, "memset" },
	{ 0xac1660cf, "__of_parse_phandle_with_args" },
	{ 0xf0fdf6cb, "__stack_chk_fail" },
	{ 0x2bd5b16e, "kmalloc_caches" },
	{ 0x4fede42a, "kmalloc_trace" },
	{ 0xfc4f48f4, "__drm_gem_duplicate_shadow_plane_state" },
	{ 0x4829a47e, "memcpy" },
	{ 0x122c3a7e, "_printk" },
	{ 0x8bd6097c, "__devm_drm_dev_alloc" },
	{ 0xf073b296, "drmm_mode_config_init" },
	{ 0x87e9be6e, "platform_get_resource" },
	{ 0x83d9c412, "devm_ioremap_resource" },
	{ 0x2f19d425, "_dev_info" },
	{ 0xea6bb0e0, "of_reserved_mem_device_init_by_idx" },
	{ 0x2b6ae9eb, "dma_set_mask" },
	{ 0x7431ba6e, "dma_set_coherent_mask" },
	{ 0x8123acc, "of_address_to_resource" },
	{ 0x7be56444, "dma_alloc_attrs" },
	{ 0xfd71104d, "drm_universal_plane_init" },
	{ 0x9633657, "drm_plane_enable_fb_damage_clips" },
	{ 0x49350dd7, "drmm_crtc_init_with_planes" },
	{ 0x613d34f7, "drm_dev_enter" },
	{ 0x56470118, "__warn_printk" },
	{ 0xd7c63583, "drm_atomic_helper_damage_iter_init" },
	{ 0xb6a6b711, "drm_fb_clip_offset" },
	{ 0x32dddcb0, "drm_fb_blit" },
	{ 0x6b5c2b06, "drm_atomic_helper_damage_iter_next" },
	{ 0xe8a034df, "drm_dev_exit" },
	{ 0x4ecb3f21, "__drm_gem_reset_shadow_plane" },
	{ 0x70d854d6, "drm_atomic_helper_check_plane_state" },
	{ 0x55eb38da, "drm_format_info" },
	{ 0x6910e4cd, "drm_format_info_min_pitch" },
	{ 0x8dcde343, "drm_gem_fb_create" },
	{ 0xb060f395, "drm_atomic_helper_check" },
	{ 0x94e54ce4, "drm_atomic_helper_commit" },
	{ 0xf1331fa6, "drm_atomic_helper_update_plane" },
	{ 0x3a39fe32, "drm_atomic_helper_disable_plane" },
	{ 0x7ad75548, "drm_plane_cleanup" },
	{ 0x5e6c66e, "drm_gem_begin_shadow_fb_access" },
	{ 0x4cdcaa4e, "drm_gem_end_shadow_fb_access" },
	{ 0xe7f85674, "drm_atomic_helper_crtc_reset" },
	{ 0x58b3995e, "drm_crtc_cleanup" },
	{ 0xecd59bec, "drm_atomic_helper_set_config" },
	{ 0xdbc60921, "drm_atomic_helper_page_flip" },
	{ 0x76e1f595, "drm_atomic_helper_crtc_duplicate_state" },
	{ 0xe38030f4, "drm_atomic_helper_crtc_destroy_state" },
	{ 0x773354b7, "module_layout" },
};

MODULE_INFO(depends, "drm_kms_helper,drm");


MODULE_INFO(srcversion, "4D00FD46DCF1267F3284524");
