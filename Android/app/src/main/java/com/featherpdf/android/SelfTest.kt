package com.featherpdf.android

import android.graphics.Bitmap
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.view.MotionEvent
import com.featherpdf.engine.Engine
import java.io.File
import java.io.FileOutputStream

/**
 * The build check on an emulator (.github/workflows/android.yml): opens a document from the app's own folder and goes
 * through the screen as a person would (edit mode, a paragraph changed, a highlight, a note, a pen stroke drawn with
 * touches, finding, night colours, undo and redo, saving), writing report.txt and pictures of the screen into `out`.
 * Started only by ViewerActivity's EXTRA_PATH with a file inside the app's own folder.
 */
class SelfTest(private val a: ViewerActivity, private val out: File) {
    private val main = Handler(Looper.getMainLooper())
    private val lines = ArrayList<String>()
    private val steps = ArrayList<Pair<String, () -> Unit>>()
    private var fails = 0

    private fun check(what: String, ok: Boolean) {
        lines.add((if (ok) "ok   " else "FAIL ") + what)
        if (!ok) fails++
    }

    private fun shot(name: String) {
        runCatching {
            val b = a.screenshot()
            FileOutputStream(File(out, "$name.png")).use { b.compress(Bitmap.CompressFormat.PNG, 100, it) }
            b.recycle()
        }.onFailure { lines.add("note picture $name: ${it.message}") }
    }

    private fun intExtra(name: String, default: List<Float>): List<Float> =
        a.intent.getStringExtra(name)?.split(' ')?.mapNotNull { it.toFloatOrNull() }?.takeIf { it.size == default.size } ?: default

    private fun step(name: String, action: () -> Unit) { steps.add(name to action) }

    /** Runs the steps one after another, each once the pages on screen are drawn (or after a few seconds). */
    private fun next(i: Int, waited: Long = 0) {
        if (i >= steps.size) { finish(); return }
        if (!a.documentView.settled && waited < 8000) { main.postDelayed({ next(i, waited + 200) }, 200); return }
        val (name, action) = steps[i]
        runCatching(action).onFailure { check("$name (crashed: $it)", false) }
        main.postDelayed({ next(i + 1) }, 900)
    }

    fun start() {
        out.mkdirs()
        File(out, "report.txt").delete()
        val doc = a.documentHandle
        val at = intExtra("com.featherpdf.selftest.at", listOf(0f, 100f, 700f))          // page, x, y of a paragraph
        val sweep = intExtra("com.featherpdf.selftest.sweep", listOf(0f, 72f, 760f, 300f, 760f))
        val append = a.intent.getStringExtra(ViewerActivity.EXTRA_EDIT_APPEND) ?: "Edited on Android."

        step("open") {
            check("the document opened with ${Engine.pageCount(doc)} pages", Engine.pageCount(doc) > 0)
            check("its pages are drawn", a.documentView.settled)
            check("the title bar shows the file's name (${a.titleShown})", a.titleShown.endsWith("in.pdf"))
            shot("01-open")
        }
        // Nothing under the system bars (Android 15 draws apps under them): the title bar below the status bar, the page
        // below the title bar and the tools, and above the navigation bar; then with the tools at the bottom.
        fun bars(): Pair<Int, Int> {
            val ins = a.window.decorView.rootWindowInsets ?: return Pair(0, 0)
            return if (android.os.Build.VERSION.SDK_INT >= 30) {
                val i = ins.getInsets(android.view.WindowInsets.Type.systemBars())
                Pair(i.top, i.bottom)
            } else @Suppress("DEPRECATION") Pair(ins.systemWindowInsetTop, ins.systemWindowInsetBottom)
        }
        fun top(v: android.view.View) = IntArray(2).also { v.getLocationOnScreen(it) }[1]
        fun bottom(v: android.view.View) = top(v) + v.height
        step("clear of the system bars") {
            val (statusBar, navBar) = bars()
            val screen = a.window.decorView.height
            val title = (a.headerView as android.view.ViewGroup).getChildAt(0)
            lines.add("note status bar $statusBar, navigation bar $navBar, screen $screen; title ${top(title)}-${bottom(title)}, " +
                      "page ${top(a.documentView)}-${bottom(a.documentView)}, tools ${top(a.toolsView)}-${bottom(a.toolsView)}")
            check("the system reports its bars (status bar $statusBar px)", statusBar > 0)
            check("the title bar is below the status bar", top(title) + title.paddingTop >= statusBar && title.height > title.paddingTop)
            check("the tools are at the top, under the title bar", top(a.toolsView) >= bottom(title) && bottom(a.toolsView) <= top(a.documentView))
            check("the page starts below the title bar and the tools", top(a.documentView) >= bottom(a.headerView))
            check("the page ends above the navigation bar", bottom(a.documentView) <= screen - navBar)
            a.placeTools(false)
        }
        step("tools at the bottom") {
            val (_, navBar) = bars()
            val screen = a.window.decorView.height
            check("tools at the bottom: above the navigation bar", bottom(a.toolsView) - a.toolsView.paddingBottom <= screen - navBar && top(a.toolsView) >= bottom(a.documentView))
            check("tools at the bottom: the page still starts below the title bar", top(a.documentView) >= bottom(a.headerView))
            shot("07-tools-at-bottom")
            a.placeTools(true)
        }
        step("edit mode") {
            a.chooseTool(DocumentView.Tool.EDIT_TEXT)
            check("edit mode outlines the text", Engine.editableBlocks(doc, 0).isNotEmpty())
        }
        step("edit mode picture") { shot("02-edit-mode") }
        step("edit a paragraph") {
            var why: String? = "not run"
            a.editParagraph(at[0].toInt(), at[1], at[2]) { why = it }
            check("a paragraph is changed ($why)", why == null)
            check("the new words are in the document", Engine.find(doc, append, false, false).isNotEmpty())
        }
        step("edited picture") { shot("03-edited") }
        step("highlight") {
            val r = Engine.selectionRects(doc, sweep[0].toInt(), sweep[1], sweep[2], sweep[3], sweep[4])
            check("text is selected for a highlight", r.isNotEmpty())
            check("the highlight is added", Engine.addMarkup(doc, sweep[0].toInt(), Engine.MARKUP_HIGHLIGHT, r, 0xFFFFEB3B.toInt()))
            check("a note is added", Engine.addNote(doc, 0, 400f, 300f, "From the build check", 0xFFFFC107.toInt()))
            a.changedDocument()
        }
        step("draw with touches") {
            a.chooseTool(DocumentView.Tool.DRAW)
            val v = a.documentView
            val (cx, cy) = v.pageCenterOnScreen(v.currentPage)
            val before = countInk(doc, v.currentPage)
            val t0 = SystemClock.uptimeMillis()
            fun ev(action: Int, x: Float, y: Float, t: Long) = MotionEvent.obtain(t0, t, action, x, y, 0).also { v.dispatchTouchEvent(it); it.recycle() }
            ev(MotionEvent.ACTION_DOWN, cx - 120, cy, t0)
            for (k in 1..12) ev(MotionEvent.ACTION_MOVE, cx - 120 + k * 20f, cy + if (k % 2 == 0) 30f else -30f, t0 + k * 16L)
            ev(MotionEvent.ACTION_UP, cx + 120, cy, t0 + 220)
            check("a stroke drawn with a finger becomes a drawing on the page", countInk(doc, v.currentPage) > before)
            a.chooseTool(DocumentView.Tool.READ)
        }
        step("annotated picture") { shot("04-annotated") }
        step("find") {
            a.runSearch(append.split(' ').first())
            val m = a.documentView.marks
            check("finding shows the matches", m.isNotEmpty())
            val r = a.documentView.markOnScreen(0)
            check("the first match is scrolled into view", r != null && r.top >= 0 && r.bottom <= a.documentView.height && r.width() > 2)
        }
        step("found picture") { shot("05-found") }
        step("night colours") { a.documentView.colours = Engine.COLOURS_NIGHT }
        step("night picture") {
            shot("06-night")
            a.documentView.colours = Engine.COLOURS_NORMAL
        }
        step("undo and redo") {
            check("undo", Engine.undo(doc))
            check("redo", Engine.redo(doc))
            a.changedDocument()
        }
        step("save") {
            val bytes = Engine.bytesToSave(doc)
            check("the document is written", bytes != null && bytes.size > 500)
            bytes?.let { File(out, "saved.pdf").writeBytes(it); Engine.markSaved(doc) }
            check("then it is not modified", !Engine.isModified(doc))
        }
        main.postDelayed({ next(0) }, 600)
    }

    /** How many drawings a page has: the annotations found on a grid of points that are ink. */
    private fun countInk(doc: Long, page: Int): Int {
        val kind = arrayOfNulls<String>(2)
        val seen = HashSet<Int>()
        val size = Engine.pageSizes(doc)
        val w = size[2 * page]
        val h = size[2 * page + 1]
        var y = 0f
        while (y < h) {
            var x = 0f
            while (x < w) {
                val i = Engine.annotAt(doc, page, x, y, kind)
                if (i >= 0 && kind[0] == "drawing") seen.add(i)
                x += 10f
            }
            y += 10f
        }
        return seen.size
    }

    private fun finish() {
        lines.add(if (fails == 0) "ALL PASSED" else "SOME FAILED")
        File(out, "report.txt").writeText(lines.joinToString("\n") + "\nDONE\n")
        a.finish()
    }
}
