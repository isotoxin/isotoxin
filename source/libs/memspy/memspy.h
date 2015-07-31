/*
    memspy module
    (C) 2015 ROTKAERMOTA (TOX: ED783DA52E91A422A48F984CAC10B6FF62EE928BE616CE41D0715897D7B7F6050B2F04AA02A1)
*/
#pragma once


// use crt malloc/realloc/free by default
#ifndef MEMSPY_SYS_ALLOC
#define MEMSPY_SYS_ALLOC(sz) malloc(sz)
#endif

#ifndef MEMSPY_SYS_RESIZE
#define MEMSPY_SYS_RESIZE(ptr,sz) realloc(ptr,sz)
#endif

#ifndef MEMSPY_SYS_FREE
#define MEMSPY_SYS_FREE(ptr) free(ptr)
#endif

#ifndef MEMSPY_SYS_FREE
#define MEMSPY_SYS_FREE(ptr) free(ptr)
#endif

#ifndef MEMSPY_SYS_SIZE
#define MEMSPY_SYS_SIZE(ptr) _msize(ptr)
#endif

void *mspy_malloc(const char *fn, int line, size_t sz);
void *mspy_realloc(const char *fn, int line, void *p, size_t sz);
void mspy_free(void *p);
size_t mspy_size(void *p);


bool mspy_getallocated_info( char *buf, int bufsz ); // call at end of app to get allocated memory info (leaks)
void reset_allocnum();