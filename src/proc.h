#ifndef XBPS_TUI_PROC_H
#define XBPS_TUI_PROC_H

#include <stdio.h>
#include <sys/types.h>

typedef struct { FILE *f; pid_t pid; } Pipe;

Pipe run_pipe(char *const argv[]);
void close_pipe(Pipe *p);
int close_pipe_status(Pipe *p);
void run_interactive(char *const argv[]);
char *run_interactive_lastline(char *const argv[]);
char *run_capture_trim(char *const argv[]);
char **sudo_wrap(char *const argv[]);

#endif