ME2FS_SOURCE = me2fs_main.c me2fs_super.c me2fs_block.c me2fs_inode.c \
			   me2fs_dir.c me2fs_namei.c me2fs_file.c me2fs_ialloc.c

obj-m += me2fs.o
me2fs-objs := $(ME2FS_SOURCE:.c=.o)

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

insmod:
	sudo insmod me2fs.ko

rmmod:
	sudo rmmod me2fs.ko

mount:
	sudo mount -t me2fs -o loop ../ext2.img ../mnt

mount2:
	sudo insmod me2fs.ko
	sudo mount -t me2fs -o loop ../ext2.img ../mnt

umount:
	sudo umount ../mnt

umount2:
	sudo umount ../mnt
	sudo rmmod me2fs.ko

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
