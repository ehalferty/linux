#include <linux/module.h>
#include <linux/init.h>

struct kobject * graphics_kobject;
static int foo;

static ssize_t foo_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
        return sprintf(buf, "%d\n", foo);
}

static ssize_t foo_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) {
        sscanf(buf, "%du", &foo);
        return count;
}

static struct kobj_attribute foo_attribute;

static int __init graphics_init (void) {
        pr_debug("IN-KERNEL GRAPHICS ENABLED\n");

        graphics_kobject = kobject_create_and_add("graphics", kernel_kobj);
        if(!graphics_kobject) {
                return -ENOMEM;
        }

        foo_attribute.attr.name = "foo";
        foo_attribute.attr.mode = 0660;
        foo_attribute.show = foo_show;
        foo_attribute.store = foo_store;

        int error = sysfs_create_file(graphics_kobject, &foo_attribute.attr);
        if (error) {
                pr_debug("failed to create the foo file in /sys/kernel/graphics \n");
        }
        return 0;
}

static void __exit graphics_exit (void)
{
        kobject_put(graphics_kobject);
        pr_debug ("Graphics module uninitialized successfully \n");
}

module_init(graphics_init);
module_exit(graphics_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("edwardhalferty");
