#include "graphics.h"
#include <linux/module.h>
#include <linux/init.h>

#include <linux/fs.h>
#include <asm/segment.h>
#include <asm/uaccess.h>
#include <linux/buffer_head.h>

#include "../include/linux/syscalls.h"
#include "../include/linux/fb.h"

extern int num_registered_fb;
extern struct fb_info *registered_fb[FB_MAX] __read_mostly;

struct kobject * graphics_kobject;
static int call;
static struct kobj_attribute call_attribute;

static ssize_t call_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf);
static ssize_t call_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count);
static int __init graphics_init(void);
static void __exit graphics_exit(void);
uint32_t pixel_color(uint8_t r, uint8_t g, uint8_t b, struct fb_var_screeninfo *vinfo);


static ssize_t call_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
        return sprintf(buf, "%d\n", call);
}

static ssize_t call_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) {
        sscanf(buf, "%du", &call);
        return count;
}

uint32_t pixel_color(uint8_t r, uint8_t g, uint8_t b, struct fb_var_screeninfo *vinfo) {
	return (r<<vinfo->red.offset) | (g<<vinfo->green.offset) | (b<<vinfo->blue.offset);
}

// Called as one of the last thing the kernel does before passing control to init process(es)
// Sets up the double-buffered framebuffer, renders a grey background, marks the screen as clean.
void graphics_setup(void) {
        struct fb_info *fb = registered_fb[0];
        long screensize, location;
        int x = 0, y = 0, val = 0;
	struct fb_fix_screeninfo *finfo = &(fb->fix);
	struct fb_var_screeninfo *vinfo = &(fb->var);
        ssize_t res;

        printk("SETTING UP GRAPHICS\n");
        
        // TODO: Should check if num_registered_fb > 1 and decide which to show??!
        printk("num_registered_fb=%d\n", num_registered_fb);
        printk("xres=%d yres=%d xoffset=%d yoffset=%d bits_per_pixel=%d line_length=%d\n", vinfo->xres, vinfo->yres, vinfo->xoffset, vinfo->yoffset, vinfo->bits_per_pixel, finfo->line_length);
        screensize = vinfo->yres_virtual * finfo->line_length;
        while (x < screensize) {
                *((uint32_t*)(fb->screen_base + x)) = 0x80;
                x++;
        }

        // while (y < vinfo->yres) {
        //         while (x < vinfo->xres - 1) {
        //                 location = (x + vinfo->xoffset) * (vinfo->bits_per_pixel / 8) + (y + vinfo->yoffset) * finfo->line_length;
        //                 *((uint32_t*)(fb->screen_base + location)) = pixel_color(
        //                         (x > y) ? val : 0xFF - val,
        //                         (y > x) ? val : 0xFF - val,
        //                         (x == y) ? val : 0xFF - val,
        //                         vinfo);
        //                 x++;
        //                 val++;
        //         }
        //         y++;
        // }
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













                        // res = fb_write()
                        // static ssize_t fb_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
                        // *((uint32_t*)(fbp + location)) = pixel_color(
                        //         (x > y) ? val : 0xFF - val,
                        //         (y > x) ? val : 0xFF - val,
                        //         (x == y) ? val : 0xFF - val,
                        //         &vinfo);
// extern long kern_open(const char __user * filename, int flags, umode_t mode);
// extern ssize_t kernel_read(struct file *file, void *buf, size_t count, loff_t *pos);
// extern ssize_t kernel_write(struct file *file, const void *buf, size_t count, loff_t *pos);
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
// #include <../lib/get_kernel_error_str.h>
// static struct open_how how;
// #include <../fs/internal.h>
// static struct file *file_open(const char *path, int flags, int rights);
// static void file_close(struct file *file);
// static int file_read(struct file *file, unsigned long long offset, unsigned char *data, unsigned int size);
// static int file_sync(struct file *file);


        // ssize_t res;
        // struct file *file;
        // char * buffer;
        // loff_t pos = 0;
        // int i = 0;

	// struct fb_fix_screeninfo finfo;
	// struct fb_var_screeninfo vinfo;


        // file = filp_open("/dev/fb0", O_RDWR, 0600);
        // if (file < 0) {
        //         panic("Failed to init graphics!! Couldn't open /dev/fb0 err=%d\n", file);
        // }

        // buffer = kmalloc(8192, GFP_KERNEL);

        // while (i < 8192) {
        //         buffer[i] = 0x80;
        //         i++;
        // }

        // //ssize_t ksys_write(unsigned int fd, const char __user *buf, size_t count)
        // pos = vfs_setpos(file, 0, 4096);
        // res = ksys_write(file, buffer, 1024);

        // // ssize_t kernel_write(struct file *file, const void *buf, size_t count, loff_t *pos)


        // printk("------ res=%d\n", res);

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

// struct fb_info *registered_fb[FB_MAX] __read_mostly;

// struct fb_info {
// 	atomic_t count;
// 	int node;
// 	int flags;
// 	/*
// 	 * -1 by default, set to a FB_ROTATE_* value by the driver, if it knows
// 	 * a lcd is not mounted upright and fbcon should rotate to compensate.
// 	 */
// 	int fbcon_rotate_hint;
// 	struct mutex lock;		/* Lock for open/release/ioctl funcs */
// 	struct mutex mm_lock;		/* Lock for fb_mmap and smem_* fields */
// 	struct fb_var_screeninfo var;	/* Current var */
// 	struct fb_fix_screeninfo fix;	/* Current fix */
// 	struct fb_monspecs monspecs;	/* Current Monitor specs */
// 	struct work_struct queue;	/* Framebuffer event queue */
// 	struct fb_pixmap pixmap;	/* Image hardware mapper */
// 	struct fb_pixmap sprite;	/* Cursor hardware mapper */
// 	struct fb_cmap cmap;		/* Current cmap */
// 	struct list_head modelist;      /* mode list */
// 	struct fb_videomode *mode;	/* current mode */

// #if IS_ENABLED(CONFIG_FB_BACKLIGHT)
// 	/* assigned backlight device */
// 	/* set before framebuffer registration,
// 	   remove after unregister */
// 	struct backlight_device *bl_dev;

// 	/* Backlight level curve */
// 	struct mutex bl_curve_mutex;
// 	u8 bl_curve[FB_BACKLIGHT_LEVELS];
// #endif
// #ifdef CONFIG_FB_DEFERRED_IO
// 	struct delayed_work deferred_work;
// 	struct fb_deferred_io *fbdefio;
// #endif

// 	const struct fb_ops *fbops;
// 	struct device *device;		/* This is the parent */
// 	struct device *dev;		/* This is this fb device */
// 	int class_flag;                    /* private sysfs flags */
// #ifdef CONFIG_FB_TILEBLITTING
// 	struct fb_tile_ops *tileops;    /* Tile Blitting */
// #endif
// 	union {
// 		char __iomem *screen_base;	/* Virtual address */
// 		char *screen_buffer;
// 	};
// 	unsigned long screen_size;	/* Amount of ioremapped VRAM or 0 */
// 	void *pseudo_palette;		/* Fake palette of 16 colors */
// #define FBINFO_STATE_RUNNING	0
// #define FBINFO_STATE_SUSPENDED	1
// 	u32 state;			/* Hardware state i.e suspend */
// 	void *fbcon_par;                /* fbcon use-only private area */
// 	/* From here on everything is device dependent */
// 	void *par;
// 	/* we need the PCI or similar aperture base/size not
// 	   smem_start/size as smem_start may just be an object
// 	   allocated inside the aperture so may not actually overlap */
// 	struct apertures_struct {
// 		unsigned int count;
// 		struct aperture {
// 			resource_size_t base;
// 			resource_size_t size;
// 		} ranges[0];
// 	} *apertures;

// 	bool skip_vt_switch; /* no VT switch on suspend/resume required */
// };