#pragma once

//
//  Header hacks
//

#include <sys/stat.h>
#include <sys/syscall.h>
#include <linux/unistd.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <vector>
#include <string>

/// Implement support for naming threads, missing from c++11
void set_thread_name(const char* name);

template <typename T, unsigned N>
char (&ComputeArraySize(T (&)[N]))[N];
#define ARRAY_SIZE(Array) sizeof(ComputeArraySize(Array))

extern uint_fast8_t p__debug_level;

#ifdef ANDROID
#include <sstream>
#include <android/log.h>
#include "android_utils.h"

#define OURNAME "TOOLSTEST"
#ifndef NDEBUG
#define DLOG3(_format, ...) do { if (p__debug_level >= 3) { ((void)__android_log_print(ANDROID_LOG_DEBUG, OURNAME, "%s:%d: " _format, __FILE__, __LINE__, ## __VA_ARGS__)); } } while(0)
#define DLOG2(_format, ...) do { if (p__debug_level >= 2) { ((void)__android_log_print(ANDROID_LOG_DEBUG, OURNAME, "%s:%d: " _format, __FILE__, __LINE__, ## __VA_ARGS__)); } } while(0)
#define DLOG(_format, ...) do { if (p__debug_level >= 1) { ((void)__android_log_print(ANDROID_LOG_DEBUG, OURNAME, "%s:%d: " _format, __FILE__, __LINE__, ## __VA_ARGS__)); } } while(0)
#else
#define DLOG3(_format, ...)
#define DLOG2(_format, ...)
#define DLOG(_format, ...)
#endif

#define ILOG(_format, ...) ((void)__android_log_print(ANDROID_LOG_INFO, OURNAME, "%s:%d: " _format, __FILE__, __LINE__, ## __VA_ARGS__))
#define WLOG(_format, ...) ((void)__android_log_print(ANDROID_LOG_WARN, OURNAME, "%s:%d: " _format, __FILE__, __LINE__, ## __VA_ARGS__))
#define ELOG(_format, ...) ((void)__android_log_print(ANDROID_LOG_ERROR, OURNAME, "%s:%d: " _format, __FILE__, __LINE__, ## __VA_ARGS__))
#define FELOG(_format, ...) ((void)__android_log_print(ANDROID_LOG_FATAL, OURNAME, "%s:%d: " _format, __FILE__, __LINE__, ## __VA_ARGS__))
#define ABORT(_format, ...) do { ((void)__android_log_print(ANDROID_LOG_FATAL, OURNAME, "%s:%d: " _format, __FILE__, __LINE__, ## __VA_ARGS__)); abort(); } while(0)

// Hack to workaround strange missing support for std::to_string in Android
template <typename T>
std::string _to_string(T value)
{
    std::ostringstream os;
    os << value;
    return os.str();
}

int STOI(const std::string& value);

#else // !ANDROID

#ifndef NDEBUG
/// Using DLOGn() instead of DLOG(n,...) so that we can conditionally compile without some of them
#define DLOG3(_format, ...) do { if (p__debug_level >= 3) { fprintf(stdout, "%s:%d " _format "\n", __FILE__, __LINE__, ## __VA_ARGS__); } } while(0)
#define DLOG2(_format, ...) do { if (p__debug_level >= 2) { fprintf(stdout, "%s:%d " _format "\n", __FILE__, __LINE__, ## __VA_ARGS__); } } while(0)
#define DLOG(_format, ...) do { if (p__debug_level >= 1) { fprintf(stdout, "%s:%d " _format "\n", __FILE__, __LINE__, ## __VA_ARGS__); } } while(0)
#else
#define DLOG3(_format, ...)
#define DLOG2(_format, ...)
#define DLOG(_format, ...)
#endif
#define ILOG(_format, ...) fprintf(stdout, "%s:%d " _format "\n", __FILE__, __LINE__, ## __VA_ARGS__);
#define WLOG(_format, ...) fprintf(stdout, "%s:%d " _format "\n", __FILE__, __LINE__, ## __VA_ARGS__);
#define ELOG(_format, ...) fprintf(stderr, "%s:%d " _format "\n", __FILE__, __LINE__, ## __VA_ARGS__);
#define FELOG(_format, ...) fprintf(stderr, "%s:%d " _format "\n", __FILE__, __LINE__, ## __VA_ARGS__);
#define ABORT(_format, ...) do { fprintf(stderr, "%s:%d " _format "\n", __FILE__, __LINE__, ## __VA_ARGS__); abort(); } while(0)

#define _to_string(_x) std::to_string(_x)
#define STOI(_x) std::stoi(_x)

#endif

// Another weird android issue...
#if defined(ANDROID) && !defined(UINT32_MAX)
#define UINT32_MAX (4294967295U)
#endif

static inline bool is_debug() { return p__debug_level; }
char keypress();
bool match(const char* in, const char* short_form, const char* long_form);
int get_arg(char** in, int i, int argc);
void usage();
