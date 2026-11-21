package com.featherpdf.android

import android.app.Activity
import android.app.AlertDialog
import android.content.Intent
import android.graphics.Bitmap
import android.graphics.Typeface
import android.graphics.drawable.GradientDrawable
import android.net.Uri
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.Switch
import android.widget.TextView
import android.widget.Toast
import com.featherpdf.engine.Engine
import java.io.File
import java.io.FileOutputStream

/**
 * Settings, as in Feather PDF for Windows: the groups on the left (on a phone, a list first), each setting with a line
 * saying what it does and its control on the right. A change applies at once and is kept.
 */
class SettingsActivity : Activity() {

    /** One setting: a switch (get/set 0 or 1), a choice among `choices`, or a button. */
    class Item(
        val kind: Kind, val label: String, val help: String,
        val choices: List<String> = emptyList(),
        val get: () -> Int = { 0 }, val set: (Int) -> Unit = {},
        val press: () -> Unit = {},
    ) { enum class Kind { TOGGLE, CHOICE, BUTTON } }

    class Category(val name: String, val summary: String, val items: List<Item>)

    private lateinit var cats: List<Category>
    private var page = -1                         // the category shown (-1: the list of categories, on a phone)
    private lateinit var body: LinearLayout       // the category's settings
    private var list: LinearLayout? = null        // the categories beside it (wide screens)
    private lateinit var bar: android.widget.Toolbar
    private val wide get() = resources.configuration.screenWidthDp >= 600
    private fun dp(v: Int) = (v * resources.displayMetrics.density).toInt()

    override fun onCreate(savedInstanceState: Bundle?) {
        Settings.applyTheme(this)
        super.onCreate(savedInstanceState)
        cats = categories()
        page = savedInstanceState?.getInt(EXTRA_PAGE) ?: intent.getIntExtra(EXTRA_PAGE, -1)
        if (wide && page < 0) page = 0
        build()
        // the build check: only into the app's own folder
        val own = getExternalFilesDir(null)?.canonicalPath
        intent.getStringExtra(EXTRA_SELFTEST)?.let { path ->
            if (own != null && File(path).canonicalPath.startsWith(own)) Handler(Looper.getMainLooper()).postDelayed({ selfTest(File(path)) }, 800)
        }
    }

    override fun onSaveInstanceState(out: Bundle) {
        super.onSaveInstanceState(out)
        out.putInt(EXTRA_PAGE, page)
    }

    // ---------------------------------------------------------------------------------------------------------------
    // The settings

    private fun categories(): List<Category> {
        val a = this
        val pageColourNames = listOf("Normal", "Dark (night mode)", "Dimmed (softer white)", "Blackboard: inverted", "Blackboard: original colours")
        val pageColourValues = intArrayOf(Engine.COLOURS_NORMAL, Engine.COLOURS_NIGHT, Engine.COLOURS_DIMMED, Engine.COLOURS_BLACKBOARD, Engine.COLOURS_BLACKBOARD_ORIGINAL)
        return listOf(
            Category("Appearance", "Theme, where the tools are", listOf(
                Item(Item.Kind.CHOICE, "Theme", "Colours of the screens and dialogs. Like the phone follows the phone's light or dark mode.",
                     listOf("Light", "Dark", "AMOLED Dark (true black)", "Like the phone"),
                     get = { Settings.theme(a) }, set = { Settings.setTheme(a, it); recreate() }),
                Item(Item.Kind.CHOICE, "Tools", "Where the tools (Read, Edit text, Add text, Highlight, Draw, Note, Erase) are while a document is open.",
                     listOf("At the top, under the title", "At the bottom of the screen"),
                     get = { if (Settings.toolsOnTop(a)) 0 else 1 }, set = { Settings.setToolsOnTop(a, it == 0) }),
            )),
            Category("PDF Viewing", "Page colours, last page read, screen", listOf(
                Item(Item.Kind.CHOICE, "Page colours", "Dark turns pages dark for reading at night; Dimmed softens bright white. Blackboard makes the page pure black (for AMOLED screens) with white text: Inverted turns everything into a negative, pictures included; Original colours keeps pictures, coloured text and highlights in their own colours.",
                     pageColourNames, get = { pageColourValues.indexOf(Settings.pageColours(a)).coerceAtLeast(0) },
                     set = { Settings.setPageColours(a, pageColourValues[it]) }),
                Item(Item.Kind.TOGGLE, "Open documents where they were last read", "The page of the last 20 documents is remembered.",
                     get = { if (Settings.restorePositions(a)) 1 else 0 }, set = { Settings.setRestorePositions(a, it != 0) }),
                Item(Item.Kind.TOGGLE, "Keep the screen on while reading", "The screen does not turn off by itself while a document is open.",
                     get = { if (Settings.keepScreenOn(a)) 1 else 0 }, set = { Settings.setKeepScreenOn(a, it != 0) }),
            )),
            Category("Editing", "Highlight and drawing colours, line width", listOf(
                Item(Item.Kind.CHOICE, "Highlight colour", "For highlights: with the Highlight tool, and for selected text.", Settings.colourNames,
                     get = { Settings.highlightIndex(a) }, set = { Settings.setHighlightIndex(a, it) }),
                Item(Item.Kind.CHOICE, "Drawing colour", "For the pen and new text.", Settings.colourNames,
                     get = { Settings.drawIndex(a) }, set = { Settings.setDrawIndex(a, it) }),
                Item(Item.Kind.CHOICE, "Line width", "For the pen.", Settings.widthNames,
                     get = { Settings.widthIndex(a) }, set = { Settings.setWidthIndex(a, it) }),
            )),
            Category("Performance", "Pages kept ready in memory", listOf(
                Item(Item.Kind.CHOICE, "Pages kept ready in memory", "More pages make paging back and forth faster and use more memory.",
                     listOf("Few", "Normal", "Many"), get = { Settings.cacheIndex(a) }, set = { Settings.setCacheIndex(a, it) }),
            )),
            Category("Advanced", "Default app, recent files, versions, reset", listOf(
                Item(Item.Kind.BUTTON, "Default PDF application", "Opens the phone's settings, where you choose the app that opens PDF files.",
                     listOf("Open settings…"), press = { defaultApps() }),
                Item(Item.Kind.BUTTON, "Recent files", "The list on the start screen.", listOf("Clear the list"), press = {
                    AlertDialog.Builder(a).setMessage("Clear the list of recent files?")
                        .setPositiveButton("Clear") { _, _ -> Recents.clear(a); Toast.makeText(a, "Cleared.", Toast.LENGTH_SHORT).show() }
                        .setNegativeButton("Cancel", null).show()
                }),
                Item(Item.Kind.BUTTON, "New versions", "The newest Feather PDF is on its releases page (or on Google Play, when installed from there).",
                     listOf("Open the releases page"), press = {
                         runCatching { startActivity(Intent(Intent.ACTION_VIEW, Uri.parse("https://github.com/akshsim1991/FeatherPDF-releases"))) }
                     }),
                Item(Item.Kind.BUTTON, "About", "Who made Feather PDF, privacy, the open-source licences.", listOf("About Feather PDF"),
                     press = { About.show(a) }),
                Item(Item.Kind.BUTTON, "Reset settings", "Puts every setting on this page and the others back as it was when Feather PDF was installed. Recent files are kept.",
                     listOf("Reset all…"), press = {
                         AlertDialog.Builder(a).setMessage("Put every setting back to its default? Recent files are kept.")
                             .setPositiveButton("Reset") { _, _ -> Settings.reset(a); recreate() }
                             .setNegativeButton("Cancel", null).show()
                     }),
            )),
        )
    }

    private fun defaultApps() {
        val tries = listOf(Intent(android.provider.Settings.ACTION_MANAGE_DEFAULT_APPS_SETTINGS), Intent(android.provider.Settings.ACTION_SETTINGS))
        for (i in tries) if (runCatching { startActivity(i) }.isSuccess) return
    }

    // ---------------------------------------------------------------------------------------------------------------
    // The screen

    private fun build() {
        val root = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; setBackgroundColor(Chrome.backColour(this@SettingsActivity)) }
        bar = Chrome.toolbar(this, "Settings")
        root.addView(bar)
        val main = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL }
        if (wide) {
            list = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; setPadding(dp(8), dp(12), dp(8), dp(12)) }
            main.addView(ScrollView(this).apply { addView(list) }, LinearLayout.LayoutParams(dp(240), ViewGroup.LayoutParams.MATCH_PARENT))
        }
        body = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; setPadding(dp(20), dp(8), dp(20), dp(16)) }
        main.addView(ScrollView(this).apply { addView(body); isFillViewport = true }, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.MATCH_PARENT, 1f))
        root.addView(main, LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f))
        setContentView(root)
        Chrome.fitSystemBars(this, root, bar, main)
        show(page)
    }

    private fun show(p: Int) {
        page = p
        body.removeAllViews()
        list?.let { l ->
            l.removeAllViews()
            cats.forEachIndexed { i, c -> l.addView(categoryRow(c, i, selected = i == p)) }
        }
        if (p < 0) {   // a phone: the list of groups first
            bar.title = "Settings"
            cats.forEachIndexed { i, c -> body.addView(categoryRow(c, i, selected = false)) }
        } else {
            val c = cats[p]
            bar.title = if (wide) "Settings" else c.name
            body.addView(TextView(this).apply {
                text = c.name; textSize = 22f; typeface = Typeface.DEFAULT_BOLD; setTextColor(Chrome.textColour(this@SettingsActivity))
                setPadding(0, dp(12), 0, dp(8))
            })
            c.items.forEachIndexed { i, it ->
                if (i > 0) body.addView(View(this).apply { setBackgroundColor(0x22808080) }, LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 1))
                body.addView(itemRow(it))
            }
        }
        body.addView(TextView(this).apply {
            text = "Changes apply at once and are kept."
            alpha = 0.6f; textSize = 13f; setTextColor(Chrome.textColour(this@SettingsActivity))
            setPadding(0, dp(24), 0, 0)
        })
    }

    private fun categoryRow(c: Category, i: Int, selected: Boolean): View {
        val row = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(16), dp(14), dp(16), dp(14))
            isClickable = true
            setOnClickListener { show(i) }
            if (selected) background = GradientDrawable().apply { cornerRadius = dp(8).toFloat(); setColor(0x33C62828) }
        }
        row.addView(TextView(this).apply { text = c.name; textSize = 17f; setTextColor(Chrome.textColour(this@SettingsActivity)); if (selected) typeface = Typeface.DEFAULT_BOLD })
        if (!wide) row.addView(TextView(this).apply { text = c.summary; textSize = 13f; alpha = 0.65f; setTextColor(Chrome.textColour(this@SettingsActivity)) })
        return row
    }

    private fun itemRow(it: Item): View {
        val text = Chrome.textColour(this)
        val row = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER_VERTICAL; setPadding(0, dp(14), 0, dp(14)) }
        val words = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; setPadding(0, 0, dp(12), 0) }
        words.addView(TextView(this).apply { this.text = it.label; textSize = 16f; setTextColor(text) })
        words.addView(TextView(this).apply { this.text = it.help; textSize = 13f; alpha = 0.7f; setTextColor(text); setPadding(0, dp(2), 0, 0) })
        row.addView(words, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
        when (it.kind) {
            Item.Kind.TOGGLE -> {
                val sw = Switch(this).apply { isChecked = it.get() != 0; setOnCheckedChangeListener { _, on -> it.set(if (on) 1 else 0) } }
                row.addView(sw)
                row.setOnClickListener { sw.toggle() }
            }
            Item.Kind.CHOICE -> {
                val value = TextView(this).apply {
                    this.text = it.choices.getOrElse(it.get()) { "" }
                    setTextColor(0xFFE53935.toInt()); textSize = 15f; gravity = Gravity.END
                    maxWidth = dp(150)
                }
                row.addView(value)
                row.setOnClickListener { _ ->
                    AlertDialog.Builder(this).setTitle(it.label).setSingleChoiceItems(it.choices.toTypedArray(), it.get()) { d, i ->
                        it.set(i)
                        value.text = it.choices.getOrElse(i) { "" }
                        d.dismiss()
                    }.setNegativeButton("Cancel", null).show()
                }
            }
            Item.Kind.BUTTON -> row.addView(Button(this).apply { this.text = it.choices.firstOrNull() ?: ""; isAllCaps = false; setOnClickListener { _ -> it.press() } })
        }
        return row
    }

    @Deprecated("Back handling of the platform")
    override fun onBackPressed() {
        if (!wide && page >= 0 && intent.getIntExtra(EXTRA_PAGE, -1) < 0) { show(-1); return }   // back to the list of groups
        @Suppress("DEPRECATION") super.onBackPressed()
    }

    // ---------------------------------------------------------------------------------------------------------------
    // The build check: each group's page as a picture, and settings changed through the screen are kept

    private fun selfTest(out: File) {
        out.mkdirs()
        val lines = ArrayList<String>()
        fun check(what: String, ok: Boolean) { lines.add((if (ok) "ok   " else "FAIL ") + what) }
        fun shot(name: String) = runCatching {
            val v = window.decorView
            val b = Bitmap.createBitmap(v.width, v.height, Bitmap.Config.ARGB_8888)
            v.draw(android.graphics.Canvas(b))
            FileOutputStream(File(out, "$name.png")).use { b.compress(Bitmap.CompressFormat.PNG, 100, it) }
        }
        val h = Handler(Looper.getMainLooper())
        val steps = ArrayList<() -> Unit>()
        steps.add { show(-1); }
        steps.add { shot("08-settings"); check("the groups are listed (${cats.size})", cats.size == 5 && body.childCount >= 5) }
        cats.indices.forEach { i ->
            steps.add { show(i) }
            steps.add { shot("09-settings-${i + 1}-${cats[i].name.lowercase().replace(' ', '-')}"); check("${cats[i].name}: its ${cats[i].items.size} settings shown", body.childCount >= cats[i].items.size + 2) }
        }
        steps.add {
            // as a person would: tap the setting's row, then a choice (the dialog is the same code; its result is set)
            val hl = cats[2].items[0]; hl.set(2)
            check("Editing > Highlight colour: Blue is kept", Settings.highlightIndex(this) == 2 && Settings.highlightColour(this) == Settings.colour(2))
            val keep = cats[1].items[2]; keep.set(1)
            check("PDF Viewing > Keep the screen on: kept", Settings.keepScreenOn(this))
            val tools = cats[0].items[1]; tools.set(1)
            check("Appearance > Tools at the bottom: kept", !Settings.toolsOnTop(this))
            Settings.reset(this)
            check("Reset puts them back", Settings.highlightIndex(this) == 0 && !Settings.keepScreenOn(this) && Settings.toolsOnTop(this))
        }
        steps.add {
            lines.add(if (lines.none { it.startsWith("FAIL") }) "ALL PASSED" else "SOME FAILED")
            File(out, "settings-report.txt").writeText(lines.joinToString("\n") + "\nDONE\n")
            finish()
        }
        fun run(i: Int) { if (i < steps.size) { runCatching(steps[i]).onFailure { check("step ${i + 1} crashed: $it", false) }; h.postDelayed({ run(i + 1) }, 700) } }
        run(0)
    }

    companion object {
        const val EXTRA_PAGE = "com.featherpdf.settings.page"
        const val EXTRA_SELFTEST = "com.featherpdf.settings.selftest"
    }
}
