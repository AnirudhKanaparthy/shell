#ifndef OS_H_
#define OS_H_

#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>

extern char** environ;

// TODO: Use String_View* instead of char**
// No new memory is allocated during creation of Cmd,
// it is temporarily allocated in os_cmd_run to interface with glibc properly
typedef struct {
    char** items;
    size_t len;
    size_t cap;
} Cmd;

typedef struct {
    Cmd* items;
    size_t len;
    size_t cap;
} Cmd_Arr;

typedef struct {
    char** env;
    const char* workdir;

    int fd_stderr;
    int fd_stdout;
    int fd_stdin;

    bool no_reset;
    bool async;
} Cmd_Opt;

#define da_make(arr, initial_size)                                                  \
    do {                                                                            \
        (arr)->cap = initial_size;                                                  \
        (arr)->items = realloc((arr)->items, sizeof(*((arr)->items)) * (arr)->cap); \
        assert((arr)->items && "Could not reallocate array");                       \
    } while(0)

#define da_grow(arr, new_size)                                                          \
    do {                                                                                \
        if((arr)->cap < (new_size)) {                                                   \
            (arr)->cap = (new_size);                                                    \
            (arr)->items = realloc((arr)->items, sizeof(*((arr)->items)) * (arr)->cap); \
            assert((arr)->items && "Could not reallocate array");                       \
        }                                                                               \
    } while(0)

// Inspired by @rexim
#define da_append(arr, val)                                                             \
    do {                                                                                \
        if((arr)->len >= (arr)->cap) {                                                  \
            (arr)->cap = (arr)->cap > 0 ? 2*(arr)->cap : 256;                           \
            (arr)->items = realloc((arr)->items, sizeof(*((arr)->items)) * (arr)->cap); \
            assert((arr)->items && "Could not reallocate array");                       \
        }                                                                               \
        (arr)->items[(arr)->len++] = (val);                                             \
    } while(0)

#pragma clang diagnostic ignored "-Winitializer-overrides"
#define os_cmd_run(cmd, ...) os_cmd_run_opt(cmd, (Cmd_Opt){.env=environ, .async=false, .no_reset=false, __VA_ARGS__})

Cmd  os_cmd_create(const char* cmd_line, size_t len);
void os_cmd_free(Cmd cmd);
void os_cmd_printf(Cmd cmd);
int  os_cmd_run_opt(Cmd* cmd, Cmd_Opt opts);

#endif // OS_H_

#ifdef OS_IMPLEMENTATION

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

void os_cmd_printf(Cmd cmd) {
    printf("CMD: ");
    for(size_t i = 0; i < cmd.len; ++i) {
        printf("%s ", cmd.items[i]);
    }
    printf("\n");
}

int os_cmd_run_opt(Cmd* cmd, Cmd_Opt opts) {
    os_cmd_printf(*cmd);

    int pid = fork();
    switch(pid) {
        case -1: return -1;
        case  0: {

            if(opts.fd_stderr > 0) {
                if(dup2(opts.fd_stderr, STDERR_FILENO) == -1) {
                    perror("[ERR] os_cmd_run_opt - dup2 - stderr");
                    exit(1);
                }
                close(opts.fd_stderr);
            }
            if(opts.fd_stdout > 0) {
                if(dup2(opts.fd_stdout, STDOUT_FILENO) == -1) {
                    perror("[ERR] os_cmd_run_opt - dup2 - stdout");
                    exit(1);
                }
                close(opts.fd_stdout);
            }
            if(opts.fd_stdin  > 0) {
                if(dup2(opts.fd_stdin,  STDIN_FILENO) == -1) {
                    perror("[ERR] os_cmd_run_opt - dup2 - stdin");
                    exit(1);
                }
                close(opts.fd_stdin);
            }

            if(opts.workdir && chdir(opts.workdir) == -1) {
                perror("[ERR] os_cmd_run_opt - chdir - workdir");
                exit(1);
            }

            environ = opts.env;
            da_append(cmd, NULL);

            execvp(cmd->items[0], cmd->items);

            perror("[ERR] os_cmd_run_opt - execvp");
            exit(1);
        }
        default: {
            if(!opts.no_reset) cmd->len = 0;

            if(!opts.async) {
                int status = 0;
                if(waitpid(pid, &status, 0) != pid) return -1;
                return WEXITSTATUS(status);
            }
            return pid;
        }
    }
}

// TODO: Get rid of this, this has caused so much confusion and bugs.
Cmd os_cmd_create(const char* cmd_line, size_t len) {
    Cmd cmd = {0};
 
    size_t prev = 0;
    while(prev < len && isspace(cmd_line[prev])) ++prev;

    for(size_t i = prev; i < len;) {
        if(isspace(cmd_line[i])) {
            da_append(&cmd, strndup(cmd_line+prev, i-prev));

            ++i;
            while(i < len && isspace(cmd_line[i])) ++i;
            prev = i;
        } else {
            ++i;
        }
    }
    if(prev < len) {
        da_append(&cmd, strndup(cmd_line+prev, len-prev));
    }

    return cmd;
}

// This assumes the C-string in Cmd are heap allocated
void os_cmd_free(Cmd cmd) {
    for(size_t i = 0; i < cmd.len; ++i) {
        free(cmd.items[i]);
    }
    free(cmd.items);
}

#endif // OS_IMPLEMENTATION
