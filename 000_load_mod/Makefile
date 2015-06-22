ME2FS_SOURCE = me2fs_main.c

obj-m += me2fs.o
me2fs-objs := $(ME2FS_SOURCE:.c=.o)

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

mount:
	sudo mount -t me2fs -o loop ../ext2.img ../mnt

umount:
	sudo umount ../mnt

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
