// SVR Anthology's Setup tab on the phone (the PC Anthology launcher's Setup, without running Yumboy11's
// installer - it is a Windows program): the original program from the player's own SvR 2011 ISO, Yumboy11's
// installed Anthology folder copied to the phone (moved into games/SVR Anthology), and his saves and DLC.
// The PC launcher's "Install to phone" does all of this over USB instead.

package io.github.kaikoclanworth1.svr2011;

import android.content.Intent;
import android.net.Uri;
import android.os.Environment;
import android.os.ParcelFileDescriptor;
import android.provider.DocumentsContract;
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

    private final LauncherActivity a_;
    private final LinearLayout view_;
    private final TextView iso_, data_, saves_;
    private final ProgressBar progress_;
    private boolean busy_;
    private volatile boolean cancel_;

    AnthologySetupPage(LauncherActivity a) {
        a_ = a;
        view_ = a.column();
        LinearLayout pc = a.card(view_, "Easiest: from your PC");
        TextView t = a.text("Set SVR Anthology up on your PC with the SvR Anthology Launcher (your own SvR 2011 ISO and "
            + "Yumboy11's own installer), then on its Phone page connect this phone over USB and click Install to "
            + "phone. It copies everything below for you.", 14, LauncherActivity.kText);
        t.setPadding(0, a.dp(10), 0, a.dp(10));
        pc.addView(t);

        LinearLayout c1 = a.card(view_, "1. Your SvR 2011 Xbox 360 ISO");
        iso_ = state(c1);
        Button pickIso = a.button("Choose the ISO…", LauncherActivity.kRed);
        pickIso.setOnClickListener(v -> pickIso());
        c1.addView(pickIso, a.fullWidth(6));
        hint(c1, "The original program (default.xex) is taken from it - the recomp runs that, with Yumboy11's data.");

        LinearLayout c2 = a.card(view_, "2. The Anthology Yumboy11's installer made");
        data_ = state(c2);
        hint(c2, "Copy the \"" + Flavor.kAnthologyData + "\" folder his installer made on your PC to this phone (USB "
            + "cable or SD card; about 28 GB), with the \"" + kSavesFolder + "\" folder beside it.");
        Button find = a.button("Find it", LauncherActivity.kRed);
        find.setOnClickListener(v -> findData());
        Button pickData = a.button("Choose the folder…", LauncherActivity.kBackground);
        pickData.setOnClickListener(v -> pickFolder(this::useData));
        c2.addView(a.pair(find, pickData), a.fullWidth(6));
        hint(c2, "It is moved into games/" + InstallActivity.kFolderName + " (copied if it is on another drive).");

        LinearLayout c3 = a.card(view_, "3. Yumboy11's saves and DLC");
        saves_ = state(c3);
        Button pickSaves = a.button("Install his saves and DLC…", LauncherActivity.kBackground);
        pickSaves.setOnClickListener(v -> installSaves());
        c3.addView(pickSaves, a.fullWidth(6));
        hint(c3, "From his \"" + kSavesFolder + "\" folder (it is only read).");

        progress_ = new ProgressBar(a, null, android.R.attr.progressBarStyleHorizontal);
        progress_.setMax(1000);
        progress_.setVisibility(View.GONE);
        progress_.setProgressTintList(android.content.res.ColorStateList.valueOf(LauncherActivity.kRed));
        view_.addView(progress_, a.fullWidth(12));
        Button stop = a.button("Stop", LauncherActivity.kBackground);
        stop.setOnClickListener(v -> cancel_ = true);
        view_.addView(stop, a.fullWidth(6));
    }

    View view() { return view_; }

    private TextView state(LinearLayout card) {
        TextView t = a_.text("", 15, LauncherActivity.kText);
        t.setPadding(0, a_.dp(10), 0, a_.dp(4));
        card.addView(t);
        return t;
    }

    private void hint(LinearLayout card, String s) {
        TextView t = a_.text(s, 13, LauncherActivity.kDim);
        t.setPadding(0, a_.dp(6), 0, a_.dp(10));
        card.addView(t);
    }

    static File game() { return InstallActivity.gameFolder(); }

    void refresh() {
        File g = game();
        iso_.setText(new File(g, "Original/default.xex").length() == kXexSize ? "✓ The original program is here."
            : "Not done yet.");
        data_.setText(new File(new File(g, Flavor.kAnthologyData), "default.xex").isFile()
            ? "✓ The Anthology is in games/" + InstallActivity.kFolderName + "." : "Not done yet.");
        String[] s = new File(g, "Saves").list();
        saves_.setText(s != null && s.length > 0 ? "✓ Saves are here." : "Not done yet (optional).");
    }

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
        return true;
    }

    private void done(String message) {
        busy_ = false;
        progress_.setVisibility(View.GONE);
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

    // ── 1. the original program from the ISO ──

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
                InstallPage.Disc disc = new InstallPage.Disc();
                String err = disc.open(fin.getChannel());
                if (err == null) err = disc.check();
                if (err != null) return "That ISO can't be used: " + err + ".";
                InstallPage.Entry xex = disc.find("default.xex");
                if (xex.size != kXexSize) return "That ISO's default.xex isn't the original disc's (another version).";
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
                return "The original program is copied from your ISO.";
            }
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

    // ── 2. the Anthology folder ──

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

    // The places a player copies a folder to: the storage's top, Download, games, and the installer's folder in
    // those.
    private void findData() {
        File root = Environment.getExternalStorageDirectory();
        String[] places = {"", "Download", "games", "Download/SVR Anthology 1.0 Installer", "SVR Anthology 1.0 Installer",
            "games/SVR Anthology 1.0 Installer", "Documents"};
        for (String p : places) {
            File d = new File(p.isEmpty() ? root : new File(root, p), Flavor.kAnthologyData);
            if (new File(d, "default.xex").isFile()) {
                useData(d);
                return;
            }
        }
        a_.status("\"" + Flavor.kAnthologyData + "\" wasn't found in the phone's storage, Download or games: choose it.");
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
        // (another drive: copied, resumably - files already there at the same size stay)
        long[] total = {0};
        size(src, total);
        if (total[0] + (256L << 20) > InstallPage.free())
            return "Not enough free space: " + FileOps.human(total[0]) + " needed, " + FileOps.human(InstallPage.free()) + " free.";
        long[] done = {0};
        try {
            if (!copyTree(src, dest, total[0], done, true)) return "Stopped. Choose it again to carry on where it stopped.";
            copyTree(src, dest, total[0], done, false);  // (default.xex last: the app counts the game as there then)
        } catch (IOException e) {
            return "Copying failed: " + e.getMessage();
        }
        if (cancel_) return "Stopped. Choose it again to carry on where it stopped.";
        return "The Anthology is copied into games/" + InstallActivity.kFolderName + ". You can delete the copy at "
            + src.getAbsolutePath() + " to free space.";
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
        // (beside the Anthology folder the player had: found without asking)
        String srcParent = a_.getPreferences(android.content.Context.MODE_PRIVATE).getString("anthology_source", null);
        File root = Environment.getExternalStorageDirectory();
        File[] places = {srcParent != null ? new File(srcParent, kSavesFolder) : null, new File(root, kSavesFolder),
            new File(new File(root, "Download"), kSavesFolder), new File(new File(root, "games"), kSavesFolder),
            new File(new File(root, "Download/SVR Anthology 1.0 Installer"), kSavesFolder)};
        for (File p : places) {
            if (p != null && new File(p, "XENIA INSTALL ME").isDirectory()) {
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
}
