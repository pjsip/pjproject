/* $id$ */

#include <errno.h>
#include <stdio.h>
#include <fcntl.h>

#define READ    0
#define WRITE   1
#define STATUS  2

static int stdout_pipe[3];
static int stdin_pipe[3];

void destroy_stdio_pipe();

int init_stdio_pipe()
{
    int rc;

    /* Set stdout no buffer */
    setvbuf(stdout, NULL, _IONBF, 0);

    rc = pipe(stdout_pipe);
    if (rc != 0)
        goto on_error;

    stdout_pipe[STATUS] = 1;

    rc = dup2(stdout_pipe[WRITE], 1);
    if (rc < 0)
        goto on_error;

    rc = pipe(stdin_pipe);
    if (rc != 0)
        goto on_error;

    stdin_pipe[STATUS] = 1;

    rc = dup2(stdin_pipe[READ], 0);
    if (rc < 0)
        goto on_error;

    return 0;

on_error:
    rc = errno;
    destroy_stdio_pipe();
    return rc;
}

void destroy_stdio_pipe()
{
    if (stdout_pipe[STATUS]) {
        close(stdout_pipe[READ]);
        close(stdout_pipe[WRITE]);
    }
    stdout_pipe[STATUS] = 0;

    if (stdin_pipe[STATUS]) {
        close(stdin_pipe[READ]);
        close(stdin_pipe[WRITE]);
    }
    stdin_pipe[STATUS] = 0;
}

int read_from_stdout(char *ch)
{
    int rc;

    rc = read(stdout_pipe[READ], ch, 1);
    if (rc == 0) {
        /* EOF */
        return -1;
    } else if (rc == -1) {
        /* ERROR */
        return -2;
    }

    return 0;
}

int write_to_stdin(const char *st)
{
    int rc;
    char buf[100];
    int len;

    len = snprintf(buf, sizeof(buf), "%s\r\n", st);
    rc = write(stdin_pipe[WRITE], buf, len);

    return rc;
}

