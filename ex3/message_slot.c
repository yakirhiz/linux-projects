// Declare what kind of code we want
// from the header files. Defining __KERNEL__
// and MODULE allows us to access kernel-level
// code not usually available to userspace programs.
#undef __KERNEL__
#define __KERNEL__
#undef MODULE
#define MODULE


#include <linux/kernel.h>   /* We're doing kernel work */
#include <linux/module.h>   /* Specifically, a module */
#include <linux/fs.h>       /* for register_chrdev */
#include <linux/uaccess.h>  /* for get_user and put_user */
#include <linux/string.h>   /* for memset. NOTE - not string.h!*/
#include <linux/slab.h>       /* for kmalloc/kfree */

MODULE_LICENSE("GPL");

//Our custom definitions of IOCTL operations
#include "message_slot.h"

typedef struct channel {
	int id;
	// DEL: char message[BUF_LEN];
	char *message; // NULL indicates no message
	int msglen; // 0 indicates no message
	struct channel *next;
} Channel;

typedef struct message_slot {
	int minor;
	Channel* curr_channel;
	int channel_id;
} Slot;

static Channel* channels_lists[MAX_SLOTS]; // Initialized by the kernel

//================== DEVICE FUNCTIONS ===========================
static int device_open( struct inode* inode,
                        struct file*  file )
{
	Slot *slot;
	
	printk("Invoking device_open()\n");
	
	slot = kmalloc(sizeof(Slot), GFP_KERNEL);
	if (slot == NULL) {
		printk("kmalloc() failed");
		return -ENOMEM;
	}
		
	slot->minor = iminor(inode);
	slot->curr_channel = NULL;
	slot->channel_id = -1;
	
	file->private_data = (void*) slot;
	
	return SUCCESS;
}

//---------------------------------------------------------------
static int device_release( struct inode* inode,
                           struct file*  file )
{
	Slot *slot;
	
	printk("Invoking device_release()\n");
	
	slot = (Slot*) file->private_data; 
	
	kfree(slot);
	
	return SUCCESS;
}

//---------------------------------------------------------------
// a process which has already opened
// the device file attempts to read from it
static ssize_t device_read( struct file* file,
                            char __user* buffer,
                            size_t       length,
                            loff_t*      offset )
{
	Slot *slot;
	Channel *channel;
	int i;
	
	printk("Invoking device_read()\n");
	
	// Checking the validity of user space buffer.
	if (buffer == NULL) {
		return -EINVAL;
	}
	
	slot = (Slot*) file->private_data;
	
	if (slot->curr_channel == NULL) {
		printk("Channel has not been set");
		return -EINVAL;
	}

	channel = slot->curr_channel;
	
	if (length < channel->msglen) {
		printk("Buffer is too small.");
		return -ENOSPC;
	}
	
	if (channel->msglen == 0) { // MAYBE: message == NULL
		return -EWOULDBLOCK;
	}
	
	printk("Reading from channel %d", channel->id);
	
	for (i = 0; i < channel->msglen; ++i) {
		if (put_user(channel->message[i], &buffer[i]) < 0) {
			return -EFAULT;
		}
	}
	
	return i; // Returns the number of bytes read
}

//---------------------------------------------------------------
// a process which has already opened
// the device file attempts to write to it
static ssize_t device_write( struct file*       file,
                             const char __user* buffer,
                             size_t             length,
                             loff_t*            offset )
{
	Slot *slot;
	Channel *channel;
	char *new_message;
	int i;
	
	printk("Invoking device_write()\n");
	
	// Checking the validity of user space buffer.
	if (buffer == NULL) {
		return -EINVAL;
	}
	
	if (length == 0 || length > BUF_LEN) {
		return -EMSGSIZE;
	}
	
	slot = (Slot*) file->private_data;
	
	if (slot->curr_channel == NULL) {
		printk("Channel has not been set");
		return -EINVAL;
	}

	channel = slot->curr_channel;
	
	printk("Writing to channel %d\n", channel->id);
	
	// Allocating memory for the new message
	new_message = kmalloc(length * sizeof(char), GFP_KERNEL);
	if (new_message == NULL) {
		printk("kmalloc() failed\n");
		return -ENOMEM;
	}
	
	for (i = 0; i < length; ++i) {
		if (get_user(new_message[i], &buffer[i]) < 0) {
			kfree(new_message);
			return -EFAULT;
		}
	}
	
	// kfree'ing old message
	kfree(channel->message);

	channel->message = new_message;
	channel->msglen = length; // Update message length
	
	return i; // Returns the number of bytes written
}

//----------------------------------------------------------------
static long device_ioctl( struct   file* file,
                          unsigned int   ioctl_command_id,
                          unsigned long  ioctl_param )
{
	Slot *slot;
	int minor;
	Channel *iter, *prev, *new;
	
	printk("Invoking device_ioctl()\n");
	
	if (ioctl_command_id != MSG_SLOT_CHANNEL || ioctl_param == 0) {
		return -EINVAL;
	}
	
	slot = (Slot*) file->private_data;
	
	printk("Changing to channel %ld\n", ioctl_param);
	
	minor = slot->minor;
	
	iter = channels_lists[minor];
	prev = NULL;
	
	while (iter != NULL) {
		if (iter->id == ioctl_param) {
			slot->curr_channel = iter;
			
			return SUCCESS;
		} else {
			prev = iter;
			iter = iter->next;
		}
	}
	
	// Add new channel
	new = kmalloc(sizeof(Channel), GFP_KERNEL);
	if (new == NULL) {
		printk("kmalloc() failed");
		return -ENOMEM;
	}
	
	new->id = ioctl_param;
	new->message = NULL;
	new->msglen = 0;
	new->next = NULL;
	
	if (prev == NULL) {
		channels_lists[minor] = new;
	} else {
		prev->next = new;
	}
	
	slot->curr_channel = new;

	return SUCCESS;
}

//==================== DEVICE SETUP =============================

// This structure will hold the functions to be called
// when a process does something to the device we created
struct file_operations Fops =
{
	.owner          = THIS_MODULE,
	.read           = device_read,
	.write          = device_write,
	.open           = device_open,
	.unlocked_ioctl = device_ioctl,
	.release        = device_release,
};

//---------------------------------------------------------------
// Initialize the module - Register the character device
static int __init device_init(void)
{
	int rc = -1;

	rc = register_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME, &Fops); // Register driver

	// Negative values signify an error
	if (rc < 0)
	{
		printk(KERN_ERR "Registration failed for %d\n", MAJOR_NUM);
		return rc;
	}

	printk("Registration is successful.\n");
	printk("Create a device file: mknod /dev/%s c %d 0\n", DEVICE_RANGE_NAME, MAJOR_NUM);
	printk("You can echo/cat to/from the device file.\n");
	printk("Don't forget to 'rm' the device file and 'rmmod' when you're done.\n");

	return 0;
}

//---------------------------------------------------------------
static void __exit device_cleanup(void)
{
	int i;
	Channel *iter, *prev;
	
	// kfree'ing all lists in channels_lists
	for(i = 0; i < MAX_SLOTS; ++i){
		iter = channels_lists[i];
		
		// kfree'ing all the channels in channels_lists[i] linked-list
		while (iter != NULL) {
			prev = iter;
			iter = iter->next;
			kfree(prev->message);
			kfree(prev);
		}
	}
	
	// Unregister the device, should always succeed
	unregister_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME);
	printk("Unregistration is successful.\n");
}

//---------------------------------------------------------------
module_init(device_init);
module_exit(device_cleanup);

//========================= END OF FILE =========================
