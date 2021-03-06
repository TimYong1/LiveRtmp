//
// Created by HWilliam on 2021/5/16.
//

/**
 * jni函数，面向Java
 *
 * 初始化
 * RTMP_init
 *
 * 连接
 * RTMP_JNI_Connect
 *
 * 发送数据
 * RTMP_JNI_SendData
 *
 * 销毁
 * RTMP_destroy
 */

#include <jni.h>
#include "rtmp_wrap.h"
#include "x264_wrap.h"
#include <cassert>
#include "log_abs.h"
#include "x264.h"
#include "VideoEncoder.h"
#include "safe_queue.h"
#include "hwutil.h"

// <editor-fold defaultstate="collapsed" desc="全局变量定义">
namespace {
    /**** 字符串常量定义 ****/
    const char *TAG = "LiveRtmp";
    const char *CLASS_RMP_JNI = "com/hwilliamgo/livertmp/jni/RTMPJni";
    const char *CLASS_X264_JNI = "com/hwilliamgo/livertmp/jni/X264Jni";
    const char *CLASS_RTMP_X264_JNI = "com/hwilliamgo/livertmp/jni/RTMPX264Jni";

    /**** java类定义 ****/
    jclass jni_class_rtmp = nullptr;
    jclass jni_class_x264 = nullptr;
    jclass jni_class_rtmp_x264 = nullptr;

    /**** 引擎定义 ****/
    VideoEncoder *videoEncoder = nullptr;

    /**
     * Rtmp状态
     */
    enum class RtmpStatus : int {
        INIT,
        START,
        STOP,
        DESTROY
    };
    RtmpStatus rtmpStatus = RtmpStatus::DESTROY;

    pthread_t pid;
    //阻塞式队列
//    SafeQueue<RTMPPacket *> *packets = nullptr;
}
// </editor-fold>

static void createBlockingQueue(SafeQueue<RTMPPacket *> *packets) {

}

static void destroyBlockingQueue(SafeQueue<RTMPPacket *> *packets) {
    if (packets) {
        packets->clear();
        delete packets;
    }
}

// <editor-fold defaultstate="collapsed" desc="JNI函数定义-RTMP">
static void RTMP_init(JNIEnv *env, jclass clazz) {
    MyLog::init_log(LogType::SimpleLog, TAG);
    MyLog::v(__func__);
}

static jboolean RTMP_JNI_Connect(JNIEnv *env, jclass clazz, jstring url_) {
    const char *url = env->GetStringUTFChars(url_, nullptr);
    if (url) {
        MyLog::v("Java_com_hwilliamgo_livertmp_ScreenLive_connect, url=%s", url);
    }
//    链接   服务器   重试几次
    int ret = RtmpWrap::connect(url);
    env->ReleaseStringUTFChars(url_, url);
    return ret;
}

static jboolean RTMP_JNI_SendData(JNIEnv *env, jclass clazz, jbyteArray data_,
                                  jint len,
                                  jlong tms) {
    int ret;
    jbyte *data = env->GetByteArrayElements(data_, nullptr);
    ret = RtmpWrap::sendVideo(data, len, tms);
    env->ReleaseByteArrayElements(data_, data, 0);
    return ret;
}

static void RTMP_destroy(JNIEnv *env, jclass clazz) {
    MyLog::destroy_log();
}
// </editor-fold>

// <editor-fold defaultstate="collapsed" desc="JNI函数定义-RTMP_X264">
static void RTMPX264Jni_native_init(JNIEnv *env, jclass clazz) {
    rtmpStatus = RtmpStatus::INIT;
    MyLog::init_log(LogType::SimpleLog, TAG);
    MyLog::v(__func__);
    videoEncoder = new VideoEncoder();
}

static void RTMPX264Jni_native_setVideoEncoderInfo
        (JNIEnv *env, jclass clazz, jint width, jint heigth, jint fps, jint bitrate) {
    if (videoEncoder) {
        videoEncoder->setVideoEncInfo(width, heigth, fps, bitrate);
    }
}

static void RTMPX264Jni_native_start
        (JNIEnv *env, jclass clazz, jstring path) {
    rtmpStatus = RtmpStatus::START;
    const char *url = env->GetStringUTFChars(path, nullptr);

    int ret = RtmpWrap::connect(url);
//    if (packets) {
//
//    }

    env->ReleaseStringUTFChars(path, url);
}

static void RTMPX264Jni_native_pushVideo
        (JNIEnv *env, jclass clazz, jbyteArray yuvData) {

}

static void RTMPX264Jni_native_stop
        (JNIEnv *env, jclass clazz) {
    rtmpStatus = RtmpStatus::STOP;
}

static void RTMPX264Jni_native_release
        (JNIEnv *env, jclass clazz) {
    rtmpStatus = RtmpStatus::DESTROY;
    if (videoEncoder) {
        delete videoEncoder;
        videoEncoder = nullptr;
    }
}
// </editor-fold>

static void X264Jni_init(JNIEnv *env, jclass clazz) {
    MyLog::init_log(LogType::SimpleLog, TAG);
    X264Wrap::init();
}

static void
X264Jni_setVideoCodecInfo(JNIEnv *env, jclass clazz, int width, int height, int fps, int bitrate) {
    X264Wrap::setVideoCodecInfo(width, height, fps, bitrate);
}

static void X264Jni_encode(JNIEnv *env, jclass clazz, jbyteArray yuvData) {
    int8_t *data = env->GetByteArrayElements(yuvData, nullptr);
    X264Wrap::encode(data);
    env->ReleaseByteArrayElements(yuvData, data, JNI_ABORT);
}

static void X264Jni_destroy(JNIEnv *env, jclass clazz) {
    X264Wrap::destroy();
    MyLog::destroy_log();
}

// <editor-fold defaultstate="collapsed" desc="动态加载jni函数">
static JNINativeMethod g_methods_rtmp[] = {
        {"init",     "()V",                   (void *) RTMP_init},
        {"sendData", "([BIJ)Z",               (void *) RTMP_JNI_SendData},
        {"connect",  "(Ljava/lang/String;)Z", (void *) RTMP_JNI_Connect},
        {"destroy",  "()V",                   (void *) RTMP_destroy}
};
static JNINativeMethod g_methods_x264[] = {
        {"init",              "()V",     (void *) X264Jni_init},
        {"setVideoCodecInfo", "(IIII)V", (void *) X264Jni_setVideoCodecInfo},
        {"encode",            "([B)V",   (void *) X264Jni_encode},
        {"destroy",           "()V",     (void *) X264Jni_destroy}
};
static JNINativeMethod g_methods_rtmp_x264[] = {
        {"native_init",                "()V",                   (void *) RTMPX264Jni_native_init},
        {"native_setVideoEncoderInfo", "(IIII)V",               (void *) RTMPX264Jni_native_setVideoEncoderInfo},
        {"native_start",               "(Ljava/lang/String;)V", (void *) RTMPX264Jni_native_start},
        {"native_pushVideo",           "([B)V",                 (void *) RTMPX264Jni_native_pushVideo},
        {"native_stop",                "()V",                   (void *) RTMPX264Jni_native_stop},
        {"native_release",             "()V",                   (void *) RTMPX264Jni_native_release}
};

static void registerJniMethod(JNIEnv *env,
                              jclass *jniClass,
                              const char *classFullName,
                              JNINativeMethod *allMethods,
                              int methodLength) {
    *jniClass = env->FindClass(classFullName);
    env->RegisterNatives(*jniClass, allMethods, methodLength);
}

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    JNIEnv *env = nullptr;

    if (vm->GetEnv((void **) &env, JNI_VERSION_1_4) != JNI_OK) {
        return -1;
    }
    assert(env != nullptr);

    registerJniMethod(env, &jni_class_rtmp, CLASS_RMP_JNI,
                      g_methods_rtmp, NELEM(g_methods_rtmp));
    registerJniMethod(env, &jni_class_x264, CLASS_X264_JNI,
                      g_methods_x264, NELEM(g_methods_x264));
    registerJniMethod(env, &jni_class_rtmp_x264, CLASS_RTMP_X264_JNI,
                      g_methods_rtmp_x264, NELEM(g_methods_rtmp_x264));

    return JNI_VERSION_1_4;
}

JNIEXPORT void JNI_OnUnload(JavaVM *jvm, void *reserved) {

}
// </editor-fold>


