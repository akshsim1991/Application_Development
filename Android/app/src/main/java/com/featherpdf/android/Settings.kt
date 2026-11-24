package com.featherpdf.android

import android.app.Activity
import android.content.Context
import android.content.Intent
import android.content.res.Configuration
import com.featherpdf.engine.Engine

/**
 * Every preference of the Android app, kept on the phone, in the groups of Feather PDF for Windows (Settings there):
 * Appearance, PDF Viewing, Editing, Performance, Advanced. Tabs and File Explorer have no counterpart on a phone.
 * SettingsActivity shows them; a change applies at once and is kept.
 */
object Settings {
    private const val PREFS = "settings"
    private fun p(c: Context) = c.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
    private fun int(c: Context, k: String, d: Int) = p(c).getInt(k, d)
    private fun bool(c: Context, k: String, d: Boolean) = p(c).getBoolean(k, d)
    private fun put(c: Context, k: String, v: Int) = p(c).edit().putInt(k, v).apply()
    private fun put(c: Context, k: String, v: Boolean) = p(c).edit().putBoolean(k, v).apply()

    // ---- Appearance
    const val THEME_LIGHT = 0; const val THEME_DARK = 1; const val THEME_AMOLED = 2; const val THEME_SYSTEM = 3
    fun theme(c: Context) = int(c, "theme", THEME_SYSTEM)
    fun setTheme(c: Context, v: Int) = put(c, "theme", v)
    fun toolsOnTop(c: Context) = bool(c, "toolsOnTop", true)
    fun setToolsOnTop(c: Context, v: Boolean) = put(c, "toolsOnTop", v)

    /** Whether the app is dark now: the theme chosen, or the phone's own when "Like the phone". */
    fun isDark(c: Context) = when (theme(c)) {
        THEME_LIGHT -> false
        THEME_DARK, THEME_AMOLED -> true
        else -> (c.resources.configuration.uiMode and Configuration.UI_MODE_NIGHT_MASK) == Configuration.UI_MODE_NIGHT_YES
    }
    fun isAmoled(c: Context) = theme(c) == THEME_AMOLED

    /** Call before super.onCreate: the activity takes the theme chosen. */
    fun applyTheme(a: Activity) {
        val name = when (theme(a)) {
            THEME_LIGHT -> "FeatherTheme.Light"
            THEME_DARK -> "FeatherTheme.Dark"
            THEME_AMOLED -> "FeatherTheme.Amoled"
            else -> "FeatherTheme"
        }
        val id = a.resources.getIdentifier(name, "style", a.packageName)
        if (id != 0) a.setTheme(id)
    }

    // ---- PDF Viewing
    fun pageColours(c: Context) = int(c, "pageColours", Engine.COLOURS_NORMAL)
    fun setPageColours(c: Context, v: Int) = put(c, "pageColours", v)
    fun restorePositions(c: Context) = bool(c, "restorePositions", true)
    fun setRestorePositions(c: Context, v: Boolean) = put(c, "restorePositions", v)
    fun keepScreenOn(c: Context) = bool(c, "keepScreenOn", false)
    fun setKeepScreenOn(c: Context, v: Boolean) = put(c, "keepScreenOn", v)

    // ---- Editing (the colour names of Feather PDF for Windows)
    val colourNames = listOf("Yellow", "Green", "Blue", "Pink", "Orange", "Red", "Purple")
    private val colourValues = intArrayOf(0xFFFFEB3B.toInt(), 0xFF66BB6A.toInt(), 0xFF42A5F5.toInt(), 0xFFF06292.toInt(),
                                          0xFFFFA726.toInt(), 0xFFE53935.toInt(), 0xFFAB47BC.toInt())
    fun colour(i: Int) = colourValues[i.coerceIn(0, colourValues.size - 1)]
    fun highlightIndex(c: Context) = int(c, "highlightColour", 0)
    fun setHighlightIndex(c: Context, v: Int) = put(c, "highlightColour", v)
    fun highlightColour(c: Context) = colour(highlightIndex(c))
    fun drawIndex(c: Context) = int(c, "drawingColour", 5)
    fun setDrawIndex(c: Context, v: Int) = put(c, "drawingColour", v)
    fun drawColour(c: Context) = colour(drawIndex(c))
    val widthNames = listOf("Thin", "Medium", "Thick")
    private val widthValues = floatArrayOf(1f, 2.5f, 5f)
    fun widthIndex(c: Context) = int(c, "lineWidth", 1)
    fun setWidthIndex(c: Context, v: Int) = put(c, "lineWidth", v)
    fun lineWidth(c: Context) = widthValues[widthIndex(c).coerceIn(0, 2)]

    // ---- Performance
    fun cacheIndex(c: Context) = int(c, "pageCache", 1)
    fun setCacheIndex(c: Context, v: Int) = put(c, "pageCache", v)
    /** Bytes of drawn pages kept ready: few, normal, many (never more than a third of what the app may use). */
    fun cacheBytes(c: Context): Long {
        val mb = longArrayOf(32, 96, 192)[cacheIndex(c).coerceIn(0, 2)]
        return minOf(mb shl 20, Runtime.getRuntime().maxMemory() / 3)
    }

    // ---- Viewing: the sidebar (page thumbnails and bookmarks), shown or not
    fun sidebarOpen(c: Context) = bool(c, "sidebarOpen", c.resources.configuration.screenWidthDp >= 600)
    fun setSidebarOpen(c: Context, v: Boolean) = put(c, "sidebarOpen", v)

    // ---- Advanced: new versions (Updater)
    fun checkUpdates(c: Context) = bool(c, "checkUpdates", true)
    fun setCheckUpdates(c: Context, v: Boolean) = put(c, "checkUpdates", v)
    fun lastUpdateCheck(c: Context) = p(c).getLong("lastUpdateCheck", 0L)
    fun setLastUpdateCheck(c: Context, v: Long) = p(c).edit().putLong("lastUpdateCheck", v).apply()

    /** Every setting back as it was when the app was installed (recent files are kept). */
    fun reset(c: Context) { p(c).edit().clear().apply() }

    /** The Settings screen, on the page asked for. */
    fun show(a: Activity, page: Int = -1) {
        a.startActivity(Intent(a, SettingsActivity::class.java).putExtra(SettingsActivity.EXTRA_PAGE, page))
    }
}
