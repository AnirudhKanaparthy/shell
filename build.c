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
    bool force_rebuild = (argc >= 2) && (strncmp(argv[1], "-r", sizeof("-r")-1) == 0);

    Cmd cmd = {0};
    int status = 0;

    long file_ts = get_mtime(__FILE__);
    if(file_ts == -1) return 1;

    long exec_ts = get_mtime(argv[0]);
    if(exec_ts == -1) return 1;

    if(file_ts > exec_ts) {
        // Dependency was modified at a later time than the target
        printf("[INFO] builder executable is outdated, building new...\n");

        os_cmd_append(&cmd, "gcc");
        os_cmd_append(&cmd, "-Wall");
        os_cmd_append(&cmd, "-Wextra");
        os_cmd_append(&cmd, "-Wno-unknown-pragmas");
        os_cmd_append(&cmd, "-o");
        os_cmd_append(&cmd, argv[0]);
        os_cmd_append(&cmd, __FILE__);
        status = os_cmd_run(&cmd);
        if(status != 0) return 1;
        printf("[INFO] exit status: %d\n", status);

        char** targs;
        if(!force_rebuild) {
            targs = (char**)malloc(sizeof(char*) * (argc+2));
            for(size_t i = 0; i < argc; ++i) {
                targs[i] = argv[i];
            }
            targs[argc] = "-r";
            targs[argc+1] = NULL;
        } else {
            targs = argv;
        }
        execvp(argv[0], targs);
        fprintf(stderr, "[ERR] Could not load in the latest built program image\n");
        exit(1);
    }

    file_ts = get_mtime("shell.c");
    if(file_ts == -1) return 1;

    exec_ts = get_mtime("shell"); // This may not exist

    long file2_ts = get_mtime("os.h");
    if(file2_ts == -1) return 1;

    if(force_rebuild || file_ts > exec_ts || file2_ts > exec_ts) {
        // Dependency was modified at a later time than the target
        os_cmd_append(&cmd, "gcc");
        os_cmd_append(&cmd, "-Wall");
        os_cmd_append(&cmd, "-Wextra");
        os_cmd_append(&cmd, "-Wno-unknown-pragmas");
        os_cmd_append(&cmd, "-Wno-override-init");
        os_cmd_append(&cmd, "-ggdb");
        os_cmd_append(&cmd, "-fsanitize=address"); // TODO: Fix the memory leak
        os_cmd_append(&cmd, "-o");
        os_cmd_append(&cmd, "shell");
        os_cmd_append(&cmd, "shell.c");

        status = os_cmd_run(&cmd);
        if(status != 0) return 1;
        printf("[INFO] exit status: %d\n", status);
    }

    printf("[INFO] Build complete\n");
    return 0;
}
