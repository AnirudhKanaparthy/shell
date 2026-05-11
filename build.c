#define OS_IMPLEMENTATION
#include "os.h"

#include <sys/stat.h>

long get_mtime(const char* path) {
    struct stat sb;
    if(stat(path, &sb) != 0) return -1;
    if((sb.st_mode & S_IFMT) != S_IFREG) return -1;
    return sb.st_mtim.tv_sec;
}

int main(int argc, char* argv[]) {
    (void)argc;

    Cmd cmd = {0};
    int status = 0;

    long file_ts = get_mtime(__FILE__);
    if(file_ts == -1) return 1;

    long exec_ts = get_mtime(argv[0]);
    if(exec_ts == -1) return 1;

    if(file_ts > exec_ts) {
        // Dependency was modified at a later time than the target
        printf("[INFO] builder executable is outdated, building new...\n");

        da_append(&cmd, "gcc");
        da_append(&cmd, "-Wall");
        da_append(&cmd, "-Wextra");
        da_append(&cmd, "-Wno-unknown-pragmas");
        da_append(&cmd, "-o");
        da_append(&cmd, argv[0]);
        da_append(&cmd, __FILE__);
        status = os_cmd_run(&cmd);
        if(status != 0) return 1;
        printf("[INFO] exit status: %d\n", status);

        execvp(argv[0], argv);
        fprintf(stderr, "[ERR] Could not load in the latest built program image\n");
        exit(1);
    }

    file_ts = get_mtime("shell.c");
    if(file_ts == -1) return 1;

    exec_ts = get_mtime("shell");
    if(exec_ts == -1) return 1;

    long file2_ts = get_mtime("os.h");
    if(file2_ts == -1) return 1;

    if(file_ts > exec_ts || file2_ts > exec_ts) {
        // Dependency was modified at a later time than the target
        da_append(&cmd, "gcc");
        da_append(&cmd, "-Wall");
        da_append(&cmd, "-Wextra");
        da_append(&cmd, "-Wno-unknown-pragmas");
        da_append(&cmd, "-Wno-override-init");
        da_append(&cmd, "-o");
        da_append(&cmd, "shell");
        da_append(&cmd, "shell.c");

        status = os_cmd_run(&cmd);
        if(status != 0) return 1;
        printf("[INFO] exit status: %d\n", status);
    }

    printf("[INFO] Build complete\n");
    return 0;
}

