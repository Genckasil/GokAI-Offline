package com.gokai.offline;

import android.app.Activity;
import android.os.Bundle;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.provider.OpenableColumns;
import android.webkit.JavascriptInterface;
import android.webkit.WebChromeClient;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import org.json.JSONObject;

import java.io.File;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.FileOutputStream;

public class MainActivity extends Activity {

    static {
        System.loadLibrary("gokai_native");
    }

    private WebView webView;

    private static final int MODEL_REQ = 200;

    public native String nativeTest();

    public native boolean nativeLoadModel(String path);

    public native boolean nativeIsModelLoaded();

    public native String nativeGenerate(String text);

    public native void nativeUnloadModel();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        webView = new WebView(this);
        setContentView(webView);

        WebSettings settings = webView.getSettings();

        settings.setJavaScriptEnabled(true);
        settings.setDomStorageEnabled(true);
        settings.setAllowFileAccess(true);
        settings.setAllowContentAccess(true);

        webView.setWebViewClient(new WebViewClient());
        webView.setWebChromeClient(new WebChromeClient());

        webView.addJavascriptInterface(
                new GokAIBridge(),
                "GokAIAndroid"
        );

        webView.loadUrl(
                "file:///android_asset/index.html"
        );
    }

    public class GokAIBridge {

        @JavascriptInterface
        public void pickModel() {

            runOnUiThread(() -> {

                Intent intent =
                        new Intent(
                                Intent.ACTION_OPEN_DOCUMENT
                        );

                intent.addCategory(
                        Intent.CATEGORY_OPENABLE
                );

                intent.setType("*/*");

                startActivityForResult(
                        intent,
                        MODEL_REQ
                );
            });
        }

        @JavascriptInterface
        public boolean isModelLoaded() {
            return nativeIsModelLoaded();
        }

        @JavascriptInterface
        public boolean modelExists() {
            return getModelFile().exists();
        }

        @JavascriptInterface
        public String getEngineStatus() {
            return nativeTest();
        }

        @JavascriptInterface
        public void loadSavedModel() {

            new Thread(() -> {

                File model =
                        getModelFile();

                if (!model.exists()) {

                    sendJs(
                            "window.onModelError(" +
                                    JSONObject.quote(
                                            "Kayıtlı model bulunamadı."
                                    ) +
                                    ");"
                    );

                    return;
                }

                sendJs(
                        "window.onModelLoading(" +
                                JSONObject.quote(
                                        "Model yükleniyor..."
                                ) +
                                ");"
                );

                boolean ok =
                        nativeLoadModel(
                                model.getAbsolutePath()
                        );

                if (ok) {

                    sendJs(
                            "window.onModelReady(" +
                                    JSONObject.quote(
                                            "GökAI Offline hazır."
                                    ) +
                                    ");"
                    );

                } else {

                    sendJs(
                            "window.onModelError(" +
                                    JSONObject.quote(
                                            "Model yüklenemedi."
                                    ) +
                                    ");"
                    );
                }

            }).start();
        }

        @JavascriptInterface
        public void askOffline(String text) {

            new Thread(() -> {

                try {

                    if (!nativeIsModelLoaded()) {

                        sendJs(
                                "window.onAIResult(" +
                                        JSONObject.quote(
                                                "Önce offline modeli yükle."
                                        ) +
                                        ");"
                        );

                        return;
                    }

                    if (text == null ||
                            text.trim().isEmpty()) {

                        return;
                    }

                    sendJs(
                            "window.onAIThinking();"
                    );

                    String answer =
                            nativeGenerate(
                                    text.trim()
                            );

                    if (answer == null ||
                            answer.trim().isEmpty()) {

                        answer =
                                "Bu soruya cevap üretemedim.";
                    }

                    sendJs(
                            "window.onAIResult(" +
                                    JSONObject.quote(
                                            answer
                                    ) +
                                    ");"
                    );

                } catch (Exception e) {

                    sendJs(
                            "window.onAIResult(" +
                                    JSONObject.quote(
                                            "Model hatası: " +
                                                    e.getMessage()
                                    ) +
                                    ");"
                    );
                }

            }).start();
        }

        @JavascriptInterface
        public void unloadModel() {

            nativeUnloadModel();

            sendJs(
                    "window.onModelUnloaded();"
            );
        }
    }

    @Override
    protected void onActivityResult(
            int requestCode,
            int resultCode,
            Intent data
    ) {

        super.onActivityResult(
                requestCode,
                resultCode,
                data
        );

        if (
                requestCode == MODEL_REQ &&
                resultCode == RESULT_OK &&
                data != null &&
                data.getData() != null
        ) {

            Uri uri =
                    data.getData();

            String name =
                    getFileName(uri);

            if (
                    name == null ||
                    !name.toLowerCase().endsWith(".gguf")
            ) {

                sendJs(
                        "window.onModelError(" +
                                JSONObject.quote(
                                        "Lütfen .gguf uzantılı bir model seç."
                                ) +
                                ");"
                );

                return;
            }

            copyAndLoadModel(uri);
        }
    }

    private void copyAndLoadModel(
            Uri uri
    ) {

        new Thread(() -> {

            try {

                File target =
                        getModelFile();

                File parent =
                        target.getParentFile();

                if (
                        parent != null &&
                        !parent.exists()
                ) {

                    parent.mkdirs();
                }

                sendJs(
                        "window.onModelLoading(" +
                                JSONObject.quote(
                                        "Model telefona aktarılıyor..."
                                ) +
                                ");"
                );

                InputStream input =
                        getContentResolver()
                                .openInputStream(uri);

                if (input == null) {

                    throw new Exception(
                            "Model dosyası açılamadı."
                    );
                }

                OutputStream output =
                        new FileOutputStream(
                                target
                        );

                byte[] buffer =
                        new byte[
                                1024 * 1024
                        ];

                long total =
                        getFileSize(uri);

                long copied = 0;

                int read;

                int lastPercent =
                        -1;

                while (
                        (read =
                                input.read(buffer))
                                != -1
                ) {

                    output.write(
                            buffer,
                            0,
                            read
                    );

                    copied += read;

                    if (total > 0) {

                        int percent =
                                (int) (
                                        copied *
                                                100 /
                                                total
                                );

                        if (
                                percent != lastPercent &&
                                percent % 2 == 0
                        ) {

                            lastPercent =
                                    percent;

                            sendJs(
                                    "window.onModelProgress(" +
                                            percent +
                                            ");"
                            );
                        }
                    }
                }

                output.flush();
                output.close();
                input.close();

                sendJs(
                        "window.onModelLoading(" +
                                JSONObject.quote(
                                        "Model açılıyor..."
                                ) +
                                ");"
                );

                boolean loaded =
                        nativeLoadModel(
                                target.getAbsolutePath()
                        );

                if (loaded) {

                    sendJs(
                            "window.onModelReady(" +
                                    JSONObject.quote(
                                            "GökAI Offline hazır."
                                    ) +
                                    ");"
                    );

                } else {

                    throw new Exception(
                            "GGUF modeli motor tarafından açılamadı."
                    );
                }

            } catch (Exception e) {

                sendJs(
                        "window.onModelError(" +
                                JSONObject.quote(
                                        e.getMessage()
                                ) +
                                ");"
                );
            }

        }).start();
    }

    private File getModelFile() {

        return new File(
                new File(
                        getFilesDir(),
                        "models"
                ),
                "gokai-model.gguf"
        );
    }

    private String getFileName(
            Uri uri
    ) {

        String result =
                "model.gguf";

        Cursor cursor =
                null;

        try {

            cursor =
                    getContentResolver()
                            .query(
                                    uri,
                                    null,
                                    null,
                                    null,
                                    null
                            );

            if (
                    cursor != null &&
                    cursor.moveToFirst()
            ) {

                int index =
                        cursor.getColumnIndex(
                                OpenableColumns
                                        .DISPLAY_NAME
                        );

                if (index >= 0) {

                    result =
                            cursor.getString(
                                    index
                            );
                }
            }

        } catch (Exception ignored) {

        } finally {

            if (cursor != null) {
                cursor.close();
            }
        }

        return result;
    }

    private long getFileSize(
            Uri uri
    ) {

        Cursor cursor =
                null;

        try {

            cursor =
                    getContentResolver()
                            .query(
                                    uri,
                                    null,
                                    null,
                                    null,
                                    null
                            );

            if (
                    cursor != null &&
                    cursor.moveToFirst()
            ) {

                int index =
                        cursor.getColumnIndex(
                                OpenableColumns.SIZE
                        );

                if (index >= 0) {

                    return cursor.getLong(
                            index
                    );
                }
            }

        } catch (Exception ignored) {

        } finally {

            if (cursor != null) {
                cursor.close();
            }
        }

        return -1;
    }

    private void sendJs(
            String javascript
    ) {

        runOnUiThread(() -> {

            if (webView != null) {

                webView.evaluateJavascript(
                        javascript,
                        null
                );
            }
        });
    }

    @Override
    protected void onDestroy() {

        nativeUnloadModel();

        if (webView != null) {
            webView.destroy();
        }

        super.onDestroy();
    }
}
