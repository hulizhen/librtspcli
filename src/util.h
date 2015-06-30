/*********************************************************************
 * File Name    : util.h
 * Description  : Provide some utilities.
 * Author       : Hu Lizhen
 * Create Date  : 2012-12-21
 ********************************************************************/

#ifndef __UTIL_H__
#define __UTIL_H__


#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>

#ifndef UINT_MAX
#define UINT_MAX    0xFFFFFFFF
#endif

#ifndef INT_MAX
#define INT_MAX     0x7FFFFFFF
#endif

#define MILLION (1000 * 1000)
#define THOUSAND 1000


enum status_code {
    CONTINUE                            = 100,
    OK                                  = 200,
    CREATED                             = 201,
    LOW_ON_STORAGE_SPACE                = 250,
    MULTIPLE_CHOICES                    = 300,
    MOVED_PERMANENTLY                   = 301,
    MOVED_TEMPORARILY                   = 302,
    SEE_OTHER                           = 303,
    NOT_MODIFIED                        = 304,
    USE_PROXY                           = 305,
    BAD_REQUEST                         = 400,
    UNAUTHORIZED                        = 401,
    PAYMENT_REQUIRED                    = 402,
    FORBIDDEN                           = 403,
    NOT_FOUND                           = 404,
    METHOD_NOT_ALLOWED                  = 405,
    NOT_ACCEPTABLE                      = 406,
    PROXY_AUTHENTICATION_REQUIRED       = 407,
    REQUEST_TIME_OUT                    = 408,
    GONE                                = 410,
    LENGTH_REQUIRED                     = 411,
    PRECONDITION_FAILED                 = 412,
    REQUEST_ENTITY_TOO_LARGE            = 413,
    REQUEST_URI_TOO_LARGE               = 414,
    UNSUPPORTED_MEDIA_TYPE              = 415,
    PARAMETER_NOT_UNDERSTOOD            = 451,
    CONFERENCE_NOT_FOUND                = 452,
    NOT_ENOUGH_BANDWIDTH                = 453,
    SESSION_NOT_FOUND                   = 454,
    METHOD_NOT_VALID_IN_THIS_STATE      = 455,
    HEADER_FIELD_NOT_VALID_FOR_RESOURCE = 456,
    INVALID_RANGE                       = 457,
    PARAMETER_IS_READ_ONLY              = 458,
    AGGREGATE_OPERATION_NOT_ALLOWED     = 459,
    ONLY_AGGREGATE_OPERATION_ALLOWED    = 460,
    UNSUPPORTED_TRANSPORT               = 461,
    DESTINATION_UNREACHABLE             = 462,
    INTERNAL_SERVER_ERROR               = 500,
    NOT_IMPLEMENTED                     = 501,
    BAD_GATEWAY                         = 502,
    SERVICE_UNAVAILABLE                 = 503,
    GATEWAY_TIME_OUT                    = 504,
    VERSION_NOT_SUPPORTED               = 505,
    OPTION_NOT_SUPPORTED                = 551,
};

/* Wrapped function [malloc() and free()], do the nessary stuff. */
#define mallocz(sz)                             \
    ({                                          \
        void *ptr = malloc(sz);                 \
        if (ptr) {                              \
            memset(ptr, 0, sz);                 \
        }                                       \
        ptr;                                    \
    })

#define freez(ptr)                              \
    ({                                          \
        if (ptr) {                              \
            free(ptr);                          \
            ptr = NULL;                         \
        }                                       \
    })

/**
 * Print elapsed time in micro-seconds since last time.
 *
 * NOTE:
 * non-reentrant.
 */
#define elapsed_time() do {                                             \
        struct timeval now_tv;                                          \
        static struct timeval old_tv;                                   \
        gettimeofday(&now_tv, NULL);                                    \
        printd(DEBUG "---------------------------------"                \
               "Elapsed micro-seconds since last time: [%d]us\n",       \
               (now_tv.tv_sec * MILLION + now_tv.tv_usec) -             \
               (old_tv.tv_sec * MILLION + old_tv.tv_usec));             \
        old_tv.tv_sec = now_tv.tv_sec;                                  \
        old_tv.tv_usec = now_tv.tv_usec;                                \
    } while (0)


const char *get_status_reason(unsigned int code);

int connect_nonb(int sd, const struct sockaddr *sap, socklen_t salen, int timeout);
int monitor_sd_event(int ep_fd, int fd, unsigned int ev);
int update_sd_event(int ep_fd, int fd, unsigned int ev);
int set_block_mode(int fd, int mode);

char *make_date_hdr(void);
unsigned long long time_now(void);
void print_hex(const char *buf, unsigned sz);


#endif /* __UTIL_H__ */
