#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

__visible struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0xb89a34a1, __VMLINUX_SYMBOL_STR(module_layout) },
	{ 0x16e17547, __VMLINUX_SYMBOL_STR(kmalloc_caches) },
	{ 0x5a1ce12, __VMLINUX_SYMBOL_STR(sb_min_blocksize) },
	{ 0xd5a9a853, __VMLINUX_SYMBOL_STR(__bread) },
	{ 0x33da3355, __VMLINUX_SYMBOL_STR(inc_nlink) },
	{ 0x7d694a44, __VMLINUX_SYMBOL_STR(init_user_ns) },
	{ 0xa723154, __VMLINUX_SYMBOL_STR(mount_bdev) },
	{ 0xd04032a6, __VMLINUX_SYMBOL_STR(make_kgid) },
	{ 0x27e1a049, __VMLINUX_SYMBOL_STR(printk) },
	{ 0x4431c37c, __VMLINUX_SYMBOL_STR(__brelse) },
	{ 0xe5f801da, __VMLINUX_SYMBOL_STR(make_kuid) },
	{ 0x6210fa93, __VMLINUX_SYMBOL_STR(unlock_new_inode) },
	{ 0xf45590dc, __VMLINUX_SYMBOL_STR(kill_block_super) },
	{ 0xbdfb6dbb, __VMLINUX_SYMBOL_STR(__fentry__) },
	{ 0x3b88f3a0, __VMLINUX_SYMBOL_STR(kmem_cache_alloc_trace) },
	{ 0xd11b7c2, __VMLINUX_SYMBOL_STR(register_filesystem) },
	{ 0x37a0cba, __VMLINUX_SYMBOL_STR(kfree) },
	{ 0xa7c48408, __VMLINUX_SYMBOL_STR(d_make_root) },
	{ 0x73a1540d, __VMLINUX_SYMBOL_STR(unregister_filesystem) },
	{ 0x9200deec, __VMLINUX_SYMBOL_STR(iget_locked) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "C03252B705EA84271A8A9EE");
