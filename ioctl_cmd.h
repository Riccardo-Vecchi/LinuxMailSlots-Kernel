#include <linux/ioctl.h>	// IOCTL setting utility

#define IOCTL_DRIVER_NUM 75	// Arbitrary number unique in the system

#define SET_BLOCKING _IO(IOCTL_DRIVER_NUM, 2)
#define SET_NONBLOCKING _IO(IOCTL_DRIVER_NUM, 5)
#define SET_MAXIMUM_MSG_SIZE _IOW(IOCTL_DRIVER_NUM, 7, int)
