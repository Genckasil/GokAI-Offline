#include <jni.h>

extern "C"
JNIEXPORT jstring JNICALL
Java_com_gokai_offline_MainActivity_nativeTest(
        JNIEnv *env,
        jobject /* thiz */) {

    return env->NewStringUTF("GokAI Offline native motor hazir");
}
