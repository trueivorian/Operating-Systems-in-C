/*
 *  chardev.c: Creates a read-only char device that says how many times
 *  you've read from the dev file
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <asm/uaccess.h>	/* for put_user */
#include <charDeviceDriver.h>
#include "ioctl.h"

#define MSG_MAX 4*1024

MODULE_LICENSE("GPL");

struct k_list{
	struct list_head list;
	char* data;
};

LIST_HEAD(head);

DEFINE_MUTEX(devLock);
DEFINE_MUTEX(rwLock);

static long ALL_MSGS_MAX = 2*1024*1024;
static long CURR_MSGS = 0;

/* 
 * This function is called whenever a process tries to do an ioctl on our
 * device file. We get two extra parameters (additional to the inode and file
 * structures, which all device functions get): the number of the ioctl called
 * and the parameter given to the ioctl function.
 *
 * If the ioctl is write or read/write (meaning output is returned to the
 * calling process), the ioctl call returns the output of this function.
 *
 */
static long device_ioctl(struct file *file,	/* see include/linux/fs.h */
		 unsigned int ioctl_num,	/* number and param for ioctl */
		 unsigned long ioctl_param)
{

	/* 
	 * Switch according to the ioctl called 
	 */
	if (ioctl_num == 0) {
		mutex_lock (&rwLock);
		if(ioctl_param > ALL_MSGS_MAX){
	    		ALL_MSGS_MAX = ioctl_param;
			mutex_unlock (&rwLock);
			return SUCCESS;
		}
		mutex_unlock (&rwLock);

	}

	/* no operation defined - return failure */
	return -EINVAL;

}


/*
 * This function is called when the module is loaded
 */
int init_module(void)
{
        Major = register_chrdev(0, DEVICE_NAME, &fops);

	if (Major < 0) {
	  printk(KERN_ALERT "Registering char device failed with %d\n", Major);
	  return Major;
	}

	printk(KERN_INFO "I was assigned major number %d. To talk to\n", Major);
	printk(KERN_INFO "the driver, create a dev file with\n");
	printk(KERN_INFO "'mknod /dev/%s c %d 0'.\n", DEVICE_NAME, Major);
	printk(KERN_INFO "Try various minor numbers. Try to cat and echo to\n");
	printk(KERN_INFO "the device file.\n");
	printk(KERN_INFO "Remove the device file and module when done.\n");

	return SUCCESS;
}

/*
 * This function is called when the module is unloaded
 */
void cleanup_module(void)
{
	struct k_list *cursor, *temp;

	/*  Unregister the device */
	unregister_chrdev(Major, DEVICE_NAME);

	/* Delete linked list */
	list_for_each_entry_safe(cursor, temp, &head, list){
		list_del(&cursor->list);
		kfree(cursor->data);
		kfree(cursor);
	}
}

/*
 * Methods
 */

/* 
 * Called when a process tries to open the device file, like
 * "cat /dev/mycharfile"
 */
static int device_open(struct inode *inode, struct file *file)
{
    
    mutex_lock (&devLock);
    if (Device_Open) {
	mutex_unlock (&devLock);
	return -EBUSY;
    }
    Device_Open++;
    mutex_unlock (&devLock);
    //sprintf(msg, "I already told you %d times Hello world!\n", counter++);
    //msg_Ptr = msg;
    try_module_get(THIS_MODULE);
    
    return SUCCESS;
}

/* Called when a process closes the device file. */
static int device_release(struct inode *inode, struct file *file)
{
    	mutex_lock (&devLock);
	Device_Open--;		/* We're now ready for our next caller */
	mutex_unlock (&devLock);
	/* 
	 * Decrement the usage count, or else once you opened the file, you'll
	 * never get get rid of the module. 
	 */
	module_put(THIS_MODULE);

	return 0;
}

/* 
 * Called when a process, which already opened the dev file, attempts to
 * read from it.
 */
static ssize_t device_read(struct file *filp,	/* see include/linux/fs.h   */
			   char *buffer,	/* buffer to fill with data */
			   size_t length,	/* length of the buffer     */
			   loff_t * offset)
{
	struct k_list *node;

	if(!list_empty(&head)){
		node = list_first_entry(&head, struct k_list, list);

		if(!copy_to_user(buffer, node->data, length)){
			mutex_lock (&rwLock);
			list_del(&node->list);
			mutex_unlock (&rwLock);

			kfree(node->data);
			kfree(node);
		}
	} else{
		return -EAGAIN;
	}

	return length;
}

/* Called when a process writes to dev file: echo "hi" > /dev/hello  */
static ssize_t device_write(struct file *filp, const char *buff, size_t len, loff_t * off)
{
	
	if(sizeof(char)*len > MSG_MAX){ // Message too big
		return -EINVAL;
	}

	mutex_lock (&rwLock);
	if((CURR_MSGS + (sizeof(char)*len)) > ALL_MSGS_MAX){ // Message list full
		mutex_unlock (&rwLock);
		return -EAGAIN;
	} else{
		struct k_list *temp_node = kmalloc(sizeof(struct k_list), GFP_KERNEL);

		temp_node->data = kmalloc(sizeof(char)*len, GFP_KERNEL);
		CURR_MSGS += sizeof(char)*len;

		// copy pointer over from user to kernel
		if(!copy_from_user(temp_node->data, buff, len)){
			INIT_LIST_HEAD(&temp_node->list);

			list_add(&temp_node->list, &head);
		}else{ // Error copying pointers over
			mutex_unlock (&rwLock);
			return -EINVAL;
		}
	}

	mutex_unlock (&rwLock);

	// Return number of bytes
	return len;
}
