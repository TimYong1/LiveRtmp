//
// Created by HWilliam on 2021/5/16.
//

#ifndef LIVE_RTMP_WRAP_H
#define LIVE_RTMP_WRAP_H

#include "../../librtmp/rtmp.h"

namespace RtmpWrap {
    typedef struct Live {
        RTMP *rtmp;

        int16_t sps_len;
        int8_t *sps;

        int16_t pps_len;
        int8_t *pps;
    } Live;

/**
 * rtmp 建立连接
 * @param url url
 * @return 1表示成功，0表示失败
 */
    int connect(const char *url);

/**
 * 发送视频编码数据
 * @param buf 编码视频数据
 * @param len 数据长度
 * @param tms 时间戳
 * @return
 */
    int sendVideo(int8_t *buf, int len, long tms);
}


#endif //LIVE_RTMP_WRAP_H
