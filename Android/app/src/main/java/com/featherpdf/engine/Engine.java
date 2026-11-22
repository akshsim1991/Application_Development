package com.featherpdf.engine;

import java.nio.ByteBuffer;

/**
 * The shared Feather PDF engine (the same C++ code as the Windows and Mac apps), through bridge.cpp.
 * Plain Java without Android classes, so it is also tested on a desktop Java (tests/android/bridge_check.sh).
 * Coordinates on a page are PDF points with the origin at the bottom left. Every call is safe from any thread:
 * the bridge lets one in at a time.
 */
public final class Engine {
    private Engine() {}

    /** What the engine asks of the system: Android's cryptography and picture classes (or a desktop Java's). */
    public interface SystemHooks {
        /** SHA-256, -384 or -512 (alg = 256, 384, 512) of data. */
        byte[] hash(int alg, byte[] data);
        /** AES without padding, CBC with iv or ECB when iv is null; in is a multiple of 16 bytes. */
        byte[] aes(boolean cbc, byte[] key, byte[] iv, byte[] in);
        byte[] random(int n);
        /** PNG or JPEG of w x h BGRA pixels. */
        byte[] encodeImage(byte[] bgra, int w, int h, boolean jpeg, int quality, boolean alpha);
        /** A picture file as 4 bytes width, 4 bytes height (big-endian), then BGRA pixels; null if it is not one. */
        byte[] decodeImage(String path);
    }

    /** Outcomes of open(). */
    public static final int OPEN_OK = 0, OPEN_PASSWORD_NEEDED = 4, OPEN_WRONG_PASSWORD = 5;
    /** Page colours for render(). */
    public static final int COLOURS_NORMAL = 0, COLOURS_NIGHT = 1, COLOURS_DIMMED = 2, COLOURS_BLACKBOARD = 3, COLOURS_BLACKBOARD_ORIGINAL = 4;
    /** Kinds for addMarkup(). */
    public static final int MARKUP_HIGHLIGHT = 0, MARKUP_UNDERLINE = 1, MARKUP_STRIKEOUT = 2;

    private static boolean loaded;

    /** Loads the native library and PDFium. Returns null, or why it could not start. */
    public static synchronized String start(SystemHooks hooks) {
        if (!loaded) {
            try {
                System.loadLibrary("featherpdf");
            } catch (UnsatisfiedLinkError e) {
                return "The Feather PDF engine could not be loaded: " + e.getMessage();
            }
            loaded = true;
        }
        return init(hooks);
    }

    static native String init(SystemHooks hooks);

    public static native long open(String path, String password, int[] result);
    public static native String describe(int result, String name);
    public static native void close(long doc);

    public static native int pageCount(long doc);
    /** Width, height (points) of each page in turn. */
    public static native float[] pageSizes(long doc);
    public static native boolean render(long doc, int page, ByteBuffer rgba, int width, int height, int stride,
                                        int startX, int startY, int sizeX, int sizeY, int colourMode);
    public static native float[] pageToDevice(long doc, int page, int sizeX, int sizeY, float[] points);
    public static native float[] deviceToPage(long doc, int page, int sizeX, int sizeY, float x, float y);

    /** Left, bottom, right, top of each line of the text between two points. */
    public static native float[] selectionRects(long doc, int page, float x1, float y1, float x2, float y2);
    public static native String selectionText(long doc, int page, float x1, float y1, float x2, float y2);

    /** Kind (0 text, 1 picture), left, bottom, right, top of each block edit mode outlines. */
    public static native float[] editableBlocks(long doc, int page);
    /**
     * The paragraph at a point (null if none), kept for replaceParagraph(). info (11 values): left, top, width, height,
     * size, bold, italic, family (0 sans, 1 serif, 2 mono), colour (ARGB), lines, alignment.
     */
    public static native String paragraphAt(long doc, int page, float x, float y, double[] info);
    /** Null when done, else why not. */
    public static native String replaceParagraph(long doc, String text, int family, boolean bold, boolean italic, double size, int color);

    /** The equation at a point (null if none; with any, a line of text read as one), kept for replaceEquation(). info (6): size, left, bottom, right, top, 1 when it may be read wrongly (two lines). */
    public static native String equationAt(long doc, int page, float x, float y, boolean any, double[] info);
    /** Sets the equation found last again (empty: removes it). Null when done, else why not. */
    public static native String replaceEquation(long doc, String source, double size);
    /** A new equation starting at (x, y) on its baseline. Null when done, else why not. */
    public static native String insertEquation(long doc, int page, double x, double y, String source, double size, int color);
    /** The equation drawn for a preview: ARGB pixels, dims gets width and height; null when the notation is wrong (error[0] says why). */
    public static native int[] equationPreview(String source, double size, double scale, int[] dims, String[] error);
    /** x² and x₂: the text as superscripts (kind 1) or subscripts (-1), or plain again when it already is. */
    public static native String scriptText(String text, int kind);
    public static native boolean addTextBox(long doc, int page, double x, double y, String text, double size, int color);

    public static native boolean addMarkup(long doc, int page, int kind, float[] rects, int color);
    public static native boolean addInk(long doc, int page, float[][] strokes, int color, float width);
    public static native boolean addNote(long doc, int page, float x, float y, String text, int color);
    /** The annotation at a point, or -1; kindAndText[0] gets its kind, [1] its comment. */
    public static native int annotAt(long doc, int page, float x, float y, String[] kindAndText);
    public static native boolean removeAnnot(long doc, int page, int index);
    public static native boolean setAnnotText(long doc, int page, int index, String text);

    /** Page, left, bottom, right, top of each match. */
    public static native float[] find(long doc, String query, boolean matchCase, boolean wholeWord);
    /** "depth TAB page TAB title" for each bookmark, depth first. */
    public static native String[] bookmarks(long doc);

    public static native boolean rotatePages(long doc, int[] pages, int quarterTurns);
    public static native boolean deletePages(long doc, int[] pages);
    public static native boolean undo(long doc);
    public static native boolean redo(long doc);
    public static native boolean canUndo(long doc);
    public static native boolean canRedo(long doc);
    public static native boolean isModified(long doc);
    /** Who made Feather PDF, its history, credits and how to write to the developer (shared with the desktop apps). */
    public static native String aboutCredits();
    public static native String feedbackAddress();
    public static native byte[] bytesToSave(long doc);
    public static native void markSaved(long doc);
}
