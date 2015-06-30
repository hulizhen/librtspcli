/*********************************************************************
 * File Name    : parser.c
 * Description  : Parser of RTSP response message.
 * Author       : Hu Lizhen
 * Create Date  : 2012-12-26
 ********************************************************************/

#include <stdio.h>
#include "util.h"
#include "log.h"
#include "list.h"
#include "rtsp_cli.h"
#include "parser.h"


/**
 * Return the next pointer.
 *
 * Refer to RFC2326 section 4:
 * Lines are terminated by CRLF, but we should be prepared to
 * also interpret CR and LF by themselves as line terminators.
 */
static const char *get_next_line(const char *str)
{
    const char *line = NULL;
    if (!str) {
        return NULL;
    }

    while (*str) {
        if ((*str == '\r' && *(str + 1) == '\n' && *(str + 2) == '\r' && *(str + 3) == '\n') ||
            (*str == '\r' && *(str + 1) == '\r') ||
            (*str == '\n' && *(str + 1) == '\n')) {
            break;
        } else if (*str == '\r' && *(str + 1) == '\n') {
            line = str + 2;
            break;
        } else if (*str == '\r' || *str == '\n') {
            line = str + 1;
            break;
        }
        str++;
    }
    return line;
}

struct sdp_info *alloc_sdp_info(void)
{
    struct sdp_info *sdp = NULL;
    int i = 0;

    sdp = mallocz(sizeof(*sdp));
    if (!sdp) {
        printd("Allocate memory for struct sdp_info failed!\n");
        return NULL;
    }

    for (i = 0; i < 2; i++) {
        INIT_LIST_HEAD(&sdp->sdp_m[i].sdp_a_list);
    }
    return sdp;
}

void free_sdp_info(struct sdp_info *sdp)
{
    freez(sdp);
    return;
}

static int parse_sdp_info(struct sdp_info *sdp, const char *line)
{
    int i = 0;
    int v = 0;
    struct sdp_a *a = NULL;
    enum media_type media = 0;
    struct sdp_a *sdp_a = NULL;
    struct sdp_a *tmp = NULL;

    while (line) {
        switch (line[0]) {
        case 'v':
            if (sscanf(line, "v=%d", &v) == 1) {
                if (v) {        /* Current SDP version is zero. */
                    printd(WARNING "Wrong SDP version!\n");
                    goto err;
                }
            }
            break;
        case 'o':
            /* Not parse this line yet. */
            break;
        case 's':
            /* Not parse this line yet. */
            break;
        case 't':
            /* Not parse this line yet. */
            break;
        case 'm':
            if (!strncmp(line, "m=video", strlen("m=video"))) {
                media = MEDIA_TYPE_VIDEO;
            } else if (!strncmp(line, "m=audio", strlen("m=audio"))) {
                media = MEDIA_TYPE_AUDIO;
            } else {
                break;
            }

            sdp->sdp_m[media].enable = 1;
            if (sscanf(line, "%*[^ ] %d %s %d",
                       &sdp->sdp_m[media].port,
                       sdp->sdp_m[media].proto,
                       (int *)&sdp->sdp_m[media].pt) != 3) {
                printd(ERR "sscanf() for sdp_m line failed!\n");
                goto err;
            }

            /* Get the attribute corresponding with the media. */
            while (line) {
                line = get_next_line(line);
                if (!strncmp(line, "a=rtpmap", strlen("a=rtpmap"))) {
                    a = mallocz(sizeof(*a));
                    if (!a) {
                        printd(ERR "Allocate memory for struct sdp_a failed!\n");
                        goto err;
                    }
                    memcpy(a->name, "rtpmap", strlen("rtpmap"));
                    if (sscanf(line, "a=rtpmap:%d %[^/]/%d%*s",
                               (int *)&a->rtpmap.pt,
                               a->rtpmap.enc_name,
                               &a->rtpmap.clk_rate) != 3) {
                        printd(ERR "sscanf() for sdp_a line failed!\n");
                        goto err;
                    }
                    list_add_tail(&a->entry, &sdp->sdp_m[media].sdp_a_list);
                } else if (!strncmp(line, "a=control", strlen("a=control"))) {
                    a = mallocz(sizeof(*a));
                    if (!a) {
                        printd(ERR "Allocate memory for struct sdp_a failed!\n");
                        goto err;
                    }
                    memcpy(a->name, "control", strlen("control"));
                    if (sscanf(line, "a=control:%s", a->control.track) != 1) {
                        printd(ERR "sscanf() for sdp_a line failed!\n");
                        goto err;
                    }
                    list_add_tail(&a->entry, &sdp->sdp_m[media].sdp_a_list);
                } else if (!strncmp(line, "b=AS", strlen("b=AS"))) {
                } else {
                    /*
                     * Here we minus 1, so that the next call of
                     * get_next_line() at the end of the outermost while loop
                     * can get the current line again.
                     */
                    line--;
                    break;
                }
            }
            break;
        default:
            /* Needn't parse this just break. */
            break;
        }

        line = get_next_line(line);
    }
    return 0;

err:
    for (i = 0; i < 2; i++) {
        list_for_each_entry_safe(sdp_a, tmp, &sdp->sdp_m[i].sdp_a_list, entry) {
            list_del(&sdp_a->entry);
            freez(sdp_a);
        }
    }
    return -1;
}

static int parse_hdr_content(struct rtsp_resp *resp, const char *line)
{
    const char *ptr = NULL;

    if (sscanf(line + strlen("Content-Type: "), "%[^\r\n]",
               resp->resp_hdr.content_type) != 1) {
        return -1;
    }

    while (line) {
        line = get_next_line(line);
        if (!strncasecmp(line, "Content-Length", strlen("Content-Length"))) {
            if (sscanf(line, "Content-Length: %d",
                       &resp->resp_hdr.content_length) != 1) {
                return -1;
            }
            if (resp->resp_hdr.content_length > strlen("\r\n")) {
                /* Skip the terminal '\r\n\r\n'. */
                if ((ptr = strstr(line, "\r\n")) != NULL) {
                    line = ptr + strlen("\r\n");
                } else {
                    if ((ptr = strstr(line, "\n")) != NULL) {
                        line = ptr + strlen("\n");
                    } else {
                        printd(WARNING "Not found line terminal.\n");
                        return -1;
                    }
                }

                if (strstr(resp->resp_hdr.content_type, "sdp")) {
                    resp->sdp_info = alloc_sdp_info();
                    if (parse_sdp_info(resp->sdp_info, line) < 0) {
                        free_sdp_info(resp->sdp_info);
                        return -1;
                    } else {
                        break;
                    }
                }
            }
        }
    }

    return 0;
}

static int parse_hdr_transport(struct rtsp_resp *resp, const char *line)
{
    const char *ptr = line;

    if (strncmp(ptr, "Transport: ", strlen("Transport: "))) {
        return -1;              /* Not a transport field! */
    }
    ptr += strlen("Transport: ");

    while (*ptr) {
        if (!strncmp(ptr, "RTP/AVP/TCP", strlen("RTP/AVP/TCP"))) {
            resp->resp_hdr.transport.intlvd_mode = 1;
        } else if (!strncmp(ptr, "RTP/AVP/UDP", strlen("RTP/AVP/UDP")) ||
                   !strncmp(ptr, "RTP/AVP", strlen("RTP/AVP"))) {
            resp->resp_hdr.transport.intlvd_mode = 0;
        } else if (!strncmp(ptr, "unicast", strlen("unicast"))) {
        } else if (!strncmp(ptr, "mode", strlen("mode"))) {
        } else if (!strncmp(ptr, "client_port", strlen("client_port"))) {
            if (!resp->resp_hdr.transport.intlvd_mode) {
                if (sscanf(ptr, "client_port=%hu-%hu",
                           &resp->resp_hdr.transport.rtp_cli_port,
                           &resp->resp_hdr.transport.rtcp_cli_port) != 2) {
                    return -1;
                }
            }
        } else if (!strncmp(ptr, "server_port", strlen("server_port"))) {
            if (!resp->resp_hdr.transport.intlvd_mode) {
                if (sscanf(ptr, "server_port=%hu-%hu",
                           &resp->resp_hdr.transport.rtp_srv_port,
                           &resp->resp_hdr.transport.rtcp_srv_port) != 2) {
                    return -1;
                }
            }
        } else if (!strncmp(ptr, "interleaved", strlen("interleaved"))) {
            if (resp->resp_hdr.transport.intlvd_mode) {
                if (sscanf(ptr, "interleaved=%hhd-%hhd",
                           &resp->resp_hdr.transport.rtp_chn,
                           &resp->resp_hdr.transport.rtcp_chn) != 2) {
                    return -1;
                }
            }
        }

        ptr = strpbrk(ptr, ";\r\n");
        while (*ptr == ';') {   /* Skip over separating ';' chars. */
            ptr++;
        }
        if (*ptr == '\0' || *ptr == '\r' || *ptr == '\n') {
            break;
        }
    }
    return 0;
}

static int parse_resp_line(struct rtsp_resp *resp, const char *line)
{
    /* Get RTSP response status line. */
    if (sscanf(line, "%s %d %[^\r\n]",
               resp->resp_line.ver,
               (int *)&resp->resp_line.code,
               resp->resp_line.reason) != 3) {
        return -1;
    }

    /* Verify RTSP version. */
    if (strncmp(resp->resp_line.ver, RTSP_VER, sizeof(RTSP_VER))) {
        return -1;
    }

    if (resp->resp_line.code != OK) {
        printd(WARNING "RTSP response error: %s!\n", resp->resp_line.reason);
        return -1;
    }

    return 0;
}

static int parse_resp_hdrs(struct rtsp_resp *resp, const char *line)
{
    int cseq_found = 0;

    while (line) {
        if (!strncasecmp(line, "CSeq", strlen("CSeq"))) {
            if (sscanf(line + strlen("CSeq: "), "%d",
                       &resp->resp_hdr.cseq) != 1) {
                return -1;
            }
            cseq_found = 1;
        } else if (!strncasecmp(line, "Date", strlen("Date"))) {
            if (sscanf(line + strlen("Date: "), "%[^\r\n]",
                       resp->resp_hdr.date) != 1) {
                return -1;
            }
        } else if (!strncasecmp(line, "Session", strlen("Session"))) {
            if (sscanf(line + strlen("Session: "), "%llu",
                       &resp->resp_hdr.sess_id) != 1) {
                return -1;
            }
        } else if (!strncasecmp(line, "Public", strlen("Public"))) {
            if (sscanf(line + strlen("Public: "), "%[^\r\n]",
                       resp->resp_hdr.public) != 1) {
                return -1;
            }
        } else if (!strncasecmp(line, "Transport", strlen("Transport"))) {
            if (parse_hdr_transport(resp, line) < 0) {
                return -1;
            }
        } else if (!strncasecmp(line, "Content-Type", strlen("Content-Type"))) {
            if (parse_hdr_content(resp, line) < 0) {
                return -1;
            } else {
                break;
            }
        }
        line = get_next_line(line);
    }

    if (!cseq_found) {
        printd(WARNING "CSeq not found!\n");
        return -1;
    }
    return 0;
}

int parse_rtsp_resp(struct rtsp_sess *sessp, struct rtsp_resp *resp,
                    const char *msg, unsigned int sz)
{
    const char *line = msg;

    print_rtsp_msg(msg, sz);

    /* Parse response line. */
    if (parse_resp_line(resp, line) < 0) {
        return -1;
    }

    /* Parse response headers. */
    line = get_next_line(line);
    if (parse_resp_hdrs(resp, line) < 0) {
        return -1;
    }

    return 0;
}
