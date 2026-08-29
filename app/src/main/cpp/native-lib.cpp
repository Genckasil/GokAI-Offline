#include <jni.h>
#include <string>
#include "llama.h"

static llama_model * g_model = nullptr;
static llama_context * g_context = nullptr;
static bool backend_ready = false;

static void free_model() {

    if (g_context != nullptr) {
        llama_free(g_context);
        g_context = nullptr;
    }

    if (g_model != nullptr) {
        llama_model_free(g_model);
        g_model = nullptr;
    }
}

extern "C"
JNIEXPORT jstring JNICALL
Java_com_gokai_offline_MainActivity_nativeTest(
        JNIEnv * env,
        jobject /* thiz */) {

    return env->NewStringUTF(
            "GokAI Offline llama.cpp motoru hazır"
    );
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_gokai_offline_MainActivity_nativeLoadModel(
        JNIEnv * env,
        jobject /* thiz */,
        jstring modelPath) {

    if (modelPath == nullptr) {
        return JNI_FALSE;
    }

    if (!backend_ready) {
        llama_backend_init();
        backend_ready = true;
    }

    free_model();

    const char * path =
            env->GetStringUTFChars(
                    modelPath,
                    nullptr
            );

    if (path == nullptr) {
        return JNI_FALSE;
    }

    llama_model_params modelParams =
            llama_model_default_params();

    // Telefon için önce CPU kullanıyoruz.
    // Böylece daha fazla cihazda stabil çalışır.
    modelParams.n_gpu_layers = 0;

    g_model =
            llama_model_load_from_file(
                    path,
                    modelParams
            );

    env->ReleaseStringUTFChars(
            modelPath,
            path
    );

    if (g_model == nullptr) {
        return JNI_FALSE;
    }

    llama_context_params contextParams =
            llama_context_default_params();

    // İlk sürümde RAM kullanımını kontrollü tutuyoruz.
    contextParams.n_ctx = 2048;
    contextParams.n_batch = 512;
    contextParams.n_ubatch = 512;

    // Android telefonda makul başlangıç.
    contextParams.n_threads = 4;
    contextParams.n_threads_batch = 4;

    g_context =
            llama_init_from_model(
                    g_model,
                    contextParams
            );

    if (g_context == nullptr) {

        llama_model_free(g_model);
        g_model = nullptr;

        return JNI_FALSE;
    }

    return JNI_TRUE;
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_gokai_offline_MainActivity_nativeIsModelLoaded(
        JNIEnv * /* env */,
        jobject /* thiz */) {

    return
            g_model != nullptr &&
            g_context != nullptr
            ? JNI_TRUE
            : JNI_FALSE;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_gokai_offline_MainActivity_nativeUnloadModel(
        JNIEnv * /* env */,
        jobject /* thiz */) {

    free_model();
}
