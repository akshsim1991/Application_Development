package com.featherpdf.android

import android.app.Activity
import android.app.AlertDialog
import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.content.res.Configuration
import android.graphics.Bitmap
import android.graphics.Color
import android.graphics.Typeface
import android.graphics.drawable.GradientDrawable
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.provider.OpenableColumns
import android.text.InputType
import android.view.Gravity
import android.view.KeyEvent
import android.view.Menu
import android.view.MenuItem
import android.view.View
import android.view.ViewGroup
import android.view.inputmethod.EditorInfo
import android.view.inputmethod.InputMethodManager
import android.widget.Button
import android.widget.CheckBox
import android.widget.EditText
import android.widget.FrameLayout
import android.widget.HorizontalScrollView
import android.widget.ImageButton
import android.widget.ImageView
import android.text.Editable
import android.text.TextWatcher
import android.widget.LinearLayout
import android.widget.ProgressBar
import android.widget.TextView
import android.widget.Toast
import com.featherpdf.engine.Engine
import java.io.File
import java.io.FileOutputStream
import java.security.MessageDigest
import java.util.concurrent.Executors

/**
 * The open documents, each in a tab (as in Feather PDF for Windows and macOS): reading, finding, editing text,
 * annotating, page tools, saving. A sidebar shows the pages as thumbnails and the bookmarks; a tap on the page shows
 * the page alone, full screen (another tap brings the bars back).
 */
class ViewerActivity : Activity(), DocumentView.Listener {

    /** A document open in a tab; the one shown is also in doc, uri, name and localPath below. */
    private class Tab(var doc: Long, var uri: Uri?, var name: String, var localPath: String?, var page: Int = 0, var view: FloatArray? = null)
    private val tabs = ArrayList<Tab>()
    private var current = -1

    private var doc = 0L
    private var uri: Uri? = null                // where the document came from (null: opened from a path)
    private var name = "Document"
    private var localPath: String? = null       // the copy the engine reads
    private lateinit var view: DocumentView
    private lateinit var pageLabel: TextView
    private lateinit var toolRow: LinearLayout
    private lateinit var searchRow: LinearLayout
    private lateinit var searchField: EditText
    private lateinit var searchCount: TextView
    private lateinit var busy: ProgressBar
    private val toolButtons = HashMap<DocumentView.Tool, Button>()
    private val io = Executors.newSingleThreadExecutor()
    private val main = Handler(Looper.getMainLooper())
    private var hits = FloatArray(0)
    private var hitIndex = -1
    private var selfTest: SelfTest? = null

    private val density get() = resources.displayMetrics.density
    private fun dp(v: Int) = (v * density).toInt()

    // ---------------------------------------------------------------------------------------------------------------
    // Opening

    private var themeAtStart = -1

    override fun onCreate(savedInstanceState: Bundle?) {
        Settings.applyTheme(this)
        themeAtStart = Settings.theme(this)
        super.onCreate(savedInstanceState)
        buildScreen()
        FeatherApp.startError?.let { fail(it); return }
        val testPath = intent.getStringExtra(EXTRA_PATH)
        val ownFiles = getExternalFilesDir(null)?.canonicalPath
        if (testPath != null && ownFiles != null && File(testPath).canonicalPath.startsWith(ownFiles)) {
            // the build check: a file in the app's own folder, and a script of steps (SelfTest)
            name = File(testPath).name
            localPath = testPath
            selfTest = SelfTest(this, File(intent.getStringExtra(EXTRA_SELFTEST) ?: File(ownFiles, "selftest").path))
            openLocal("")
            return
        }
        val u = incomingUri(intent) ?: run { fail("There is no document to open."); return }
        openUri(u)
        Updater.checkOnOpen(this)
    }

    /** Another document opened (from the start screen, a file app, a share): in a tab of its own, or its tab if open. */
    override fun onNewIntent(i: Intent) {
        super.onNewIntent(i)
        if (selfTest != null) return
        incomingUri(i)?.let { openUri(it) }
    }

    /** Opens `u` in a new tab (its copy read in the background); a document already open shows its tab. */
    private fun openUri(u: Uri) {
        if (opening) { if (u !in waiting) waiting.add(u); return }   // one at a time (several chosen at once)
        tabs.indexOfFirst { it.uri == u }.takeIf { it >= 0 }?.let { switchTo(it); openNext(); return }
        opening = true
        stashTab()
        uri = u
        name = displayName(u)
        localPath = null
        doc = 0L
        title = name
        busy.visibility = View.VISIBLE
        io.execute {
            val copy = runCatching { copyToCache(u) }
            main.post {
                busy.visibility = View.GONE
                copy.onSuccess { localPath = it; openLocal("") }
                    .onFailure { fail("The document could not be read: ${it.message ?: it.javaClass.simpleName}.\n\nOpen it again from its folder.") }
            }
        }
    }

    private fun incomingUri(i: Intent): Uri? = when (i.action) {
        Intent.ACTION_SEND -> if (Build.VERSION.SDK_INT >= 33) i.getParcelableExtra(Intent.EXTRA_STREAM, Uri::class.java)
                              else @Suppress("DEPRECATION") i.getParcelableExtra(Intent.EXTRA_STREAM)
        else -> i.data
    }

    private fun displayName(u: Uri): String {
        runCatching {
            contentResolver.query(u, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null)?.use { c ->
                if (c.moveToFirst()) c.getString(0)?.let { return it }
            }
        }
        return u.lastPathSegment?.substringAfterLast('/')?.takeIf { it.isNotBlank() } ?: "Document.pdf"
    }

    /** The document as a file the engine can read: a copy in the app's cache, one for each address. */
    private fun copyToCache(u: Uri): String {
        val dir = File(cacheDir, "open").apply { mkdirs() }
        val key = MessageDigest.getInstance("SHA-256").digest(u.toString().toByteArray()).take(8).joinToString("") { "%02x".format(it) }
        val f = File(dir, "$key.pdf")
        contentResolver.openInputStream(u)?.use { input -> FileOutputStream(f).use { input.copyTo(it) } }
            ?: throw IllegalStateException("no access")
        return f.path
    }

    private fun openLocal(password: String) {
        val path = localPath ?: return
        val result = IntArray(1)
        val h = Engine.open(path, password, result)
        if (h == 0L) {
            when (result[0]) {
                Engine.OPEN_PASSWORD_NEEDED, Engine.OPEN_WRONG_PASSWORD -> askPassword(result[0] == Engine.OPEN_WRONG_PASSWORD)
                else -> fail(Engine.describe(result[0], name))
            }
            return
        }
        doc = h
        title = name
        uri?.let { Recents.add(this, it.toString(), name) }
        val first = if (Settings.restorePositions(this)) uri?.let { Recents.pageOf(this, it.toString()) } ?: 0 else 0
        tabs.add(Tab(doc, uri, name, localPath, first))
        current = tabs.size - 1
        closeSearch()
        view.setDocument(doc, first)
        refreshTabs()
        sidebarDocumentChanged()
        updateMenus()
        if (tabs.size == 1) selfTest?.start()   // (the build check runs once, on its first document)
        openNext()
    }

    private fun askPassword(wrong: Boolean) {
        val field = EditText(this).apply {
            inputType = InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_VARIATION_PASSWORD
            hint = "Password"
        }
        val box = padded(field)
        AlertDialog.Builder(this)
            .setTitle(if (wrong) "Wrong password" else "This document is protected")
            .setMessage(if (wrong) "Try again." else "Type its password to open it.")
            .setView(box)
            .setPositiveButton("Open") { _, _ -> openLocal(field.text.toString()) }
            .setNegativeButton("Cancel") { _, _ -> if (tabs.isEmpty()) finish() else { restoreTab(); openNext() } }
            .setOnCancelListener { if (tabs.isEmpty()) finish() else { restoreTab(); openNext() } }
            .show()
        field.requestFocus()
    }

    /** A document could not be opened: the window closes when it was the only one; otherwise its tabs stay. */
    private fun fail(message: String) {
        if (tabs.isNotEmpty()) {
            restoreTab()
            openNext()
            AlertDialog.Builder(this).setTitle("Feather PDF").setMessage(message).setPositiveButton("OK", null).show()
            return
        }
        AlertDialog.Builder(this).setTitle("Feather PDF").setMessage(message)
            .setPositiveButton("Close") { _, _ -> finish() }.setOnCancelListener { finish() }.show()
    }

    // ---------------------------------------------------------------------------------------------------------------
    // Tabs

    private var opening = false                 // a document is being opened (read, or its password asked)
    private val waiting = ArrayList<Uri>()       // the next ones to open

    /** The document being opened is done with (open, refused or failed): the next one chosen. */
    private fun openNext() {
        opening = false
        if (waiting.isNotEmpty()) openUri(waiting.removeAt(0))
    }

    /** The shown tab's state kept in it (before another is opened or shown). */
    private fun stashTab() {
        if (doc == 0L) return   // (another document is being opened: the tab keeps what it has)
        val t = tabs.getOrNull(current) ?: return
        t.doc = doc; t.uri = uri; t.name = name; t.localPath = localPath
        if (doc != 0L) { t.page = view.currentPage; t.view = view.viewState() }
    }

    /** Back to the shown tab's document (after another could not be opened). */
    private fun restoreTab() {
        val t = tabs.getOrNull(current) ?: return
        doc = t.doc; uri = t.uri; name = t.name; localPath = t.localPath
        title = name
        updateMenus()
    }

    fun switchTo(i: Int) {
        if (i !in tabs.indices) return
        if (i == current) return
        stashTab()
        current = i
        val t = tabs[i]
        doc = t.doc; uri = t.uri; name = t.name; localPath = t.localPath
        closeSearch()
        view.setDocument(doc, t.page)
        t.view?.let { s -> view.post { view.restoreViewState(s) } }
        refreshTabs()
        sidebarDocumentChanged()
        updateMenus()
    }

    /** Closes tab `i`, asking first about unsaved changes; the last one closes the window. */
    fun closeTab(i: Int) {
        if (i !in tabs.indices) return
        stashTab()
        val t = tabs[i]
        if (t.doc != 0L && Engine.isModified(t.doc) && selfTest == null) {
            if (i != current) switchTo(i)
            AlertDialog.Builder(this).setTitle("Save the changes to ${t.name}?")
                .setPositiveButton("Save") { _, _ -> save(null) { dropTab(tabs.indexOf(t)) } }
                .setNegativeButton("Don\u2019t save") { _, _ -> dropTab(tabs.indexOf(t)) }
                .setNeutralButton("Cancel", null).show()
            return
        }
        dropTab(i)
    }

    private fun dropTab(i: Int) {
        if (i !in tabs.indices) return
        val t = tabs.removeAt(i)
        t.uri?.let { Recents.setPage(this, it.toString(), if (i == current) view.currentPage else t.page) }
        if (t.doc != 0L) Engine.close(t.doc)
        if (tabs.isEmpty()) { doc = 0L; current = -1; finish(); return }
        val next = if (i < current) current - 1 else minOf(i, tabs.size - 1).coerceAtLeast(0)
        current = -1   // (the closed tab's state is not kept)
        val n = tabs[next]
        current = next
        doc = n.doc; uri = n.uri; name = n.name; localPath = n.localPath
        closeSearch()
        view.setDocument(doc, n.page)
        n.view?.let { s -> view.post { view.restoreViewState(s) } }
        refreshTabs()
        sidebarDocumentChanged()
        updateMenus()
    }

    /** The tab strip: each document's name (a dot while it has unsaved changes), × to close it, and + to open another. */
    private fun refreshTabs() {
        if (!::tabStrip.isInitialized) return
        tabStrip.removeAllViews()
        val text = Chrome.textColour(this)
        tabs.forEachIndexed { i, t ->
            val selected = i == current
            val modified = (if (selected) doc else t.doc).let { it != 0L && Engine.isModified(it) }
            val cell = LinearLayout(this).apply {
                orientation = LinearLayout.HORIZONTAL
                gravity = Gravity.CENTER_VERTICAL
                setPadding(dp(12), 0, dp(4), 0)
                background = GradientDrawable().apply {
                    setColor(if (selected) Chrome.bottomColour(this@ViewerActivity) else 0x00000000)
                    cornerRadii = floatArrayOf(dp(8).toFloat(), dp(8).toFloat(), dp(8).toFloat(), dp(8).toFloat(), 0f, 0f, 0f, 0f)
                }
                setOnClickListener { switchTo(i) }
                setOnLongClickListener { closeTab(i); true }
                contentDescription = "Tab ${i + 1}: ${t.name}"
            }
            cell.addView(TextView(this).apply {
                this.text = (if (modified) "\u2022 " else "") + (if (selected) name else t.name)
                isSingleLine = true
                ellipsize = android.text.TextUtils.TruncateAt.MIDDLE
                maxWidth = dp(180)
                textSize = 14f
                setTextColor(if (selected) text else Color.WHITE)
                typeface = if (selected) Typeface.DEFAULT_BOLD else Typeface.DEFAULT
            })
            cell.addView(TextView(this).apply {
                this.text = "\u00D7"
                textSize = 18f
                setPadding(dp(8), 0, dp(6), 0)
                setTextColor(if (selected) text else Color.WHITE)
                contentDescription = "Close ${t.name}"
                setOnClickListener { closeTab(i) }
            })
            tabStrip.addView(cell, LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, dp(40)).apply { setMargins(dp(2), dp(4), dp(2), 0) })
        }
        tabStrip.addView(TextView(this).apply {
            this.text = "+"
            textSize = 22f
            gravity = Gravity.CENTER
            setTextColor(Color.WHITE)
            contentDescription = "Open another PDF"
            setOnClickListener { pickDocument() }
        }, LinearLayout.LayoutParams(dp(44), dp(40)).apply { setMargins(0, dp(4), 0, 0) })
        tabScroll.post { tabStrip.getChildAt(current)?.let { c -> tabScroll.requestChildRectangleOnScreen(c, android.graphics.Rect(0, 0, c.width, c.height), false) } }
    }

    /** + in the tab strip: one or more PDFs, each in a tab. */
    private fun pickDocument() {
        val i = Intent(Intent.ACTION_OPEN_DOCUMENT).addCategory(Intent.CATEGORY_OPENABLE).setType("application/pdf")
            .putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true)
        runCatching { startActivityForResult(i, REQ_OPEN) }.onFailure { Toast.makeText(this, "No app to choose files with.", Toast.LENGTH_LONG).show() }
    }

    // ---------------------------------------------------------------------------------------------------------------
    // The screen

    private lateinit var root: LinearLayout
    private lateinit var header: LinearLayout
    private lateinit var frame: FrameLayout
    private lateinit var toolScroll: HorizontalScrollView
    private lateinit var tabStrip: LinearLayout
    private lateinit var tabScroll: HorizontalScrollView
    private var titleBar: android.widget.Toolbar? = null

    /** The document's name in the title bar (a dot before it while there are unsaved changes). */
    override fun onTitleChanged(t: CharSequence?, color: Int) {
        super.onTitleChanged(t, color)
        titleBar?.title = t
    }

    private fun buildScreen() {
        root = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        // the title bar, the find row and (unless Settings puts it at the bottom) the tools, all under the status bar
        header = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; setBackgroundColor(Chrome.barColour(this@ViewerActivity)) }
        titleBar = Chrome.toolbar(this, name)
        titleBar?.setNavigationIcon(android.R.drawable.ic_menu_sort_by_size)   // the sidebar (pages and bookmarks)
        titleBar?.navigationContentDescription = "Pages and bookmarks"
        titleBar?.setNavigationOnClickListener { toggleSidebar() }
        header.addView(titleBar)
        // the tabs: one for each open document, as on Windows and macOS
        tabStrip = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL; setPadding(dp(4), 0, dp(4), 0) }
        tabScroll = HorizontalScrollView(this).apply {
            isHorizontalScrollBarEnabled = false
            addView(tabStrip)
        }
        header.addView(tabScroll)
        searchRow = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            setPadding(dp(8), dp(4), dp(4), dp(4))
            setBackgroundColor(Chrome.bottomColour(this@ViewerActivity))
            visibility = View.GONE
        }
        searchField = EditText(this).apply {
            hint = "Find in document"
            isSingleLine = true
            imeOptions = EditorInfo.IME_ACTION_SEARCH
            setOnEditorActionListener { _, id, ev ->
                if (id == EditorInfo.IME_ACTION_SEARCH || ev?.keyCode == KeyEvent.KEYCODE_ENTER) { runSearch(); true } else false
            }
        }
        searchCount = TextView(this).apply { setPadding(dp(8), 0, dp(8), 0) }
        searchRow.addView(searchField, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
        searchRow.addView(searchCount)
        searchRow.addView(iconButton(android.R.drawable.arrow_up_float, "Previous") { stepHit(-1) })
        searchRow.addView(iconButton(android.R.drawable.arrow_down_float, "Next") { stepHit(1) })
        searchRow.addView(iconButton(android.R.drawable.ic_menu_close_clear_cancel, "Close") { closeSearch() })
        header.addView(searchRow)
        root.addView(header)

        frame = FrameLayout(this).apply { setBackgroundColor(if (Settings.isAmoled(this@ViewerActivity)) Color.BLACK else if (isNight()) 0xFF0B0C0E.toInt() else 0xFFCFD8DC.toInt()) }
        view = DocumentView(this).also { it.listener = this }
        applySettings()
        frame.addView(view, FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT))
        pageLabel = TextView(this).apply {
            setTextColor(Color.WHITE)
            textSize = 13f
            setPadding(dp(10), dp(4), dp(10), dp(4))
            background = GradientDrawable().apply { cornerRadius = dp(12).toFloat(); setColor(0x99000000.toInt()) }
            setOnClickListener { askPage() }
        }
        frame.addView(pageLabel, FrameLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT, Gravity.BOTTOM or Gravity.END).apply { setMargins(0, 0, dp(12), dp(12)) })
        busy = ProgressBar(this).apply { visibility = View.GONE }
        frame.addView(busy, FrameLayout.LayoutParams(dp(48), dp(48), Gravity.CENTER))
        buildSidebar()
        root.addView(frame, LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f))

        toolRow = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL; setPadding(dp(4), dp(2), dp(4), dp(2)) }
        val tools = listOf(
            DocumentView.Tool.READ to "Read", DocumentView.Tool.EDIT_TEXT to "Edit text", DocumentView.Tool.ADD_TEXT to "Add text",
            DocumentView.Tool.HIGHLIGHT to "Highlight", DocumentView.Tool.DRAW to "Draw", DocumentView.Tool.NOTE to "Note",
            DocumentView.Tool.ERASE to "Erase")
        for ((t, label) in tools) {
            val b = Button(this, null, android.R.attr.borderlessButtonStyle).apply {
                text = label
                isAllCaps = false
                minWidth = 0
                minimumWidth = 0
                setPadding(dp(12), paddingTop, dp(12), paddingBottom)
                setOnClickListener { chooseTool(t) }
                if (t == DocumentView.Tool.DRAW) setOnLongClickListener { choosePen(); true }
            }
            toolButtons[t] = b
            // a wide screen shares its width among the tools; a narrow one scrolls them, fading at the edge
            toolRow.addView(b, LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
        }
        toolScroll = HorizontalScrollView(this).apply {
            isHorizontalScrollBarEnabled = false
            isFillViewport = true
            isHorizontalFadingEdgeEnabled = true
            setFadingEdgeLength(dp(40))
            addView(toolRow)
            setBackgroundColor(Chrome.bottomColour(this@ViewerActivity))
        }
        setContentView(root)
        placeTools(Settings.toolsOnTop(this))
        chooseTool(DocumentView.Tool.READ)
    }

    /** The tools under the title bar (onTop) or at the bottom of the screen, clear of the system bars either way. */
    fun placeTools(onTop: Boolean) {
        toolsOnTopNow = onTop
        (toolScroll.parent as? ViewGroup)?.removeView(toolScroll)
        if (onTop) header.addView(toolScroll) else root.addView(toolScroll)
        for (v in listOf<View>(header, frame, toolScroll)) v.setPadding(0, 0, 0, 0)
        Chrome.fitSystemBars(this, root, header, if (onTop) frame else toolScroll)
    }

    private fun isNight() = Chrome.isNight(this)

    /** Settings (page colours, pen, highlight colour, keeping the screen on, memory, where the tools are), applied now. */
    private fun applySettings() {
        view.colours = Settings.pageColours(this)
        view.penColor = Settings.drawColour(this)
        view.penWidthPt = Settings.lineWidth(this)
        view.budget = Settings.cacheBytes(this)
        if (Settings.keepScreenOn(this)) window.addFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        else window.clearFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
    }

    private var toolsOnTopNow = true
    private var toldTheme = false

    override fun onResume() {
        super.onResume()
        applySettings()
        if (::toolScroll.isInitialized && Settings.toolsOnTop(this) != toolsOnTopNow) placeTools(Settings.toolsOnTop(this))
        if (Settings.theme(this) != themeAtStart) {
            // the screens are drawn again in the new colours; a document with unsaved changes keeps its screen until it is closed
            if (doc == 0L || !Engine.isModified(doc)) recreate()
            else if (!toldTheme) { toldTheme = true; Toast.makeText(this, "The new theme shows when this document is next opened.", Toast.LENGTH_LONG).show() }
        }
    }

    private fun iconButton(icon: Int, label: String, action: () -> Unit) = ImageButton(this, null, android.R.attr.borderlessButtonStyle).apply {
        setImageResource(icon)
        contentDescription = label
        setOnClickListener { action() }
    }

    private fun padded(v: View) = FrameLayout(this).apply { setPadding(dp(20), dp(8), dp(20), 0); addView(v) }

    private val hints = mapOf(
        DocumentView.Tool.READ to null,
        DocumentView.Tool.EDIT_TEXT to "Tap any outlined text to change it.",
        DocumentView.Tool.ADD_TEXT to "Tap where the new text goes.",
        DocumentView.Tool.HIGHLIGHT to "Sweep a finger over the text to highlight it.",
        DocumentView.Tool.DRAW to "Draw with one finger; two fingers move the page. Hold Draw for colour and width.",
        DocumentView.Tool.NOTE to "Tap where the note goes.",
        DocumentView.Tool.ERASE to "Tap a highlight, drawing or note to remove it.")

    fun chooseTool(t: DocumentView.Tool) {
        if (t == DocumentView.Tool.DRAW && view.tool == DocumentView.Tool.DRAW) { choosePen(); return }
        view.tool = t
        for ((k, b) in toolButtons) {
            b.setTextColor(if (k == t) 0xFFC62828.toInt() else if (isNight()) 0xFFE0E0E0.toInt() else 0xFF37474F.toInt())
            b.typeface = if (k == t) Typeface.DEFAULT_BOLD else Typeface.DEFAULT
        }
        hints[t]?.let { if (selfTest == null) Toast.makeText(this, it, Toast.LENGTH_SHORT).show() }
    }

    /** The pen's colour and width (also in Settings > Editing; kept). */
    private fun choosePen() {
        val labels = (Settings.colourNames.map { "Colour: $it" } + Settings.widthNames.map { "Width: $it" }).toTypedArray()
        AlertDialog.Builder(this).setTitle("Pen").setItems(labels) { _, i ->
            if (i < Settings.colourNames.size) Settings.setDrawIndex(this, i) else Settings.setWidthIndex(this, i - Settings.colourNames.size)
            applySettings()
            view.tool = DocumentView.Tool.DRAW
        }.show()
    }

    override fun onPageChanged(page: Int, count: Int) {
        pageLabel.text = "${page + 1} / $count"
        if (::sidebar.isInitialized && sidebar.visibility == View.VISIBLE) {
            thumbAdapter.notifyDataSetChanged()
            val first = pagesList.firstVisiblePosition; val last = pagesList.lastVisiblePosition
            if (page < first || page > last) pagesList.setSelection(maxOf(0, page - 1))   // (the page shown stays in sight)
        }
    }

    // ---------------------------------------------------------------------------------------------------------------
    // The sidebar: the pages as thumbnails, and the bookmarks (as in Feather PDF for Windows)

    private lateinit var sidebar: LinearLayout
    private lateinit var pagesList: android.widget.ListView
    private lateinit var marksList: android.widget.ListView
    private lateinit var pagesTab: TextView
    private lateinit var marksTab: TextView
    private val thumbs = android.util.LruCache<Int, Bitmap>(64)
    private val thumbIo = Executors.newSingleThreadExecutor()
    private val thumbAsked = HashSet<Int>()
    private var thumbDoc = 0L
    private var pageSizes = FloatArray(0)
    private var marks = listOf<Triple<Int, Int, String>>()   // level, page, title
    private val sidebarWidth get() = minOf(dp(280), (resources.displayMetrics.widthPixels * 0.78f).toInt())
    private val thumbWidth get() = sidebarWidth - dp(56)

    private fun buildSidebar() {
        val text = Chrome.textColour(this)
        sidebar = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(Chrome.bottomColour(this@ViewerActivity))
            elevation = dp(8).toFloat()
            isClickable = true   // (taps stay in it)
            visibility = View.GONE
        }
        val head = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER_VERTICAL }
        fun tabLabel(label: String, which: Int) = TextView(this).apply {
            this.text = label; textSize = 15f; gravity = Gravity.CENTER; setTextColor(text)
            setPadding(dp(8), dp(12), dp(8), dp(12))
            setOnClickListener { showSidebarPage(which) }
        }
        pagesTab = tabLabel("Pages", 0)
        marksTab = tabLabel("Bookmarks", 1)
        head.addView(pagesTab, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
        head.addView(marksTab, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
        head.addView(iconButton(android.R.drawable.ic_menu_close_clear_cancel, "Close the sidebar") { setSidebar(false) })
        sidebar.addView(head)
        pagesList = android.widget.ListView(this).apply {
            divider = null
            adapter = thumbAdapter
            setOnItemClickListener { _, _, pos, _ -> view.goToPage(pos); if (!wideScreen()) setSidebar(false) }
        }
        marksList = android.widget.ListView(this).apply {
            setOnItemClickListener { _, _, pos, _ ->
                val m = marks.getOrNull(pos) ?: return@setOnItemClickListener
                if (m.second >= 0) { view.goToPage(m.second); if (!wideScreen()) setSidebar(false) }
            }
        }
        sidebar.addView(pagesList, LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f))
        sidebar.addView(marksList, LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f))
        frame.addView(sidebar, FrameLayout.LayoutParams(sidebarWidth, ViewGroup.LayoutParams.MATCH_PARENT, Gravity.START))
        showSidebarPage(0)
    }

    private fun wideScreen() = resources.configuration.screenWidthDp >= 600

    private fun showSidebarPage(which: Int) {
        pagesList.visibility = if (which == 0) View.VISIBLE else View.GONE
        marksList.visibility = if (which == 1) View.VISIBLE else View.GONE
        for ((t, on) in listOf(pagesTab to (which == 0), marksTab to (which == 1))) {
            t.typeface = if (on) Typeface.DEFAULT_BOLD else Typeface.DEFAULT
            t.background = if (on) GradientDrawable().apply { setColor(0x22C62828) } else null
        }
    }

    fun toggleSidebar() = setSidebar(sidebar.visibility != View.VISIBLE)

    fun setSidebar(open: Boolean) {
        if (open && fullScreen) setFullScreen(false)
        sidebar.visibility = if (open) View.VISIBLE else View.GONE
        Settings.setSidebarOpen(this, open)
        if (open) { thumbAdapter.notifyDataSetChanged(); pagesList.setSelection(maxOf(0, view.currentPage - 1)) }
    }

    /** Another document in the sidebar: its thumbnails and bookmarks. */
    private fun sidebarDocumentChanged() {
        if (!::sidebar.isInitialized) return
        thumbs.evictAll()
        thumbAsked.clear()
        thumbDoc = doc
        pageSizes = if (doc != 0L) Engine.pageSizes(doc) else FloatArray(0)
        marks = if (doc != 0L) Engine.bookmarks(doc).mapNotNull { row ->
            val p = row.split('\t', limit = 3)
            if (p.size < 2) null else Triple(p[0].toIntOrNull() ?: 0, p[1].toIntOrNull() ?: -1, p.getOrElse(2) { "" })
        } else emptyList()
        marksList.adapter = android.widget.ArrayAdapter(this, android.R.layout.simple_list_item_1,
            if (marks.isEmpty()) listOf("This document has no bookmarks.") else marks.map { "\u2003".repeat(it.first.coerceAtMost(6)) + it.third })
        thumbAdapter.notifyDataSetChanged()
        if (Settings.sidebarOpen(this) && doc != 0L && !fullScreen) sidebar.visibility = View.VISIBLE
    }

    private val thumbAdapter = object : android.widget.BaseAdapter() {
        override fun getCount() = if (doc == 0L) 0 else pageSizes.size / 2
        override fun getItem(p: Int) = p
        override fun getItemId(p: Int) = p.toLong()
        override fun getView(p: Int, convert: View?, parent: ViewGroup?): View {
            val cell = (convert as? LinearLayout) ?: LinearLayout(this@ViewerActivity).apply {
                orientation = LinearLayout.VERTICAL
                gravity = Gravity.CENTER_HORIZONTAL
                setPadding(dp(8), dp(8), dp(8), dp(4))
                addView(ImageView(context).apply { scaleType = ImageView.ScaleType.FIT_XY; setBackgroundColor(Color.WHITE) })
                addView(TextView(context).apply { textSize = 12f; gravity = Gravity.CENTER; setTextColor(Chrome.textColour(this@ViewerActivity)) })
            }
            val img = cell.getChildAt(0) as ImageView
            val w = thumbWidth
            val h = if (pageSizes.size > 2 * p + 1 && pageSizes[2 * p] > 0) (w * pageSizes[2 * p + 1] / pageSizes[2 * p]).toInt() else (w * 1.41f).toInt()
            img.layoutParams = LinearLayout.LayoutParams(w, h)
            val here = p == view.currentPage
            img.setPadding(if (here) dp(3) else 1, if (here) dp(3) else 1, if (here) dp(3) else 1, if (here) dp(3) else 1)
            img.setBackgroundColor(if (here) 0xFFC62828.toInt() else 0x33808080)
            (cell.getChildAt(1) as TextView).text = "${p + 1}"
            val b = thumbs.get(p)
            img.setImageBitmap(b)
            if (b == null) askThumb(p, w, h)
            return cell
        }
    }

    /** A page's thumbnail, drawn in the background. */
    private fun askThumb(p: Int, w: Int, h: Int) {
        if (!thumbAsked.add(p)) return
        val handle = doc
        val mode = view.colours
        runCatching {
            thumbIo.execute {
                val bmp = runCatching {
                    val buf = java.nio.ByteBuffer.allocateDirect(w * h * 4)
                    if (!Engine.render(handle, p, buf, w, h, w * 4, 0, 0, w, h, mode)) null
                    else Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888).also { buf.position(0); it.copyPixelsFromBuffer(buf) }
                }.getOrNull()
                main.post {
                    if (handle != thumbDoc || bmp == null) return@post
                    thumbs.put(p, bmp)
                    thumbAdapter.notifyDataSetChanged()
                }
            }
        }
    }

    // ---------------------------------------------------------------------------------------------------------------
    // Full screen: a tap on the page (reading) hides the bars, the tabs, the tools and the sidebar; another brings them back

    private var fullScreen = false

    fun setFullScreen(on: Boolean) {
        fullScreen = on
        header.visibility = if (on) View.GONE else View.VISIBLE
        toolScroll.visibility = if (on) View.GONE else View.VISIBLE
        if (on) sidebar.visibility = View.GONE else if (Settings.sidebarOpen(this) && doc != 0L) sidebar.visibility = View.VISIBLE
        if (Build.VERSION.SDK_INT >= 30) {
            window.insetsController?.let { c ->
                if (on) {
                    c.systemBarsBehavior = android.view.WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
                    c.hide(android.view.WindowInsets.Type.statusBars() or android.view.WindowInsets.Type.navigationBars())
                } else c.show(android.view.WindowInsets.Type.statusBars() or android.view.WindowInsets.Type.navigationBars())
            }
        } else {
            @Suppress("DEPRECATION")
            val keep = View.SYSTEM_UI_FLAG_LAYOUT_STABLE or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN or View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
            @Suppress("DEPRECATION")
            window.decorView.systemUiVisibility = if (on) keep or View.SYSTEM_UI_FLAG_FULLSCREEN or View.SYSTEM_UI_FLAG_HIDE_NAVIGATION or View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                                                  else keep or (if (Build.VERSION.SDK_INT >= 26 && !isNight()) View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR else 0)
        }
    }

    override fun onTapOutside() {
        if (view.tool == DocumentView.Tool.READ) setFullScreen(!fullScreen)
    }

    // ---------------------------------------------------------------------------------------------------------------
    // Menus

    private var menuRef: Menu? = null

    override fun onCreateOptionsMenu(menu: Menu): Boolean {
        menuRef = menu
        menu.add(0, M_FIND, 0, "Find").setIcon(android.R.drawable.ic_menu_search).setShowAsAction(MenuItem.SHOW_AS_ACTION_IF_ROOM)
        menu.add(0, M_UNDO, 1, "Undo").setIcon(android.R.drawable.ic_menu_revert).setShowAsAction(MenuItem.SHOW_AS_ACTION_IF_ROOM)
        menu.add(0, M_SAVE, 2, "Save").setIcon(android.R.drawable.ic_menu_save).setShowAsAction(MenuItem.SHOW_AS_ACTION_IF_ROOM)
        menu.add(0, M_REDO, 3, "Redo")
        menu.add(0, M_SAVE_AS, 4, "Save a copy…")
        menu.add(0, M_SHARE, 5, "Share…")
        menu.add(0, M_GOTO, 6, "Go to page…")
        menu.add(0, M_PAGES, 7, "Pages")
        menu.add(0, M_BOOKMARKS, 7, "Bookmarks")
        menu.add(0, M_OPEN, 13, "Open another PDF…")
        menu.add(0, M_CLOSE_TAB, 14, "Close this tab")
        menu.add(0, M_COLOURS, 8, "Page colours…")
        menu.add(0, M_ROTATE, 9, "Turn this page")
        menu.add(0, M_DELETE, 10, "Delete this page…")
        menu.add(0, M_SETTINGS, 11, "Settings…")
        menu.add(0, M_ABOUT, 12, "About Feather PDF")
        updateMenus()
        return true
    }

    private fun updateMenus() {
        val m = menuRef ?: return
        val open = doc != 0L
        for (id in listOf(M_FIND, M_SAVE, M_SAVE_AS, M_SHARE, M_GOTO, M_BOOKMARKS, M_PAGES, M_ROTATE, M_DELETE, M_CLOSE_TAB)) m.findItem(id)?.isEnabled = open
        m.findItem(M_UNDO)?.isEnabled = open && Engine.canUndo(doc)
        m.findItem(M_REDO)?.isEnabled = open && Engine.canRedo(doc)
        title = if (open && Engine.isModified(doc)) "• $name" else name
        refreshTabs()
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        when (item.itemId) {
            M_FIND -> openSearch()
            M_UNDO -> if (Engine.undo(doc)) changedDocument()
            M_REDO -> if (Engine.redo(doc)) changedDocument()
            M_SAVE -> save(null)
            M_SAVE_AS -> saveAs()
            M_SHARE -> share()
            M_GOTO -> askPage()
            M_BOOKMARKS -> { setSidebar(true); showSidebarPage(1) }
            M_PAGES -> { setSidebar(true); showSidebarPage(0) }
            M_OPEN -> pickDocument()
            M_CLOSE_TAB -> closeTab(current)
            M_COLOURS -> chooseColours()
            M_ROTATE -> if (Engine.rotatePages(doc, intArrayOf(view.currentPage), 1)) changedDocument()
            M_DELETE -> deletePage()
            M_SETTINGS -> Settings.show(this)
            M_ABOUT -> About.show(this)
            else -> return super.onOptionsItemSelected(item)
        }
        return true
    }

    fun changedDocument() {
        view.documentChanged()
        hits = FloatArray(0); view.marks = hits; hitIndex = -1; searchCount.text = ""
        sidebarDocumentChanged()   // (pages turned, deleted or edited: their thumbnails again)
        updateMenus()
    }

    private fun askPage() {
        if (doc == 0L) return
        val field = EditText(this).apply { inputType = InputType.TYPE_CLASS_NUMBER; hint = "1 – ${view.pages}" }
        AlertDialog.Builder(this).setTitle("Go to page").setView(padded(field))
            .setPositiveButton("Go") { _, _ -> field.text.toString().toIntOrNull()?.let { view.goToPage(it - 1) } }
            .setNegativeButton("Cancel", null).show()
    }

    fun chooseColours() {
        val names = arrayOf("Normal", "Night (soft)", "Dimmed (warm)", "Blackboard (all inverted)", "Blackboard (pictures as they are)")
        val values = intArrayOf(Engine.COLOURS_NORMAL, Engine.COLOURS_NIGHT, Engine.COLOURS_DIMMED, Engine.COLOURS_BLACKBOARD, Engine.COLOURS_BLACKBOARD_ORIGINAL)
        AlertDialog.Builder(this).setTitle("Page colours").setSingleChoiceItems(names, values.indexOf(view.colours).coerceAtLeast(0)) { d, i ->
            view.colours = values[i]
            Settings.setPageColours(this, values[i])
            d.dismiss()
        }.show()
    }

    private fun deletePage() {
        if (view.pages <= 1) { Toast.makeText(this, "A document needs at least one page.", Toast.LENGTH_SHORT).show(); return }
        val p = view.currentPage
        AlertDialog.Builder(this).setTitle("Delete page ${p + 1}?").setMessage("Undo brings it back.")
            .setPositiveButton("Delete") { _, _ -> if (Engine.deletePages(doc, intArrayOf(p))) changedDocument() }
            .setNegativeButton("Cancel", null).show()
    }

    // ---------------------------------------------------------------------------------------------------------------
    // Finding

    private fun openSearch() {
        searchRow.visibility = View.VISIBLE
        searchField.requestFocus()
        (getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager).showSoftInput(searchField, 0)
    }

    private fun closeSearch() {
        searchRow.visibility = View.GONE
        (getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager).hideSoftInputFromWindow(searchField.windowToken, 0)
        hits = FloatArray(0); view.marks = hits; hitIndex = -1
    }

    fun runSearch(query: String = searchField.text.toString()) {
        if (query.isBlank() || doc == 0L) return
        (getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager).hideSoftInputFromWindow(searchField.windowToken, 0)
        hits = Engine.find(doc, query, false, false)
        view.marks = hits
        hitIndex = -1
        if (hits.isEmpty()) { searchCount.text = "0"; Toast.makeText(this, "Not found.", Toast.LENGTH_SHORT).show(); return }
        // the first match from the page on screen
        val here = view.currentPage
        var first = 0
        for (k in 0 until hits.size / 5) if (hits[k * 5].toInt() >= here) { first = k; break }
        hitIndex = first - 1
        stepHit(1)
    }

    private fun stepHit(d: Int) {
        val n = hits.size / 5
        if (n == 0) return
        hitIndex = ((hitIndex + d) % n + n) % n
        view.currentMark = hitIndex
        searchCount.text = "${hitIndex + 1} / $n"
        val k = hitIndex * 5
        view.showRect(hits[k].toInt(), hits[k + 1], hits[k + 2], hits[k + 3], hits[k + 4])
    }

    // ---------------------------------------------------------------------------------------------------------------
    // The tools

    override fun onPageTap(page: Int, x: Float, y: Float) {
        if (doc == 0L) return
        when (view.tool) {
            DocumentView.Tool.EDIT_TEXT -> editParagraph(page, x, y)
            DocumentView.Tool.ADD_TEXT -> addText(page, x, y)
            DocumentView.Tool.NOTE -> addNote(page, x, y)
            DocumentView.Tool.ERASE -> erase(page, x, y)
            else -> if (!showAnnotation(page, x, y) && view.tool == DocumentView.Tool.READ) setFullScreen(!fullScreen)
        }
    }

    fun editParagraph(page: Int, x: Float, y: Float, scripted: ((String?) -> Unit)? = null) {
        // an equation (Cambria Math, LaTeX's, or one made here) opens in the equation editor
        if (scripted == null && editEquationAt(page, x, y, false)) return
        val info = DoubleArray(11)
        val text = Engine.paragraphAt(doc, page, x, y, info)
        if (text == null) { Toast.makeText(this, "There is no text here to change.", Toast.LENGTH_SHORT).show(); scripted?.invoke("no paragraph"); return }
        val body = EditText(this).apply {
            setText(text)
            inputType = InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_FLAG_MULTI_LINE or InputType.TYPE_TEXT_FLAG_CAP_SENTENCES
            minLines = 3
            maxLines = 10
            gravity = Gravity.TOP
        }
        val size = EditText(this).apply {
            inputType = InputType.TYPE_CLASS_NUMBER or InputType.TYPE_NUMBER_FLAG_DECIMAL
            setText(if (info[4] % 1.0 == 0.0) info[4].toInt().toString() else "%.1f".format(info[4]))
            width = dp(72)
        }
        val bold = CheckBox(this).apply { this.text = "Bold"; isChecked = info[5] != 0.0 }
        val italic = CheckBox(this).apply { this.text = "Italic"; isChecked = info[6] != 0.0 }
        val row = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            addView(TextView(context).apply { this.text = "Size" })
            addView(size)
            addView(bold)
            addView(italic)
        }
        var dialog: AlertDialog? = null
        // x² and x₂: the selected text made superscript (subscript), or plain again
        fun script(kind: Int) {
            val a = body.selectionStart.coerceAtLeast(0); val b = body.selectionEnd.coerceAtLeast(0)
            if (b <= a) { Toast.makeText(this, "Select the text to raise or lower first.", Toast.LENGTH_SHORT).show(); return }
            val made = Engine.scriptText(body.text.substring(a, b), kind)
            body.text.replace(a, b, made)
            body.setSelection(a, a + made.length)
        }
        val tools = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            addView(Button(context).apply { setText("x\u00B2"); isAllCaps = false; setOnClickListener { script(1) } })
            addView(Button(context).apply { setText("x\u2082"); isAllCaps = false; setOnClickListener { script(-1) } })
            addView(Button(context).apply {
                setText("\u2211 Equation"); isAllCaps = false
                setOnClickListener { dialog?.dismiss(); if (!editEquationAt(page, x, y, true)) Toast.makeText(context, "That text could not be read as an equation.", Toast.LENGTH_SHORT).show() }
            })
        }
        val box = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; addView(body); addView(row); addView(tools) }
        val apply = {
            val why = Engine.replaceParagraph(doc, body.text.toString(), info[7].toInt(), bold.isChecked, italic.isChecked,
                                              size.text.toString().toDoubleOrNull() ?: info[4], info[8].toLong().toInt())
            if (why == null) changedDocument()
            else AlertDialog.Builder(this).setTitle("Not changed").setMessage(why).setPositiveButton("OK", null).show()
            why
        }
        if (scripted != null) {   // the build check: the change it asks for, without the dialog
            body.setText(text + " " + (intent.getStringExtra(EXTRA_EDIT_APPEND) ?: "Edited on Android."))
            scripted(apply())
            return
        }
        dialog = AlertDialog.Builder(this).setTitle("Edit text").setView(padded(box))
            .setPositiveButton("Apply") { _, _ -> apply() }
            .setNegativeButton("Cancel", null).show()
    }

    // ---------------------------------------------------------------------------------------------------------------
    // The equation editor: the equation in one line (s^8, x_1, \frac{a}{b}, \sqrt{x}), buttons, and a live picture of it

    /** Opens the equation at the point in the editor; false when there is none. */
    private fun editEquationAt(page: Int, x: Float, y: Float, any: Boolean): Boolean {
        val info = DoubleArray(6)
        val source = Engine.equationAt(doc, page, x, y, any, info) ?: return false
        val title = if (info[5] != 0.0) "Equation editor (runs over two lines: check it)" else "Equation editor"
        equationDialog(title, source, info[0], true) { src, size, remove ->
            if (!remove && src == source && Math.abs(size - info[0]) < 0.05) return@equationDialog
            val why = Engine.replaceEquation(doc, if (remove) "" else src, size)
            if (why == null) changedDocument()
            else AlertDialog.Builder(this).setTitle("Not changed").setMessage(why).setPositiveButton("OK", null).show()
        }
        return true
    }

    private fun insertEquationAt(page: Int, x: Float, y: Float) {
        equationDialog("Insert equation", "", 12.0, false) { src, size, _ ->
            if (src.isBlank()) return@equationDialog
            val why = Engine.insertEquation(doc, page, x.toDouble(), y.toDouble(), src, size, 0xFF000000.toInt())
            if (why == null) changedDocument()
            else AlertDialog.Builder(this).setTitle("Not added").setMessage(why).setPositiveButton("OK", null).show()
        }
    }

    private fun equationDialog(title: String, source: String, size0: Double, existing: Boolean, done: (String, Double, Boolean) -> Unit) {
        val field = EditText(this).apply {
            setText(source)
            textSize = 18f
            inputType = InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS or InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD
            hint = "s^8 + 9s^6 = 0   G(s) = \\frac{2K}{s(s+2)}"
            setSelection(source.length)
        }
        val size = EditText(this).apply {
            inputType = InputType.TYPE_CLASS_NUMBER or InputType.TYPE_NUMBER_FLAG_DECIMAL
            setText(if (size0 % 1.0 == 0.0) size0.toInt().toString() else "%.1f".format(size0))
            width = dp(72)
        }
        val preview = ImageView(this).apply {
            setBackgroundColor(Color.WHITE)
            adjustViewBounds = true
            minimumHeight = dp(90)
            scaleType = ImageView.ScaleType.FIT_CENTER
        }
        val error = TextView(this).apply { setTextColor(Color.rgb(0xC6, 0x28, 0x28)) }
        fun sizeValue() = size.text.toString().toDoubleOrNull()?.takeIf { it in 4.0..144.0 } ?: 12.0
        fun render() {
            val dims = IntArray(2)
            val err = arrayOfNulls<String>(1)
            val src = field.text.toString()
            val px = if (src.isBlank()) null else Engine.equationPreview(src, sizeValue(), 2.0 * density, dims, err)
            preview.setImageBitmap(if (px != null) Bitmap.createBitmap(px, dims[0], dims[1], Bitmap.Config.ARGB_8888) else null)
            error.text = if (src.isBlank()) "" else err[0] ?: ""
        }
        // a button's text at the cursor; a template wraps the selected text (the cursor goes inside it otherwise)
        fun put(what: String) {
            val a = field.selectionStart.coerceAtLeast(0); val b = field.selectionEnd.coerceAtLeast(a)
            val sel = field.text.substring(a, b)
            val slot = what.indexOf('\u0001')
            var text = what; var caret = what.length
            if (slot >= 0) {
                text = what.substring(0, slot) + sel + what.substring(slot + 1)
                caret = if (sel.isNotEmpty()) text.indexOf("{}", slot + sel.length).let { if (it >= 0) it + 1 else text.length } else slot
            }
            field.text.replace(a, b, text)
            field.setSelection((a + caret).coerceAtMost(field.text.length))
            field.requestFocus()
        }
        val structures = listOf("x\u00B2" to "^{\u0001}", "x\u2082" to "_{\u0001}", "a\u2044b" to "\\frac{\u0001}{}", "\u221A" to "\\sqrt{\u0001}",
            "\u207F\u221A" to "\\sqrt[\u0001]{}", "( )" to "\\left(\u0001\\right)", "|x|" to "\\left|\u0001\\right|", "\u2211" to "\\sum_{\u0001}^{}",
            "\u222B" to "\\int_{\u0001}^{}", "lim" to "\\lim_{\u0001}", "sin" to "\\sin \u0001", "abc" to "\\text{\u0001}")
        val signs = "+\u2212\u00B1\u00D7\u00B7\u00F7=\u2260\u2248\u2264\u2265<>\u221E\u2192\u2202\u2032\u00B0\u2208"
        val greek = "\u03B1\u03B2\u03B3\u03B4\u03B5\u03B8\u03BB\u03BC\u03C0\u03C1\u03C3\u03C4\u03C6\u03C9\u0394\u03A3\u03A9"
        fun strip(items: List<Pair<String, String>>) = HorizontalScrollView(this).apply {
            addView(LinearLayout(context).apply {
                orientation = LinearLayout.HORIZONTAL
                for ((label, insert) in items) addView(Button(context).apply {
                    setText(label); isAllCaps = false; minWidth = dp(44); minimumWidth = dp(44)
                    setOnClickListener { put(insert) }
                })
            })
        }
        val sizeRow = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            addView(TextView(context).apply { setText("Size") })
            addView(size)
        }
        val hintText = TextView(this).apply {
            setText("s^8 is a power, x_1 an index (use { } for more: e^{-st}). \\frac{top}{bottom} is a fraction, \\sqrt{x} a root.")
            textSize = 12f
        }
        val box = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            addView(field); addView(hintText)
            addView(strip(structures)); addView(strip(signs.map { it.toString() to it.toString() })); addView(strip(greek.map { it.toString() to it.toString() }))
            addView(sizeRow); addView(preview); addView(error)
        }
        field.addTextChangedListener(object : TextWatcher {
            override fun beforeTextChanged(s: CharSequence?, a: Int, b: Int, c: Int) {}
            override fun onTextChanged(s: CharSequence?, a: Int, b: Int, c: Int) {}
            override fun afterTextChanged(e: Editable?) { render() }
        })
        size.addTextChangedListener(object : TextWatcher {
            override fun beforeTextChanged(s: CharSequence?, a: Int, b: Int, c: Int) {}
            override fun onTextChanged(s: CharSequence?, a: Int, b: Int, c: Int) {}
            override fun afterTextChanged(e: Editable?) { render() }
        })
        render()
        val b = AlertDialog.Builder(this).setTitle(title).setView(android.widget.ScrollView(this).apply { addView(padded(box)) })
            .setPositiveButton(if (existing) "Apply" else "Insert", null)
            .setNegativeButton("Cancel", null)
        if (existing) b.setNeutralButton("Remove") { _, _ -> done("", sizeValue(), true) }
        val dlg = b.show()
        dlg.getButton(AlertDialog.BUTTON_POSITIVE).setOnClickListener {
            render()
            if (error.text.isNotEmpty()) return@setOnClickListener   // the reason shows under the picture
            dlg.dismiss()
            done(field.text.toString(), sizeValue(), false)
        }
    }

    private fun addText(page: Int, x: Float, y: Float) {
        val body = EditText(this).apply { hint = "Text"; inputType = InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_FLAG_MULTI_LINE }
        AlertDialog.Builder(this).setTitle("Add text").setView(padded(body))
            .setNeutralButton("Equation\u2026") { _, _ -> insertEquationAt(page, x, y) }
            .setPositiveButton("Add") { _, _ ->
                val t = body.text.toString()
                // in the drawing colour (Settings > Editing), as on Windows; yellow would not be read on white paper, so it is dark grey then
                val colour = if (Settings.drawIndex(this) == 0) 0xFF202020.toInt() else Settings.drawColour(this)
                if (t.isNotBlank() && Engine.addTextBox(doc, page, x.toDouble(), y.toDouble(), t, 12.0, colour)) changedDocument()
                else if (t.isNotBlank()) Toast.makeText(this, "The text could not be added (letters of Western languages only).", Toast.LENGTH_LONG).show()
            }
            .setNegativeButton("Cancel", null).show()
    }

    private fun addNote(page: Int, x: Float, y: Float) {
        val body = EditText(this).apply { hint = "Note"; inputType = InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_FLAG_MULTI_LINE }
        AlertDialog.Builder(this).setTitle("Add a note").setView(padded(body))
            .setPositiveButton("Add") { _, _ -> if (Engine.addNote(doc, page, x, y, body.text.toString(), 0xFFFFC107.toInt())) changedDocument() }
            .setNegativeButton("Cancel", null).show()
    }

    private fun erase(page: Int, x: Float, y: Float) {
        val kind = arrayOfNulls<String>(2)
        val i = Engine.annotAt(doc, page, x, y, kind)
        if (i < 0) { Toast.makeText(this, "Tap a highlight, drawing or note.", Toast.LENGTH_SHORT).show(); return }
        AlertDialog.Builder(this).setTitle("Remove this ${kind[0] ?: "annotation"}?")
            .setPositiveButton("Remove") { _, _ -> if (Engine.removeAnnot(doc, page, i)) changedDocument() }
            .setNegativeButton("Cancel", null).show()
    }

    /** Reading: a tap on a note or a commented highlight shows its comment (true); elsewhere, nothing is shown (false). */
    private fun showAnnotation(page: Int, x: Float, y: Float): Boolean {
        val kind = arrayOfNulls<String>(2)
        val i = Engine.annotAt(doc, page, x, y, kind)
        if (i < 0 || kind[1].isNullOrBlank()) return false
        val body = EditText(this).apply { setText(kind[1]); inputType = InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_FLAG_MULTI_LINE }
        AlertDialog.Builder(this).setTitle(kind[0]?.replaceFirstChar { it.uppercase() } ?: "Note").setView(padded(body))
            .setPositiveButton("Save") { _, _ -> if (body.text.toString() != kind[1] && Engine.setAnnotText(doc, page, i, body.text.toString())) changedDocument() }
            .setNeutralButton("Remove") { _, _ -> if (Engine.removeAnnot(doc, page, i)) changedDocument() }
            .setNegativeButton("Close", null).show()
        return true
    }

    override fun onTextSwept(page: Int, x1: Float, y1: Float, x2: Float, y2: Float) {
        val rects = Engine.selectionRects(doc, page, x1, y1, x2, y2)
        if (rects.isEmpty()) return
        if (view.tool == DocumentView.Tool.HIGHLIGHT) {
            if (Engine.addMarkup(doc, page, Engine.MARKUP_HIGHLIGHT, rects, Settings.highlightColour(this))) changedDocument()
            return
        }
        val text = Engine.selectionText(doc, page, x1, y1, x2, y2)
        val actions = arrayOf("Copy", "Highlight", "Underline", "Strike out")
        AlertDialog.Builder(this).setTitle(text.take(80) + if (text.length > 80) "…" else "")
            .setItems(actions) { _, i ->
                when (i) {
                    0 -> {
                        (getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager).setPrimaryClip(ClipData.newPlainText("PDF text", text))
                        Toast.makeText(this, "Copied.", Toast.LENGTH_SHORT).show()
                    }
                    else -> {
                        val kind = intArrayOf(Engine.MARKUP_HIGHLIGHT, Engine.MARKUP_UNDERLINE, Engine.MARKUP_STRIKEOUT)[i - 1]
                        val colour = intArrayOf(Settings.highlightColour(this), 0xFF1E88E5.toInt(), 0xFFE53935.toInt())[i - 1]
                        if (Engine.addMarkup(doc, page, kind, rects, colour)) changedDocument()
                    }
                }
                view.clearSelection()
            }
            .setOnCancelListener { view.clearSelection() }
            .show()
    }

    override fun onStroke(page: Int, points: FloatArray) {
        if (Engine.addInk(doc, page, arrayOf(points), view.penColor, view.penWidthPt)) changedDocument()
    }

    // ---------------------------------------------------------------------------------------------------------------
    // Saving

    /** Writes the document to `target` (null: where it came from; asks for a place when it cannot be written there). */
    fun save(target: Uri?, then: (() -> Unit)? = null) {
        if (doc == 0L) return
        val dest = target ?: uri
        if (dest == null) { saveAs(); return }
        val bytes = Engine.bytesToSave(doc)
        if (bytes == null) { Toast.makeText(this, "The document could not be written.", Toast.LENGTH_LONG).show(); return }
        busy.visibility = View.VISIBLE
        io.execute {
            val ok = runCatching {
                // "wt" truncates; some places only offer "w"
                val out = runCatching { contentResolver.openOutputStream(dest, "wt") }.getOrNull() ?: contentResolver.openOutputStream(dest, "w")
                out!!.use { it.write(bytes) }
            }.isSuccess
            main.post {
                busy.visibility = View.GONE
                if (ok) {
                    Engine.markSaved(doc)
                    if (target != null) { uri = target; name = displayName(target); Recents.add(this, target.toString(), name) }
                    updateMenus()
                    Toast.makeText(this, "Saved.", Toast.LENGTH_SHORT).show()
                    then?.invoke()
                } else {
                    AlertDialog.Builder(this).setTitle("Not saved here").setMessage("This place does not let Feather PDF change the file. Save a copy instead?")
                        .setPositiveButton("Save a copy…") { _, _ -> saveAs() }.setNegativeButton("Cancel", null).show()
                }
            }
        }
    }

    private var afterSaveAs: (() -> Unit)? = null

    private fun saveAs(then: (() -> Unit)? = null) {
        afterSaveAs = then
        val i = Intent(Intent.ACTION_CREATE_DOCUMENT).addCategory(Intent.CATEGORY_OPENABLE).setType("application/pdf")
            .putExtra(Intent.EXTRA_TITLE, if (name.endsWith(".pdf", true)) name else "$name.pdf")
        runCatching { startActivityForResult(i, REQ_SAVE_AS) }.onFailure { Toast.makeText(this, "No place to save to.", Toast.LENGTH_LONG).show() }
    }

    @Deprecated("Activity result API of the platform")
    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        @Suppress("DEPRECATION") super.onActivityResult(requestCode, resultCode, data)
        if (requestCode == REQ_SAVE_AS && resultCode == RESULT_OK) data?.data?.let { u ->
            runCatching { contentResolver.takePersistableUriPermission(u, Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_GRANT_WRITE_URI_PERMISSION) }
            save(u, afterSaveAs)
        }
        if (requestCode == REQ_OPEN && resultCode == RESULT_OK && data != null) {   // + : each chosen PDF in a tab
            val chosen = ArrayList<Uri>()
            data.clipData?.let { c -> for (k in 0 until c.itemCount) c.getItemAt(k).uri?.let { chosen.add(it) } }
            if (chosen.isEmpty()) data.data?.let { chosen.add(it) }
            for (u in chosen) {
                val flags = Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_GRANT_WRITE_URI_PERMISSION
                runCatching { contentResolver.takePersistableUriPermission(u, flags) }
                    .onFailure { runCatching { contentResolver.takePersistableUriPermission(u, Intent.FLAG_GRANT_READ_URI_PERMISSION) } }
                openUri(u)
            }
        }
    }

    private fun share() {
        val bytes = Engine.bytesToSave(doc) ?: return
        val dir = File(cacheDir, "share").apply { mkdirs(); listFiles()?.forEach { it.delete() } }
        val safe = name.replace(Regex("[^\\p{L}\\p{N} ._()-]"), "_").let { if (it.endsWith(".pdf", true)) it else "$it.pdf" }
        File(dir, safe).writeBytes(bytes)
        val u = Uri.parse("content://${ShareProvider.AUTHORITY}/${Uri.encode(safe)}")
        val send = Intent(Intent.ACTION_SEND).setType("application/pdf").putExtra(Intent.EXTRA_STREAM, u).addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        send.clipData = ClipData.newRawUri(safe, u)
        startActivity(Intent.createChooser(send, "Share $safe"))
    }

    @Deprecated("Back handling of the platform")
    override fun onBackPressed() {
        if (fullScreen) { setFullScreen(false); return }
        if (searchRow.visibility == View.VISIBLE) { closeSearch(); return }
        if (::sidebar.isInitialized && sidebar.visibility == View.VISIBLE && !wideScreen()) { setSidebar(false); return }
        // leaving: each document with unsaved changes is asked about first (its tab shown)
        stashTab()
        val unsaved = if (selfTest == null) tabs.indexOfFirst { it.doc != 0L && Engine.isModified(it.doc) } else -1
        if (unsaved >= 0) {
            switchTo(unsaved)
            AlertDialog.Builder(this).setTitle("Save the changes to $name?")
                .setPositiveButton("Save") { _, _ -> save(null) { dropTab(current); if (!isFinishing) onBackPressed() } }
                .setNegativeButton("Don\u2019t save") { _, _ -> dropTab(current); if (!isFinishing) onBackPressed() }
                .setNeutralButton("Cancel", null).show()
            return
        }
        @Suppress("DEPRECATION") super.onBackPressed()
    }

    override fun onPause() {
        super.onPause()
        stashTab()
        for (t in tabs) if (t.doc != 0L) t.uri?.let { Recents.setPage(this, it.toString(), t.page) }
    }

    override fun onDestroy() {
        super.onDestroy()
        io.shutdown()
        thumbIo.shutdown()
        stashTab()
        for (t in tabs) if (t.doc != 0L) Engine.close(t.doc)
        tabs.clear()
        doc = 0L
    }

    // ---------------------------------------------------------------------------------------------------------------
    // For the build check (SelfTest)

    val documentHandle: Long get() = doc
    val tabCount: Int get() = tabs.size
    val currentTab: Int get() = current
    val tabStripView: View get() = tabScroll
    val sidebarView: View get() = sidebar
    val fullScreenNow: Boolean get() = fullScreen
    val thumbnailsDrawn: Int get() = thumbs.size()
    val thumbnailRows: Int get() = thumbAdapter.count
    /** The build check: a file of the app's own opened in another tab. */
    fun openPathInTab(path: String) {
        stashTab()
        uri = null; name = File(path).name; localPath = path; doc = 0L
        openLocal("")
    }
    val headerView: View get() = header
    val titleShown: String get() = titleBar?.title?.toString() ?: ""
    val toolsView: View get() = toolScroll
    val documentView: DocumentView get() = view

    fun screenshot(): Bitmap {
        val root = window.decorView
        val b = Bitmap.createBitmap(root.width.coerceAtLeast(1), root.height.coerceAtLeast(1), Bitmap.Config.ARGB_8888)
        root.draw(android.graphics.Canvas(b))
        return b
    }

    companion object {
        const val EXTRA_PATH = "com.featherpdf.path"
        const val EXTRA_SELFTEST = "com.featherpdf.selftest"
        const val EXTRA_EDIT_APPEND = "com.featherpdf.selftest.append"
        private const val REQ_SAVE_AS = 1
        private const val M_FIND = 1; private const val M_UNDO = 2; private const val M_REDO = 3; private const val M_SAVE = 4
        private const val M_SAVE_AS = 5; private const val M_SHARE = 6; private const val M_GOTO = 7; private const val M_BOOKMARKS = 8
        private const val M_COLOURS = 9; private const val M_ROTATE = 10; private const val M_DELETE = 11; private const val M_ABOUT = 12
        private const val M_SETTINGS = 13; private const val M_PAGES = 14; private const val M_OPEN = 15; private const val M_CLOSE_TAB = 16
        private const val REQ_OPEN = 2
    }
}
