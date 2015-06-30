/*********************************************************************
 * File Name    : rtspcli_demo.c
 * Description  : Demostrate how to use this library.
 * Author       : Hu Lizhen
 * Create Date  : 2012-12-21
 ********************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include "librtspcli.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>

static void signal_handler(int signo)
{
    switch (signo) {
    case SIGINT:
        exit(0);
        break;
    case SIGPIPE:
        break;
    default:
        break;
    }
    return;
}

static void init_signals(void)
{
    signal(SIGINT, signal_handler);
    signal(SIGPIPE, signal_handler);
    return;
}

static int store_frm(struct chn_info *chnp, struct frm_info *frmp)
{
#if 1
    static int fd = -1;
    if (fd < 0) {
        system("rm /tmp/recvd.dat -rf");
        fd = open("/tmp/recvd.dat", O_WRONLY | O_CREAT);
        if (fd < 0) {
            perror("open file error");
            exit(1);
        }
    }
    if (write(fd, frmp->frm_buf + chnp->frm_hdr_sz, frmp->frm_sz) < 0) {
        perror("write to file error");
        exit(1);
    }
#endif
#if 1
    static int cnt = 0;
    printf("------------>[cnt=%d], size = %d\n", cnt, frmp->frm_sz);
    cnt++;
#endif
    return 0;
}

int main(int argc, char *argv[])
{
    unsigned long usr_id = 0;
    struct chn_info chn_info;

    init_signals();

    if (init_rtsp_cli(store_frm) < 0) {
        return -1;
    }

    /* remote channel information */
    memset(&chn_info, 0, sizeof(chn_info));
    chn_info.frm_hdr_sz = 0;
    chn_info.usr_data = NULL;
    usr_id = open_chn("rtsp://172.18.16.158:10554/udp/av0_0", &chn_info, 0);
    if (!usr_id) {
        return -1;
    }

//    close_chn(usr_id);
    while (1) {
        sleep(3);
    }
    return 0;
}
