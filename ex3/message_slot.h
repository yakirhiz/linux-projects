#ifndef MSG_SLT_H
#define MSG_SLT_H

#include <linux/ioctl.h>

#define MAJOR_NUM 240 // The major device number.

#define MSG_SLOT_CHANNEL _IOW(MAJOR_NUM, 0, unsigned long)

#define DEVICE_RANGE_NAME "message_slot"
#define BUF_LEN 128
#define SUCCESS 0

#endif /* MSG_SLT_H */