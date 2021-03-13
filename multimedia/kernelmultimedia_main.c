#include "kernelmultimedia_main.h"
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <asm/segment.h>
#include <asm/uaccess.h>
#include <linux/buffer_head.h>
#include "../include/linux/syscalls.h"
#include "../include/linux/fb.h"

struct ReturnValue {
        struct ReturnValue *next;
        uint8_t *data;
        uint32_t size;
        uint32_t pid;
};
struct ReturnValue *returnValues = 0;
struct WindowRef {
        struct WindowRef *next;
        uint32_t pid;
        uint64_t addr;
};
struct __attribute__((__packed__)) Message {
    uint32_t code;
    uint64_t window;
    uint64_t argA;
    uint64_t argB;
    uint32_t handled;
    uint32_t miscDataLength;
    uint8_t * miscData;
};
#define NUM_MESSAGES_PER_PRIORITY 16
struct MessageQueueRef {
        struct MessageQueueRef *next;
        uint32_t pid;
        uint64_t addr;
        struct Message highPriority[NUM_MESSAGES_PER_PRIORITY];
        struct Message mediumPriority[NUM_MESSAGES_PER_PRIORITY];
        struct Message lowPriority[NUM_MESSAGES_PER_PRIORITY];
        struct Message lowestPriority[NUM_MESSAGES_PER_PRIORITY];
};
struct TempMessage { // TODO: Maybe use this for API calls too
    uint32_t code;
    uint64_t window;
    uint64_t argA;
    uint64_t argB;
    uint32_t handled;
    uint32_t miscDataLength;
    uint8_t * miscData;
} __packed;
struct TempMessageQueue {
    struct TempMessage highPriority[NUM_MESSAGES_PER_PRIORITY];
    struct TempMessage mediumPriority[NUM_MESSAGES_PER_PRIORITY];
    struct TempMessage lowPriority[NUM_MESSAGES_PER_PRIORITY];
    struct TempMessage lowestPriority[NUM_MESSAGES_PER_PRIORITY];
} __packed;
struct TempMessageQueue tempMessageQueue;
static struct WindowRef *windowRefs = 0;
static struct MessageQueueRef *messageQueueRefs = 0;
extern struct fb_info *registered_fb[FB_MAX] __read_mostly; // TODO: __read_mostly? That's not true... is it?
struct kobject * kernelmultimedia_kobject;
static struct kobj_attribute call_attribute;
static struct fb_info *fb;
static long screensize;
static struct fb_fix_screeninfo *finfo;
static struct fb_var_screeninfo *vinfo;
static ssize_t call_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf);
static ssize_t call_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count);
static int __init kernelmultimedia_init(void);
static void __exit kernelmultimedia_exit(void);
static void drawNewWindow(struct WindowRef *windowRef);
static uint32_t pixel_color(uint8_t r, uint8_t g, uint8_t b, struct fb_var_screeninfo *vinfo);

static void drawNewWindow(struct WindowRef *windowRef) {}
static ssize_t call_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
        int size, pid =  task_pid_nr(current);
        struct ReturnValue *prev = 0;
        struct ReturnValue *returnValue = returnValues; // Go through return values to see if there is one for that pid.
        while (returnValue != NULL) {
                if (returnValue->pid == pid) break;
                prev = returnValue;
                returnValue = returnValue->next;
        }
        if (returnValue != NULL) {
                if (prev == NULL) { returnValues = returnValue->next; }
                else if (returnValue->next != NULL) { prev->next = returnValue->next; }
                memcpy(buf, returnValue->data, returnValue->size);
                size = returnValue->size;
                kfree(returnValue);
                return size;
        }
        return 0;
}
#define KERNELMULTIMEDIA_API_REGISTER_WINDOW 1001
#define KERNELMULTIMEDIA_API_REGISTER_MESSAGE_QUEUE 1002
#define KERNELMULTIMEDIA_API_CHECK_MESSAGES 1003
#define KERNELMULTIMEDIA_MSG_WELCOME 9998
#define KERNELMULTIMEDIA_MSG_HELLO 9999
#define KERNELMULTIMEDIA_MSG_WINDOW_INIT 20000
static ssize_t call_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) {
        int i, j, pid =  task_pid_nr(current);
        uint32_t code = 0, miscDataLength = 0;
        uint64_t argA = 0, argB = 0;
        struct MessageQueueRef *messageQueueRef;
        struct ReturnValue *returnValue, *newReturnValue;
        for (i = 0; i < 4; i++) { code |= ((buf[i] << (i * 8)) & (0xFF << (i * 8))); }
        for (i = 0; i < 8; i++) { argA |= ((buf[i + 4] << (i * 8)) & (0xFF << (i * 8))); }
        for (i = 0; i < 8; i++) { argB |= ((buf[i + 12] << (i * 8)) & (0xFF << (i * 8))); }
        for (i = 0; i < 4; i++) { miscDataLength |= ((buf[i + 20] << (i * 8)) & (0xFF << (i * 8))); }
        // printk("RECEIVED KERNELMULTIMEDIA API CALL: code=%u argA=%08llx argB=%08llx miscDatLen=%u\n", code, argA, argB, miscDataLength);
        if (code == KERNELMULTIMEDIA_API_REGISTER_WINDOW) {
                struct WindowRef *windowRef, *newWindowRef;
                struct Message *newMessage;
                printk("RECEIVED KERNELMULTIMEDIA_API_REGISTER_WINDOW from PID %d ADDR=0x%08llx\n", pid, argA);
                newWindowRef = (struct WindowRef *)kmalloc(sizeof(struct WindowRef), GFP_KERNEL);
                windowRef = windowRefs;
                if (windowRef == NULL) { windowRef = newWindowRef; }
                else {
                        while (windowRef->next != NULL) {
                                if (windowRef->addr == argA) { /*Already registered this window? TODO: Ignore or error? */ }
                                windowRef = windowRef->next;
                        }
                        windowRef->next = newWindowRef;
                }
                // Send an init message to the window. First, find internal queues for this thread
                messageQueueRef = messageQueueRefs;
                if (messageQueueRef != NULL) {
                        while (messageQueueRef != NULL) {
                                if (messageQueueRef->pid == pid) break;
                                messageQueueRef = messageQueueRef->next;
                        }
                }
                if (messageQueueRef == NULL) { /* TODO */ printk("Uh-oh, no message queue for this thread yet, but tried to create a window. That's an error!\n"); }
                else {
                        printk("Found message queue for PID\n");
                        newMessage = (struct Message *)kmalloc(sizeof(struct Message), GFP_KERNEL);
                        newMessage->code = KERNELMULTIMEDIA_MSG_WINDOW_INIT;
                        newMessage->window = argA;
                        newMessage->handled = 0;
                        for (i = 0; i < NUM_MESSAGES_PER_PRIORITY; i++) {
                                if (messageQueueRef->lowPriority[i].code == 0 || messageQueueRef->lowPriority[i].handled == 1) {
                                        memcpy(&messageQueueRef->lowPriority[i], newMessage, sizeof(struct Message));
                                        break;
                                }
                        }
                }
                drawNewWindow(newWindowRef); // TODO: Should actually do this after the window callback gets to do some setup (send it a pre-draw init message)
                newReturnValue = (struct ReturnValue *)kmalloc(sizeof(struct ReturnValue), GFP_KERNEL);
                newReturnValue->pid = pid;
                newReturnValue->next = NULL;
                newReturnValue->size = 16;
                newReturnValue->data = (char *)kmalloc(16, GFP_KERNEL);
                sprintf(newReturnValue->data, "Hello, world!");
                returnValue = returnValues;
                if (returnValue == NULL) { returnValues = newReturnValue; }
                else {
                        while (returnValue->next != NULL) {
                                if (returnValue->pid == pid) {
                                        printk("Ruh-roh, already a return value for that PID. This is a big problem... pid=%d\n", pid);
                                        // This will probably happen if someone has more than one thread calling this API.
                                        // Probably best to discourage that, or use thread IDs somehow to keep track of return values.
                                        break;
                                }
                                returnValue = returnValue->next;
                        }
                        returnValue->next = newReturnValue;
                }
        } else if (code == KERNELMULTIMEDIA_API_REGISTER_MESSAGE_QUEUE) {
                struct MessageQueueRef *messageQueueRef, *newMessageQueueRef;
                printk("RECEIVED KERNELMULTIMEDIA_API_REGISTER_MESSAGE_QUEUE from PID %d ADDR=0x%08llx\n", pid, argA);
                newMessageQueueRef = (struct MessageQueueRef *)kmalloc(sizeof(struct MessageQueueRef), GFP_KERNEL);
                for (i = 0; i < NUM_MESSAGES_PER_PRIORITY; i++) { newMessageQueueRef->highPriority[i].code = 0; }
                for (i = 0; i < NUM_MESSAGES_PER_PRIORITY; i++) { newMessageQueueRef->mediumPriority[i].code = 0; }
                for (i = 0; i < NUM_MESSAGES_PER_PRIORITY; i++) { newMessageQueueRef->lowPriority[i].code = 0; }
                for (i = 0; i < NUM_MESSAGES_PER_PRIORITY; i++) { newMessageQueueRef->lowestPriority[i].code = 0; }
                // Put a high-pririty hello-world message into the queue
                newMessageQueueRef->highPriority[0].code = KERNELMULTIMEDIA_MSG_WELCOME;
                newMessageQueueRef->highPriority[0].handled = 0;
                newMessageQueueRef->pid = pid;
                newMessageQueueRef->addr = argA;
                messageQueueRef = messageQueueRefs;
                if (messageQueueRef == NULL) { messageQueueRefs = newMessageQueueRef; }
                else {
                        while (messageQueueRef->next != NULL) {
                                if (messageQueueRef->addr == argA) { /*Already registered this window? TODO: Ignore or error? */ }
                                messageQueueRef = messageQueueRef->next;
                        }
                        messageQueueRef->next = newMessageQueueRef;
                }
                // TODO: What to return?
                newReturnValue = (struct ReturnValue *)kmalloc(sizeof(struct ReturnValue), GFP_KERNEL);
                newReturnValue->pid = pid;
                newReturnValue->next = NULL;
                newReturnValue->size = 16;
                newReturnValue->data = (char *)kmalloc(16, GFP_KERNEL);
                sprintf(newReturnValue->data, "Hello, world!");
                returnValue = returnValues;
                if (returnValue == NULL) { returnValues = newReturnValue; }
                else {
                        while (returnValue->next != NULL) {
                                if (returnValue->pid == pid) {
                                        // This will probably happen if someone has more than one thread calling this API.
                                        // Probably best to discourage that, or use thread IDs somehow to keep track of return values.
                                        printk("Ruh-roh, already a return value for that PID. This is a big problem... pid=%d\n", pid);
                                        break;
                                }
                                returnValue = returnValue->next;
                        }
                        returnValue->next = newReturnValue;
                }
        } else if (code == KERNELMULTIMEDIA_API_CHECK_MESSAGES) {
                struct MessageQueueRef *messageQueueRef;
                uint32_t res, newMessages = 0;
                // When messages are sent to a thread, the kernel queues them up in an internal queue. When this call happens,
                // the kernel can safely assume that the user isn't messing with messages. The kernel copies any pending
                // messages from it's own queue into the user's queue.
                
                // Find internal queues for this thread
                messageQueueRef = messageQueueRefs;
                if (messageQueueRef != NULL) {
                        while (messageQueueRef != NULL) {
                                if (messageQueueRef->pid == pid) break;
                                messageQueueRef = messageQueueRef->next;
                        }
                }
                if (messageQueueRef != NULL) {
                        // Check if there are any messages on the internal queue for this thread.
                        for (i = 0; i < NUM_MESSAGES_PER_PRIORITY; i++) {
                                struct Message *highPriority = &(messageQueueRef->highPriority[i]), *mediumPriority = &(messageQueueRef->mediumPriority[i]);
                                struct Message *lowPriority = &(messageQueueRef->lowPriority[i]), *lowestPriority = &(messageQueueRef->lowestPriority[i]);
                                if (highPriority->code != 0 || mediumPriority->code != 0 || lowPriority->code != 0 || lowestPriority->code != 0) {
                                        newMessages = 1;
                                        break;
                                }
                        }
                        if (newMessages) {
                                // Clone the user message queue
                                res = copy_from_user(&tempMessageQueue, (struct TempMessageQueue *)messageQueueRef->addr, sizeof(struct TempMessageQueue));
                                // Go back through the list, and move messages from the internal queue to the temp queue.
                                for (i = 0; i < NUM_MESSAGES_PER_PRIORITY; i++) {
                                        struct Message *highPriority = &(messageQueueRef->highPriority[i]), *mediumPriority = &(messageQueueRef->mediumPriority[i]);
                                        struct Message *lowPriority = &(messageQueueRef->lowPriority[i]), *lowestPriority = &(messageQueueRef->lowestPriority[i]);
                                        if (highPriority->code) {
                                                // Go through the temp queue to find a spot
                                                uint64_t foundASlot = 0;
                                                for (j = 0; j < NUM_MESSAGES_PER_PRIORITY; j++) {
                                                        if (tempMessageQueue.highPriority[j].code == 0) {
                                                                // Copy from the internal queue
                                                                memcpy(&tempMessageQueue.highPriority[j], highPriority, sizeof(struct TempMessageQueue));
                                                                // Remove from internal queue
                                                                highPriority->code = 0;
                                                                foundASlot = 1;
                                                                break;
                                                        }
                                                }
                                                if (!foundASlot) { /* TODO: Do something if we can't find a place for it? (other priorities too) */ }
                                        }
                                        if (mediumPriority->code) {
                                                // Go through the temp queue to find a spot
                                                uint64_t foundASlot = 0;
                                                for (j = 0; j < NUM_MESSAGES_PER_PRIORITY; j++) {
                                                        if (tempMessageQueue.mediumPriority[j].code == 0) {
                                                                // Copy from the internal queue
                                                                memcpy(&tempMessageQueue.mediumPriority[j], mediumPriority, sizeof(struct TempMessageQueue));
                                                                // Remove from internal queue
                                                                mediumPriority->code = 0;
                                                                foundASlot = 1;
                                                                break;
                                                        }
                                                }
                                        }
                                        if (lowPriority->code) {
                                                printk("Found a low-priority message to insert\n");
                                                // Go through the temp queue to find a spot
                                                uint64_t foundASlot = 0;
                                                for (j = 0; j < NUM_MESSAGES_PER_PRIORITY; j++) {
                                                        if (tempMessageQueue.lowPriority[j].code == 0) {
                                                                // Copy from the internal queue
                                                                memcpy(&tempMessageQueue.lowPriority[j], lowPriority, sizeof(struct TempMessageQueue));
                                                                // Remove from internal queue
                                                                lowPriority->code = 0;
                                                                foundASlot = 1;
                                                                break;
                                                        }
                                                }
                                                printk("Inserted at index %d code=%d handled=%d\n", j, tempMessageQueue.lowPriority[j].code, tempMessageQueue.lowPriority[j].handled);
                                        }
                                        if (lowestPriority->code) {
                                                // Go through the temp queue to find a spot
                                                uint64_t foundASlot = 0;
                                                for (j = 0; j < NUM_MESSAGES_PER_PRIORITY; j++) {
                                                        if (tempMessageQueue.lowestPriority[j].code == 0) {
                                                                // Copy from the internal queue
                                                                memcpy(&tempMessageQueue.lowestPriority[j], lowestPriority, sizeof(struct TempMessageQueue));
                                                                // Remove from internal queue
                                                                lowestPriority->code = 0;
                                                                foundASlot = 1;
                                                                break;
                                                        }
                                                }
                                        }
                                }
                                // Write temp queue back to user memory
                                res = copy_to_user((struct TempMessageQueue *)messageQueueRef->addr, &tempMessageQueue, sizeof(struct TempMessageQueue));
                                // TODO: Eventually: Copy miscData also (once we have messages that use it)
                        }
                }
                // TODO: What to return?
                newReturnValue = (struct ReturnValue *)kmalloc(sizeof(struct ReturnValue), GFP_KERNEL);
                newReturnValue->pid = pid;
                newReturnValue->next = NULL;
                newReturnValue->size = 16;
                newReturnValue->data = (char *)kmalloc(16, GFP_KERNEL);
                sprintf(newReturnValue->data, "Hello, world!");
                returnValue = returnValues;
                if (returnValue == NULL) {
                        returnValues = newReturnValue;
                } else {
                        while (returnValue->next != NULL) {
                                if (returnValue->pid == pid) {
                                        // This will probably happen if someone has more than one thread calling this API.
                                        // Probably best to discourage that, or use thread IDs somehow to keep track of return values.
                                        printk("Ruh-roh, already a return value for that PID. This is a big problem... pid=%d\n", pid);
                                        break;
                                }
                                returnValue = returnValue->next;
                        }
                        returnValue->next = newReturnValue;
                }
        }
        return count;
}
static uint32_t pixel_color(uint8_t r, uint8_t g, uint8_t b, struct fb_var_screeninfo *vinfo) {
	return (r<<vinfo->red.offset) | (g<<vinfo->green.offset) | (b<<vinfo->blue.offset);
}
// Called as one of the last thing the kernel does before passing control to init process(es)
// Sets up the double-buffered framebuffer, renders a grey background, marks the screen as clean.
void kernelmultimedia_setup() {
        long location;
        int x = 0, y = 0;
        printk("SETTING UP KERNEL MULTIMEDIA\n");
        fb = registered_fb[0];
	finfo = &(fb->fix);
	vinfo = &(fb->var);
        screensize = vinfo->yres_virtual * finfo->line_length;
        for (y = 0; y < vinfo->yres; y++) {
                for (x = 0; x < vinfo->xres - 1; x++) {
                        location = (x + vinfo->xoffset) * (vinfo->bits_per_pixel/8) + (y + vinfo->yoffset) * finfo->line_length;
                        *((uint32_t *)(fb->screen_base + location)) = pixel_color(0x80, 0x80, 0x80, vinfo);
                }
        }
}
EXPORT_SYMBOL(kernelmultimedia_setup);
static int __init kernelmultimedia_init(void) {
        int error;
        printk("KERNEL MULTIMEDIA ENABLED\n");
        kernelmultimedia_kobject = kobject_create_and_add("multimedia", kernel_kobj);
        if(!kernelmultimedia_kobject) { return -ENOMEM; }
        call_attribute.attr.name = "call";
        call_attribute.attr.mode = 0660;
        call_attribute.show = call_show;
        call_attribute.store = call_store;
        error = sysfs_create_file(kernelmultimedia_kobject, &call_attribute.attr);
        if (error) { printk("Failed to create node: /sys/kernel/multimedia/call \n"); return -1; }
        return 0;
}
static void __exit kernelmultimedia_exit(void) {
        kobject_put(kernelmultimedia_kobject);
        printk("Kernel multimedia module uninitialized successfully\n");
}
module_init(kernelmultimedia_init);
module_exit(kernelmultimedia_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("edwardhalferty");



                // printk("RECEIVED KERNELMULTIMEDIA_API_CHECK_MESSAGES from PID %d\n", pid);
                                                                // printk("INSERTING MESSAGE AT INDEX %d CODE %d\n", j, highPriority->code);
                                // printk("XYZ=%d\n", tempMessageQueue.highPriority[0].code);
                                // printk("XYZ=%d\n", tempMessageQueue.highPriority[1].code);

// extern int num_registered_fb;

        // printk("num_registered_fb=%d\n", num_registered_fb); // TODO: Should check if num_registered_fb > 1 and decide which to show??!
        // printk("xres=%d yres=%d xoffset=%d yoffset=%d bits_per_pixel=%d line_length=%d yres_virtual=%d line_length=%d screensize=%ld\n",
        //         vinfo->xres, vinfo->yres, vinfo->xoffset, vinfo->yoffset, vinfo->bits_per_pixel,
        //         finfo->line_length, vinfo->yres_virtual, finfo->line_length, screensize);

        // Init windows with a few empty pointers
        // windows = kmalloc(INITIAL_WINDOW_ARRAY_SIZE * sizeof(struct Window *), GFP_KERNEL);
        // numWindows = INITIAL_WINDOW_ARRAY_SIZE;

// static void drawNewWindow(struct WindowRef *windowRef) {
//         // struct Window * temp = kmalloc(sizeof(struct Window), GFP_KERNEL);
//         // struct Window * temp2 = virt_to_phys(windowRef->window);
//         // printk("LOCATION IN USERSPACE: %llx LOCATION IN PHYSICAL SPACE: %llx LOCATION OF TEMP: %llx\n", windowRef->window, temp2, temp);
//         // copy_from_user(temp, temp2, sizeof(struct Window));
//         // printk("PHYS: %02x %02x %02x %02x\n", ((char *)temp)[0], ((char *)temp)[1], ((char *)temp)[2], ((char *)temp)[3]);
        
//         // printk("DRAW NEW WINDOW x=%d y=%d width=%d height=%d\n", temp->x, temp->y, temp->width, temp->height);
//         // long location;
//         // int x = window->x, y = window->y;
//         // while (y < window->y + window->height) {
//         //         x = window->x;
//         //         while (x < window->x + window->width - 1) {
//         //                 location = (x + vinfo->xoffset) * (vinfo->bits_per_pixel/8) + (y + vinfo->yoffset) * finfo->line_length;
//         //                 *((uint32_t *)(fb->screen_base + location)) = pixel_color(0xFF, 0xFF, 0xFF, vinfo);
//         //                 x++;
//         //         }
//         //         y++;
//         // }
//         // kfree(temp);
// }



                // TODO
                // //struct MessageQueueRef
                // uint8_t *buf3 = kmalloc(1024, GFP_KERNEL);
                // i = 0;
                // while (i < 1024) {
                //         buf3[i] = 0xFF;
                //         i++;
                // }
                // // uint64_t asdf = 0xFFFFFFFFFFFFFFFF;
                // printk("22222222 messageQueue=%llu\n", argA);
                // printk("22222222 argB=%llu\n", argB);
                // uint64_t ress = copy_to_user(argA, buf3, 10);
                // printk("]]]]res=%llu\n", ress);
                // struct ReturnValue *newReturnValue = (struct ReturnValue *)kmalloc(sizeof(struct ReturnValue), GFP_KERNEL);
                // newReturnValue->pid = pid;
                // newReturnValue->next = NULL;
                // newReturnValue->size = 16;
                // newReturnValue->data = (char *)kmalloc(16, GFP_KERNEL);
                // sprintf(newReturnValue->data, "Hello, world 2!");



                // struct CreateWindowInfo * info = kmalloc(sizeof(struct CreateWindowInfo), GFP_KERNEL);
                // memcpy(info, &buf[24], sizeof(struct CreateWindowInfo));
                // printk("x=%d y=%d width=%d height=%d\n", info->x, info->y, info->width, info->height);
                // while (i < numWindows) {
                //         if (windowRefs[i] == NULL) {
                //                 found = 1;
                //                 break;
                //         }
                //         i++;
                // }
                // if (!found) {
                //         printk("ERROR!! Need to krealloc windowrefs.\n");
                // } else {
                //         windowRefs[i] = kmalloc(sizeof(struct WindowRef), GFP_KERNEL);
                //         windowRefs[i]->pid = pid;
                //         windowRefs[i]->addr = argA;
                //         drawNewWindow(windowRefs[i]);
                // }

                // struct CreateWindowInfo * info = (struct CreateWindowInfo *)(buf + 24);
                // uint32_t x = 0, y = 0, width = 0, height = 0;
                // i = 0;
                // while (i < 4) { x |= buf[i + 24] << (i * 8); i++; }
                // i = 0;
                // while (i < 4) { y |= buf[i + 28] << (i * 8); i++; }
                // i = 0;
                // while (i < 4) { width |= buf[i + 32] << (i * 8); i++; }
                // i = 0;
                // while (i < 4) { height |= buf[i + 36] << (i * 8); i++; }



                // printk("%lu %lu %lu %lu", x, y, width, height);
// static void drawNewWindow(struct Window * window) {
//         // struct Window * temp = kmalloc(sizeof(struct Window), GFP_KERNEL);
//         // struct Window * temp2 = virt_to_phys(windowRef->window);
//         // printk("LOCATION IN USERSPACE: %llx LOCATION IN PHYSICAL SPACE: %llx LOCATION OF TEMP: %llx\n", windowRef->window, temp2, temp);
//         // copy_from_user(temp, temp2, sizeof(struct Window));
//         // printk("PHYS: %02x %02x %02x %02x\n", ((char *)temp)[0], ((char *)temp)[1], ((char *)temp)[2], ((char *)temp)[3]);
        
//         // printk("DRAW NEW WINDOW x=%d y=%d width=%d height=%d\n", temp->x, temp->y, temp->width, temp->height);
//         // long location;
//         // int x = temp->x, y = temp->y;
//         // while (y < temp->y + temp->height) {
//         //         x = temp->x;
//         //         while (x < temp->x + temp->width - 1) {
//         //                 location = (x + vinfo->xoffset) * (vinfo->bits_per_pixel/8) + (y + vinfo->yoffset) * finfo->line_length;
//         //                 *((uint32_t*)(fb->screen_base + location)) = pixel_color(0xFF, 0xFF, 0xFF, vinfo);
//         //                 x++;
//         //         }
//         //         y++;
//         // }
//         // kfree(temp);
// }
// static void drawNewWindow(struct Window * window);
                // printk("")



                // while (i < numWindows) {
                //         if (windows[i] == NULL) {
                //                 found = 1;
                //                 break;
                //         }
                //         i++;
                // }
                // if (!found) {
                //         printk("ERROR!! Need to krealloc windows.\n");
                // } else {
                //         printk()
                //         // windows[i] = kmalloc(sizeof(struct WindowRef), GFP_KERNEL);
                //         // windows[i]->pid = pid;
                //         // printk("RECEIVING WINDOW %02x %02x\n", buf[4], buf[5]);
                //         // printk("RECEIVING WINDOW %02x %02x\n", buf[6], buf[7]);
                //         // printk("RECEIVING WINDOW %02x %02x\n", buf[8], buf[9]);
                //         // printk("RECEIVING WINDOW %02x %02x\n", buf[10], buf[11]);
                //         // windows[i]->window = 0;
                //         // windows[i]->window = ((unsigned long long)windows[i]->window) | ((buf[4] << 0) & 0xFF);
                //         // windows[i]->window = ((unsigned long long)windows[i]->window) | ((buf[5] << 8) & 0xFF00);
                //         // windows[i]->window = ((unsigned long long)windows[i]->window) | ((buf[6] << 16) & 0xFF0000);
                //         // windows[i]->window = ((unsigned long long)windows[i]->window) | ((buf[7] << 24) & 0xFF000000);
                //         // windows[i]->window = ((unsigned long long)windows[i]->window) | ((buf[8] << 32) & 0xFF00000000);
                //         // windows[i]->window = ((unsigned long long)windows[i]->window) | ((buf[9] << 40) & 0xFF0000000000);
                //         // windows[i]->window = ((unsigned long long)windows[i]->window) | ((buf[10] << 48) & 0xFF000000000000);
                //         // windows[i]->window = ((unsigned long long)windows[i]->window) | ((buf[11] << 56) & 0xFF00000000000000);


                //         // // windows[i]->window = (struct Window *)(
                //         // //         ((unsigned long long)buf[4]) | ((unsigned long long)buf[5] << 8)
                //         // //         | ((unsigned long long)buf[6] << 16) | ((unsigned long long)buf[7] << 24)
                //         // //         | ((unsigned long long)buf[8] << 32) | ((unsigned long long)buf[9] << 40)
                //         // //         | ((unsigned long long)buf[10] << 48) | ((unsigned long long)buf[11] << 56));
                //         // printk("RECEIVING WINDOW %llx\n", windows[i]->window);
                //         // drawNewWindow(windows[i]);
                // }
                // struct ReturnValue *newReturnValue = (struct ReturnValue *)kmalloc(sizeof(struct ReturnValue), GFP_KERNEL);
                // newReturnValue->pid = pid;
                // newReturnValue->next = NULL;
                // newReturnValue->size = 16;
                // newReturnValue->data = (char *)kmalloc(16, GFP_KERNEL);
                // sprintf(newReturnValue->data, "Hello, world!");

                // struct ReturnValue *returnValue = returnValues;
                // if (returnValue == NULL) {
                //         returnValues = newReturnValue;
                // } else {
                //         while (returnValue->next != NULL) {
                //                 if (returnValue->pid == pid) {
                //                         // This will probably happen if someone has more than one thread calling this API.
                //                         // Probably best to discourage that, or use thread IDs somehow to keep track of return values.
                //                         printk("Ruh-roh, already a return value for that PID. This is a big problem... pid=%d\n", pid);
                //                         break;
                //                 }
                //                 returnValue = returnValue->next;
                //         }
                //         returnValue->next = newReturnValue;
                // }


        // if (miscDataLength > 0) {
        //         miscData = (char *)kmalloc(miscDataLength, GFP_KERNEL);
        //         memcpy(miscData, &buf[28], miscDataLength);
        // }
        // if (miscDataLength > 0) {
        //         printk("MISC DATA:\n");
        //         i = 0;
        //         while (i < miscDataLength) {
        //                 if (i % 8 == 0) {
        //                         printk("%02x", miscData[i]);
        //                 } else {
        //                         printk("%02x ", miscData[i]);
        //                 }
        //                 // printk(KERN_CONT, "%x", miscData[i]);
        //                 // printk(KERN_CONT, (i % 8 == 0) ? "\n" : " ");
        //                 i++;
        //         }
        // }

        // printk("%02x %02x %02x %02x %02x %02x %02x %02x\n", miscData[0], miscData[1], miscData[2], miscData[3], miscData[4], miscData[5], miscData[6], miscData[7]);

        // uint32_t code = buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);

        // uint32_t argA = buf[4] | (buf[5] << 8) | (buf[6] << 16) | (buf[7] << 24);

        // uint32_t code, uint64_t argA, uint64_t argB, uint8_t * miscData, uint32_t miscDataLength


// xres=1280 yres=1024 xoffset=0 yoffset=0 bits_per_pixel=24 line_length=3840 yres_virtual=1024 line_length=3840 screensize=3932160

        // int i = 0;
        // while (i < screensize) {
        //         *((uint32_t*)(fb->screen_base + i)) = 0x80;
        //         i++;
        // }

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