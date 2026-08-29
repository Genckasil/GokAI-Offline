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

    // Telefon için başlangıç ayarları
    ctx_params.n_ctx = 4096;
    ctx_params.n_batch = 512;
    ctx_params.n_ubatch = 512;

    ctx_params.n_threads = 4;
    ctx_params.n_threads_batch = 4;

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

    // İlk sürüm CPU.
    // Daha sonra hızlandırma ekleyebiliriz.
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
JNIEXPORT jstring JNICALL
Java_com_gokai_offline_MainActivity_nativeGenerate(
        JNIEnv * env,
        jobject /* thiz */,
        jstring userText) {

    if (g_model == nullptr) {

        return env->NewStringUTF(
                "Önce offline modeli yükle."
        );
    }

    if (userText == nullptr) {

        return env->NewStringUTF(
                "Bir şey sorman gerekiyor."
        );
    }

    const char * raw_user =
            env->GetStringUTFChars(
                    userText,
                    nullptr
            );

    if (raw_user == nullptr) {

        return env->NewStringUTF(
                "Mesaj okunamadı."
        );
    }

    std::string user(raw_user);

    env->ReleaseStringUTFChars(
            userText,
            raw_user
    );

    /*
     * Her yeni soruda temiz context.
     * Böylece önceki KV cache taşmıyor.
     * Sohbet hafızasını daha sonra Java tarafında
     * kontrollü olarak prompta ekleyeceğiz.
     */
    if (!create_context()) {

        return env->NewStringUTF(
                "Model çalışma belleği oluşturulamadı."
        );
    }

    const llama_vocab * vocab =
            llama_model_get_vocab(
                    g_model
            );

    if (vocab == nullptr) {

        return env->NewStringUTF(
                "Model sözlüğü yüklenemedi."
        );
    }

    /*
     * GökAI kimliği.
     */
    std::string system_prompt =
            "Senin adın GökAI. "
            "Göktuğ Ege Genç tarafından geliştirilmiş bir yapay zekasın. "
            "Kullanıcı sana 'sen kimsin', 'adın ne', "
            "'hangi yapay zekasın' veya benzeri bir soru sorarsa "
            "kendini GökAI olarak tanıt. "
            "Kendini ChatGPT olarak tanıtma. "
            "Türkçe konuş. "
            "Doğal, yardımcı ve anlaşılır cevaplar ver.";

    std::vector<llama_chat_message> messages;

    messages.push_back({
            "system",
            system_prompt.c_str()
    });

    messages.push_back({
            "user",
            user.c_str()
    });

    const char * chat_template =
            llama_model_chat_template(
                    g_model,
                    nullptr
            );

    std::vector<char> formatted(8192);

    int formatted_len =
            llama_chat_apply_template(
                    chat_template,
                    messages.data(),
                    messages.size(),
                    true,
                    formatted.data(),
                    formatted.size()
            );

    if (formatted_len < 0) {

        return env->NewStringUTF(
                "Modelin sohbet şablonu uygulanamadı."
        );
    }

    if (formatted_len >
            (int) formatted.size()) {

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

        if (formatted_len < 0) {

            return env->NewStringUTF(
                    "Sohbet hazırlanamadı."
            );
        }
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
                "Mesaj tokenlara ayrılamadı."
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
                "Mesaj işlenemedi."
        );
    }

    llama_batch batch =
            llama_batch_get_one(
                    tokens.data(),
                    tokens.size()
            );

    int decode_result =
            llama_decode(
                    g_context,
                    batch
            );

    if (decode_result != 0) {

        return env->NewStringUTF(
                "Model soruyu işleyemedi."
        );
    }

    /*
     * Sampler
     */
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
                    0.75f
            )
    );

    llama_sampler_chain_add(
            sampler,
            llama_sampler_init_dist(
                    LLAMA_DEFAULT_SEED
            )
    );

    std::string response;

    const int MAX_NEW_TOKENS = 512;

    for (int i = 0;
         i < MAX_NEW_TOKENS;
         i++) {

        llama_token new_token =
                llama_sampler_sample(
                        sampler,
                        g_context,
                        -1
                );

        if (llama_vocab_is_eog(
                vocab,
                new_token
        )) {

            break;
        }

        char buffer[512];

        int piece_len =
                llama_token_to_piece(
                        vocab,
                        new_token,
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

        llama_batch next_batch =
                llama_batch_get_one(
                        &new_token,
                        1
                );

        int result =
                llama_decode(
                        g_context,
                        next_batch
                );

        if (result != 0) {
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
        JNIEnv * /* env */,
        jobject /* thiz */) {

    free_all();
}
