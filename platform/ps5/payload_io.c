/* Public SDK userland payload. Paths/argv and stdio depend on the human's loader.
 * No default paths, assets, saves, deployment, controller or display APIs. */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int read_fixture(const char *path) {
    char buffer[1024];
    size_t count, total = 0;
    FILE *file = fopen(path, "rb");
    if (!file) { printf("read-open=failed errno=%d\n", errno); return 1; }
    while ((count = fread(buffer, 1, sizeof buffer, file))) total += count;
    int failed = ferror(file) != 0;
    if (fclose(file)) failed = 1;
    printf("read=%s bytes=%zu\n", failed ? "failed" : "passed", total);
    return failed;
}

static int scratch_roundtrip(const char *path) {
    static const char marker[] = "pe-ps5 isolated scratch\n";
    char actual[sizeof marker];
    int fd = open(path, O_RDWR | O_CREAT | O_EXCL, 0600);
    if (fd < 0) { printf("scratch-create=failed errno=%d\n", errno); return 1; }
    size_t written = 0, received = 0;
    int failed = 0;
    while (written < sizeof marker - 1) {
        ssize_t count = write(fd, marker + written, sizeof marker - 1 - written);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) { failed = 1; break; }
        written += (size_t)count;
    }
    if (!failed && lseek(fd, 0, SEEK_SET) != 0) failed = 1;
    /* Read from the descriptor we exclusively created, without reopening a path.
     * Use a private scratch directory: path replacement is outside this probe. */
    while (!failed && received < sizeof actual) {
        ssize_t count = read(fd, actual + received, sizeof actual - received);
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) { failed = 1; break; }
        if (count == 0) break;
        received += (size_t)count;
    }
    if (received != sizeof marker - 1 ||
        memcmp(actual, marker, sizeof marker - 1)) failed = 1;
    if (close(fd)) failed = 1;
    if (unlink(path)) failed = 1;
    printf("scratch-roundtrip=%s\n", failed ? "failed" : "passed");
    return failed;
}

int main(int argc, char **argv) {
    puts("pe-ps5-io entry=main stdio=attempted runtime=unverified");
    if (fflush(stdout)) return 1;
    if (argc != 3 || !argv || !argv[1] || !argv[2]) {
        puts("Requires loader argv: READ_ONLY_FIXTURE UNUSED_SCRATCH_PATH; no I/O attempted");
        fflush(stdout);
        return 2;
    }
    int failed = read_fixture(argv[1]);
    /* Do not attempt any write when the read fixture is inaccessible. */
    if (!failed) failed = scratch_roundtrip(argv[2]);
    puts("graphics=unavailable input=unavailable console-validation=human-required");
    if (fflush(stdout)) failed = 1;
    return failed ? 1 : 2;
}
