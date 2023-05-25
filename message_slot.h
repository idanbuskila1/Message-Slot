#ifndef CHARDEV_H
#define CHARDEV_H

#include <linux/ioctl.h>
#define MAJOR_NUM 235
#define BUF_LEN 128
#define DEVICE_RANGE_NAME "char_dev"
#define DEVICE_FILE_NAME "message_slot"

#define MSG_SLOT_CHANNEL _IOW(MAJOR_NUM, 0, unsigned long)

typedef struct channel{
    unsigned int id;
    char *msg_buffer;
    unsigned int length;
    struct channel *next;
}channel;

typedef struct slot{
    unsigned int minor;
    channel *slot_channels;
    struct slot *next;
}slot;
#endif
