/*********************************************************************
 * File Name    : log.c
 * Description  :
 * Author       : Hu Lizhen
 * Create Date  : 2012-12-21
 ********************************************************************/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include "log.h"


#if LOG_SWITCH
static char *log_color[] = {
    [LOG_EMERG]     = COLOR_EMERG,
    [LOG_ALERT]     = COLOR_ALERT,
    [LOG_CRIT]      = COLOR_CRIT,
    [LOG_ERR]       = COLOR_ERR,
    [LOG_WARNING]   = COLOR_WARNING,
    [LOG_NOTICE]    = COLOR_NOTICE,
    [LOG_INFO]      = COLOR_INFO,
    [LOG_DEBUG]     = COLOR_DEBUG,
};
#endif

int log_msg(const char *fmt, ...)
{
    int n = 0;
#if LOG_SWITCH
    int level = DFL_LOG_LEVEL;
    va_list ap = NULL;
    const char *ptr = NULL;
    const char *log_fmt = NULL;

    /* Find log level. */
    ptr = strstr(fmt, LOG_FMT);
    if (ptr) {
        log_fmt = LOG_FMT;
        ptr += strlen(LOG_FMT);
    } else {
        ptr = fmt;
    }
    if (*ptr == '<' &&       /* Found '<' */
        *(ptr + 2) == '>' && /* Found '>' */
        *(ptr + 1) >= '0' &&
        *(ptr + 1) <= '7') {
        level = *(ptr + 1) - '0';
        ptr += 3;
    }

    /* Log the message. */
    if (level <= THRESHOLD_LOG_LEVEL) {
        va_start(ap, fmt);
        n += fprintf(stderr, "%s", log_color[level]);
        if (log_fmt) {
            n += vfprintf(stderr, log_fmt, ap);
            va_arg(ap, char *);     /* Skip __FILE__ */
            va_arg(ap, char *);     /* Skip __FUNCTION__ */
            va_arg(ap, int);        /* Skip __LINE__ */
        }
        n += vfprintf(stderr, ptr, ap);
        va_end(ap);
    }
#endif
    return n;
}

/*
 * Print pretty RTSP request or response message.
 */
void print_rtsp_msg(const char *msg, unsigned sz)
{
#if LOG_SWITCH
    char buf[RTSP_MSG_SZ] = {0};

    if (sz >= sizeof(buf)) {
        printd(WARNING "RTSP message is too long T_T\n");
        return;
    }

    printf("\n\033[1;33;41m[Begin]======================= RTSP Message =========================[Begin]\033[0m\n"
           "\033[32m");
    snprintf(buf, sz, "%s", msg);
    printf("%s\033[0m"
           "\033[33m[End]==================================================================[End]\033[0m\n\n\n", buf);
#endif
    return;
}
