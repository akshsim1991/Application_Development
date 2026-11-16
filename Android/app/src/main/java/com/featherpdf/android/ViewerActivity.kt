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
import android.widget.LinearLayout
import android.widget.ProgressBar
import android.widget.TextView
import android.widget.Toast
import com.featherpdf.engine.Engine
import java.io.File
import java.io.FileOutputStream
import java.security.MessageDigest
import java.util.concurrent.Executors

/** One open document: reading, finding, editing text, annotating, page tools, saving. */
class ViewerActivity : Activity(), DocumentView.Listener {

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

    override fun onCreate(savedInstanceState: Bundle?) {
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
        uri = u
        name = displayName(u)
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
        view.setDocument(doc, uri?.let { Recents.pageOf(this, it.toString()) } ?: 0)
        updateMenus()
        selfTest?.start()
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
            .setNegativeButton("Cancel") { _, _ -> finish() }
            .setOnCancelListener { finish() }
            .show()
        field.requestFocus()
    }

    private fun fail(message: String) {
        AlertDialog.Builder(this).setTitle("Feather PDF").setMessage(message)
            .setPositiveButton("Close") { _, _ -> finish() }.setOnCancelListener { finish() }.show()
    }

    // ---------------------------------------------------------------------------------------------------------------
    // The screen

    private fun buildScreen() {
        val root = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        searchRow = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            setPadding(dp(8), dp(4), dp(4), dp(4))
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
        root.addView(searchRow)

        val frame = FrameLayout(this)
        view = DocumentView(this).also { it.listener = this }
        view.colours = getPreferences(MODE_PRIVATE).getInt("colours", Engine.COLOURS_NORMAL)
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
                setOnClickListener { chooseTool(t) }
                if (t == DocumentView.Tool.DRAW) setOnLongClickListener { choosePen(); true }
            }
            toolButtons[t] = b
            toolRow.addView(b)
        }
        val scroll = HorizontalScrollView(this).apply { isHorizontalScrollBarEnabled = false; addView(toolRow) }
        scroll.setBackgroundColor(if (isNight()) 0xFF1E2124.toInt() else Color.WHITE)
        root.addView(scroll)
        setContentView(root)
        chooseTool(DocumentView.Tool.READ)
    }

    private fun isNight() = (resources.configuration.uiMode and Configuration.UI_MODE_NIGHT_MASK) == Configuration.UI_MODE_NIGHT_YES

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

    private fun choosePen() {
        val colours = listOf("Red" to 0xFFD32F2F.toInt(), "Blue" to 0xFF1565C0.toInt(), "Green" to 0xFF2E7D32.toInt(),
                              "Black" to 0xFF212121.toInt(), "Yellow" to 0xFFFBC02D.toInt())
        val widths = listOf("Fine" to 1f, "Medium" to 2.5f, "Thick" to 5f)
        val labels = (colours.map { "Colour: ${it.first}" } + widths.map { "Width: ${it.first}" }).toTypedArray()
        AlertDialog.Builder(this).setTitle("Pen").setItems(labels) { _, i ->
            if (i < colours.size) view.penColor = colours[i].second else view.penWidthPt = widths[i - colours.size].second
            view.tool = DocumentView.Tool.DRAW
        }.show()
    }

    override fun onPageChanged(page: Int, count: Int) {
        pageLabel.text = "${page + 1} / $count"
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
        menu.add(0, M_BOOKMARKS, 7, "Bookmarks")
        menu.add(0, M_COLOURS, 8, "Page colours…")
        menu.add(0, M_ROTATE, 9, "Turn this page")
        menu.add(0, M_DELETE, 10, "Delete this page…")
        menu.add(0, M_ABOUT, 11, "About Feather PDF")
        updateMenus()
        return true
    }

    private fun updateMenus() {
        val m = menuRef ?: return
        val open = doc != 0L
        for (id in listOf(M_FIND, M_SAVE, M_SAVE_AS, M_SHARE, M_GOTO, M_BOOKMARKS, M_ROTATE, M_DELETE)) m.findItem(id)?.isEnabled = open
        m.findItem(M_UNDO)?.isEnabled = open && Engine.canUndo(doc)
        m.findItem(M_REDO)?.isEnabled = open && Engine.canRedo(doc)
        title = if (open && Engine.isModified(doc)) "• $name" else name
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
            M_BOOKMARKS -> showBookmarks()
            M_COLOURS -> chooseColours()
            M_ROTATE -> if (Engine.rotatePages(doc, intArrayOf(view.currentPage), 1)) changedDocument()
            M_DELETE -> deletePage()
            M_ABOUT -> about()
            else -> return super.onOptionsItemSelected(item)
        }
        return true
    }

    fun changedDocument() {
        view.documentChanged()
        hits = FloatArray(0); view.marks = hits; hitIndex = -1; searchCount.text = ""
        updateMenus()
    }

    private fun askPage() {
        if (doc == 0L) return
        val field = EditText(this).apply { inputType = InputType.TYPE_CLASS_NUMBER; hint = "1 – ${view.pages}" }
        AlertDialog.Builder(this).setTitle("Go to page").setView(padded(field))
            .setPositiveButton("Go") { _, _ -> field.text.toString().toIntOrNull()?.let { view.goToPage(it - 1) } }
            .setNegativeButton("Cancel", null).show()
    }

    private fun showBookmarks() {
        val rows = Engine.bookmarks(doc)
        if (rows.isEmpty()) { Toast.makeText(this, "This document has no bookmarks.", Toast.LENGTH_SHORT).show(); return }
        val parsed = rows.map { val p = it.split('\t', limit = 3); Triple(p[0].toInt(), p[1].toInt(), p.getOrElse(2) { "" }) }
        val labels = parsed.map { " ".repeat(it.first.coerceAtMost(6)) + it.third }.toTypedArray()
        AlertDialog.Builder(this).setTitle("Bookmarks").setItems(labels) { _, i -> if (parsed[i].second >= 0) view.goToPage(parsed[i].second) }.show()
    }

    private fun chooseColours() {
        val names = arrayOf("Normal", "Night (soft)", "Dimmed (warm)", "Blackboard (all inverted)", "Blackboard (pictures as they are)")
        val values = intArrayOf(Engine.COLOURS_NORMAL, Engine.COLOURS_NIGHT, Engine.COLOURS_DIMMED, Engine.COLOURS_BLACKBOARD, Engine.COLOURS_BLACKBOARD_ORIGINAL)
        AlertDialog.Builder(this).setTitle("Page colours").setSingleChoiceItems(names, values.indexOf(view.colours).coerceAtLeast(0)) { d, i ->
            view.colours = values[i]
            getPreferences(MODE_PRIVATE).edit().putInt("colours", values[i]).apply()
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

    private fun about() {
        val info = packageManager.getPackageInfo(packageName, 0)
        val codename = runCatching {
            @Suppress("DEPRECATION")
            packageManager.getApplicationInfo(packageName, android.content.pm.PackageManager.GET_META_DATA).metaData?.getString("com.featherpdf.codename")
        }.getOrNull().orEmpty()
        AlertDialog.Builder(this).setTitle("Feather PDF ${info.versionName} “$codename”")
            .setMessage("Free, for ever. The same PDF engine as Feather PDF for Windows and Mac.\n\nThis first Android version reads, finds, " +
                        "edits text, highlights, draws, adds notes, turns and deletes pages, and saves.")
            .setPositiveButton("OK", null).show()
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
            else -> showAnnotation(page, x, y)
        }
    }

    fun editParagraph(page: Int, x: Float, y: Float, scripted: ((String?) -> Unit)? = null) {
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
        val box = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; addView(body); addView(row) }
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
        AlertDialog.Builder(this).setTitle("Edit text").setView(padded(box))
            .setPositiveButton("Apply") { _, _ -> apply() }
            .setNegativeButton("Cancel", null).show()
    }

    private fun addText(page: Int, x: Float, y: Float) {
        val body = EditText(this).apply { hint = "Text"; inputType = InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_FLAG_MULTI_LINE }
        AlertDialog.Builder(this).setTitle("Add text").setView(padded(body))
            .setPositiveButton("Add") { _, _ ->
                val t = body.text.toString()
                if (t.isNotBlank() && Engine.addTextBox(doc, page, x.toDouble(), y.toDouble(), t, 12.0, 0xFF202020.toInt())) changedDocument()
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

    /** Reading: a tap on a note or a commented highlight shows its comment. */
    private fun showAnnotation(page: Int, x: Float, y: Float) {
        val kind = arrayOfNulls<String>(2)
        val i = Engine.annotAt(doc, page, x, y, kind)
        if (i < 0 || kind[1].isNullOrBlank()) return
        val body = EditText(this).apply { setText(kind[1]); inputType = InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_FLAG_MULTI_LINE }
        AlertDialog.Builder(this).setTitle(kind[0]?.replaceFirstChar { it.uppercase() } ?: "Note").setView(padded(body))
            .setPositiveButton("Save") { _, _ -> if (body.text.toString() != kind[1] && Engine.setAnnotText(doc, page, i, body.text.toString())) changedDocument() }
            .setNeutralButton("Remove") { _, _ -> if (Engine.removeAnnot(doc, page, i)) changedDocument() }
            .setNegativeButton("Close", null).show()
    }

    override fun onTextSwept(page: Int, x1: Float, y1: Float, x2: Float, y2: Float) {
        val rects = Engine.selectionRects(doc, page, x1, y1, x2, y2)
        if (rects.isEmpty()) return
        if (view.tool == DocumentView.Tool.HIGHLIGHT) {
            if (Engine.addMarkup(doc, page, Engine.MARKUP_HIGHLIGHT, rects, 0xFFFFEB3B.toInt())) changedDocument()
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
                        val colour = intArrayOf(0xFFFFEB3B.toInt(), 0xFF1E88E5.toInt(), 0xFFE53935.toInt())[i - 1]
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
        if (searchRow.visibility == View.VISIBLE) { closeSearch(); return }
        if (doc != 0L && Engine.isModified(doc) && selfTest == null) {
            AlertDialog.Builder(this).setTitle("Save the changes to $name?")
                .setPositiveButton("Save") { _, _ -> save(null) { finish() } }
                .setNegativeButton("Don’t save") { _, _ -> finish() }
                .setNeutralButton("Cancel", null).show()
            return
        }
        @Suppress("DEPRECATION") super.onBackPressed()
    }

    override fun onPause() {
        super.onPause()
        if (doc != 0L) uri?.let { Recents.setPage(this, it.toString(), view.currentPage) }
    }

    override fun onDestroy() {
        super.onDestroy()
        io.shutdown()
        if (doc != 0L) { Engine.close(doc); doc = 0L }
    }

    // ---------------------------------------------------------------------------------------------------------------
    // For the build check (SelfTest)

    val documentHandle: Long get() = doc
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
    }
}
