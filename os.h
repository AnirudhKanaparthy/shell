#ifndef OS_H_
#define OS_H_

#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>

extern char** environ;

typedef struct {
    const char* data;
    ssize_t len;
} String_View;

typedef struct {
    String_View* items;
    size_t len;
    size_t cap;
} String_View_Arr;

String_View_Arr* sv_split(String_View_Arr* arr, String_View line, char delim);
char* sv_cstr(String_View str);
String_View sv_trim(String_View sv, char cutc);

typedef struct {
    String_View* items;
    size_t len;
    size_t cap;
} Cmd;

#define os_cmd_sv(sv) ((Cmd){.items=(sv).items, .len=(sv).len, .cap=(sv).cap})

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
#define os_cmd_append(cmd, str)                                         \
    do {                                                                \
        String_View sv = (String_View){.data=(str), .len=strlen(str)};  \
        da_append((cmd), sv);                                           \
    } while(0)

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

char* sv_cstr(String_View str) {
    return strndup(str.data, str.len);
}

String_View sv_trim(String_View sv, char cutc) {
    // Left side
    while((sv.len-1) >= 0 && *(sv.data) == cutc) {
        ++(sv.data);
        --(sv.len);
    }

    // Right side
    while((sv.len-1) >= 0 && sv.data[sv.len-1] == cutc) {
        --(sv.len);
    }

    return sv;
}

char** os_cmd_args(Cmd cmd) {
    char** cmd_cstr = (char**)malloc(sizeof(char*) * (cmd.len+1));
    for(size_t i = 0; i < cmd.len; ++i) {
        cmd_cstr[i] = sv_cstr(cmd.items[i]);
    }
    cmd_cstr[cmd.len] = NULL;
    return cmd_cstr;
}

String_View_Arr* sv_split(String_View_Arr* arr, String_View line, char delim) {
    size_t prev = 0;
    for(ssize_t i = 0; i < line.len; ++i) {
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

void os_cmd_printf(Cmd cmd) {
    printf("CMD: ");
    for(size_t i = 0; i < cmd.len; ++i) {
        printf("%.*s ", (int)cmd.items[i].len, cmd.items[i].data);
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

            char** args = os_cmd_args(*cmd);
            execvp(args[0], args);

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

#endif // OS_IMPLEMENTATION
