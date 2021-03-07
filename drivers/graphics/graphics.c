#include <linux/module.h>
#include <linux/init.h>

static int __init graphics_init (void)
{
        pr_debug("Graphics module initialized successfully \n");
        return 0;
}

static void __exit graphics_exit (void)
{
        pr_debug ("Graphics module uninitialized successfully \n");
}

module_init(graphics_init);
module_exit(graphics_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("edwardhalferty");
