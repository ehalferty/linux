#include "graphics.h"
#include <linux/module.h>
#include <linux/init.h>

#include <linux/fs.h>
#include <asm/segment.h>
#include <asm/uaccess.h>
#include <linux/buffer_head.h>
// #include <../lib/get_kernel_error_str.h>

// #include <../fs/internal.h>
#include "../include/linux/syscalls.h"
#include "../include/linux/fb.h"

extern long kern_open(const char __user * filename, int flags, umode_t mode);
extern ssize_t kernel_read(struct file *file, void *buf, size_t count, loff_t *pos);
extern ssize_t kernel_write(struct file *file, const void *buf, size_t count, loff_t *pos);


struct kobject * graphics_kobject;
static int call;
static struct kobj_attribute call_attribute;
static struct open_how how;

static ssize_t call_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf);
static ssize_t call_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count);
// static struct file *file_open(const char *path, int flags, int rights);
// static void file_close(struct file *file);
// static int file_read(struct file *file, unsigned long long offset, unsigned char *data, unsigned int size);
// static int file_sync(struct file *file);
static int __init graphics_init(void);
static void __exit graphics_exit(void);

// struct grphcs_window_class {


// 	u32 signature;		/* 0 Magic number = "VESA" */
// 	u16 version;		/* 4 */
// 	far_ptr vendor_string;	/* 6 */
// 	u32 capabilities;	/* 10 */
// 	far_ptr video_mode_ptr;	/* 14 */
// 	u16 total_memory;	/* 18 */

// 	u8 reserved[236];	/* 20 */
// } __attribute__ ((packed));


// inline struct open_how build_open_how(int flags, umode_t mode)
// {
// 	struct open_how how = {
// 		.flags = flags & VALID_OPEN_FLAGS,
// 		.mode = mode & S_IALLUGO,
// 	};

// 	/* O_PATH beats everything else. */
// 	if (how.flags & O_PATH)
// 		how.flags &= O_PATH_FLAGS;
// 	/* Modes should only be set for create-like flags. */
// 	if (!WILL_CREATE(how.flags))
// 		how.mode = 0;
// 	return how;
// }


static ssize_t call_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
        return sprintf(buf, "%d\n", call);
}

static ssize_t call_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) {
        sscanf(buf, "%du", &call);
        return count;
}

// static struct file *file_open(const char *path, int flags, int rights) {
//     struct file *filp = NULL;
//     mm_segment_t oldfs;
//     int err = 0;

//     oldfs = get_fs();
//     set_fs(get_ds());
//     filp = filp_open(path, flags, rights);
//     set_fs(oldfs);
//     if (IS_ERR(filp)) {
//         err = PTR_ERR(filp);
//         return NULL;
//     }
//     return filp;
// }

// static void file_close(struct file *file) 
// {
//     filp_close(file, NULL);
// }

// static int file_read(struct file *file, unsigned long long offset, unsigned char *data, unsigned int size) {
//     mm_segment_t oldfs;
//     int ret;

//     oldfs = get_fs();
//     set_fs(get_ds());

//     ret = vfs_read(file, data, size, &offset);

//     set_fs(oldfs);
//     return ret;
// }

// static int file_write(struct file *file, unsigned long long offset, unsigned char *data, unsigned int size) {
//     mm_segment_t oldfs;
//     int ret;

//     oldfs = get_fs();
//     set_fs(get_ds());

//     ret = vfs_write(file, data, size, &offset);

//     set_fs(oldfs);
//     return ret;
// }

// static int file_sync(struct file *file) {
//     vfs_fsync(file, 0);
//     return 0;
// }

// static struct kobj_attribute call_attribute;



// static int core_alua_write_tpg_metadata(
// 	const char *path,
// 	unsigned char *md_buf,
// 	u32 md_buf_len)
// {
// 	struct file *file = filp_open(path, O_RDWR | O_CREAT | O_TRUNC, 0600);
// 	loff_t pos = 0;
// 	int ret;

// 	if (IS_ERR(file)) {
// 		pr_err("filp_open(%s) for ALUA metadata failed\n", path);
// 		return -ENODEV;
// 	}
// 	ret = kernel_write(file, md_buf, md_buf_len, &pos);
// 	if (ret < 0)
// 		pr_err("Error writing ALUA metadata file: %s\n", path);
// 	fput(file);
// 	return (ret < 0) ? -EIO : 0;
// }


// Called as one of the last thing the kernel does before passing control to init process(es)
// Sets up the double-buffered framebuffer, renders a grey background, marks the screen as clean.
void graphics_setup(void) {
        ssize_t res;
        struct file *file;
        char * buffer;
        loff_t pos = 0;
        int i = 0;

	struct fb_fix_screeninfo finfo;
	struct fb_var_screeninfo vinfo;

        printk("SETTING UP GRAPHICS\n");

        file = filp_open("/dev/fb0", O_RDWR, 0600);
        if (file < 0) {
                panic("Failed to init graphics!! Couldn't open /dev/fb0 err=%d\n", file);
        }

        buffer = kmalloc(8192, GFP_KERNEL);

        while (i < 8192) {
                buffer[i] = 0x80;
                i++;
        }

        //ssize_t ksys_write(unsigned int fd, const char __user *buf, size_t count)
        pos = vfs_setpos(file, 0, 4096);
        res = ksys_write(file, buffer, 1024);

        // ssize_t kernel_write(struct file *file, const void *buf, size_t count, loff_t *pos)


        printk("------ res=%d\n", res);

        // TODO: What would error look like?
        // res = kernel_write

        // printk("------ file=%d\n", file);


        // res = kernel_write(struct file *file, const void *buf, size_t count, loff_t *pos)

        // extern ssize_t kernel_write(struct file *file, const void *buf, size_t count, loff_t *pos);



        // fb_fd = do_sys_open(AT_FDCWD, "/dev/fb0", 0, O_RDWR);

        // fs = current_thread_info()->addr_limit;
        // set_fs (get_ds());

        // fb_fd = do_sys_open(AT_FDCWD, "/dev/fb0", O_RDWR, 0);
        // printk("fb_fd=%d\n", fb_fd);
	// kern_ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo);
        
        // current_thread_info()->addr_limit = fs;


        // int errno2;

        // long do_sys_open(int dfd, const char __user *filename, int flags, umode_t mode)
        // {
        //         struct open_how how = build_open_how(flags, mode);
        //         return do_sys_openat2(dfd, filename, &how);
        // }

        // struct open_how how = build_open_how(0, O_RDWR);
        // printk("------ do_sys_open result=%ld", fb_fd);
        // if (fb_fd == -1) {
        //         errno2 = errno;
        //         printk("Failed to open /dev/fb0! errno=%d strerror=%s\n", errno, get_kernel_error_str(errno));
        //         exit_handler();
        // } else {
        //         printk("DID OPEN /dev/fb0 FROM KERNEL!!!\n");
        // }
        // // int fb_fd = do_sys_openat2(AT_FDCWD, "/dev/fb0", &how);

	// int fb_fd = open("/dev/fb0", O_RDWR);
}

static int __init graphics_init(void) {
        int error;
        printk("IN-KERNEL GRAPHICS ENABLED\n");

        graphics_kobject = kobject_create_and_add("graphics", kernel_kobj);
        if(!graphics_kobject) {
                return -ENOMEM;
        }

        call_attribute.attr.name = "call";
        call_attribute.attr.mode = 0660;
        call_attribute.show = call_show;
        call_attribute.store = call_store;

        error = sysfs_create_file(graphics_kobject, &call_attribute.attr);
        if (error) {
                printk("Failed to create node: /sys/kernel/graphics/call \n");
                return -1;
        }
        // TODO: Should clear screen here to show that graphics is loaded
        return 0;
}

static void __exit graphics_exit(void) {
        kobject_put(graphics_kobject);
        printk("Graphics module uninitialized successfully \n");
}

module_init(graphics_init);
module_exit(graphics_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("edwardhalferty");
