package org.nightjar.re4;

import android.content.Intent;
import android.content.SharedPreferences;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.system.Os;
import android.util.Log;

import org.libsdl.app.SDLActivity;

import java.io.ByteArrayOutputStream;
import java.io.File; // dossiers du jeu et de la configuration
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;

/**
 * Point d'entree Android. SDLActivity charge libSDL2.so puis libmain.so, et appelle SDL_main -
 * c'est-a-dire le main() du runtime - avec les arguments rendus par getArguments().
 *
 * Le disque et la configuration vivent dans le repertoire propre a l'application,
 * /sdcard/Android/data/org.nightjar.re4/files/, que adb peut remplir sans permission :
 *   files/game/         le disque extrait (files/ et sys/), comme sous Windows
 *   files/config.toml   facultatif
 *
 * Les extras de l'intention dont le nom commence par NJ_ deviennent des variables
 * d'environnement, pour piloter les memes diagnostics que sous Windows :
 *   adb shell am start -n org.nightjar.re4/.RE4Activity --es NJ_AUTOSTART 1
 */
public class RE4Activity extends SDLActivity {
    private static final String TAG = "RE4";
    /** Faux dans un build de diffusion (build_apk.sh, RELEASE=1) : aucune variable NJ_ recue par
     *  l'intention - toute application du telephone peut lancer cette activite, exportee -, et rien
     *  dans logcat. */
    static final boolean DEV = true;
    private static final int PICK_DISC = 4401;
    private static final String PREFS = "launcher";
    private static final String KEY_DISC = "disc_uri";
    private static final String KEY_DISC2 = "disc2_uri";
    // disque demande au selecteur en cours (1 ou 2)
    private static volatile int sPickDisc = 1;

    private static RE4Activity sInstance;
    // -1 : choix en cours, -2 : annule, >= 0 : descripteur du disque choisi
    private static volatile int sPicked = -1;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        Bundle extras = getIntent() != null ? getIntent().getExtras() : null;
        if (DEV && extras != null) {
            for (String key : extras.keySet()) {
                if (!key.startsWith("NJ_")) continue;
                String value = String.valueOf(extras.get(key));
                try {
                    Os.setenv(key, value, true);
                    Log.i(TAG, "env " + key + "=" + value);
                } catch (Exception e) {
                    if (DEV) Log.w(TAG, "setenv " + key + " : " + e);
                }
            }
        }
        try {
            Os.setenv("NJ_FILES_DIR", getExternalFilesDir(null).getAbsolutePath(), true);
        } catch (Exception e) {
            if (DEV) Log.w(TAG, "setenv NJ_FILES_DIR : " + e);
        }
        // Icones tactiles : dossier interne ui_buttons/ (14/10). Dans textures_hd/, elles passaient
        // pour un mod de textures (onglet Textures, rapport) : l'ancien dossier est retire.
        installAssets("textures_hd/buttons/mobile", "ui_buttons/mobile");
        removeOldButtons();
        sInstance = this;
        super.onCreate(savedInstanceState);
        // Mesures sans ecran (NJ_HEADLESS, 30/09) : le processeur reste eveille ecran eteint ; cote
        // natif, SDL ne bloque plus la boucle quand l'activite passe en arriere-plan (main.cpp).
        if (DEV && extras != null && extras.containsKey("NJ_HEADLESS")) {
            android.os.PowerManager pm = (android.os.PowerManager) getSystemService(POWER_SERVICE);
            sWake = pm.newWakeLock(android.os.PowerManager.PARTIAL_WAKE_LOCK, "re4:headless");
            sWake.acquire();
        }
    }
    private static android.os.PowerManager.WakeLock sWake;

    // ---- appele par le lanceur natif (nWiiRuntime/src/launcher/host.cpp) ----

    /** GET HTTPS, certificat verifie par le systeme. Rend le code HTTP, un saut de ligne, puis le corps ; null si le reseau ne repond pas. */
    public static String httpsGet(String url) {
        HttpURLConnection c = null;
        try {
            c = (HttpURLConnection) new URL(url).openConnection();
            c.setConnectTimeout(8000);
            c.setReadTimeout(8000);
            c.setRequestProperty("User-Agent", "Nightjar");
            c.setRequestProperty("Accept", "application/vnd.github+json");
            int code = c.getResponseCode();
            InputStream in = code < 400 ? c.getInputStream() : c.getErrorStream();
            ByteArrayOutputStream out = new ByteArrayOutputStream();
            if (in != null) {
                byte[] buf = new byte[8192];
                int n;
                while ((n = in.read(buf)) > 0 && out.size() < (1 << 20)) out.write(buf, 0, n);
                in.close();
            }
            return code + "\n" + out.toString("UTF-8");
        } catch (Exception e) {
            if (DEV) Log.w(TAG, "httpsGet : " + e);
            return null;
        } finally {
            if (c != null) c.disconnect();
        }
    }

    /**
     * Requete HTTPS quelconque (connexion Discord). headers : lignes "Nom: valeur" separees par
     * CRLF ; body envoye tel quel s'il n'est pas vide. Rend "<code>\n<corps>", null sans reseau.
     */
    public static String httpsRequest(String method, String url, String headers, String body) {
        HttpURLConnection c = null;
        try {
            c = (HttpURLConnection) new URL(url).openConnection();
            c.setRequestMethod(method);
            c.setConnectTimeout(8000);
            c.setReadTimeout(8000);
            c.setRequestProperty("User-Agent", "Nightjar");
            for (String line : headers.split("\r\n")) {
                int colon = line.indexOf(':');
                if (colon > 0) c.setRequestProperty(line.substring(0, colon).trim(), line.substring(colon + 1).trim());
            }
            if (!body.isEmpty()) {
                byte[] data = body.getBytes("UTF-8");
                c.setDoOutput(true);
                c.setFixedLengthStreamingMode(data.length);
                c.getOutputStream().write(data);
            }
            int code = c.getResponseCode();
            InputStream in = code < 400 ? c.getInputStream() : c.getErrorStream();
            ByteArrayOutputStream out = new ByteArrayOutputStream();
            if (in != null) {
                byte[] buf = new byte[8192];
                int n;
                while ((n = in.read(buf)) > 0 && out.size() < (1 << 20)) out.write(buf, 0, n);
                in.close();
            }
            return code + "\n" + out.toString("UTF-8");
        } catch (Exception e) {
            if (DEV) Log.w(TAG, "httpsRequest : " + e);
            return null;
        } finally {
            if (c != null) c.disconnect();
        }
    }

    /** Ouvre le selecteur de fichiers du systeme (Storage Access Framework). */
    public static void pickDisc(int disc) {
        sPicked = -1;
        sPickDisc = disc;
        final RE4Activity a = sInstance;
        if (a == null) { sPicked = -2; return; }
        a.runOnUiThread(() -> {
            Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            i.addCategory(Intent.CATEGORY_OPENABLE);
            i.setType("*/*");
            try {
                a.startActivityForResult(i, PICK_DISC);
            } catch (Exception e) {
                if (DEV) Log.w(TAG, "pickDisc : " + e);
                sPicked = -2;
            }
        });
    }

    public static int pollPickedDisc() { return sPicked; }

    // ---- connexion Discord (13/10) : appli Discord d'abord, navigateur en repli ----
    private static volatile String sDiscordAuth = null;

    /** Ouvre la demande d'autorisation. Avec l'appli Discord installee, appUrl
     *  (discord://action/oauth2/authorize?...) ouvre son ecran « Authorize » sur le compte deja connecte ;
     *  sinon (ou preferApp faux), webUrl dans un onglet du navigateur. Rend faux si rien ne s'ouvre. */
    public static boolean openDiscordAuth(String appUrl, String webUrl, boolean preferApp) {
        sDiscordAuth = null;
        final RE4Activity a = sInstance;
        if (a == null) return false;
        if (preferApp) {
            try {
                Intent i = new Intent(Intent.ACTION_VIEW, Uri.parse(appUrl));
                i.setPackage("com.discord");
                i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                a.startActivity(i);
                return true;
            } catch (Exception e) {
                if (DEV) Log.i(TAG, "appli Discord absente : " + e);
            }
        }
        try {
            Intent w = new Intent(Intent.ACTION_VIEW, Uri.parse(webUrl));
            // onglet personnalise (Custom Tabs) quand le navigateur le gere : session de discord.com partagee
            Bundle b = new Bundle();
            b.putBinder("android.support.customtabs.extra.SESSION", null);
            w.putExtras(b);
            w.putExtra("android.support.customtabs.extra.TOOLBAR_COLOR", 0xff0b0b0d);
            w.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            a.startActivity(w);
            return true;
        } catch (Exception e) {
            if (DEV) Log.w(TAG, "openDiscordAuth : " + e);
            return false;
        }
    }

    /** Adresse de retour recue par AuthCallback (null tant que rien n'est arrive). */
    public static String pollDiscordAuth() { return sDiscordAuth; }

    static void deliverDiscordAuth(String uri) { sDiscordAuth = uri; }

    // ---- sauvegardes et rapport (13/10) : selecteur de fichiers du systeme, menu Partager ----
    private static final int PICK_IMPORT = 4402;
    private static final int CREATE_EXPORT = 4403;
    // import : null en cours, "" annule, sinon chemin de la copie locale
    private static volatile String sImport = null;
    // export : 0 en cours, 1 fait, -1 annule ou echec
    private static volatile int sExport = 0;
    private static String sExportSrc;

    /** Relance l'application dans un processus neuf (appel natif, apres la preparation du jeu), puis tue celui-ci. */
    public static void restartApp() {
        final RE4Activity a = sInstance;
        if (a == null) return;
        a.runOnUiThread(() -> {
            try {
                Intent i = a.getPackageManager().getLaunchIntentForPackage(a.getPackageName());
                if (i == null) return;
                i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK);
                a.startActivity(i);
                new android.os.Handler(android.os.Looper.getMainLooper()).postDelayed(
                        () -> android.os.Process.killProcess(android.os.Process.myPid()), 400);
            } catch (Exception e) {
                // la relance a echoue : le jeu continue dans ce processus
            }
        });
    }

    /** Choisir un fichier a importer (.gci, .raw) ; copie dans le cache, lue par le natif. */
    public static void pickImportFile() {
        sImport = null;
        final RE4Activity a = sInstance;
        if (a == null) { sImport = ""; return; }
        a.runOnUiThread(() -> {
            Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            i.addCategory(Intent.CATEGORY_OPENABLE);
            i.setType("*/*");
            try {
                a.startActivityForResult(i, PICK_IMPORT);
            } catch (Exception e) {
                sImport = "";
            }
        });
    }

    public static String pollImportFile() { return sImport; }

    /** Enregistrer une copie de srcPath ou le joueur le choisit (Telechargements, Drive...). */
    public static void exportFile(String srcPath, String name) {
        sExport = 0;
        sExportSrc = srcPath;
        final RE4Activity a = sInstance;
        if (a == null) { sExport = -1; return; }
        a.runOnUiThread(() -> {
            Intent i = new Intent(Intent.ACTION_CREATE_DOCUMENT);
            i.addCategory(Intent.CATEGORY_OPENABLE);
            i.setType("application/octet-stream");
            i.putExtra(Intent.EXTRA_TITLE, name);
            try {
                a.startActivityForResult(i, CREATE_EXPORT);
            } catch (Exception e) {
                sExport = -1;
            }
        });
    }

    public static int pollExport() { return sExport; }

    /** Menu Partager : le texte du rapport, et les fichiers joints (copies servies par ReportProvider). */
    public static boolean shareReport(String text, String[] files) {
        final RE4Activity a = sInstance;
        if (a == null) return false;
        try {
            File dir = new File(a.getCacheDir(), "report");
            if (!dir.isDirectory() && !dir.mkdirs()) return false;
            File[] old = dir.listFiles();
            if (old != null) for (File f : old) f.delete();
            java.util.ArrayList<Uri> uris = new java.util.ArrayList<>();
            for (String p : files) {
                File src = new File(p);
                if (!src.isFile()) continue;
                File dst = new File(dir, src.getName());
                copy(new java.io.FileInputStream(src), new java.io.FileOutputStream(dst));
                uris.add(Uri.parse("content://" + ReportProvider.AUTHORITY + "/" + dst.getName()));
            }
            Intent s = new Intent(uris.isEmpty() ? Intent.ACTION_SEND : Intent.ACTION_SEND_MULTIPLE);
            s.setType(uris.isEmpty() ? "text/plain" : "*/*");
            s.putExtra(Intent.EXTRA_TEXT, text);
            if (!uris.isEmpty()) {
                s.putParcelableArrayListExtra(Intent.EXTRA_STREAM, uris);
                android.content.ClipData clip = android.content.ClipData.newRawUri("report", uris.get(0));
                for (int i = 1; i < uris.size(); ++i) clip.addItem(new android.content.ClipData.Item(uris.get(i)));
                s.setClipData(clip);
                s.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            }
            a.runOnUiThread(() -> {
                try {
                    a.startActivity(Intent.createChooser(s, "Share the Nightjar report"));
                } catch (Exception e) {
                    if (DEV) Log.w(TAG, "shareReport : " + e);
                }
            });
            return true;
        } catch (Exception e) {
            if (DEV) Log.w(TAG, "shareReport : " + e);
            return false;
        }
    }

    private static void copy(InputStream in, java.io.OutputStream out) throws java.io.IOException {
        try (InputStream i = in; java.io.OutputStream o = out) {
            byte[] buf = new byte[1 << 16];
            for (int n; (n = i.read(buf)) > 0; ) o.write(buf, 0, n);
        }
    }

    /** Le disque choisi a un lancement precedent, rouvert avec l'autorisation conservee. */
    public static int openSavedDisc(int disc) {
        RE4Activity a = sInstance;
        if (a == null) return -1;
        String uri = a.getSharedPreferences(PREFS, MODE_PRIVATE).getString(disc == 2 ? KEY_DISC2 : KEY_DISC, null);
        return uri == null ? -1 : a.openFd(Uri.parse(uri));
    }

    private int openFd(Uri uri) {
        try {
            ParcelFileDescriptor pfd = getContentResolver().openFileDescriptor(uri, "r");
            return pfd == null ? -1 : pfd.detachFd();  // le natif le lit par /proc/self/fd/N
        } catch (Exception e) {
            if (DEV) Log.w(TAG, "openFd : " + e);
            return -1;
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode == PICK_IMPORT) {
            Uri uri = resultCode == RESULT_OK && data != null ? data.getData() : null;
            if (uri == null) { sImport = ""; return; }
            String name = "import.bin";
            try (Cursor c = getContentResolver().query(uri, null, null, null, null)) {
                if (c != null && c.moveToFirst()) {
                    int col = c.getColumnIndex(android.provider.OpenableColumns.DISPLAY_NAME);
                    if (col >= 0 && c.getString(col) != null) name = c.getString(col).replaceAll("[/\\\\]", "_");
                }
            } catch (Exception ignored) {
            }
            final String fname = name;
            new Thread(() -> {
                try {
                    File dir = new File(getCacheDir(), "import");
                    if (!dir.isDirectory()) dir.mkdirs();
                    File dst = new File(dir, fname);
                    copy(getContentResolver().openInputStream(uri), new java.io.FileOutputStream(dst));
                    sImport = dst.getAbsolutePath();
                } catch (Exception e) {
                    sImport = "";
                }
            }).start();
            return;
        }
        if (requestCode == CREATE_EXPORT) {
            Uri uri = resultCode == RESULT_OK && data != null ? data.getData() : null;
            if (uri == null || sExportSrc == null) { sExport = -1; return; }
            new Thread(() -> {
                try {
                    copy(new java.io.FileInputStream(sExportSrc), getContentResolver().openOutputStream(uri, "w"));
                    sExport = 1;
                } catch (Exception e) {
                    sExport = -1;
                }
            }).start();
            return;
        }
        if (requestCode != PICK_DISC) {
            super.onActivityResult(requestCode, resultCode, data);
            return;
        }
        Uri uri = resultCode == RESULT_OK && data != null ? data.getData() : null;
        if (uri == null) { sPicked = -2; return; }
        try {
            getContentResolver().takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
        } catch (Exception e) {
            if (DEV) Log.w(TAG, "takePersistableUriPermission : " + e);
        }
        SharedPreferences.Editor ed = getSharedPreferences(PREFS, MODE_PRIVATE).edit();
        ed.putString(sPickDisc == 2 ? KEY_DISC2 : KEY_DISC, uri.toString()).apply();
        int fd = openFd(uri);
        sPicked = fd >= 0 ? fd : -2;
    }

    /**
     * Copie un dossier des assets de l'APK dans le dossier de l'application (le moteur y lit
     * textures_hd/ en chemin relatif). Seulement les fichiers absents ou de taille differente.
     */
    private void installAssets(String dir, String dest) {
        File root = getExternalFilesDir(null);
        try {
            String[] names = getAssets().list(dir);
            if (names == null) return;
            File out = new File(root, dest);
            if (!out.isDirectory() && !out.mkdirs()) return;
            byte[] buf = new byte[1 << 16];
            for (String name : names) {
                File dst = new File(out, name);
                try (InputStream in = getAssets().open(dir + "/" + name)) {
                    ByteArrayOutputStream data = new ByteArrayOutputStream();
                    for (int n; (n = in.read(buf)) > 0; ) data.write(buf, 0, n);
                    if (dst.length() == data.size()) continue;
                    try (java.io.FileOutputStream os = new java.io.FileOutputStream(dst)) {
                        data.writeTo(os);
                    }
                }
            }
        } catch (Exception e) {
            if (DEV) Log.w(TAG, "assets " + dir + " : " + e);
        }
    }

    /** Ancien emplacement des icones (textures_hd/buttons/mobile) ; dossiers parents retires s'ils sont vides. */
    private void removeOldButtons() {
        File root = getExternalFilesDir(null);
        File old = new File(root, "textures_hd/buttons/mobile");
        File[] files = old.listFiles();
        if (files == null) return;
        for (File f : files) f.delete();
        old.delete();
        new File(root, "textures_hd/buttons").delete();
        new File(root, "textures_hd").delete();
    }

    /** SDL est liee statiquement dans libmain.so : une seule bibliotheque a charger. */
    @Override
    protected String[] getLibraries() {
        return new String[] { "main" };
    }

    @Override
    protected String[] getArguments() {
        File root = getExternalFilesDir(null);
        File game = new File(root, "game");
        File config = new File(root, "config.toml");
        if (DEV) Log.i(TAG, "disque : " + game + (game.isDirectory() ? "" : " (ABSENT)"));
        return new String[] { game.getAbsolutePath(), config.getAbsolutePath() };
    }
}
