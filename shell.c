
#include <signal.h>

#define OS_IMPLEMENTATION
#include "os.h"

#define LOG_DBG(...) (void)0

void sigint_handler(int sig) { (void)sig; }

typedef struct {
    const char* data;
    size_t len;
} String_View;

typedef struct {
    String_View* items;
    size_t len;
    size_t cap;
} String_View_Arr;

String_View_Arr* sv_split(String_View_Arr* arr, String_View line, char delim) {
    size_t prev = 0;
    for(size_t i = 0; i < line.len; ++i) {
        if(line.data[i] == delim) {
            size_t len = i - prev;
            String_View sv = (String_View){
                .data=line.data+prev,
                .len=len,
            };
            da_append(arr, sv);
            i += 1;
            prev = i;
        }
    }

    size_t len = line.len - prev;
    String_View sv = (String_View){
        .data=line.data+prev,
        .len=len,
    };
    da_append(arr, sv);

    return arr;
}

typedef struct {
    int in;
    int out;
} Pipe_Pair;

typedef struct {
    Pipe_Pair* items;
    size_t len;
    size_t cap;
} Pipe_Pair_Arr;

typedef struct {
    int* items;
    size_t len;
    size_t cap;
} Processes;

int main() {
    char cmd_buffer[1024] = {0};

    // SIGINT Trap
    signal(SIGINT, sigint_handler);

    Cmd_Arr cmds = {0};
    Processes procs = {0};
    String_View_Arr cmd_lines = {0};
    while(true) {
        printf("$ ");

        if(!fgets(cmd_buffer, sizeof(cmd_buffer), stdin)) { break; }
        size_t len = strnlen(cmd_buffer, sizeof(cmd_buffer)) - 1;
        cmd_buffer[len] = '\0';

        if(strncmp(cmd_buffer, "exit", 4) == 0) { break; }

        cmd_lines.len = 0;
        if(!sv_split(&cmd_lines, (String_View){.data=cmd_buffer, .len=len}, '|')) {
            assert(0 && "TODO: Handle error");
        }

        cmds.len = 0;
        for(size_t i = 0; i < cmd_lines.len; ++i) {
            Cmd cmd = os_cmd_create(cmd_lines.items[i].data, cmd_lines.items[i].len);
            if(cmd.len == 0) continue;
            da_append(&cmds, cmd);
        }
        if(cmds.len <= 0) continue;

        if(cmds.len == 1) {
            int status = os_cmd_run(cmds.items, .no_reset=true);
            LOG_DBG("exited with: %d\n", status);
            os_cmd_free(cmds.items[0]);

            continue;
        }

        procs.len = 0;
        int prev_in = -1;
        for(size_t i = 0; i < cmds.len-1; ++i) {
            int pipe_pair[2] = {0};
            if(pipe(pipe_pair) != 0) { assert(0 && "TODO: Handle error"); }

            int pid = os_cmd_run(cmds.items+i, .no_reset=true, .async=true, .fd_stdin=prev_in, .fd_stdout=pipe_pair[1]);

            if(prev_in != -1 && close(prev_in) != 0) return 1;
            if(close(pipe_pair[1]) != 0) return 1;

            prev_in = pipe_pair[0];

            LOG_DBG("started process: %d\n", pid);
            os_cmd_free(cmds.items[i]);
            da_append(&procs, pid);
        }
        int pid = os_cmd_run(cmds.items+(cmds.len-1), .no_reset=true, .async=true, .fd_stdin=prev_in, .fd_stdout=-1);
        if(prev_in != -1 && close(prev_in) != 0) return 1;
        LOG_DBG("started process: %d\n", pid);
        os_cmd_free(cmds.items[cmds.len-1]);
        da_append(&procs, pid);

        LOG_DBG("started all processes\n");

        for(size_t i = 0; i < cmds.len; ++i) {
            int status = 0;
            int pid = wait(&status);
            LOG_DBG("child exited: %d\n", pid);
        }
    }
    free(cmds.items);
    free(procs.items);
    free(cmd_lines.items);

    printf("Bye!\n");
    return 0;
}
