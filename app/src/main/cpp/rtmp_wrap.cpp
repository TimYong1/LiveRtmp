#include "rtmp_wrap.h"
#include <jni.h>
#include <string>
#include <android/log.h>
#include  "librtmp/rtmp.h"


#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,"David",__VA_ARGS__)

static Live *globalLive = nullptr;

int connect(const char *url) {
    int ret;
    int urlLen = strlen(url + 1);
    char *inputUrl = new char[urlLen];
    memset(inputUrl, 0, urlLen + 1);
    memcpy(inputUrl, url, urlLen + 1);

    do {
//        实例化
        globalLive = (Live *) malloc(sizeof(Live));
        memset(globalLive, 0, sizeof(Live));

        globalLive->rtmp = RTMP_Alloc();
        RTMP_Init(globalLive->rtmp);
        globalLive->rtmp->Link.timeout = 10;
        if (strlen(inputUrl) == 0) {
            LOGI("input url is empty");
            return false;
        }
        LOGI("connect %s", inputUrl);
        if (!(ret = RTMP_SetupURL(globalLive->rtmp, inputUrl))) break;
        RTMP_EnableWrite(globalLive->rtmp);
        LOGI("RTMP_Connect");
        if (!(ret = RTMP_Connect(globalLive->rtmp, 0))) break;
        LOGI("RTMP_ConnectStream ");
        if (!(ret = RTMP_ConnectStream(globalLive->rtmp, 0))) break;
        LOGI("connect success");
    } while (false);
// sps  pps javabean
    if (!ret && globalLive) {
        free(globalLive);
        globalLive = nullptr;
    }
    return ret;
}


//传递第一帧      00 00 00 01 67 64 00 28ACB402201E3CBCA41408681B4284D4  0000000168  EE 06 F2 C0
static void initSpsPps(int8_t *data, int len, Live *live) {

    for (int i = 0; i < len; i++) {
//        防止越界
        if (i + 4 < len) {
            if (data[i] == 0x00 && data[i + 1] == 0x00
                && data[i + 2] == 0x00
                && data[i + 3] == 0x01) {
                if (data[i + 4] == 0x68) {
                    live->sps_len = i - 4;
//                    new一个数组
                    live->sps = static_cast<int8_t *>(malloc(live->sps_len));
//                    sps解析出来了
                    memcpy(live->sps, data + 4, live->sps_len);

//                    解析pps
                    live->pps_len = len - (4 + live->sps_len) - 4;
//                    实例化PPS 的数组
                    live->pps = static_cast<int8_t *>(malloc(live->pps_len));
//                    rtmp  协议

                    memcpy(live->pps, data + 4 + live->sps_len + 4, live->pps_len);
                    LOGI("sps:%d pps:%d", live->sps_len, live->pps_len);
                    break;
                }
            }

        }
    }
}

static RTMPPacket *createVideoPackage(Live *live) {
//sps  pps 的 packaet
    int body_size = 16 + live->sps_len + live->pps_len;
    RTMPPacket *packet = (RTMPPacket *) malloc(sizeof(RTMPPacket));
//    实例化数据包
    RTMPPacket_Alloc(packet, body_size);
    int i = 0;
    packet->m_body[i++] = 0x17;
    //AVC sequence header 设置为0x00
    packet->m_body[i++] = 0x00;
    //CompositionTime
    packet->m_body[i++] = 0x00;
    packet->m_body[i++] = 0x00;
    packet->m_body[i++] = 0x00;
    //AVC sequence header
    packet->m_body[i++] = 0x01;
//    原始 操作

    packet->m_body[i++] = live->sps[1]; //profile 如baseline、main、 high

    packet->m_body[i++] = live->sps[2]; //profile_compatibility 兼容性
    packet->m_body[i++] = live->sps[3]; //profile level
    packet->m_body[i++] = 0xFF;//已经给你规定好了
    packet->m_body[i++] = 0xE1; //reserved（111） + lengthSizeMinusOne（5位 sps 个数） 总是0xe1
//高八位
    packet->m_body[i++] = (live->sps_len >> 8) & 0xFF;
//    低八位
    packet->m_body[i++] = live->sps_len & 0xff;
//    拷贝sps的内容
    memcpy(&packet->m_body[i], live->sps, live->sps_len);
    i += live->sps_len;
//    pps
    packet->m_body[i++] = 0x01; //pps number
//rtmp 协议
    //pps length
    packet->m_body[i++] = (live->pps_len >> 8) & 0xff;
    packet->m_body[i++] = live->pps_len & 0xff;
//    拷贝pps内容
    memcpy(&packet->m_body[i], live->pps, live->pps_len);
//packaet
//视频类型
    packet->m_packetType = RTMP_PACKET_TYPE_VIDEO;
//
    packet->m_nBodySize = body_size;
//    视频 04
    packet->m_nChannel = 0x04;
    packet->m_nTimeStamp = 0;
    packet->m_hasAbsTimestamp = 0;
    packet->m_headerType = RTMP_PACKET_SIZE_LARGE;
    packet->m_nInfoField2 = live->rtmp->m_stream_id;
    return packet;
}

static RTMPPacket *createVideoPackage(int8_t *buf, int len, const long tms, Live *live) {
    buf += 4;
//长度
    RTMPPacket *packet = (RTMPPacket *) malloc(sizeof(RTMPPacket));
    int body_size = len + 9;
//初始化RTMP内部的body数组
    RTMPPacket_Alloc(packet, body_size);


    if (buf[0] == 0x65) {
        packet->m_body[0] = 0x17;
        LOGI("发送关键帧 data");
    } else {
        packet->m_body[0] = 0x27;
        LOGI("发送非关键帧 data");
    }
//    固定的大小
    packet->m_body[1] = 0x01;
    packet->m_body[2] = 0x00;
    packet->m_body[3] = 0x00;
    packet->m_body[4] = 0x00;

    //长度
    packet->m_body[5] = (len >> 24) & 0xff;
    packet->m_body[6] = (len >> 16) & 0xff;
    packet->m_body[7] = (len >> 8) & 0xff;
    packet->m_body[8] = (len) & 0xff;

    //数据
    memcpy(&packet->m_body[9], buf, len);
    packet->m_packetType = RTMP_PACKET_TYPE_VIDEO;
    packet->m_nBodySize = body_size;
    packet->m_nChannel = 0x04;
    packet->m_nTimeStamp = tms;
    packet->m_hasAbsTimestamp = 0;
    packet->m_headerType = RTMP_PACKET_SIZE_LARGE;
    packet->m_nInfoField2 = live->rtmp->m_stream_id;
    return packet;
}

static int sendPacket(RTMPPacket *packet) {
    int r = RTMP_SendPacket(globalLive->rtmp, packet, 1);
    RTMPPacket_Free(packet);
    free(packet);
    return r;
}


//传递第一帧      00 00 00 01 67 64 00 28ACB402201E3CBCA41408081B4284D4  0000000168 EE 06 F2 C0
int sendVideo(int8_t *buf, int len, long tms) {
    int ret = 0;
    if (buf[4] == 0x67) {
//        缓存sps 和pps 到全局变量 不需要推流
        if (globalLive && (!globalLive->pps || !globalLive->sps)) {
//缓存 没有推流
            initSpsPps(buf, len, globalLive);
        }
        return ret;
    }
//    I帧
    if (buf[4] == 0x65) {//关键帧
//         推两个
//sps 和 ppps 的paclet  发送sps pps
        RTMPPacket *packet = createVideoPackage(globalLive);
        sendPacket(packet);
//        发送I帧
    }
//    两个   I帧  0x17  B P 0x27
    RTMPPacket *packet2 = createVideoPackage(buf, len, tms, globalLive);
    ret = sendPacket(packet2);
    return ret;
}