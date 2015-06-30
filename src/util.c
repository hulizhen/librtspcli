/*********************************************************************
 * File Name    : util.c
 * Description  : Provide some utilities.
 * Author       : Hu Lizhen
 * Create Date  : 2012-12-21
 ********************************************************************/

#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <sys/epoll.h>
#include "rtsp_cli.h"
#include "log.h"
#include "util.h"


struct code_and_reason {
    enum status_code code;
    const char *reason;
};

static struct code_and_reason status_code_and_reason[] = {
    {CONTINUE,                              "Continue"},
    {OK,                                    "OK"},
    {CREATED,                               "Created"},
    {LOW_ON_STORAGE_SPACE,                  "Low on Storage Space"},
    {MULTIPLE_CHOICES,                      "Multiple Choices"},
    {MOVED_PERMANENTLY,                     "Moved Permanently"},
    {MOVED_TEMPORARILY,                     "Moved Temporarily"},
    {SEE_OTHER,                             "See Other"},
    {NOT_MODIFIED,                          "Not Modified"},
    {USE_PROXY,                             "Use Proxy"},
    {BAD_REQUEST,                           "Bad Request"},
    {UNAUTHORIZED,                          "Unauthorized"},
    {PAYMENT_REQUIRED,                      "Payment Required"},
    {FORBIDDEN,                             "Forbidden"},
    {NOT_FOUND,                             "Not Found"},
    {METHOD_NOT_ALLOWED,                    "Method Not Allowed"},
    {NOT_ACCEPTABLE,                        "Not Acceptable"},
    {PROXY_AUTHENTICATION_REQUIRED,         "Proxy Authentication Required"},
    {REQUEST_TIME_OUT,                      "Request Time-out"},
    {GONE,                                  "Gone"},
    {LENGTH_REQUIRED,                       "Length Required"},
    {PRECONDITION_FAILED,                   "Precondition Failed"},
    {REQUEST_ENTITY_TOO_LARGE,              "Request Entity Too Large"},
    {REQUEST_URI_TOO_LARGE,                 "Request-URI Too Large"},
    {UNSUPPORTED_MEDIA_TYPE,                "Unsupported Media Type"},
    {PARAMETER_NOT_UNDERSTOOD,              "Parameter Not Understood"},
    {CONFERENCE_NOT_FOUND,                  "Conference Not Found"},
    {NOT_ENOUGH_BANDWIDTH,                  "Not Enough Bandwidth"},
    {SESSION_NOT_FOUND,                     "Session Not Found"},
    {METHOD_NOT_VALID_IN_THIS_STATE,        "Method Not Valid in This State"},
    {HEADER_FIELD_NOT_VALID_FOR_RESOURCE,   "Header Field Not Valid for Resource"},
    {INVALID_RANGE,                         "Invalid Range"},
    {PARAMETER_IS_READ_ONLY,                "Parameter Is Read-Only"},
    {AGGREGATE_OPERATION_NOT_ALLOWED,       "Aggregate operation not allowed"},
    {ONLY_AGGREGATE_OPERATION_ALLOWED,      "Only aggregate operation allowed"},
    {UNSUPPORTED_TRANSPORT,                 "Unsupported transport"},
    {DESTINATION_UNREACHABLE,               "Destination unreachable"},
    {INTERNAL_SERVER_ERROR,                 "Internal Server Error"},
    {NOT_IMPLEMENTED,                       "Not Implemented"},
    {BAD_GATEWAY,                           "Bad Gateway"},
    {SERVICE_UNAVAILABLE,                   "Service Unavailable"},
    {GATEWAY_TIME_OUT,                      "Gateway Time-out"},
    {VERSION_NOT_SUPPORTED,                 "Version not supported"},
    {OPTION_NOT_SUPPORTED,                  "Option not supported"},
};

const char *get_status_reason(unsigned int code)
{
    int i = 0;
    const char *reason = "Corresponding Reason Undefined";
    int num = sizeof(status_code_and_reason) / sizeof(*status_code_and_reason);

    for (i = 0; i < num; i++) {
        if (status_code_and_reason[i].code == code) {
            reason = status_code_and_reason[i].reason;
        }
    }
    return reason;
}

int connect_nonb(int sd, const struct sockaddr *sap, socklen_t salen, int timeout)
{
    int flags = 0;
    int ret = 0;
    int err = 0;
    socklen_t len;
    fd_set rset, wset;
    struct timeval tv;
    
    flags = fcntl(sd, F_GETFL, 0);
    if (flags < 0) {
        perrord(ERR "fcntl [F_GETFL] error");
        return -1;
    }
    if (fcntl(sd, F_SETFL, flags | O_NONBLOCK) < 0) {
        perrord(ERR "fcntl [F_SETFL] error");
        return -1;
    }

    ret = connect(sd, sap, salen);
    if (ret < 0) {
        if(errno != EINPROGRESS) {
            perrord(ERR "Connect in non-blocking mode error");
            return -1;
        }
    }

    /* Do whatever we want while the connection is tacking place. */
    if (!ret) {
        goto done;
    }

    FD_ZERO(&rset);
    FD_SET(sd, &rset);
    wset = rset;
    tv.tv_sec = timeout;
    tv.tv_usec = 0;
    if (!select(sd + 1, &rset, &wset, NULL, timeout ? &tv : NULL)) {
        errno = ETIMEDOUT;
        return -1;
    }
    if (FD_ISSET(sd, &rset) || FD_ISSET(sd, &wset)) {
        len = sizeof(err);
        if (getsockopt(sd, SOL_SOCKET, SO_ERROR, &err, &len) < 0) {
            perrord(ERR "getsockopt [SO_ERROR] error");
            return -1;
        }
    } else {
        perrord(ERR "select error: socket not set!\n");
    }

done:
    if (fcntl(sd, F_SETFL, flags) < 0) {
        perrord(ERR "fcntl [F_SETFL] error");
        return -1;
    }

    if (err) {
        errno = err;
        perrord(ERR "Connect in non-blocking error");
        return -1;
    }
    return 0;
}

/**
 * This will add specified @ev to the epoll events.
 * 
 * @ev: Generally, we provide EPOLLIN, EPOLLOUT, EPOLLET for the caller.
 */
int monitor_sd_event(int ep_fd, int fd, unsigned int ev)
{
    struct epoll_event ee;

    ee.events = ev;
    ee.data.fd = fd;
    if (epoll_ctl(ep_fd, EPOLL_CTL_ADD, fd, &ee) < 0) {
        perrord(ERR "Add fd to epoll events error");
        return -1;
    }
    return 0;
}

/**
 * This will modify specified @ev to the epoll events.
 * 
 * @ev: Generally, we provide EPOLLIN, EPOLLOUT, EPOLLET for the caller.
 */
int update_sd_event(int ep_fd, int fd, unsigned int ev)
{
    struct epoll_event ee;

    ee.events = ev;
    ee.data.fd = fd;
    if (epoll_ctl(ep_fd, EPOLL_CTL_MOD, fd, &ee) < 0) {
        perrord(ERR "Add fd to epoll events error");
        return -1;
    }
    return 0;
}

/*
 * Set socket descriptor block mode:
 * block:       1
 * nonblock:    0
 */
int set_block_mode(int fd, int mode)
{
    int flags = 0;

    if ((flags = fcntl(fd, F_GETFL)) < 0) {
        perrord(ERR "Get flags of socket error");
        return -1;
    }

    flags = mode ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
    if (fcntl(fd, F_SETFL, flags) < 0) {
        perrord(ERR "Set flags of socket error");
        return -1;
    }
    return 0;
}

char *make_date_hdr(void)
{
    static char date[MAX_DATE_SZ] = {0};
    time_t tm;

    tm = time(NULL);
    strftime(date, sizeof(date),
             "%a, %b %d %Y %H:%M:%S GMT", gmtime(&tm));
    return date;
}

unsigned long long time_now(void)
{
    struct timeval now;

    gettimeofday(&now, NULL);
    return (unsigned long long)(now.tv_sec) * MILLION + now.tv_usec;
}

void print_hex(const char *buf, unsigned sz)
{
    int i = 0;

    printf("--------------------print_hex [%d]-------------------------------\n", sz);
    for (i = 0; i < sz; i++) {
        printf("%02x ", (unsigned char)buf[i]);
        if ((i + 1) % 16 == 0) {
            printf("\n");
        } else if ((i + 1) % 8 == 0) {
            printf(" ");
        }
    }
    printf("\n-----------------------------------------------------------------\n\n");

    return;
}
