/*********************************************************************
 * File Name    : librtspcli.c
 * Description  : Interface to user layer.
 * Author       : Hu Lizhen
 * Create Date  : 2012-12-21
 ********************************************************************/

#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "librtspcli.h"
#include "rtsp_method.h"
#include "rtsp_cli.h"
#include "log.h"


/* A global variable. */
struct rtsp_cli rtsp_cli;


unsigned long open_chn(char *uri, struct chn_info *chnp, int intlvd)
{
    unsigned long usr_id = 0;
    struct rtsp_sess *sessp = NULL;
    struct sockaddr_in srv_addr;
    char ip_addr[16] = {0};
    unsigned short port = DFL_RTSP_PORT;

    if (strlen(uri) >= MAX_URI_SZ||
        intlvd < 0 || intlvd > 1 ||
        !chnp) {
        printd(ERR "Wrong server address or channel information!\n");
        return 0;
    }

    /* RTSP server address. */
    if (sscanf(uri, "rtsp://%[0-9.]:%hu%*s", ip_addr, &port) != 2) {
        if (sscanf(uri, "rtsp://%[0-9.]%*s", ip_addr) != 1) {
        }
    }
    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(port);
    srv_addr.sin_addr.s_addr = inet_addr(ip_addr);

    sessp = create_rtsp_sess(uri, &srv_addr, chnp, intlvd);
    usr_id = (unsigned long)sessp;

    return usr_id;
}

void close_chn(unsigned long usr_id)
{
    struct rtsp_sess *sessp = NULL;

    if (!usr_id) {
        printd(ERR "Illegal user ID!\n");
        return;
    }
    sessp = (struct rtsp_sess *)usr_id;

    sessp->todo = RTSP_METHOD_TEARDOWN;
    send_method_teardown(sessp);
    return;
}

int init_rtsp_cli(store_frm_t store_frm)
{
    if (!store_frm) {
        printf("\033[31m    *** RTSP callback MUST be set first! ***\033[0m\n");
        return -1;
    }
    rtsp_cli.store_frm = store_frm;
    INIT_LIST_HEAD(&rtsp_cli.rtsp_sess_list);
    pthread_mutex_init(&rtsp_cli.list_mutex, NULL);

    return 0;
}

void deinit_rtsp_cli(void)
{
    struct rtsp_sess *sessp = NULL;
    struct rtsp_sess *tmp = NULL;

    list_for_each_entry_safe(sessp, tmp, &rtsp_cli.rtsp_sess_list, entry) {
        destroy_rtsp_sess(sessp);
    }

    pthread_mutex_destroy(&rtsp_cli.list_mutex);
    return;
}

int chn_playing(unsigned long usr_id)
{
    struct rtsp_sess *sessp = NULL;

    if (!usr_id) {
        printd(ERR "Illegal user ID!\n");
        return 0;
    }
    sessp = (struct rtsp_sess *)usr_id;

    return sessp->rtsp_state == RTSP_STATE_PLAYING;
}
