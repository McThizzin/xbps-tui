#include <fcntl.h>
#include <pty.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/wait.h>

#include <ncurses.h>

#include "proc.h"

Pipe run_pipe(char *const argv[]) {
    Pipe p = { NULL, -1 };
    int fds[2];
    if (pipe(fds) != 0) return p;
    pid_t pid = fork();
    if (pid < 0) { close(fds[0]); close(fds[1]); return p; }
    if (pid == 0) {
        close(fds[0]);
        dup2(fds[1], STDOUT_FILENO);
        close(fds[1]);
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) dup2(devnull, STDERR_FILENO);
        execvp(argv[0], argv);
        _exit(127);
    }
    close(fds[1]);
    p.f = fdopen(fds[0], "r");
    p.pid = pid;
    return p;
}

void close_pipe(Pipe *p) {
    if (p->f) fclose(p->f);
    if (p->pid > 0) waitpid(p->pid, NULL, 0);
    p->f = NULL; p->pid = -1;
}

int close_pipe_status(Pipe *p) {
    int status = 0;
    if (p->f) {
        char buf[512];
        while (fread(buf, 1, sizeof(buf), p->f) > 0) { /* drain */ }
        fclose(p->f);
    }
    if (p->pid > 0) waitpid(p->pid, &status, 0);
    p->f = NULL; p->pid = -1;
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

/* Run a command with full control of the terminal (xbps-remove/-install
 * may prompt the user interactively). Leaves curses, runs, comes back. */
void run_interactive(char *const argv[]) {
    def_prog_mode();
    endwin();
    pid_t pid = fork();
    if (pid == 0) {
        execvp(argv[0], argv);
        _exit(127);
    }
    int status;
    waitpid(pid, &status, 0);
    reset_prog_mode();
    refresh();
}

/* Echo child output back to the real terminal. The real tty may have ONLCR
 * disabled (ncurses raw() mode), so translate bare '\n' to "\r\n" here; a '\n'
 * already preceded by '\r' (progress lines) is left alone. */
static void echo_tty(const char *buf, size_t n) {
    char out[8192];
    size_t olen = 0;
    for (size_t i = 0; i < n; i++) {
        if (buf[i] == '\n' && (i == 0 || buf[i - 1] != '\r'))
            out[olen++] = '\r';
        out[olen++] = buf[i];
        if (olen > sizeof(out) - 2) {
            write(STDOUT_FILENO, out, olen);
            olen = 0;
        }
    }
    if (olen) write(STDOUT_FILENO, out, olen);
}

/* Same as run_interactive(), but the child's stdout/stderr go through a pty
 * (so it still thinks it talks to a terminal and keeps line-buffered, live
 * output) while the parent mirrors it to the real terminal and keeps the
 * last newline-terminated output line (the "N downloaded, N installed, ..."
 * summary). stdin stays on the real terminal, so sudo/xbps prompts still
 * read real input. Returns strdup()ed last line (possibly ""), free(). */
char *run_interactive_lastline(char *const argv[]) {
    def_prog_mode();
    endwin();

    char *last = strdup("");
    char *cur = malloc(1); cur[0] = 0;
    size_t curlen = 0, curcap = 1;

    int master = -1, slave = -1;
    int ok = openpty(&master, &slave, NULL, NULL, NULL) == 0;
    if (ok) {
        struct termios t;
        if (tcgetattr(slave, &t) == 0) {
            cfmakeraw(&t); /* no ONLCR: pass bytes raw to the real terminal */
            tcsetattr(slave, TCSANOW, &t);
        }
        struct winsize ws;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0)
            ioctl(slave, TIOCSWINSZ, &ws);
    }
    pid_t pid = fork();
    if (pid == 0) {
        if (ok) {
            close(master);
            dup2(slave, STDOUT_FILENO);
            dup2(slave, STDERR_FILENO);
            close(slave);
        }
        execvp(argv[0], argv);
        _exit(127);
    }
    if (pid < 0) { ok = 0; }

    if (ok) {
        close(slave);
        char buf[4096];
        ssize_t n;
        while ((n = read(master, buf, sizeof(buf))) > 0) {
            echo_tty(buf, (size_t)n);
            for (ssize_t i = 0; i < n; i++) {
                if (buf[i] == '\n') {
                    while (curlen > 0 && cur[curlen - 1] == '\r') curlen--;
                    if (curlen > 0) {
                        char *nl = malloc(curlen + 1);
                        memcpy(nl, cur, curlen);
                        nl[curlen] = 0;
                        free(last);
                        last = nl;
                    }
                    curlen = 0;
                } else if (curlen + 1 >= curcap) {
                    curcap *= 2;
                    char *nc = realloc(cur, curcap + 1);
                    if (nc) {
                        cur = nc;
                        cur[curlen++] = buf[i];
                        cur[curlen] = 0;
                    }
                } else {
                    cur[curlen++] = buf[i];
                    cur[curlen] = 0;
                }
            }
        }
        close(master);
    }

    int status;
    if (pid > 0) waitpid(pid, &status, 0);

    if (!last[0] && curlen > 0) {
        while (curlen > 0 && (cur[curlen - 1] == '\r' || cur[curlen - 1] == ' ')) curlen--;
        if (curlen > 0) {
            char *nl = malloc(curlen + 1);
            memcpy(nl, cur, curlen);
            nl[curlen] = 0;
            free(last);
            last = nl;
        }
    }
    free(cur);

    char *start = last;
    while (*start == ' ') start++;
    if (start != last) memmove(last, start, strlen(start) + 1);
    size_t ll = strlen(last);
    while (ll > 0 && (last[ll - 1] == ' ' || last[ll - 1] == '\r' || last[ll - 1] == '\t')) last[--ll] = 0;

    reset_prog_mode();
    refresh();
    return last;
}

/* Wrap argv with "sudo -p Password: " and return a new NULL-terminated array
 * with borrowed string pointers. Free the array, not the strings. */
char **sudo_wrap(char *const argv[]) {
    int n = 0;
    while (argv[n]) n++;
    char **out = malloc(sizeof(char *) * (size_t)(n + 4));
    out[0] = "sudo";
    out[1] = "-p";
    out[2] = "Password: ";
    for (int i = 0; i < n; i++) out[i + 3] = argv[i];
    out[n + 3] = NULL;
    return out;
}

char *run_capture_trim(char *const argv[]) {
    Pipe p = run_pipe(argv);
    char *result = malloc(1); result[0] = 0;
    size_t total = 0;
    if (p.f) {
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), p.f)) > 0) {
            result = realloc(result, total + n + 1);
            memcpy(result + total, buf, n);
            total += n;
            result[total] = 0;
        }
    }
    close_pipe(&p);
    while (total > 0 && (result[total - 1] == '\n' || result[total - 1] == '\r' || result[total - 1] == ' '))
        result[--total] = 0;
    return result;
}