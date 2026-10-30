// Xbox 360 save packages (STFS) on the phone: the PC launchers' launcher/stfs.c import_one in Java, for
// SVR Anthology's Setup tab - Yumboy11's saves ("XENIA INSTALL ME" in his "Saves and DLC" folder) into the
// game folder's Saves, his DLC packages into DLC. See stfs.c for the format.

package io.github.kaikoclanworth1.svr2011;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

final class Stfs {
    private Stfs() {}

    // The original game and Yumboy11's program (his saves and DLC are made for it).
    static boolean titleOk(int t) { return t == 0x5451085D || (Flavor.ANTHOLOGY && t == 0x54511A11); }

    static int be32(byte[] d, int at) {
        return (d[at] & 0xFF) << 24 | (d[at + 1] & 0xFF) << 16 | (d[at + 2] & 0xFF) << 8 | (d[at + 3] & 0xFF);
    }

    static int le24(byte[] d, int at) { return (d[at] & 0xFF) | (d[at + 1] & 0xFF) << 8 | (d[at + 2] & 0xFF) << 16; }

    private static final class Pkg {
        byte[] d;
        int sex, step0, step1, first, topLevel, sep;

        long dataBlock(long b) {
            long r = (((b + 0xAA) / 0xAA) << sex) + b;
            if (b < 0xAA) return r;
            if (b < 0x70E4) return r + (((b + 0x70E4) / 0x70E4) << sex);
            return (1L << sex) + r + (((b + 0x70E4) / 0x70E4) << sex);
        }

        // A block's offset, or -1 when it isn't in the file.
        long block(long b) {
            long at = (dataBlock(b) << 12) + first;
            return at + 0x1000 <= d.length ? at : -1;
        }

        long nextBlock(long b) {
            long l0 = 0;
            if (b >= 0xAA) {
                l0 = (b / 0xAA) * step0 + (((b / 0x70E4) + 1) << sex);
                if (b / 0x70E4 != 0) l0 += 1L << sex;
            }
            long at = (l0 << 12) + first + (b % 0xAA) * 0x18;
            if (topLevel == 0) {
                at += (long) (sep & 2) << 0xB;
            } else {
                long l1 = b < 0x70E4 ? step0 : (1L << sex) + (b / 0x70E4) * step1;
                long top = (l1 << 12) + first + ((long) (sep & 2) << 0xB) + (b / 0xAA) * 0x18 + 0x14;
                if (top >= d.length) return -1;
                at += (long) (d[(int) top] & 0x40) << 6;
            }
            if (at + 0x18 > d.length) return -1;
            int i = (int) at;
            return (d[i + 0x15] & 0xFFL) << 16 | (d[i + 0x16] & 0xFFL) << 8 | (d[i + 0x17] & 0xFFL);
        }

        boolean readBlocks(long start, int count, boolean inARow, byte[] out) {
            long b = start;
            for (int i = 0; i < count; i++) {
                long at = block(b);
                if (at < 0) return false;
                System.arraycopy(d, (int) at, out, i * 0x1000, 0x1000);
                b = inARow ? b + 1 : nextBlock(b);
                if (b < 0 && i + 1 < count) return false;
            }
            return true;
        }
    }

    static byte[] load(File f) throws IOException {
        long n = f.length();
        if (n < 0x1000 || n >= 512L << 20) return null;
        byte[] d = new byte[(int) n];
        try (InputStream in = new FileInputStream(f)) {
            int got = 0;
            while (got < d.length) {
                int r = in.read(d, got, d.length - got);
                if (r <= 0) return null;
                got += r;
            }
        }
        return d;
    }

    // One package -> dest/name (+ .info/name.header): 1 done, 0 not one of the game's saves, -1 unreadable.
    static int importOne(File path, File dest) {
        byte[] d;
        try {
            d = load(path);
        } catch (IOException e) {
            return -1;
        }
        if (d == null) return 0;
        String magic = new String(d, 0, 4, java.nio.charset.StandardCharsets.US_ASCII);
        if (!magic.equals("CON ") && !magic.equals("LIVE") && !magic.equals("PIRS")) return 0;
        if (!titleOk(be32(d, 0x360)) || be32(d, 0x344) != 1) return 0;
        Pkg k = new Pkg();
        k.d = d;
        int headerSize = be32(d, 0x340);
        k.sep = d[0x379 + 2] & 0xFF;
        int ftCount = (d[0x379 + 3] & 0xFF) | (d[0x379 + 4] & 0xFF) << 8;
        int ftBlock = le24(d, 0x379 + 5);
        long allocated = be32(d, 0x379 + 0x1C) & 0xFFFFFFFFL;
        k.sex = (~k.sep) & 1;
        k.step0 = k.sex != 0 ? 0xAC : 0xAB;
        k.step1 = k.sex != 0 ? 0x723A : 0x718F;
        k.first = (headerSize + 0xFFF) & 0xFFFFF000;
        k.topLevel = allocated <= 0xAA ? 0 : allocated <= 0x70E4 ? 1 : 2;
        if (k.topLevel == 2 || ftCount == 0 || ftCount > 64) return -1;  // (saves are far smaller)
        byte[] table = new byte[ftCount * 0x1000];
        if (!k.readBlocks(ftBlock, ftCount, false, table)) return -1;
        int fileBlocks = 0, fileStart = 0;
        long fileSize = 0;
        boolean inARow = false, found = false;
        for (int i = 0; i + 0x40 <= table.length; i += 0x40) {
            int flags = table[i + 0x28] & 0xFF;
            if ((flags & 0x3F) == 0 || (flags & 0x80) != 0) continue;  // (empty / a folder)
            fileBlocks = le24(table, i + 0x29);
            fileStart = le24(table, i + 0x2F);
            fileSize = be32(table, i + 0x34) & 0xFFFFFFFFL;
            inARow = (flags & 0x40) != 0;
            found = true;
            break;
        }
        if (!found || fileBlocks == 0 || fileSize > (long) fileBlocks * 0x1000) return -1;
        byte[] data = new byte[fileBlocks * 0x1000];
        if (!k.readBlocks(fileStart, fileBlocks, inARow, data)) return -1;
        String name = path.getName();
        File info = new File(dest, ".info");
        info.mkdirs();
        byte[] h = new byte[328];
        h[3] = 1;  // device
        h[7] = 1;  // saved game
        System.arraycopy(d, 0x411, h, 8, 0x80);  // the display name (UTF-16BE, as the package keeps it)
        for (int i = 0; i < name.length() && i < 41; i++) h[264 + i] = (byte) name.charAt(i);
        h[320] = 0x54; h[321] = 0x51; h[322] = 0x08; h[323] = 0x5D;
        try {
            write(new File(dest, name), data, (int) fileSize);
            write(new File(info, name + ".header"), h, h.length);
        } catch (IOException e) {
            return -1;
        }
        return 1;
    }

    static void write(File f, byte[] data, int n) throws IOException {
        File part = new File(f.getPath() + ".part");
        try (OutputStream o = new FileOutputStream(part)) {
            o.write(data, 0, n);
        }
        if (f.exists()) f.delete();
        if (!part.renameTo(f)) throw new IOException("can't write " + f);
    }

    // Yumboy11's "Saves and DLC" folder -> the game folder: [saves, dlc, skipped], or null without his
    // "XENIA INSTALL ME" (as the PC Anthology launcher's install_saves_dlc).
    static int[] installHis(File his, File game) {
        File src = new File(his, "XENIA INSTALL ME");
        File[] list = src.listFiles();
        if (list == null) return null;
        File saves = new File(game, "Saves"), dlc = new File(game, "DLC");
        saves.mkdirs();
        dlc.mkdirs();
        int[] n = new int[3];
        scan(src, saves, 0, n);
        for (File f : list) {
            if (!f.isFile() || f.length() < 0x348) continue;
            byte[] head = new byte[0x348];
            try (InputStream in = new FileInputStream(f)) {
                if (in.read(head) != head.length) continue;
            } catch (IOException e) {
                continue;
            }
            // (content type 2: marketplace content - his DLC packages; Vol11 and Vol12 are CON, the rest LIVE)
            String magic = new String(head, 0, 4, java.nio.charset.StandardCharsets.US_ASCII);
            if (!magic.equals("LIVE") && !magic.equals("PIRS") && !magic.equals("CON ") || be32(head, 0x344) != 2) continue;
            try (InputStream in = new FileInputStream(f); OutputStream o = new FileOutputStream(new File(dlc, f.getName()))) {
                FileOps.copy(in, o);
                n[1]++;
            } catch (IOException e) {
                n[2]++;
            }
        }
        return n;
    }

    private static void scan(File dir, File dest, int depth, int[] n) {
        File[] list = dir.listFiles();
        if (list == null) return;
        for (File f : list) {
            if (f.isDirectory()) {
                if (depth < 5) scan(f, dest, depth + 1, n);
            } else if (f.length() >= 0x1000 && f.length() < (1L << 32)) {
                int r = importOne(f, dest);
                if (r > 0) n[0]++;
                else if (r < 0) n[2]++;
            }
        }
    }
}
