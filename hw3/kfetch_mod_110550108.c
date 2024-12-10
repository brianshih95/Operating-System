#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/mutex.h>
#include <linux/sched/signal.h>
#include <linux/kernel.h>
#include <linux/utsname.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/device.h>
#include "kfetch.h"

#define DEVICE_NAME "kfetch"
#define CLASS_NAME "kfetch_class"

static int major_number;
static struct class *kfetch_class = NULL;
static struct device *kfetch_device = NULL;

static DEFINE_MUTEX(kfetch_mutex);
static int info_mask = KFETCH_FULL_INFO;

static ssize_t kfetch_read(struct file *filp, char __user *buffer, size_t length, loff_t *offset);
static ssize_t kfetch_write(struct file *filp, const char __user *buffer, size_t length, loff_t *offset);
static int kfetch_open(struct inode *inode, struct file *file);
static int kfetch_release(struct inode *inode, struct file *file);

const static struct file_operations kfetch_ops = {
    .owner = THIS_MODULE,
    .read = kfetch_read,
    .write = kfetch_write,
    .open = kfetch_open,
    .release = kfetch_release,
};

static char *get_system_info(int mask) {
    struct sysinfo si;
    struct task_struct *task;
    struct timespec64 uptime;
    char *buf = kmalloc(KFETCH_BUF_SIZE, GFP_KERNEL);
    char *p = buf;
    char line[128];
    int online_cpus = num_online_cpus();
    int total_cpus = num_possible_cpus();
    int procs = 0;
    char *ascii_art[] = {
        "        .-.        ",
        "       (.. |       ",
        "       <>  |       ",
        "      / --- \\      ",
        "     ( |   | )     ",
        "   |\\\\_)__(_//|   ",
        "  <__)------(__>   "};
    int art_lines = sizeof(ascii_art) / sizeof(ascii_art[0]);

    int hostname_len = strlen(utsname()->nodename);
    char separator[128];
    memset(separator, '-', hostname_len);
    separator[hostname_len] = '\0';

    si_meminfo(&si);
    ktime_get_boottime_ts64(&uptime);
    for_each_process(task) {
        procs++;
    }

    p += snprintf(p, KFETCH_BUF_SIZE, "                    %s\n", utsname()->nodename);

    for (int i = 0; i < art_lines || mask != 0; i++) {
        memset(line, 0, sizeof(line));

        if (i < art_lines)
            snprintf(line, sizeof(line), "%-20s", ascii_art[i]);

        if (i == 0) {
            snprintf(line + strlen(line), sizeof(line) - strlen(line), "%s", separator);
        }
        else if (mask & KFETCH_RELEASE) {
            snprintf(line + strlen(line), sizeof(line) - strlen(line), "Kernel:   %s", utsname()->release);
            mask &= ~KFETCH_RELEASE;
        }
        else if (mask & KFETCH_CPU_MODEL) {
            snprintf(line + strlen(line), sizeof(line) - strlen(line), "CPU:      %s", boot_cpu_data.x86_model_id);
            mask &= ~KFETCH_CPU_MODEL;
        }
        else if (mask & KFETCH_NUM_CPUS) {
            snprintf(line + strlen(line), sizeof(line) - strlen(line), "CPUs:     %d / %d", online_cpus, total_cpus);
            mask &= ~KFETCH_NUM_CPUS;
        }
        else if (mask & KFETCH_MEM) {
            snprintf(line + strlen(line), sizeof(line) - strlen(line), "Mem:      %lu MB / %lu MB",
                     si.freeram * 4 / 1024, si.totalram * 4 / 1024);
            mask &= ~KFETCH_MEM;
        }
        else if (mask & KFETCH_NUM_PROCS) {
            snprintf(line + strlen(line), sizeof(line) - strlen(line), "Procs:    %d", procs);
            mask &= ~KFETCH_NUM_PROCS;
        }
        else if (mask & KFETCH_UPTIME) {
            snprintf(line + strlen(line), sizeof(line) - strlen(line), "Uptime:   %lld mins",
                     div_s64(uptime.tv_sec, 60));
            mask &= ~KFETCH_UPTIME;
        }

        if (strlen(line) > 0)
            p += snprintf(p, KFETCH_BUF_SIZE - (p - buf), "%s\n", line);
    }

    return buf;
}

static int kfetch_open(struct inode *inode, struct file *file) {
    if (!mutex_trylock(&kfetch_mutex))
        return -EBUSY;
    return 0;
}

static int kfetch_release(struct inode *inode, struct file *file) {
    mutex_unlock(&kfetch_mutex);
    return 0;
}

static ssize_t kfetch_read(struct file *filp, char __user *buffer, size_t length, loff_t *offset) {
    char *kfetch_buf;
    int len;

    if (*offset > 0)
        return 0;

    kfetch_buf = get_system_info(info_mask);
    if (!kfetch_buf)
        return -ENOMEM; 
    len = strlen(kfetch_buf); 

    if (copy_to_user(buffer, kfetch_buf, len)) {
        pr_alert("Failed to copy data to user");
        kfree(kfetch_buf);
        return 0;
    }

    *offset = len;
    kfree(kfetch_buf);

    return len;
}

static ssize_t kfetch_write(struct file *filp, const char __user *buffer, size_t length, loff_t *offset) {
    int mask_info;

    if (copy_from_user(&mask_info, buffer, length)) {
        pr_alert("Failed to copy data from user");
        return 0;
    }

    info_mask = mask_info;
    return sizeof(mask_info);
}

static int __init kfetch_init(void) {
    major_number = register_chrdev(0, DEVICE_NAME, &kfetch_ops);
    kfetch_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(kfetch_class)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        return PTR_ERR(kfetch_class);
    }

    kfetch_device = device_create(kfetch_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(kfetch_device)) {
        class_destroy(kfetch_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        return PTR_ERR(kfetch_device);
    }

    mutex_init(&kfetch_mutex);
    return 0;
}

static void __exit kfetch_exit(void) {
    mutex_destroy(&kfetch_mutex);
    device_destroy(kfetch_class, MKDEV(major_number, 0));
    class_destroy(kfetch_class);
    unregister_chrdev(major_number, DEVICE_NAME);
}

module_init(kfetch_init);
module_exit(kfetch_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("110550108");
