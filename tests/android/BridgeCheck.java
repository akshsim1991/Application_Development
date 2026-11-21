import com.featherpdf.engine.Engine;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.nio.ByteBuffer;
import java.nio.file.Files;
import java.security.MessageDigest;
import java.security.SecureRandom;
import javax.crypto.Cipher;
import javax.crypto.spec.IvParameterSpec;
import javax.crypto.spec.SecretKeySpec;
import javax.imageio.ImageIO;
import java.awt.image.BufferedImage;

/**
 * Drives the Android app's engine bridge (libfeatherpdf + Engine.java) on a desktop Java, with the system services given
 * the way the app gives them (MessageDigest, Cipher, SecureRandom are the same classes on Android).
 * Usage: java BridgeCheck <work folder>   (tests/android/bridge_check.sh makes the files)
 */
public class BridgeCheck {
    static int fails = 0;
    static void check(String what, boolean ok) {
        System.out.println((ok ? "  ok   " : "  FAIL ") + what);
        if (!ok) fails++;
    }

    static final class DesktopHooks implements Engine.SystemHooks {
        public byte[] hash(int alg, byte[] data) {
            try { return MessageDigest.getInstance("SHA-" + alg).digest(data); } catch (Exception e) { return null; }
        }
        public byte[] aes(boolean cbc, byte[] key, byte[] iv, byte[] in) {
            try {
                Cipher c = Cipher.getInstance(cbc ? "AES/CBC/NoPadding" : "AES/ECB/NoPadding");
                if (cbc) c.init(Cipher.ENCRYPT_MODE, new SecretKeySpec(key, "AES"), new IvParameterSpec(iv));
                else c.init(Cipher.ENCRYPT_MODE, new SecretKeySpec(key, "AES"));
                return c.doFinal(in);
            } catch (Exception e) { return null; }
        }
        public byte[] random(int n) { byte[] b = new byte[n]; new SecureRandom().nextBytes(b); return b; }
        public byte[] encodeImage(byte[] bgra, int w, int h, boolean jpeg, int quality, boolean alpha) {
            try {
                BufferedImage im = new BufferedImage(w, h, jpeg || !alpha ? BufferedImage.TYPE_INT_RGB : BufferedImage.TYPE_INT_ARGB);
                for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
                    int i = (y * w + x) * 4;
                    im.setRGB(x, y, ((bgra[i + 3] & 255) << 24) | ((bgra[i + 2] & 255) << 16) | ((bgra[i + 1] & 255) << 8) | (bgra[i] & 255));
                }
                ByteArrayOutputStream out = new ByteArrayOutputStream();
                ImageIO.write(im, jpeg ? "jpg" : "png", out);
                return out.toByteArray();
            } catch (Exception e) { return null; }
        }
        public byte[] decodeImage(String path) {
            try {
                BufferedImage im = ImageIO.read(new File(path));
                if (im == null) return null;
                int w = im.getWidth(), h = im.getHeight();
                byte[] b = new byte[8 + w * h * 4];
                b[0] = (byte) (w >>> 24); b[1] = (byte) (w >>> 16); b[2] = (byte) (w >>> 8); b[3] = (byte) w;
                b[4] = (byte) (h >>> 24); b[5] = (byte) (h >>> 16); b[6] = (byte) (h >>> 8); b[7] = (byte) h;
                for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
                    int p = im.getRGB(x, y), i = 8 + (y * w + x) * 4;
                    b[i] = (byte) p; b[i + 1] = (byte) (p >> 8); b[i + 2] = (byte) (p >> 16); b[i + 3] = (byte) (p >>> 24);
                }
                return b;
            } catch (Exception e) { return null; }
        }
    }

    static int[] pixel(ByteBuffer b, int stride, int x, int y) {
        int i = y * stride + x * 4;
        return new int[] {b.get(i) & 255, b.get(i + 1) & 255, b.get(i + 2) & 255, b.get(i + 3) & 255};
    }

    public static void main(String[] args) throws Exception {
        String w = args[0];
        String err = Engine.start(new DesktopHooks());
        check("the engine starts (" + err + ")", err == null);
        if (err != null) System.exit(1);

        String credits = Engine.aboutCredits();
        check("the About text is the desktop apps' (who made it, how to write)", credits.startsWith("Created by: Akshaya Simha")
              && credits.contains("akshsimha@gmail.com") && credits.contains("Courtesy") && "akshsimha@gmail.com".equals(Engine.feedbackAddress()));
        System.out.println("opening");
        int[] r = new int[1];
        long d = Engine.open(w + "/doc.pdf", "", r);
        check("a PDF opens", d != 0 && r[0] == Engine.OPEN_OK);
        check("its pages counted", Engine.pageCount(d) == 3);
        float[] sz = Engine.pageSizes(d);
        check("page sizes in points (A4)", sz.length == 6 && Math.abs(sz[0] - 595.3f) < 1 && Math.abs(sz[1] - 841.9f) < 1);
        long none = Engine.open(w + "/missing.pdf", "", r);
        check("a missing file: no document, and why", none == 0 && r[0] != 0 && Engine.describe(r[0], "missing.pdf").length() > 10);
        long locked = Engine.open(w + "/locked.pdf", "", r);
        check("a protected PDF asks for its password", locked == 0 && r[0] == Engine.OPEN_PASSWORD_NEEDED);
        locked = Engine.open(w + "/locked.pdf", "wrong", r);
        check("a wrong password is told", locked == 0 && r[0] == Engine.OPEN_WRONG_PASSWORD);
        locked = Engine.open(w + "/locked.pdf", "s3cret", r);
        check("the right password opens it", locked != 0);

        System.out.println("drawing");
        int pw = 600, ph = Math.round(600 * sz[1] / sz[0]), stride = pw * 4;
        ByteBuffer buf = ByteBuffer.allocateDirect(stride * ph);
        check("a page is drawn", Engine.render(d, 0, buf, pw, ph, stride, 0, 0, pw, ph, Engine.COLOURS_NORMAL));
        int[] corner = pixel(buf, stride, 2, 2);
        check("white paper (RGBA, opaque)", corner[0] == 255 && corner[1] == 255 && corner[2] == 255 && corner[3] == 255);
        // the red box drawn at 72,72 (points from the bottom left), 100 x 50: red first, as Android's bitmaps hold it
        float[] dev = Engine.pageToDevice(d, 0, pw, ph, new float[] {122, 97});
        int[] red = pixel(buf, stride, Math.round(dev[0]), Math.round(dev[1]));
        check("colours in RGBA order (a red box reads red)", red[0] > 200 && red[1] < 60 && red[2] < 60);
        float[] back = Engine.deviceToPage(d, 0, pw, ph, dev[0], dev[1]);
        check("page and screen positions convert both ways", Math.abs(back[0] - 122) < 1 && Math.abs(back[1] - 97) < 1);
        Engine.render(d, 0, buf, pw, ph, stride, 0, 0, pw, ph, Engine.COLOURS_BLACKBOARD);
        int[] bb = pixel(buf, stride, 2, 2);
        check("blackboard colours: black paper", bb[0] == 0 && bb[1] == 0 && bb[2] == 0);
        Engine.render(d, 0, buf, pw, ph, stride, 0, 0, pw, ph, Engine.COLOURS_NIGHT);
        int[] night = pixel(buf, stride, 2, 2);
        check("night colours: dark paper", night[0] < 60 && night[0] > 10);
        // a part of the page, zoomed: the buffer shows the red box's middle at twice the size
        int zw = pw * 2, zh = ph * 2;
        float[] mid = Engine.pageToDevice(d, 0, zw, zh, new float[] {122, 97});
        ByteBuffer part = ByteBuffer.allocateDirect(200 * 200 * 4);
        Engine.render(d, 0, part, 200, 200, 800, 100 - Math.round(mid[0]), 100 - Math.round(mid[1]), zw, zh, 0);
        int[] pr = pixel(part, 800, 100, 100);
        check("a zoomed part of a page is drawn where asked", pr[0] > 200 && pr[1] < 60);

        System.out.println("text");
        String sel = Engine.selectionText(d, 0, 72, 760, 400, 760);
        check("text between two points is selected (" + sel + ")", sel.startsWith("Quarterly"));
        float[] rects = Engine.selectionRects(d, 0, 72, 760, 400, 760);
        check("and outlined", rects.length >= 4 && rects[2] > rects[0] && rects[3] > rects[1]);
        float[] blocks = Engine.editableBlocks(d, 0);
        check("edit mode finds the paragraphs", blocks.length >= 10 && blocks.length % 5 == 0);
        double[] info = new double[11];
        String para = Engine.paragraphAt(d, 0, 100, 700, info);
        check("a tapped paragraph is found (" + para + ")", para != null && para.startsWith("Sales rose"));
        check("with its size and colour", Math.abs(info[4] - 12) < 0.6 && ((int) info[8] & 0xFFFFFF) == 0);
        String why = Engine.replaceParagraph(d, para + " Café crème, naïve – done.", (int) info[7], false, false, info[4], (int) info[8]);
        check("the paragraph is changed (" + why + ")", why == null);
        float[] hits = Engine.find(d, "naïve", false, false);
        check("the new words are found, accents and all", hits.length == 5 && hits[0] == 0);
        float[] q = Engine.find(d, "Quarterly", false, false);
        check("a match is where its word is (the heading at 72, 760)", q.length == 5 && q[0] == 0 && Math.abs(q[1] - 72) < 2
              && q[2] > 755 && q[2] < 765 && q[4] > 768 && q[4] < 780 && q[3] > 140 && q[3] < 170);
        check("the document is modified, undo offered", Engine.isModified(d) && Engine.canUndo(d));
        check("text past the basic plane passes both ways", Engine.describe(1, "📄 file.pdf").contains("📄"));
        check("a box of new text is added", Engine.addTextBox(d, 0, 72, 300, "Added on Android", 14, 0xFF2040A0));
        check("and can be found", Engine.find(d, "Added on Android", false, false).length == 5);

        System.out.println("annotations");
        float[] hl = Engine.selectionRects(d, 0, 72, 760, 400, 760);
        check("a highlight", Engine.addMarkup(d, 0, Engine.MARKUP_HIGHLIGHT, hl, 0xFFFFE000));
        check("a drawing", Engine.addInk(d, 1, new float[][] {{100, 100, 150, 150, 200, 120}, {300, 300}}, 0xFFFF0000, 2));
        check("a note", Engine.addNote(d, 2, 200, 500, "Check this ✓", 0xFFFFC000));
        String[] kt = new String[2];
        int a = Engine.annotAt(d, 2, 205, 495, kt);
        check("the note is found where it was put (" + kt[0] + ")", a >= 0 && "Check this ✓".equals(kt[1]));
        check("its comment can be changed", Engine.setAnnotText(d, 2, a, "Checked"));
        int hlIndex = Engine.annotAt(d, 0, 100, 762, kt);
        check("the highlight is found (" + kt[0] + ")", hlIndex >= 0);

        System.out.println("pages, history, saving");
        check("a page turned", Engine.rotatePages(d, new int[] {1}, 1));
        float[] sz2 = Engine.pageSizes(d);
        check("its size follows the turn", Math.abs(sz2[2] - sz[3]) < 1 && Math.abs(sz2[3] - sz[2]) < 1);
        check("undo", Engine.undo(d) && Math.abs(Engine.pageSizes(d)[2] - sz[2]) < 1);
        check("redo", Engine.canRedo(d) && Engine.redo(d) && Math.abs(Engine.pageSizes(d)[2] - sz[3]) < 1);
        check("a page deleted", Engine.deletePages(d, new int[] {2}) && Engine.pageCount(d) == 2);
        byte[] bytes = Engine.bytesToSave(d);
        check("the document is written", bytes != null && bytes.length > 1000);
        Files.write(new File(w + "/saved.pdf").toPath(), bytes);
        Engine.markSaved(d);
        check("then it is no longer modified", !Engine.isModified(d));
        Engine.close(d);
        long s = Engine.open(w + "/saved.pdf", "", r);
        check("the saved file opens", s != 0 && Engine.pageCount(s) == 2);
        check("with the edit", Engine.find(s, "Café crème", false, false).length == 5);
        check("the highlight", Engine.annotAt(s, 0, 100, 762, kt) >= 0);
        check("and the turned page", Math.abs(Engine.pageSizes(s)[2] - sz[3]) < 1);
        Engine.close(s);

        System.out.println("a protected document, edited and saved (the cryptography through the app)");
        double[] li = new double[11];
        String lp = Engine.paragraphAt(locked, 0, 100, 700, li);
        check("its paragraph found", lp != null);
        check("and changed", Engine.replaceParagraph(locked, lp + " Still private.", 0, false, false, li[4], (int) li[8]) == null);
        byte[] lb = Engine.bytesToSave(locked);
        check("it is written", lb != null);
        Files.write(new File(w + "/locked-saved.pdf").toPath(), lb);
        Engine.close(locked);
        long l2 = Engine.open(w + "/locked-saved.pdf", "", r);
        check("still protected", l2 == 0 && r[0] == Engine.OPEN_PASSWORD_NEEDED);
        l2 = Engine.open(w + "/locked-saved.pdf", "s3cret", r);
        check("opens with its password, with the change", l2 != 0 && Engine.find(l2, "Still private", false, false).length == 5);
        Engine.close(l2);

        System.out.println(fails == 0 ? "ALL PASSED" : "SOME FAILED");
        System.exit(fails == 0 ? 0 : 1);
    }
}
