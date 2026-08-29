#include <jni.h>
#include <string>
#include <vector>

#include "llama.h"

static llama_model * g_model = nullptr;
static llama_context * g_context = nullptr;
static bool backend_ready = false;

static void free_context() {
    if (g_context != nullptr) {
        llama_free(g_context);
        g_context = nullptr;
    }
}

static void free_all() {
    free_context();

    if (g_model != nullptr) {
        llama_model_free(g_model);
        g_model = nullptr;
    }
}

static bool create_context() {
    if (g_model == nullptr) {
        return false;
    }

    free_context();

    llama_context_params ctx_params =
            llama_context_default_params();

    ctx_params.n_ctx = 2048;
    ctx_params.n_batch = 256;
    ctx_params.n_ubatch = 256;

    ctx_params.n_threads = 6;
    ctx_params.n_threads_batch = 6;

    g_context =
            llama_init_from_model(
                    g_model,
                    ctx_params
            );

    return g_context != nullptr;
}

extern "C"
JNIEXPORT jstring JNICALL
Java_com_gokai_offline_MainActivity_nativeTest(
        JNIEnv * env,
        jobject) {

    return env->NewStringUTF(
            "GokAI Offline motor hazır"
    );
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_gokai_offline_MainActivity_nativeLoadModel(
        JNIEnv * env,
        jobject,
        jstring modelPath) {

    if (modelPath == nullptr) {
        return JNI_FALSE;
    }

    if (!backend_ready) {
        llama_backend_init();
        backend_ready = true;
    }

    free_all();

    const char * path =
            env->GetStringUTFChars(
                    modelPath,
                    nullptr
            );

    if (path == nullptr) {
        return JNI_FALSE;
    }

    llama_model_params model_params =
            llama_model_default_params();

    model_params.n_gpu_layers = 0;

    g_model =
            llama_model_load_from_file(
                    path,
                    model_params
            );

    env->ReleaseStringUTFChars(
            modelPath,
            path
    );

    if (g_model == nullptr) {
        return JNI_FALSE;
    }

    if (!create_context()) {
        free_all();
        return JNI_FALSE;
    }

    return JNI_TRUE;
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_gokai_offline_MainActivity_nativeIsModelLoaded(
        JNIEnv *,
        jobject) {

    return
            g_model != nullptr &&
            g_context != nullptr
            ? JNI_TRUE
            : JNI_FALSE;
}

extern "C"
JNIEXPORT jstring JNICALL
Java_com_gokai_offline_MainActivity_nativeGenerate(
        JNIEnv * env,
        jobject,
        jstring userText) {

    if (g_model == nullptr) {
        return env->NewStringUTF(
                "Önce offline modeli yükle."
        );
    }

    if (userText == nullptr) {
        return env->NewStringUTF(
                "Bir soru yaz."
        );
    }

    const char * raw =
            env->GetStringUTFChars(
                    userText,
                    nullptr
            );

    if (raw == nullptr) {
        return env->NewStringUTF(
                "Mesaj okunamadı."
        );
    }

    std::string user(raw);

    env->ReleaseStringUTFChars(
            userText,
            raw
    );

    if (!create_context()) {
        return env->NewStringUTF(
                "Model belleği hazırlanamadı."
        );
    }

    const llama_vocab * vocab =
            llama_model_get_vocab(
                    g_model
            );

    if (vocab == nullptr) {
        return env->NewStringUTF(
                "Model sözlüğü bulunamadı."
        );
    }

    std::string system_prompt =
            "Sen GökAI'sın. "
            "Göktuğ Ege Genç tarafından geliştirilen bir yapay zekasın. "
            "Kendini ChatGPT olarak tanıtma. "
            "Türkçe cevap ver. "
            "Cevapların kısa, net ve faydalı olsun. "
            "Gereksiz uzun düşünme yapma.";

    std::string user_prompt =
            user +
            "\n/no_think";

    std::vector<llama_chat_message> messages;

    messages.push_back({
            "system",
            system_prompt.c_str()
    });

    messages.push_back({
            "user",
            user_prompt.c_str()
    });

    const char * chat_template =
            llama_model_chat_template(
                    g_model,
                    nullptr
            );

    std::vector<char> formatted(4096);

    int formatted_len =
            llama_chat_apply_template(
                    chat_template,
                    messages.data(),
                    messages.size(),
                    true,
                    formatted.data(),
                    formatted.size()
            );

    if (formatted_len > (int) formatted.size()) {

        formatted.resize(
                formatted_len
        );

        formatted_len =
                llama_chat_apply_template(
                        chat_template,
                        messages.data(),
                        messages.size(),
                        true,
                        formatted.data(),
                        formatted.size()
                );
    }

    if (formatted_len < 0) {
        return env->NewStringUTF(
                "Sohbet hazırlanamadı."
        );
    }

    std::string prompt(
            formatted.data(),
            formatted_len
    );

    int token_count =
            -llama_tokenize(
                    vocab,
                    prompt.c_str(),
                    prompt.size(),
                    nullptr,
                    0,
                    true,
                    true
            );

    if (token_count <= 0) {
        return env->NewStringUTF(
                "Mesaj işlenemedi."
        );
    }

    std::vector<llama_token> tokens(
            token_count
    );

    int tokenized =
            llama_tokenize(
                    vocab,
                    prompt.c_str(),
                    prompt.size(),
                    tokens.data(),
                    tokens.size(),
                    true,
                    true
            );

    if (tokenized < 0) {
        return env->NewStringUTF(
                "Mesaj tokenlara ayrılamadı."
        );
    }

    llama_batch batch =
            llama_batch_get_one(
                    tokens.data(),
                    tokens.size()
            );

    if (llama_decode(
            g_context,
            batch
    ) != 0) {

        return env->NewStringUTF(
                "Model soruyu işleyemedi."
        );
    }

    llama_sampler * sampler =
            llama_sampler_chain_init(
                    llama_sampler_chain_default_params()
            );

    llama_sampler_chain_add(
            sampler,
            llama_sampler_init_min_p(
                    0.05f,
                    1
            )
    );

    llama_sampler_chain_add(
            sampler,
            llama_sampler_init_temp(
                    0.7f
            )
    );

    llama_sampler_chain_add(
            sampler,
            llama_sampler_init_dist(
                    LLAMA_DEFAULT_SEED
            )
    );

    std::string response;

    const int MAX_NEW_TOKENS = 128;

    for (int i = 0; i < MAX_NEW_TOKENS; i++) {

        llama_token token =
                llama_sampler_sample(
                        sampler,
                        g_context,
                        -1
                );

        if (llama_vocab_is_eog(
                vocab,
                token
        )) {
            break;
        }

        char buffer[512];

        int piece_len =
                llama_token_to_piece(
                        vocab,
                        token,
                        buffer,
                        sizeof(buffer),
                        0,
                        true
                );

        if (piece_len > 0) {
            response.append(
                    buffer,
                    piece_len
            );
        }

        llama_batch next =
                llama_batch_get_one(
                        &token,
                        1
                );

        if (llama_decode(
                g_context,
                next
        ) != 0) {
            break;
        }
    }

    llama_sampler_free(
            sampler
    );

    if (response.empty()) {
        response =
                "Bu soruya cevap üretemedim.";
    }

    return env->NewStringUTF(
            response.c_str()
    );
}

extern "C"
JNIEXPORT void JNICALL
Java_com_gokai_offline_MainActivity_nativeUnloadModel(
        JNIEnv *,
        jobject) {

    free_all();
}
