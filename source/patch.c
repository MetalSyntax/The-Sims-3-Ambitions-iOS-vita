/**
 * @file patch.c
 * @brief Game-specific hooks, memory patches, and custom symbol intercepts for The Sims 3 Ambitions.
 */
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "darwin.h"
#include "imports.h"
#include "log.h"
#include "macho.h"
#include "patch.h"

// C++ allocation & exception personality stubs
static void *cpp_new(size_t sz) { return malloc(sz); }
static void cpp_delete(void *p) { free(p); }
static int dummy_personality(void) { return 0; }

// POSIX file descriptor shims
static int p_open(const char *path, int flags, int mode) {
    char p[512];
    int fd = open(darwin_path(path, p, sizeof(p)), flags, mode);
    l_debug("open(%s, 0x%x) -> %d", p, flags, fd);
    return fd;
}

static int p_fstat(int fd, struct stat *st) {
    return fstat(fd, st);
}

static off_t p_lseek(int fd, off_t offset, int whence) {
    return lseek(fd, offset, whence);
}

static ssize_t p_read(int fd, void *buf, size_t count) {
    return read(fd, buf, count);
}

static ssize_t p_write(int fd, const void *buf, size_t count) {
    return write(fd, buf, count);
}

static int p_remove(const char *path) {
    char p[512];
    return remove(darwin_path(path, p, sizeof(p)));
}

static char *p_getenv(const char *name) {
    return getenv(name);
}

static int p_setenv(const char *name, const char *val, int overwrite) {
    return setenv(name, val, overwrite);
}

static int p_printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char buf[1024];
    int r = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    l_info("[GUEST PRINTF] %s", buf);
    return r;
}

static int p_fprintf(FILE *stream, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char buf[1024];
    int r = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    l_info("[GUEST FPRINTF] %s", buf);
    return r;
}

static int p_UIApplicationMain(int argc, char *argv[], void *principalClassName, void *delegateClassName) {
    l_info("UIApplicationMain called (argc=%d)", argc);
    return 0;
}

// Game-specific import tables for The Sims 3 Ambitions
const import_entry patch_imports[] = {
    {"_open", (uintptr_t)p_open, 0},
    {"_read", (uintptr_t)p_read, 0},
    {"_write", (uintptr_t)p_write, 0},
    {"_lseek", (uintptr_t)p_lseek, 0},
    {"_fstat", (uintptr_t)p_fstat, 0},
    {"_remove", (uintptr_t)p_remove, 0},
    {"_getenv", (uintptr_t)p_getenv, 0},
    {"_setenv", (uintptr_t)p_setenv, 0},
    {"_printf", (uintptr_t)p_printf, 0},
    {"_fprintf", (uintptr_t)p_fprintf, 0},
    {"__Znwm", (uintptr_t)cpp_new, 0},
    {"__Znam", (uintptr_t)cpp_new, 0},
    {"__ZdaPv", (uintptr_t)cpp_delete, 0},
    {"___gxx_personality_sj0", (uintptr_t)dummy_personality, 0},
    {"_UIApplicationMain", (uintptr_t)p_UIApplicationMain, 0},
};
const int patch_imports_count = sizeof(patch_imports) / sizeof(patch_imports[0]);

void patch_init(void) {
    l_info("Initializing patches for The Sims 3 Ambitions...");
}

void patch_frame(void) {
    // Per-frame logic if required
}
