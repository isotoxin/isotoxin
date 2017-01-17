#pragma once

#include <errno.h>
#include <string.h>
#include <net/if.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/statvfs.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <pthread.h>
#include <poll.h>
#include <pwd.h>
#include <dirent.h>
#include <sys/sendfile.h>
#include <iconv.h>
#include <wctype.h>
#include <X11/Xlib.h>
#ifdef __linux__
#include <sys/eventfd.h>
#endif

#include <alloca.h>
#define _alloca alloca
