package com.featherpdf.android

import android.content.Context
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.DashPathEffect
import android.graphics.Paint
import android.graphics.Path
import android.graphics.Rect
import android.graphics.RectF
import android.os.Handler
import android.os.Looper
import android.view.GestureDetector
import android.view.MotionEvent
import android.view.ScaleGestureDetector
import android.view.View
import android.widget.OverScroller
import com.featherpdf.engine.Engine
import java.nio.ByteBuffer
import java.util.concurrent.Executors
import kotlin.math.max
import kotlin.math.min
import kotlin.math.roundToInt

/**
 * The pages of a document, one under another: scrolled with a finger, zoomed with two (or a double tap). Pages are drawn
 * on a background thread at the width of the screen; zoomed in, the part on screen is drawn again sharp. The tools
 * (selecting text, highlighting, drawing) take one finger; two fingers still move and zoom.
 */
class DocumentView(context: Context) : View(context) {

    enum class Tool { READ, EDIT_TEXT, ADD_TEXT, HIGHLIGHT, DRAW, NOTE, ERASE }

    interface Listener {
        /** A tap on a page: the point in page coordinates (points, origin bottom left). */
        fun onPageTap(page: Int, x: Float, y: Float)
        /** Text was selected (in READ, after a long press) or swept (HIGHLIGHT): from one page point to another. */
        fun onTextSwept(page: Int, x1: Float, y1: Float, x2: Float, y2: Float)
        /** A pen stroke ended: its points in page coordinates. */
        fun onStroke(page: Int, points: FloatArray)
        fun onPageChanged(page: Int, count: Int)
    }

    var listener: Listener? = null
    var tool: Tool = Tool.READ
        set(v) { field = v; selection = null; stroke = null; invalidate() }
    var colours: Int = Engine.COLOURS_NORMAL
        set(v) { if (field != v) { field = v; clearRenders() } }
    var penColor: Int = Color.rgb(0xD3, 0x2F, 0x2F)
    var penWidthPt: Float = 2f
    /** Rectangles to show (search matches): page, left, bottom, right, top for each; `current` is drawn stronger. */
    var marks: FloatArray = FloatArray(0)
        set(v) { field = v; invalidate() }
    var currentMark: Int = -1
        set(v) { field = v; invalidate() }

    private var doc = 0L
    private var sizes = FloatArray(0)          // width, height in points of each page
    private var pageTop = FloatArray(0)        // top of each page at zoom 1 (pixels), and the total height after the last
    private var pageCount = 0
    private var baseScale = 1f                  // pixels a point at zoom 1: the widest page fills the width
    private var zoom = 1f
    private var scrollXf = 0f
    private var scrollYf = 0f
    private var lastReportedPage = -1
    private val gap get() = 10f * resources.displayMetrics.density
    private val margin get() = 6f * resources.displayMetrics.density

    // ---- drawing pages: done on one background thread; results come back to the screen's thread
    private val renderer = Executors.newSingleThreadExecutor()
    private val main = Handler(Looper.getMainLooper())
    private var generation = 0                   // drawings asked for under an older layout or document are dropped
    private val base = LinkedHashMap<Int, Bitmap>(16, 0.75f, true)   // whole pages at zoom 1, least recently used first
    private val pendingBase = HashSet<Int>()
    private class Detail(val page: Int, val zoom: Float, val rect: RectF, val bmp: Bitmap)
    private var details: List<Detail> = emptyList()
    private val detailAsk = Runnable { askDetails() }
    private var buffer: ByteBuffer? = null      // reused by the background thread

    /** Bytes of drawn pages kept ready (Settings > Performance). */
    var budget: Long = min(Runtime.getRuntime().maxMemory() / 5, 96L shl 20)
        set(v) { field = v; trimBase() }

    // ---- tools
    private var selection: FloatArray? = null    // page, x1, y1, x2, y2 (page points) while selecting
    private var selectionRects = FloatArray(0)
    private var selecting = false
    private var stroke: MutableList<Float>? = null   // the stroke being drawn: screen x, y pairs
    private var strokePage = -1
    private var blocks = HashMap<Int, FloatArray>()  // edit mode outlines for each page

    private val pagePaint = Paint().apply { color = Color.WHITE }
    private val shadowPaint = Paint().apply { color = 0x22000000 }
    private val bmpPaint = Paint(Paint.FILTER_BITMAP_FLAG)
    private val markPaint = Paint().apply { color = 0x55FFC107 }
    private val markNowPaint = Paint().apply { color = 0x88FF6F00.toInt() }
    private val selectPaint = Paint().apply { color = 0x553D8BFD }
    private val outlinePaint = Paint().apply {
        style = Paint.Style.STROKE; color = 0xCC1E88E5.toInt()
        strokeWidth = 1.2f * resources.displayMetrics.density
    }
    private val picturePaint = Paint().apply {
        style = Paint.Style.STROKE; color = 0xCCFB8C00.toInt()
        strokeWidth = 1.2f * resources.displayMetrics.density
        pathEffect = DashPathEffect(floatArrayOf(10f, 6f), 0f)
    }
    private val inkPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE; strokeCap = Paint.Cap.ROUND; strokeJoin = Paint.Join.ROUND
    }

    private val scroller = OverScroller(context)

    // ---------------------------------------------------------------------------------------------------------------
    // The document

    fun setDocument(handle: Long, firstPage: Int) {
        doc = handle
        relayout(keepPosition = false)
        post { goToPage(firstPage) }
    }

    /** The document changed (an edit, undo, pages turned or deleted): sizes and drawings are taken again. */
    fun documentChanged() {
        blocks.clear()
        selection = null
        selectionRects = FloatArray(0)
        relayout(keepPosition = true)
        clearRenders()
    }

    val pages: Int get() = pageCount

    /** The page in the middle of the screen. */
    val currentPage: Int
        get() {
            if (pageCount == 0) return 0
            val mid = (scrollYf + height / 2f) / zoom
            for (i in 0 until pageCount) if (mid < pageTop[i + 1]) return i
            return pageCount - 1
        }

    fun goToPage(page: Int) {
        if (pageCount == 0) return
        val p = page.coerceIn(0, pageCount - 1)
        scroller.forceFinished(true)
        scrollYf = pageTop[p] * zoom - gap / 2
        clampScroll()
        changed()
    }

    /** Shows a page rectangle (left, bottom, right, top in points), scrolling so it is in the upper third. */
    fun showRect(page: Int, l: Float, b: Float, r: Float, t: Float) {
        val s = screenRect(page, l, b, r, t) ?: return
        scroller.forceFinished(true)
        scrollYf += s.top - height / 3f
        if (s.left < 0 || s.right > width) scrollXf += s.centerX() - width / 2f
        clampScroll()
        changed()
    }

    private fun relayout(keepPosition: Boolean) {
        val anchorPage = currentPage
        val anchorFrac = if (pageCount > 0 && keepPosition) ((scrollYf / zoom) - pageTop[anchorPage]) else 0f
        sizes = if (doc != 0L) Engine.pageSizes(doc) else FloatArray(0)
        pageCount = sizes.size / 2
        var widest = 1f
        for (i in 0 until pageCount) widest = max(widest, sizes[2 * i])
        val avail = max(1f, width - 2 * margin)
        baseScale = avail / widest
        pageTop = FloatArray(pageCount + 1)
        var y = gap
        for (i in 0 until pageCount) {
            pageTop[i] = y
            y += sizes[2 * i + 1] * baseScale + gap
        }
        pageTop[pageCount] = y
        if (keepPosition && pageCount > 0) {
            val p = anchorPage.coerceIn(0, pageCount - 1)
            scrollYf = (pageTop[p] + anchorFrac) * zoom
        }
        clampScroll()
        invalidate()
    }

    override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
        super.onSizeChanged(w, h, oldw, oldh)
        if (w != oldw) {
            relayout(keepPosition = true)
            clearRenders()
        }
    }

    private fun clearRenders() {
        generation++
        base.clear()
        pendingBase.clear()
        details = emptyList()
        invalidate()
    }

    override fun onDetachedFromWindow() {
        super.onDetachedFromWindow()
        generation++
        renderer.shutdownNow()
    }

    // ---------------------------------------------------------------------------------------------------------------
    // Geometry: page i at the current zoom is pageW(i) x pageH(i) pixels, its top left at (pageX(i), pageTop[i] * zoom)
    // in the content; the screen shows the content from (scrollXf, scrollYf).

    private fun pageW(i: Int) = sizes[2 * i] * baseScale * zoom
    private fun pageH(i: Int) = sizes[2 * i + 1] * baseScale * zoom
    private fun contentW() = max(width.toFloat(), (width - 2 * margin) * zoom + 2 * margin)
    private fun pageX(i: Int) = (contentW() - pageW(i)) / 2f
    private fun contentH() = pageTop[pageCount] * zoom

    private fun clampScroll() {
        scrollXf = scrollXf.coerceIn(0f, max(0f, contentW() - width))
        scrollYf = scrollYf.coerceIn(0f, max(0f, contentH() - height))
    }

    /** The page at a screen point, and the point on it in pixels of the page at the current zoom. */
    private fun hit(sx: Float, sy: Float): Triple<Int, Float, Float>? {
        val cx = sx + scrollXf
        val cy = sy + scrollYf
        for (i in 0 until pageCount) {
            val top = pageTop[i] * zoom
            if (cy < top - gap / 2) return null
            if (cy <= top + pageH(i) + gap / 2) {
                val px = (cx - pageX(i)).coerceIn(0f, pageW(i))
                val py = (cy - top).coerceIn(0f, pageH(i))
                return Triple(i, px, py)
            }
        }
        return null
    }

    private fun toPage(page: Int, px: Float, py: Float): FloatArray? {
        val p = Engine.deviceToPage(doc, page, pageW(page).roundToInt(), pageH(page).roundToInt(), px, py)
        return if (p.size == 2) p else null
    }

    /** A page rectangle (points) on the screen. */
    private fun screenRect(page: Int, l: Float, b: Float, r: Float, t: Float): RectF? {
        if (page !in 0 until pageCount) return null
        val d = Engine.pageToDevice(doc, page, pageW(page).roundToInt(), pageH(page).roundToInt(), floatArrayOf(l, b, r, t))
        if (d.size != 4) return null
        val ox = pageX(page) - scrollXf
        val oy = pageTop[page] * zoom - scrollYf
        return RectF(min(d[0], d[2]) + ox, min(d[1], d[3]) + oy, max(d[0], d[2]) + ox, max(d[1], d[3]) + oy)
    }

    private fun visiblePages(): IntRange {
        if (pageCount == 0) return IntRange.EMPTY
        var first = -1
        var last = -1
        for (i in 0 until pageCount) {
            val top = pageTop[i] * zoom - scrollYf
            val bottom = top + pageH(i)
            if (bottom >= 0 && top <= height) { if (first < 0) first = i; last = i }
        }
        return if (first < 0) IntRange.EMPTY else first..last
    }

    // ---------------------------------------------------------------------------------------------------------------
    // Drawing

    override fun onDraw(canvas: Canvas) {
        canvas.drawColor(if (colours == Engine.COLOURS_NORMAL || colours == Engine.COLOURS_DIMMED) 0xFFCFD8DC.toInt() else 0xFF0B0C0E.toInt())
        if (pageCount == 0) return
        val vis = visiblePages()
        pagePaint.color = when (colours) {
            Engine.COLOURS_NIGHT -> 0xFF1C1C1C.toInt()
            Engine.COLOURS_BLACKBOARD, Engine.COLOURS_BLACKBOARD_ORIGINAL -> Color.BLACK
            Engine.COLOURS_DIMMED -> 0xFFF0E8CD.toInt()
            else -> Color.WHITE
        }
        for (i in vis) {
            val dst = RectF(pageX(i) - scrollXf, pageTop[i] * zoom - scrollYf, 0f, 0f)
            dst.right = dst.left + pageW(i)
            dst.bottom = dst.top + pageH(i)
            canvas.drawRect(dst.left + 2, dst.top + 3, dst.right + 2, dst.bottom + 3, shadowPaint)
            val b = base[i]
            if (b != null) canvas.drawBitmap(b, null, dst, bmpPaint) else canvas.drawRect(dst, pagePaint)
            for (d in details) if (d.page == i && d.zoom == zoom) {
                canvas.drawBitmap(d.bmp, null, RectF(d.rect.left - scrollXf, d.rect.top - scrollYf, d.rect.right - scrollXf, d.rect.bottom - scrollYf), bmpPaint)
            }
            if (tool == Tool.EDIT_TEXT) drawBlocks(canvas, i)
        }
        // search matches
        var k = 0
        while (k + 4 < marks.size) {
            val p = marks[k].toInt()
            if (p in vis) screenRect(p, marks[k + 1], marks[k + 2], marks[k + 3], marks[k + 4])?.let {
                canvas.drawRect(it, if (k / 5 == currentMark) markNowPaint else markPaint)
            }
            k += 5
        }
        // text being selected
        selection?.let { s ->
            val p = s[0].toInt()
            var j = 0
            while (j + 3 < selectionRects.size) {
                screenRect(p, selectionRects[j], selectionRects[j + 1], selectionRects[j + 2], selectionRects[j + 3])?.let { canvas.drawRect(it, selectPaint) }
                j += 4
            }
        }
        // the stroke being drawn
        stroke?.let { st ->
            if (st.size >= 2) {
                val path = Path()
                path.moveTo(st[0], st[1])
                var j = 2
                while (j + 1 < st.size) { path.lineTo(st[j], st[j + 1]); j += 2 }
                inkPaint.color = penColor
                inkPaint.strokeWidth = max(1f, penWidthPt * baseScale * zoom)
                canvas.drawPath(path, inkPaint)
            }
        }
        if (!scroller.isFinished) return
        askBase(vis)
    }

    private fun drawBlocks(canvas: Canvas, page: Int) {
        val b = blocks.getOrPut(page) { Engine.editableBlocks(doc, page) }
        var j = 0
        while (j + 4 < b.size) {
            screenRect(page, b[j + 1], b[j + 2], b[j + 3], b[j + 4])?.let {
                it.inset(-2f, -2f)
                canvas.drawRect(it, if (b[j] == 1f) picturePaint else outlinePaint)
            }
            j += 5
        }
    }

    /** Whole pages at zoom 1 for the pages on screen (and the next ones), dropping the least recently used. */
    private fun askBase(vis: IntRange) {
        if (vis.isEmpty()) return
        val want = (max(0, vis.first - 1)..min(pageCount - 1, vis.last + 1))
        val gen = generation
        for (i in want) {
            if (base.containsKey(i) || i in pendingBase) continue
            pendingBase.add(i)
            val w = (sizes[2 * i] * baseScale).roundToInt().coerceAtLeast(1)
            val h = (sizes[2 * i + 1] * baseScale).roundToInt().coerceAtLeast(1)
            val mode = colours
            val handle = doc
            submit {
                val bmp = draw(handle, i, w, h, 0, 0, w, h, mode)
                main.post {
                    pendingBase.remove(i)
                    if (gen != generation || bmp == null) return@post
                    base[i] = bmp
                    trimBase()
                    invalidate()
                }
            }
        }
    }

    private fun trimBase() {
        var used = base.values.sumOf { it.byteCount.toLong() }
        val it = base.entries.iterator()
        val keep = visiblePages()
        while (used > budget && it.hasNext()) {
            val e = it.next()
            if (e.key in keep) continue
            used -= e.value.byteCount
            it.remove()
        }
    }

    /** Zoomed in, the part of each page on screen is drawn again at the current zoom, once moving stops. */
    private fun askDetails() {
        if (zoom <= 1.01f || pageCount == 0 || doc == 0L) { details = emptyList(); return }
        val gen = generation
        val z = zoom
        val mode = colours
        val handle = doc
        val jobs = ArrayList<Triple<Int, Rect, RectF>>()
        for (i in visiblePages()) {
            val left = pageX(i)
            val top = pageTop[i] * zoom
            val vis = RectF(max(left, scrollXf), max(top, scrollYf), min(left + pageW(i), scrollXf + width), min(top + pageH(i), scrollYf + height))
            if (vis.width() < 1 || vis.height() < 1) continue
            // the part, in pixels of the page at this zoom
            val part = Rect((vis.left - left).toInt(), (vis.top - top).toInt(), (vis.right - left).toInt() + 1, (vis.bottom - top).toInt() + 1)
            jobs.add(Triple(i, part, RectF(left + part.left, top + part.top, left + part.right, top + part.bottom)))
        }
        val sizesNow = jobs.map { Pair(pageW(it.first).roundToInt(), pageH(it.first).roundToInt()) }
        submit {
            val made = ArrayList<Detail>()
            for ((k, job) in jobs.withIndex()) {
                if (gen != generation) return@submit
                val (page, part, where) = job
                val (fw, fh) = sizesNow[k]
                val bmp = draw(handle, page, part.width(), part.height(), -part.left, -part.top, fw, fh, mode) ?: continue
                made.add(Detail(page, z, where, bmp))
            }
            main.post { if (gen == generation && z == zoom) { details = made; invalidate() } }
        }
    }

    private fun submit(job: () -> Unit) {
        runCatching { renderer.execute(job) }
    }

    /** On the background thread: part of a page into a new bitmap. */
    private fun draw(handle: Long, page: Int, w: Int, h: Int, startX: Int, startY: Int, sizeX: Int, sizeY: Int, mode: Int): Bitmap? {
        if (w <= 0 || h <= 0 || w.toLong() * h > 40_000_000L) return null
        val need = w * h * 4
        var b = buffer
        if (b == null || b.capacity() < need) { b = ByteBuffer.allocateDirect(need); buffer = b }
        b!!.clear()
        if (!Engine.render(handle, page, b, w, h, w * 4, startX, startY, sizeX, sizeY, mode)) return null
        b.position(0)
        b.limit(need)
        val bmp = runCatching { Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888) }.getOrNull() ?: return null
        bmp.copyPixelsFromBuffer(b)
        return bmp
    }

    private fun changed() {
        invalidate()
        val p = currentPage
        if (p != lastReportedPage) { lastReportedPage = p; listener?.onPageChanged(p, pageCount) }
        details = details.filter { it.zoom == zoom }
        removeCallbacks(detailAsk)
        postDelayed(detailAsk, 180)
    }

    override fun computeScroll() {
        if (scroller.computeScrollOffset()) {
            scrollXf = scroller.currX.toFloat()
            scrollYf = scroller.currY.toFloat()
            clampScroll()
            changed()
            postInvalidateOnAnimation()
        }
    }

    // ---------------------------------------------------------------------------------------------------------------
    // Touch

    private fun zoomTo(z: Float, fx: Float, fy: Float) {
        val nz = z.coerceIn(1f, 8f)
        if (nz == zoom) return
        val cx = (scrollXf + fx) / contentW()
        val cy = (scrollYf + fy) / zoom
        zoom = nz
        scrollXf = cx * contentW() - fx
        scrollYf = cy * zoom - fy
        clampScroll()
        changed()
    }

    private val gestures = GestureDetector(context, object : GestureDetector.SimpleOnGestureListener() {
        override fun onDown(e: MotionEvent): Boolean { scroller.forceFinished(true); return true }
        override fun onScroll(e1: MotionEvent?, e2: MotionEvent, dx: Float, dy: Float): Boolean {
            scrollXf += dx; scrollYf += dy
            clampScroll(); changed()
            return true
        }
        override fun onFling(e1: MotionEvent?, e2: MotionEvent, vx: Float, vy: Float): Boolean {
            scroller.fling(scrollXf.toInt(), scrollYf.toInt(), -vx.toInt(), -vy.toInt(), 0, max(0f, contentW() - width).toInt(), 0, max(0f, contentH() - height).toInt())
            postInvalidateOnAnimation()
            return true
        }
        override fun onDoubleTap(e: MotionEvent): Boolean {
            zoomTo(if (zoom > 1.2f) 1f else 2.5f, e.x, e.y)
            return true
        }
        override fun onSingleTapConfirmed(e: MotionEvent): Boolean {
            val h = hit(e.x, e.y) ?: return false
            val p = toPage(h.first, h.second, h.third) ?: return false
            listener?.onPageTap(h.first, p[0], p[1])
            return true
        }
        override fun onLongPress(e: MotionEvent) {
            if (tool != Tool.READ) return
            startSelection(e.x, e.y)
        }
    })

    private var lastFocusX = 0f
    private var lastFocusY = 0f
    private val scaler = ScaleGestureDetector(context, object : ScaleGestureDetector.SimpleOnScaleGestureListener() {
        override fun onScaleBegin(d: ScaleGestureDetector): Boolean {
            lastFocusX = d.focusX; lastFocusY = d.focusY
            cancelTool()
            return true
        }
        override fun onScale(d: ScaleGestureDetector): Boolean {
            // two fingers also move the pages
            scrollXf -= d.focusX - lastFocusX
            scrollYf -= d.focusY - lastFocusY
            lastFocusX = d.focusX; lastFocusY = d.focusY
            zoomTo(zoom * d.scaleFactor, d.focusX, d.focusY)
            clampScroll(); changed()
            return true
        }
    })

    private fun startSelection(sx: Float, sy: Float) {
        val h = hit(sx, sy) ?: return
        val p = toPage(h.first, h.second, h.third) ?: return
        selection = floatArrayOf(h.first.toFloat(), p[0], p[1], p[0], p[1])
        selecting = true
        updateSelection()
        performHapticFeedback(android.view.HapticFeedbackConstants.LONG_PRESS)
    }

    private fun moveSelection(sx: Float, sy: Float) {
        val s = selection ?: return
        val page = s[0].toInt()
        // the point on the page being selected, even when the finger is past its edge
        val px = (sx + scrollXf - pageX(page)).coerceIn(0f, pageW(page))
        val py = (sy + scrollYf - pageTop[page] * zoom).coerceIn(0f, pageH(page))
        val p = toPage(page, px, py) ?: return
        s[3] = p[0]; s[4] = p[1]
        updateSelection()
    }

    private fun updateSelection() {
        val s = selection ?: return
        selectionRects = Engine.selectionRects(doc, s[0].toInt(), s[1], s[2], s[3], s[4])
        invalidate()
    }

    private fun endSelection() {
        val s = selection ?: return
        selecting = false
        if (selectionRects.isNotEmpty()) listener?.onTextSwept(s[0].toInt(), s[1], s[2], s[3], s[4])
        if (tool == Tool.HIGHLIGHT) clearSelection()
    }

    fun clearSelection() {
        selection = null
        selectionRects = FloatArray(0)
        invalidate()
    }

    private fun cancelTool() {
        if (stroke != null) { stroke = null; invalidate() }
        if (selecting && tool == Tool.HIGHLIGHT) clearSelection()
        selecting = false
    }

    override fun onTouchEvent(e: MotionEvent): Boolean {
        scaler.onTouchEvent(e)
        if (e.pointerCount > 1 || scaler.isInProgress) {
            cancelTool()
            return true
        }
        val oneFingerTool = tool == Tool.DRAW || tool == Tool.HIGHLIGHT
        if (oneFingerTool || selecting) {
            when (e.actionMasked) {
                MotionEvent.ACTION_DOWN -> {
                    if (tool == Tool.DRAW) {
                        val h = hit(e.x, e.y)
                        if (h != null) { strokePage = h.first; stroke = mutableListOf(e.x, e.y); invalidate() }
                    } else if (tool == Tool.HIGHLIGHT) startSelection(e.x, e.y)
                }
                MotionEvent.ACTION_MOVE -> {
                    if (stroke != null) {
                        for (k in 0 until e.historySize) stroke!!.addAll(listOf(e.getHistoricalX(k), e.getHistoricalY(k)))
                        stroke!!.addAll(listOf(e.x, e.y))
                        invalidate()
                    } else if (selecting) moveSelection(e.x, e.y)
                }
                MotionEvent.ACTION_UP -> {
                    if (stroke != null) finishStroke() else if (selecting) endSelection()
                }
                MotionEvent.ACTION_CANCEL -> cancelTool()
            }
            if (selecting || stroke != null || oneFingerTool) {
                // a tap with a tool still reaches the page (a note, a tap on an annotation)
                if (!oneFingerTool) gestures.onTouchEvent(e)
                return true
            }
        }
        if (selection != null && e.actionMasked == MotionEvent.ACTION_DOWN && tool == Tool.READ) clearSelection()
        return gestures.onTouchEvent(e) || super.onTouchEvent(e)
    }

    private fun finishStroke() {
        val st = stroke ?: return
        stroke = null
        val page = strokePage
        if (page < 0 || st.size < 2) { invalidate(); return }
        val pts = ArrayList<Float>()
        var j = 0
        val left = pageX(page)
        val top = pageTop[page] * zoom
        while (j + 1 < st.size) {
            val px = (st[j] + scrollXf - left).coerceIn(0f, pageW(page))
            val py = (st[j + 1] + scrollYf - top).coerceIn(0f, pageH(page))
            toPage(page, px, py)?.let { pts.add(it[0]); pts.add(it[1]) }
            j += 2
        }
        invalidate()
        if (pts.size >= 2) listener?.onStroke(page, pts.toFloatArray())
    }

    /** The whole view as a picture (for the build check). */
    fun snapshot(): Bitmap {
        val b = Bitmap.createBitmap(max(1, width), max(1, height), Bitmap.Config.ARGB_8888)
        draw(Canvas(b))
        return b
    }

    /** Whether every page on screen is drawn (for the build check). */
    val settled: Boolean get() = pageCount > 0 && visiblePages().all { base.containsKey(it) } && (zoom <= 1.01f || details.isNotEmpty())

    fun pageCenterOnScreen(page: Int): Pair<Float, Float> = Pair(pageX(page) + pageW(page) / 2 - scrollXf, pageTop[page] * zoom + pageH(page) / 2 - scrollYf)

    /** Where match k of `marks` is on the screen (for the build check). */
    fun markOnScreen(k: Int): RectF? {
        val j = k * 5
        if (j + 4 >= marks.size) return null
        return screenRect(marks[j].toInt(), marks[j + 1], marks[j + 2], marks[j + 3], marks[j + 4])
    }

    /** A page point on the screen (for the build check, which taps where the text is). */
    fun screenPoint(page: Int, x: Float, y: Float): Pair<Float, Float>? {
        val r = screenRect(page, x, y, x, y) ?: return null
        return Pair(r.left, r.top)
    }

    init {
        isFocusable = true
        isClickable = true
    }
}
