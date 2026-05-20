#ifdef MAC_ENDIAN_H
#include "mac_endian.h"
#elif defined(PEBBLE_ENDIAN_H)
#include "pebble_endian.h"
#elif defined(_WIN32) || defined(WINDOWS_ENDIAN_H)
#include "windows_endian.h"
#elif defined(PICO_BUILD) || defined(PICO_ENDIAN_H) || defined(__arm__)
#include "pico_endian.h"
#else
#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif
#include <endian.h>
#endif

