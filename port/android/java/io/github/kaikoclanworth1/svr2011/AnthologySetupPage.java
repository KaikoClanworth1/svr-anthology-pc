// SVR Anthology's Setup tab on the phone: a checklist that says what is missing and how to fix it, step by
// step, with Play at the end. What the game needs (all the player's own copies):
//   1. Yumboy11's installed "WWE SmackDown vs Raw Anthology" folder (made by his installer on a PC, ~25 GB),
//   2. the original program (default.xex) from the player's own SvR 2011 ISO, in Original/,
//   3. (optional) his saves and DLC.
// Ways to get them here: the PC Anthology launcher's "Install to phone" (USB debugging), or copying the folders
// over a USB cable into games/SVR Anthology (the app finds them there, in Download and in a few other places).
// Nothing of the Anthology is in this app; the completeness check only counts files and bytes.

package io.github.kaikoclanworth1.svr2011;

import android.app.ActivityManager;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Typeface;
import android.media.MediaScannerConnection;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.os.ParcelFileDescriptor;
import android.provider.DocumentsContract;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.ByteBuffer;
import java.security.MessageDigest;

final class AnthologySetupPage {
    // The original disc's default.xex (what the recomp is made from), as the PC launcher checks it.
    static final long kXexSize = 15020032;
    static final String kXexSha256 = "6aead4cb29caa11f245d37b36277b5ab57772cffa2cf8c459d3295a3b2d3ebc1";
    static final String kSavesFolder = "WWE SmackDown vs Raw Anthology Saves and DLC";
    // Yumboy11's SVR Anthology 1.0 install, without @Alternate_Files: how many files and bytes (a count only, to
    // tell a copy that stopped part way).
    static final int kHisFiles = 907;
    static final long kHisBytes = 27247164788L;
    static final String kHowTo = "How to copy SVR Anthology here.txt";

    static final int kGood = 0xFF34C759, kWarn = 0xFFF0A028;

    private final LauncherActivity a_;
    private final LinearLayout view_;
    private final TextView summary_;
    private final Step data_, xex_, saves_, phone_;
    private final Button play_, useIso_;
    private final ProgressBar progress_;
    private final Button stop_;
    private File foundIso_;
    private boolean busy_;
    private volatile boolean cancel_;
    private String dataCheck_ = "";   // (the completeness check's result, for the setup report)

    // A checklist step: its mark, title, state line and help, then its buttons.
    private final class Step {
        final TextView mark, state;
        final LinearLayout card;

        Step(String title, String help) {
            card = new LinearLayout(a_);
            card.setOrientation(LinearLayout.VERTICAL);
            card.setBackground(a_.rounded(LauncherActivity.kCard, 12));
            card.setPadding(a_.dp(14), a_.dp(12), a_.dp(14), a_.dp(12));
            LinearLayout head = new LinearLayout(a_);
            head.setGravity(Gravity.CENTER_VERTICAL);
            mark = a_.text("", 20, LauncherActivity.kDim);
            mark.setTypeface(Typeface.DEFAULT_BOLD);
            mark.setGravity(Gravity.CENTER);
            head.addView(mark, new LinearLayout.LayoutParams(a_.dp(34), a_.dp(34)));
            TextView t = a_.text(title, 17, LauncherActivity.kText);
            t.setTypeface(Typeface.DEFAULT_BOLD);
            t.setPadding(a_.dp(10), 0, 0, 0);
            head.addView(t, new LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1));
            card.addView(head);
            state = a_.text("", 15, LauncherActivity.kText);
            state.setPadding(a_.dp(44), a_.dp(6), 0, a_.dp(2));
            card.addView(state);
            if (help != null) {
                TextView h = a_.text(help, 13, LauncherActivity.kDim);
                h.setPadding(a_.dp(44), a_.dp(6), 0, a_.dp(4));
                card.addView(h);
            }
            view_.addView(card, a_.fullWidth(12));
        }

        void set(int kind, String text) {   // kind: 1 done, 0 to do, -1 needs fixing
            mark.setText(kind > 0 ? "✓" : kind < 0 ? "!" : "○");
            mark.setTextColor(kind > 0 ? kGood : kind < 0 ? kWarn : LauncherActivity.kDim);
            mark.setBackground(a_.rounded(kind > 0 ? 0x2234C759 : kind < 0 ? 0x22F0A028 : 0x22FFFFFF, 17));
            state.setText(text);
            state.setTextColor(kind < 0 ? kWarn : LauncherActivity.kText);
        }

        void buttons(Button... b) {
            LinearLayout row = b.length == 2 ? a_.pair(b[0], b[1]) : null;
            LinearLayout.LayoutParams lp = a_.fullWidth(8);
            lp.leftMargin = a_.dp(44);
            if (row != null) card.addView(row, lp);
            else for (Button x : b) card.addView(x, lp);
        }
    }

    AnthologySetupPage(LauncherActivity a) {
        a_ = a;
        view_ = a.column();

        TextView intro = a.text("Get SVR Anthology running", 20, LauncherActivity.kText);
        intro.setTypeface(Typeface.DEFAULT_BOLD);
        view_.addView(intro);
        summary_ = a.text("", 14, LauncherActivity.kDim);
        summary_.setPadding(0, a.dp(4), 0, 0);
        view_.addView(summary_);

        data_ = new Step("1.  Yumboy11's Anthology (about 25 GB)",
            "On your PC, Yumboy11's installer made a folder named \"" + Flavor.kAnthologyData + "\". Copy that "
            + "whole folder to this phone: connect a USB cable, choose File transfer on the phone, and drop it "
            + "into games › " + InstallActivity.kFolderName + " (or Download). Then press Find it.");
        Button find = a.button("Find it", LauncherActivity.kRed);
        find.setOnClickListener(v -> findData());
        Button pickData = a.button("Choose the folder…", LauncherActivity.kBackground);
        pickData.setOnClickListener(v -> pickFolder(this::useData));
        data_.buttons(find, pickData);

        xex_ = new Step("2.  The original program from your SvR 2011 ISO",
            "The game runs the original SvR 2011 program with Yumboy11's files. Copy your own SvR 2011 Xbox 360 "
            + "ISO to the phone and choose it, or copy the \"Original\" folder from the SvR Anthology Launcher's "
            + "folder on your PC (it appears there the first time you press Play on the PC) into games › "
            + InstallActivity.kFolderName + ". Only default.xex (15 MB) is kept: you can delete the ISO afterwards.");
        Button pickIso = a.button("Choose the ISO…", LauncherActivity.kRed);
        pickIso.setOnClickListener(v -> pickIso());
        useIso_ = a.button("Use the ISO found", LauncherActivity.kBackground);
        useIso_.setOnClickListener(v -> { if (foundIso_ != null) useIsoFile(foundIso_); });
        xex_.buttons(pickIso, useIso_);

        saves_ = new Step("3.  Yumboy11's saves and DLC (optional)",
            "His \"" + kSavesFolder + "\" folder (beside the Anthology on your PC) unlocks his roster and arenas. "
            + "Copy it to games › " + InstallActivity.kFolderName + " or Download, then press the button.");
        Button pickSaves = a.button("Install his saves and DLC", LauncherActivity.kBackground);
        pickSaves.setOnClickListener(v -> installSaves());
        saves_.buttons(pickSaves);

        phone_ = new Step("4.  This phone", null);

        progress_ = new ProgressBar(a, null, android.R.attr.progressBarStyleHorizontal);
        progress_.setMax(1000);
        progress_.setVisibility(View.GONE);
        progress_.setProgressTintList(android.content.res.ColorStateList.valueOf(LauncherActivity.kRed));
        view_.addView(progress_, a.fullWidth(12));
        stop_ = a.button("Stop", LauncherActivity.kBackground);
        stop_.setOnClickListener(v -> cancel_ = true);
        stop_.setVisibility(View.GONE);
        view_.addView(stop_, a.fullWidth(6));

        play_ = a.button("Play", LauncherActivity.kRed);
        play_.setTextSize(22);
        play_.setTypeface(Typeface.DEFAULT_BOLD);
        play_.setMinHeight(a.dp(72));
        play_.setOnClickListener(v -> a_.play());
        view_.addView(play_, a.fullWidth(18));

        LinearLayout pc = a.card(view_, "Or let your PC do it");
        TextView t = a.text("The SvR Anthology Launcher on your PC (set up with your ISO and Yumboy11's installer) can "
            + "copy everything here for you: on its Phone page, follow the steps (USB debugging on, cable in, Allow), "
            + "then click Install to phone.", 14, LauncherActivity.kText);
        t.setPadding(0, a.dp(10), 0, a.dp(10));
        pc.addView(t);

        LinearLayout help = a.card(view_, "Still stuck?");
        TextView ht = a.text("Copy this setup check and paste it in Report a problem (Play tab) or on Discord: it says "
            + "what the app sees, without any of your files.", 14, LauncherActivity.kText);
        ht.setPadding(0, a.dp(10), 0, a.dp(6));
        help.addView(ht);
        Button copy = a.button("Copy the setup check", LauncherActivity.kBackground);
        copy.setOnClickListener(v -> copyCheck());
        help.addView(copy, a.fullWidth(4));
        Button report = a.button("Report a problem…", LauncherActivity.kBackground);
        report.setOnClickListener(v -> ReportForm.show(a_));
        LinearLayout.LayoutParams lp = a.fullWidth(8);
        lp.bottomMargin = a.dp(10);
        help.addView(report, lp);

        prepareFolder();
    }

    View view() { return view_; }

    static File game() { return InstallActivity.gameFolder(); }

    // games/SVR Anthology made at once, with a note in it: an empty folder an app makes doesn't show on a PC over
    // USB until the phone indexes a file in it.
    private void prepareFolder() {
        File g = game();
        g.mkdirs();
        File note = new File(g, kHowTo);
        if (note.isFile()) return;
        String s = "SVR Anthology by Yumboy11 - copy these here from your PC:\r\n\r\n"
            + "1. The \"" + Flavor.kAnthologyData + "\" folder Yumboy11's installer made.\r\n"
            + "2. The \"Original\" folder from the SvR Anthology Launcher's folder (or your SvR 2011 ISO).\r\n"
            + "3. (optional) The \"" + kSavesFolder + "\" folder.\r\n\r\n"
            + "Then open the SVR Anthology app: its Setup tab checks them.\r\n";
        try (OutputStream o = new FileOutputStream(note)) {
            o.write(s.getBytes(java.nio.charset.StandardCharsets.UTF_8));
        } catch (IOException e) {
            return;
        }
        MediaScannerConnection.scanFile(a_, new String[] {note.getPath()}, null, null);
    }

    // ── the state ──

    static boolean xexHere() { return new File(game(), "Original/default.xex").length() == kXexSize; }
    static boolean dataHere() { return new File(new File(game(), Flavor.kAnthologyData), "default.xex").isFile(); }

    void refresh() {
        if (busy_) return;
        boolean data = dataHere(), xex = xexHere();
        // 1. his folder (counted in the background: 900 files)
        if (data) {
            data_.set(1, "In games/" + InstallActivity.kFolderName + " - checking it is complete…");
            final int ticket = ++countTicket_;
            a_.background(this::countData, () -> { if (ticket == countTicket_) showCount(); });
        } else {
            ++countTicket_;   // (a count still running answers for a folder that has gone)
            dataCheck_ = "";
            File elsewhere = findDataFolder();
            data_.set(0, elsewhere != null ? "Found in " + shortPath(elsewhere) + " - press Find it to move it into place."
                : "Not on this phone yet.");
        }
        // 2. the original program
        if (xex) {
            xex_.set(1, "The original program is here.");
            useIso_.setVisibility(View.GONE);
        } else {
            File loose = findLooseXex();
            foundIso_ = loose != null ? null : findIso();
            if (loose != null) {
                xex_.set(0, "Found default.xex in " + shortPath(loose.getParentFile()) + " - press Use it.");
                foundIso_ = loose;
                useIso_.setText("Use it");
            } else if (foundIso_ != null) {
                xex_.set(0, "Found " + foundIso_.getName() + " in " + shortPath(foundIso_.getParentFile()) + ".");
                useIso_.setText("Use " + foundIso_.getName());
            } else {
                xex_.set(0, "Not done yet.");
            }
            useIso_.setVisibility(foundIso_ != null ? View.VISIBLE : View.GONE);
        }
        // 3. saves
        String[] s = new File(game(), "Saves").list();
        saves_.set(s != null && s.length > 0 ? 1 : 0, s != null && s.length > 0 ? "His saves are installed."
            : "Not installed (the game also starts without them).");
        // 4. the phone
        phoneCheck();
        // the summary and Play
        int done = (data ? 1 : 0) + (xex ? 1 : 0);
        summary_.setText(done == 2 ? "Ready. Press Play at the bottom (or on the Play tab)."
            : "Steps 1 and 2 are needed (" + done + " of 2 done). Everything comes from your own copies: "
              + "nothing of SVR Anthology is in this app.");
        play_.setEnabled(done == 2);
        play_.setAlpha(done == 2 ? 1f : 0.4f);
    }

    private int countTicket_;
    private int[] counted_;
    private long countedBytes_;

    private void countData() {
        int[] n = {0, 0};   // files, leftover .part files
        long[] b = {0};
        count(new File(game(), Flavor.kAnthologyData), n, b, true);
        counted_ = n;
        countedBytes_ = b[0];
    }

    private static void count(File dir, int[] n, long[] b, boolean top) {
        File[] l = dir.listFiles();
        if (l == null) return;
        for (File f : l) {
            if (f.isDirectory()) {
                if (!(top && f.getName().equals("@Alternate_Files"))) count(f, n, b, false);
            } else if (f.getName().endsWith(".part")) {
                n[1]++;
            } else {
                n[0]++;
                b[0] += f.length();
            }
        }
    }

    private void showCount() {
        if (counted_ == null || busy_ || !dataHere()) return;
        int files = counted_[0];
        dataCheck_ = files + " files, " + countedBytes_ + " bytes, " + counted_[1] + " unfinished";
        if (files < kHisFiles || countedBytes_ < kHisBytes || counted_[1] > 0) {
            data_.set(-1, "Not complete: " + files + " of " + kHisFiles + " files, " + FileOps.human(countedBytes_)
                + " of " + FileOps.human(kHisBytes) + ". The copy stopped part way: copy the folder again (choose "
                + "Skip for files already there), then come back here.");
            summary_.setText("Step 1 needs fixing first: the Anthology on this phone is missing files, so the game "
                + "can stop with a disc read error.");
        } else {
            data_.set(1, "In games/" + InstallActivity.kFolderName + " and complete (" + files + " files).");
        }
    }

    private void phoneCheck() {
        PackageManager pm = a_.getPackageManager();
        boolean vulkan = Build.VERSION.SDK_INT >= 24
            && pm.hasSystemFeature(PackageManager.FEATURE_VULKAN_HARDWARE_VERSION, 0x401000);   // (Vulkan 1.1)
        ActivityManager.MemoryInfo mi = new ActivityManager.MemoryInfo();
        ((ActivityManager) a_.getSystemService(android.content.Context.ACTIVITY_SERVICE)).getMemoryInfo(mi);
        long ramGb = (mi.totalMem + (512L << 20)) >> 30;
        long free = InstallPage.free();
        StringBuilder s = new StringBuilder();
        s.append(vulkan ? "Vulkan 1.1: yes" : "Vulkan 1.1: NO - the game can't run on this phone");
        s.append(" · Memory ").append(ramGb).append(" GB");
        if (ramGb < 6) s.append(" (6 GB or more is best: close other apps)");
        s.append(" · Free space ").append(FileOps.human(free));
        if (!dataHere() && free < kHisBytes + (1L << 30)) s.append(" (the Anthology needs about 26 GB)");
        String gpu = GpuInfo.known();
        if (!gpu.isEmpty()) s.append(" · ").append(gpu);
        phone_.set(vulkan ? (ramGb < 6 ? -1 : 1) : -1, s.toString());
    }

    private static String shortPath(File f) {
        String root = Environment.getExternalStorageDirectory().getPath();
        String p = f.getPath();
        return p.startsWith(root) ? (p.length() == root.length() ? "the phone's storage" : p.substring(root.length() + 1)) : p;
    }

    // ── shared ──

    private boolean start() {
        if (busy_) {
            a_.status("Wait for the step that is running (or Stop it).");
            return false;
        }
        if (!a_.gameClosed()) return false;
        busy_ = true;
        cancel_ = false;
        progress_.setProgress(0);
        progress_.setVisibility(View.VISIBLE);
        stop_.setVisibility(View.VISIBLE);
        return true;
    }

    private void done(String message) {
        busy_ = false;
        progress_.setVisibility(View.GONE);
        stop_.setVisibility(View.GONE);
        a_.status(message);
        refresh();
    }

    private void report(long done, long total, String what) {
        final int p = total > 0 ? (int) (done * 1000 / total) : 0;
        a_.runOnUiThread(() -> {
            progress_.setProgress(p);
            a_.status(what + " " + p / 10 + "% · " + FileOps.human(done) + " of " + FileOps.human(total));
        });
    }

    // The places a player copies things to.
    private static File[] places() {
        File root = Environment.getExternalStorageDirectory();
        String[] p = {"games/" + InstallActivity.kFolderName, "Download", "", "games", "Documents",
            "Download/SVR Anthology 1.0 Installer", "SVR Anthology 1.0 Installer", "games/SVR Anthology 1.0 Installer",
            "Download/" + InstallActivity.kFolderName};
        File[] f = new File[p.length];
        for (int i = 0; i < p.length; i++) f[i] = p[i].isEmpty() ? root : new File(root, p[i]);
        return f;
    }

    // ── 2. the original program ──

    // A default.xex of the right size copied by itself (the PC launcher's Original folder, or the file).
    private static File findLooseXex() {
        for (File d : places()) {
            for (File x : new File[] {new File(d, "Original/default.xex"), new File(d, "default.xex")}) {
                if (x.length() == kXexSize && !x.getPath().equals(new File(game(), "Original/default.xex").getPath()))
                    return x;
            }
        }
        return null;
    }

    private static File findIso() {
        for (File d : places()) {
            File[] l = d.listFiles();
            if (l == null) continue;
            for (File f : l)
                if (f.isFile() && f.getName().toLowerCase(java.util.Locale.ROOT).endsWith(".iso") && f.length() > (1L << 30))
                    return f;
        }
        return null;
    }

    private void useIsoFile(File f) {
        if (f.getName().equals("default.xex")) {
            if (!start()) return;
            final String[] r = new String[1];
            a_.background(() -> r[0] = copyLooseXex(f), () -> done(r[0]));
            return;
        }
        if (!start()) return;
        final String[] r = new String[1];
        a_.background(() -> {
            try (FileInputStream fin = new FileInputStream(f)) {
                r[0] = xexFrom(fin);
            } catch (IOException e) {
                r[0] = "The ISO can't be read: " + e.getMessage();
            }
        }, () -> done(r[0]));
    }

    private String copyLooseXex(File f) {
        try {
            byte[] buf = new byte[(int) kXexSize];
            try (InputStream in = new FileInputStream(f)) {
                int at = 0, n;
                while (at < buf.length && (n = in.read(buf, at, buf.length - at)) > 0) at += n;
                if (at != buf.length) return "That default.xex is cut short.";
            }
            if (!sha256(buf).equalsIgnoreCase(kXexSha256))
                return "That default.xex isn't the original SvR 2011 one (another version, or changed).";
            File dir = new File(game(), "Original");
            dir.mkdirs();
            Stfs.write(new File(dir, "default.xex"), buf, buf.length);
            return "The original program is in place.";
        } catch (Exception e) {
            return "Copying default.xex failed: " + e.getMessage();
        }
    }

    private void pickIso() {
        Intent pick = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        pick.addCategory(Intent.CATEGORY_OPENABLE);
        pick.setType("*/*");
        a_.startForResult(pick, (code, data) -> {
            if (code != android.app.Activity.RESULT_OK || data == null || data.getData() == null || !start()) return;
            final String[] r = new String[1];
            a_.background(() -> r[0] = copyXex(data.getData()), () -> done(r[0]));
        });
    }

    private String copyXex(Uri uri) {
        try (ParcelFileDescriptor pfd = a_.getContentResolver().openFileDescriptor(uri, "r")) {
            if (pfd == null) return "The disc image can't be opened.";
            try (FileInputStream fin = new FileInputStream(pfd.getFileDescriptor())) {
                return xexFrom(fin);
            }
        } catch (Exception e) {
            return "Copying from the ISO failed: " + e.getMessage();
        }
    }

    private String xexFrom(FileInputStream fin) {
        try {
            InstallPage.Disc disc = new InstallPage.Disc();
            String err = disc.open(fin.getChannel());
            if (err == null) err = disc.check();
            if (err != null) return "That ISO can't be used: " + err + ".";
            InstallPage.Entry xex = disc.find("default.xex");
            if (xex == null || xex.size != kXexSize) return "That ISO's default.xex isn't the original disc's (another version).";
            byte[] buf = new byte[(int) kXexSize];
            ByteBuffer b = ByteBuffer.wrap(buf);
            long at = disc.base + xex.sector * 2048;
            while (b.hasRemaining()) {
                if (disc.ch.read(b, at + b.position()) <= 0) return "The ISO ends early.";
            }
            if (!sha256(buf).equalsIgnoreCase(kXexSha256))
                return "That ISO's default.xex isn't the original disc's (it has been changed).";
            File dir = new File(game(), "Original");
            dir.mkdirs();
            Stfs.write(new File(dir, "default.xex"), buf, buf.length);
            return "The original program is copied from your ISO. You can delete the ISO now.";
        } catch (Exception e) {
            return "Copying from the ISO failed: " + e.getMessage();
        }
    }

    static String sha256(byte[] d) throws java.security.NoSuchAlgorithmException {
        byte[] h = MessageDigest.getInstance("SHA-256").digest(d);
        StringBuilder s = new StringBuilder();
        for (byte x : h) s.append(String.format("%02x", x));
        return s.toString();
    }

    // ── 1. the Anthology folder ──

    // A folder picked with the system's picker, as a path (the app has all-files access).
    private void pickFolder(java.util.function.Consumer<File> use) {
        Intent pick = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
        a_.startForResult(pick, (code, data) -> {
            if (code != android.app.Activity.RESULT_OK || data == null || data.getData() == null) return;
            File f = treePath(data.getData());
            if (f == null) a_.status("That folder can't be used: choose one on the phone's storage or SD card.");
            else use.accept(f);
        });
    }

    static File treePath(Uri tree) {
        String id;
        try {
            id = DocumentsContract.getTreeDocumentId(tree);
        } catch (Exception e) {
            return null;
        }
        int colon = id.indexOf(':');
        if (colon < 0) return null;
        String vol = id.substring(0, colon), rest = id.substring(colon + 1);
        File root = vol.equalsIgnoreCase("primary") ? Environment.getExternalStorageDirectory() : new File("/storage", vol);
        return rest.isEmpty() ? root : new File(root, rest);
    }

    // His folder somewhere the player copies things (not already in place), or null.
    private static File findDataFolder() {
        File dest = new File(game(), Flavor.kAnthologyData);
        for (File p : places()) {
            File d = new File(p, Flavor.kAnthologyData);
            if (new File(d, "default.xex").isFile() && !d.getPath().equals(dest.getPath())) return d;
        }
        return null;
    }

    private void findData() {
        if (dataHere()) {
            a_.status("The Anthology is in place.");
            refresh();
            return;
        }
        File d = findDataFolder();
        if (d != null) {
            useData(d);
            return;
        }
        a_.status("\"" + Flavor.kAnthologyData + "\" wasn't found in games, Download or the phone's storage: copy it "
            + "there, or press Choose the folder.");
    }

    private void useData(File picked) {
        File src = picked;
        // (the folder above it chosen: his installer's folder)
        if (!new File(src, "default.xex").isFile() && new File(new File(src, Flavor.kAnthologyData), "default.xex").isFile())
            src = new File(src, Flavor.kAnthologyData);
        if (!new File(src, "default.xex").isFile() || !new File(src, "pac").isDirectory()) {
            a_.status("That isn't the Anthology folder Yumboy11's installer made (no default.xex and pac in it).");
            return;
        }
        File dest = new File(game(), Flavor.kAnthologyData);
        if (src.getAbsolutePath().equals(dest.getAbsolutePath())) {
            a_.status("The Anthology is in place.");
            refresh();
            return;
        }
        if (!start()) return;
        final File from = src;
        a_.getPreferences(android.content.Context.MODE_PRIVATE).edit()
            .putString("anthology_source", from.getParent()).apply();
        final String[] r = new String[1];
        a_.background(() -> r[0] = moveData(from, dest), () -> done(r[0]));
    }

    private String moveData(File src, File dest) {
        dest.getParentFile().mkdirs();
        if (!dest.exists() && src.renameTo(dest)) return "The Anthology is moved into games/" + InstallActivity.kFolderName + ".";
        // (another drive, or a part copy already there: copied, resumably - files already there at the same size stay)
        long[] total = {0};
        size(src, total);
        if (total[0] + (256L << 20) > InstallPage.free() + bytesIn(dest))
            return "Not enough free space: " + FileOps.human(total[0]) + " needed, " + FileOps.human(InstallPage.free()) + " free.";
        long[] done = {0};
        try {
            if (!copyTree(src, dest, total[0], done, true)) return "Stopped. Press Find it again to carry on where it stopped.";
            copyTree(src, dest, total[0], done, false);  // (default.xex last: the app counts the game as there then)
        } catch (IOException e) {
            return "Copying failed: " + e.getMessage();
        }
        if (cancel_) return "Stopped. Press Find it again to carry on where it stopped.";
        return "The Anthology is copied into games/" + InstallActivity.kFolderName + ". You can delete the copy at "
            + shortPath(src) + " to free space.";
    }

    private static long bytesIn(File d) {
        long[] n = {0};
        size(d, n);
        return n[0];
    }

    private static void size(File f, long[] n) {
        File[] l = f.listFiles();
        if (l == null) return;
        for (File x : l) {
            if (x.isDirectory()) size(x, n);
            else n[0] += x.length();
        }
    }

    // Copies src into dest: everything but the top folder's default.xex (first), or only that (second).
    private boolean copyTree(File src, File dest, long total, long[] done, boolean allButXex) throws IOException {
        File[] l = src.listFiles();
        if (l == null) return true;
        dest.mkdirs();
        byte[] buf = new byte[8 << 20];
        for (File x : l) {
            if (cancel_) return false;
            boolean xex = x.getName().equals("default.xex") && src.getName().equals(Flavor.kAnthologyData);
            File out = new File(dest, x.getName());
            if (x.isDirectory()) {
                if (allButXex && !x.getName().equals("@Alternate_Files") && !copyDir(x, out, total, done, buf)) return false;
                continue;
            }
            if (xex == allButXex) continue;
            copyFile(x, out, total, done, buf);
        }
        return true;
    }

    private boolean copyDir(File src, File dest, long total, long[] done, byte[] buf) throws IOException {
        File[] l = src.listFiles();
        if (l == null) return true;
        dest.mkdirs();
        for (File x : l) {
            if (cancel_) return false;
            File out = new File(dest, x.getName());
            if (x.isDirectory()) {
                if (!copyDir(x, out, total, done, buf)) return false;
            } else {
                copyFile(x, out, total, done, buf);
            }
        }
        return true;
    }

    private void copyFile(File x, File out, long total, long[] done, byte[] buf) throws IOException {
        if (out.isFile() && out.length() == x.length()) {
            done[0] += x.length();
            return;
        }
        File part = new File(out.getPath() + ".part");
        try (InputStream in = new FileInputStream(x); OutputStream o = new FileOutputStream(part)) {
            int n;
            while ((n = in.read(buf)) > 0) {
                if (cancel_) break;
                o.write(buf, 0, n);
                done[0] += n;
                report(done[0], total, "Copying the Anthology…");
            }
        }
        if (cancel_) {
            part.delete();
            return;
        }
        if (out.exists()) out.delete();
        if (!part.renameTo(out)) throw new IOException("can't write " + out);
    }

    // ── 3. his saves and DLC ──

    private void installSaves() {
        // (beside the Anthology folder the player had, or where the player copies things: found without asking)
        String srcParent = a_.getPreferences(android.content.Context.MODE_PRIVATE).getString("anthology_source", null);
        java.util.List<File> look = new java.util.ArrayList<>();
        if (srcParent != null) look.add(new File(srcParent, kSavesFolder));
        for (File p : places()) look.add(new File(p, kSavesFolder));
        for (File p : look) {
            if (new File(p, "XENIA INSTALL ME").isDirectory()) {
                runSaves(p);
                return;
            }
        }
        pickFolder(f -> {
            File his = new File(f, "XENIA INSTALL ME").isDirectory() ? f : new File(f, kSavesFolder);
            if (!new File(his, "XENIA INSTALL ME").isDirectory()) {
                a_.status("That isn't his \"" + kSavesFolder + "\" folder (no \"XENIA INSTALL ME\" in it).");
                return;
            }
            runSaves(his);
        });
    }

    private void runSaves(File his) {
        if (!start()) return;
        final String[] r = new String[1];
        a_.background(() -> {
            try {
                if (new File(game(), "Saves").isDirectory()) FileOps.backup(" (before Yumboy11's saves)");
            } catch (IOException e) {
                r[0] = "Couldn't back up the saves first: " + e.getMessage();
                return;
            }
            int[] n = Stfs.installHis(his, game());
            r[0] = n == null ? "His \"XENIA INSTALL ME\" folder can't be read."
                : "Installed " + n[0] + " saves and " + n[1] + " DLC packages" + (n[2] > 0 ? " (" + n[2] + " skipped)" : "") + ".";
        }, () -> done(r[0]));
    }

    // ── the setup check, for a report ──

    String checkText() {
        StringBuilder s = new StringBuilder("SVR Anthology setup check (app ").append(a_.versionName()).append(")\n");
        s.append("1. ").append(data_.state.getText()).append(dataCheck_.isEmpty() ? "" : " [" + dataCheck_ + "]").append('\n');
        s.append("2. ").append(xex_.state.getText()).append('\n');
        s.append("3. ").append(saves_.state.getText()).append('\n');
        s.append("4. ").append(phone_.state.getText()).append('\n');
        s.append("Phone: ").append(Build.MANUFACTURER).append(' ').append(Build.MODEL).append(", Android ")
            .append(Build.VERSION.RELEASE).append('\n');
        s.append("Game folder: ").append(game().getPath());
        return s.toString();
    }

    private void copyCheck() {
        ClipboardManager cb = (ClipboardManager) a_.getSystemService(android.content.Context.CLIPBOARD_SERVICE);
        cb.setPrimaryClip(ClipData.newPlainText("SVR Anthology setup check", checkText()));
        a_.status("The setup check is copied: paste it in your report or message.");
    }
}
