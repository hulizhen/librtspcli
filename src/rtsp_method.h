/*********************************************************************
 * File Name    : rtsp_method.h
 * Description  : Implementation of RTSP method handlers.
 * Author       : Hu Lizhen
 * Create Date  : 2012-12-22
 ********************************************************************/

#ifndef __RTSP_METHOD_H__
#define __RTSP_METHOD_H__


/* RTSP methods. */
enum rtsp_method {
    RTSP_METHOD_OPTIONS,
    RTSP_METHOD_DESCRIBE,
    RTSP_METHOD_SETUP,
    RTSP_METHOD_PLAY,
    RTSP_METHOD_PAUSE,
    RTSP_METHOD_GET_PARAMETER,
    RTSP_METHOD_SET_PARAMETER,
    RTSP_METHOD_TEARDOWN,
    RTSP_METHOD_NUM,
    RTSP_METHOD_NONE = RTSP_METHOD_NUM,
};

static char *const rtsp_method_tkn[] = {
    "OPTIONS",
    "DESCRIBE",
    "SETUP",
    "PLAY",
    "PAUSE",
    "GET_PARAMETER",
    "SET_PARAMETER",
    "TEARDOWN",
};

struct rtsp_sess;

int send_method_options(struct rtsp_sess *sessp);
int send_method_describe(struct rtsp_sess *sessp);
int send_method_setup(struct rtsp_sess *sessp);
int send_method_play(struct rtsp_sess *sessp);
int send_method_pause(struct rtsp_sess *sessp);
int send_method_get_parameter(struct rtsp_sess *sessp);
int send_method_set_parameter(struct rtsp_sess *sessp);
int send_method_teardown(struct rtsp_sess *sessp);


#endif /* __RTSP_METHOD_H__ */

