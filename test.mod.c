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
	{ 0xb0ae3a88, "of_reserved_mem_device_release" },
	{ 0x2f19d425, "_dev_info" },
	{ 0x1a704aed, "platform_driver_unregister" },
	{ 0xdcb764ad, "memset" },
	{ 0xac1660cf, "__of_parse_phandle_with_args" },
	{ 0xf0fdf6cb, "__stack_chk_fail" },
	{ 0xf61c822d, "devm_kmalloc" },
	{ 0xea6bb0e0, "of_reserved_mem_device_init_by_idx" },
	{ 0xae383b88, "_dev_err" },
	{ 0x8123acc, "of_address_to_resource" },
	{ 0x1bc00274, "of_node_put" },
	{ 0x83d9c412, "devm_ioremap_resource" },
	{ 0x773354b7, "module_layout" },
};

MODULE_INFO(depends, "");

MODULE_ALIAS("of:N*T*Cpi_gpu");
MODULE_ALIAS("of:N*T*Cpi_gpuC*");

MODULE_INFO(srcversion, "79E78D8A4DC3B077615C42E");
