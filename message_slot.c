#undef __KERNEL__
#define __KERNEL__
#undef MODULE
#define MODULE

#include <linux/kernel.h>  /* We're doing kernel work */
#include <linux/module.h>  /* Specifically, a module */
#include <linux/fs.h>      /* for register_chrdev */
#include <linux/uaccess.h> /* for get_user and put_user */
#include <linux/string.h>  /* for memset. NOTE - not string.h!*/
#include <linux/slab.h> /*for kmalloc(), kfree()*/
#include "message_slot.h"

// auxilary method for kfreeing memory allocated for channels of a slot
static void freeChannels(channel *ch)
{
    if (ch->next != NULL)
        freeChannels(ch->next);
    if (ch->msg_buffer != NULL)
        kfree(ch->msg_buffer);
    kfree(ch);
}
static slot *newSlot(){
    slot *ret = (slot *)kmalloc(sizeof(slot), GFP_KERNEL);
    if(ret == NULL)//malloc failed
        return NULL;
    else{//initialize to default vals
        ret->slot_channels = NULL;
        ret->next = NULL;
        return ret;
    }
}
static channel *newChannel()
{
    channel *ret = (channel *)kmalloc(sizeof(channel), GFP_KERNEL);
    if (ret == NULL) // malloc failed
        return NULL;
    else
    { // initialize to default vals
        ret->msg_buffer = NULL;
        ret->next = NULL;
        ret->length = 0;
        return ret;
    }
}
//================== DEVICE FUNCTIONS ===========================
static int device_open(struct inode *inode, struct file *file)
{
    int cur_minor = iminor(inode);
    if(open_slots == NULL){ //this is the first slot created
        open_slots = newSlot();
        if(open_slots == NULL){//malloc failed
            printk("kmalloc failed!");
            return -ENOSPC;
        }
        open_slots->minor = cur_minor;//successfully created slot for desired minor
        return 0;
    }
    else{
        slot *cur_slot = open_slots;
        while (cur_slot->minor != cur_minor){ //search if there is allready open slot with cur_minor
            if(cur_slot->next != NULL)
                cur_slot = cur_slot->next;
            else{//scanned all slots and none is with cur_minor
                cur_slot->next = newSlot();
                cur_slot = cur_slot->next;
                if (cur_slot == NULL)
                { // malloc failed
                    printk("kmalloc failed!");
                    return -ENOSPC;
                }
                cur_slot->minor = cur_minor; // successfully created slot for desired minor at end of list
            }
        }
        return 0;//created new slot or there already been one
    }
}
static int device_release(struct inode* inode, struct file*  file)
{
    int cur_minor = iminor(inode);
    slot *prev = NULL, *cur_slot = open_slots;
    if (cur_slot == NULL)
        return 0; // no memory allocated for file
    while (cur_slot->minor != cur_minor){ 
        if (cur_slot->next != NULL){
            prev = cur_slot;
            cur_slot = cur_slot->next;
        }
        else return 0;//no memory allocated for file
    }
    if(prev == NULL)//free first node in open slots
        open_slots = open_slots->next;
    else
        prev->next = cur_slot->next;
    freeChannels(cur_slot->slot_channels);
    kfree(cur_slot);
    return 0;
}
    static long device_ioctl(struct file * file, unsigned int ioctl_command_id, unsigned long ioctl_param)
    {
        int minor;
        slot *cur_slot = open_slots;
        channel *cur_channel, *new_channel;
        if (ioctl_command_id != MSG_SLOT_CHANNEL || ioctl_param <= 0)
        {
            printk("invalid ioctl parameters!");
            return -EINVAL;
        }
        minor = iminor(file->f_inode);
        while (cur_slot != NULL)
        {
            if (cur_slot->minor == minor)
                break;
            cur_slot = cur_slot->next;
        }
        if (cur_slot == NULL)
        { // no slot detected for this minor
            printk("no slot available by this minor: ",minor);
            return -EINVAL;
        }
        cur_channel = cur_slot->slot_channels;
        while (cur_channel != NULL)
        {
            if (cur_channel->id == (int)ioctl_param)
            {                                             // channel already exists
                file->private_data = (void *)cur_channel; // point from file to active channel
                return 0;                                 // successfuly switched channel
            }
            cur_channel = cur_channel->next;
        }
        // if we reached here there is no chanel with param number
        new_channel = newChannel();
        if (new_channel == NULL)
        {
            printk("kmalloc failed!");
            return -ENOSPC;
        }
        new_channel->id = ioctl_param;
        new_channel->next = cur_slot->slot_channels;
        cur_slot->slot_channels = new_channel;    // added new channel at the beginning
        file->private_data = (void *)new_channel; // point from file to active new channel
        return 0;
}

static ssize_t device_write(struct file *file, const char __user *buffer, size_t length, loff_t *offset)
{
    channel *active_channel;
    char *channel_msg;
    int i=0;
    if(length>BUF_LEN || length<=0){
        printk("invalid message size!");
        return -EMSGSIZE;
    }
    active_channel = (channel *)file->private_data;
    if(active_channel == NULL){// no active channel
        printk("no active channel set for this slot!");
        return -EINVAL;
    }
    //free old message and malloc space for new one
    if (active_channel->msg_buffer != NULL)
        kfree(active_channel->msg_buffer);
    active_channel->msg_buffer = (char *)kmalloc(sizeof(char)*length, GFP_KERNEL);
    if (active_channel->msg_buffer == NULL){
        printk("kmalloc failed!");
        return -ENOSPC;
    }
    channel_msg = active_channel->msg_buffer;
    for(;i<length;i++){
        if(get_user(channel_msg[i],&buffer[i]) != -0)
            return -1;
    }
    active_channel->length = (int)length;
    return (int)length;
}

static ssize_t device_read(struct file *file, char __user *buffer, size_t length, loff_t *offset)
{
    char *channel_msg;
    int msg_length;
    int i=0;
    channel *active_channel = (channel *)file->private_data;

    if (active_channel == NULL)
    { // no active channel
        printk("no active channel set for this slot!");
        return -EINVAL;
    }
    channel_msg = active_channel->msg_buffer;
    msg_length = active_channel->length;
    if (channel_msg == NULL){//channel msg is empty - nothing to read
        printk("channel is empty - no msg to read here!");
        return -EWOULDBLOCK;
    }
    if(msg_length> (int)length){//buffer smaller than message
        printk("buffer sent is too small to read the message!");
        return -ENOSPC;
    }
    for (; i < msg_length; i++){
        if(put_user(channel_msg[i], &buffer[i]) != 0)
            return -1;
    }
    return length;
}
//==================== DEVICE SETUP =============================

struct file_operations Fops = {
    .owner = THIS_MODULE,
    .read = device_read,
    .write = device_write,
    .open = device_open,
    .unlocked_ioctl = device_ioctl,
    .release = device_release,
};
static int __init simple_init(void){//took parts of it from recitaion
    int rc = -1;
    // Register driver capabilities. Obtain major num
    rc = register_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME, &Fops);
    // Negative values signify an error
    if (rc < 0){
        printk(KERN_ALERT "%s registraion failed for  %d\n", DEVICE_FILE_NAME, MAJOR_NUM);
        return rc;
    }
    open_slots = NULL;//initialize open slots data structure
    return 0;
}

static void __exit simple_cleanup(void)
{
    slot *cur = open_slots, *prev;
    //memory cleanup
    while (cur != NULL)
    {
        prev = cur;
        cur = cur->next;
        freeChannels(prev->slot_channels);//free all space allocated for slot channels and messages
        kfree(prev);//free slot space
    }
    //unregister module
    unregister_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME);
}


module_init(simple_init);
module_exit(simple_cleanup);