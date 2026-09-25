#ifndef CAMORAMA_V4L_H
#define CAMORAMA_V4L_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>

#ifdef HAVE_LIBV4L2
#  include <libv4l2.h>
#else
#  define v4l2_open open
#  define v4l2_close close
#  define v4l2_read read
#  define v4l2_ioctl ioctl
#  define v4l2_mmap mmap
#  define v4l2_munmap munmap
#endif

#include "camera-backend.h"

int capture_buffers(cam_t *cam, unsigned char *outbuf, unsigned int len);
int capture_buffers_userptr(cam_t *cam, unsigned char *outbuf);

#endif /* CAMORAMA_V4L_H */
