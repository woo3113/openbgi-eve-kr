package net.openbgi.eve;

import android.os.Bundle;
import android.util.Log;
import android.view.Gravity;
import android.widget.TextView;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/**
 * The game data (the archives and scripts, gigabytes) ships inside the APK
 * under assets/EVE.  On the first launch it is copied to the app's external
 * files directory ("EVE burst error A"), where the engine looks for it
 * (system.arc in the current directory); later launches go straight to SDL.
 */
public class MainActivity extends org.libsdl.app.SDLActivity {
    private static final String TAG = "OpenBGI";
    private static final String ASSET_DIR = "EVE";
    private static final String GAME_DIR = "EVE burst error A";
    private Bundle pendingState;

    private File gameRoot() {
        return new File(getExternalFilesDir(null), GAME_DIR);
    }

    private boolean dataReady() {
        return new File(gameRoot(), "system.arc").exists();
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        Log.i(TAG, "onCreate, filesDir=" + getExternalFilesDir(null) + " ready=" + dataReady());
        if (dataReady()) {
            super.onCreate(savedInstanceState);
            Log.i(TAG, "super.onCreate returned (SDL started)");
            return;
        }
        pendingState = savedInstanceState;
        TextView tv = new TextView(this);
        tv.setText("게임 데이터 설치 중입니다...\n첫 실행은 몇 분 걸릴 수 있습니다.");
        tv.setGravity(Gravity.CENTER);
        tv.setTextSize(20);
        setContentView(tv);
        new Thread(() -> {
            try {
                File root = gameRoot();
                root.mkdirs();
                copyAssetDir(ASSET_DIR, root);
                Log.i(TAG, "game data installed to " + root);
            } catch (IOException e) {
                Log.e(TAG, "game data install failed", e);
            }
            runOnUiThread(() -> startSDL(pendingState));
        }).start();
    }

    // called from the worker once the data is in place
    private void startSDL(Bundle savedState) {
        super.onCreate(savedState);
    }

    private void copyAssetDir(String assetPath, File dest) throws IOException {
        String[] children = getAssets().list(assetPath);
        if (children == null || children.length == 0) {
            copyAssetFile(assetPath, dest); // an asset file (an empty dir would throw)
            return;
        }
        dest.mkdirs();
        for (String name : children) {
            copyAssetDir(assetPath + "/" + name, new File(dest, name));
        }
    }

    private void copyAssetFile(String assetPath, File dest) throws IOException {
        InputStream in = null;
        OutputStream out = null;
        try {
            in = getAssets().open(assetPath);
        } catch (IOException e) {
            return; // an empty directory: nothing to copy
        }
        try {
            dest.getParentFile().mkdirs();
            out = new FileOutputStream(dest);
            byte[] buf = new byte[1 << 16];
            int n;
            while ((n = in.read(buf)) > 0) {
                out.write(buf, 0, n);
            }
        } finally {
            try { out.close(); } catch (Exception ignored) { }
            try { in.close(); } catch (Exception ignored) { }
        }
    }
}
