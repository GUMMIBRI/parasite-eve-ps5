/* Early application diagnostics: no libc, heap, errno or TLS before _init_env.
 * Public libkernel returns negative errors directly. Never create game data. */
#include <stddef.h>
extern int sceKernelOpen(const char *, int, unsigned);
extern long sceKernelWrite(int, const void *, size_t);
extern int sceKernelFsync(int);
static int fd = -1;
void pe_application_stage(const char *message) {
    if (fd < 0) {
        /* FreeBSD/PS5 O_WRONLY | O_APPEND | O_CREAT. */
        fd = sceKernelOpen("/data/parasite-eve/startup.log", 0x209, 0666);
        if (fd < 0)
            fd = sceKernelOpen("/data/parasite-eve-startup.log", 0x209, 0666);
    }
    if (fd < 0) return;
    size_t length = 0;
    while (message[length]) ++length;
    while (length) {
        long n = sceKernelWrite(fd, message, length);
        if (n <= 0 || (size_t)n > length) break;
        message += n;
        length -= (size_t)n;
    }
    (void)sceKernelFsync(fd);
}
