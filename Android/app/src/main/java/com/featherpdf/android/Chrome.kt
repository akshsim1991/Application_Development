package com.featherpdf.android

import android.app.Activity
import android.graphics.Color
import android.os.Build
import android.view.ContextThemeWrapper
import android.view.View
import android.view.WindowInsets
import android.view.WindowInsetsController
import android.widget.Toolbar

/**
 * The title bar and the system bars, the same on every Android version: the app draws under the status bar and the
 * navigation (gesture) bar (Android 15 always does so), and moves its own bars out of their way, so no page or button
 * is hidden behind them.
 */
object Chrome {
    fun isNight(a: Activity) = Settings.isDark(a)   // the theme chosen in Settings, or the phone's
    fun barColour(a: Activity) = if (isNight(a)) 0xFF8E1B1B.toInt() else 0xFFC62828.toInt()
    fun bottomColour(a: Activity) = if (Settings.isAmoled(a)) Color.BLACK else if (isNight(a)) 0xFF1E2124.toInt() else Color.WHITE
    fun backColour(a: Activity) = if (Settings.isAmoled(a)) Color.BLACK else if (isNight(a)) 0xFF121417.toInt() else 0xFFECEFF1.toInt()
    fun textColour(a: Activity) = if (isNight(a)) 0xFFE8EAED.toInt() else 0xFF202124.toInt()

    /** The app's own title bar (white text on red), set as the activity's action bar so its menu works as before. */
    fun toolbar(a: Activity, title: CharSequence): Toolbar {
        val t = Toolbar(ContextThemeWrapper(a, android.R.style.ThemeOverlay_Material_Dark_ActionBar))
        t.setBackgroundColor(barColour(a))
        t.title = title
        t.popupTheme = if (isNight(a)) android.R.style.ThemeOverlay_Material_Dark else android.R.style.ThemeOverlay_Material_Light
        a.setActionBar(t)
        return t
    }

    /**
     * Draws under the system bars and pads `top` (the title bar) by the status bar, `bottom` (the tool row, or the
     * content) by the navigation bar and the keyboard, and `root` at the sides (a camera cut-out in landscape).
     */
    @Suppress("DEPRECATION")
    fun fitSystemBars(a: Activity, root: View, top: View, bottom: View) {
        val w = a.window
        if (Build.VERSION.SDK_INT >= 30) {
            w.setDecorFitsSystemWindows(false)
            w.insetsController?.setSystemBarsAppearance(
                if (isNight(a)) 0 else WindowInsetsController.APPEARANCE_LIGHT_NAVIGATION_BARS,
                WindowInsetsController.APPEARANCE_LIGHT_STATUS_BARS or WindowInsetsController.APPEARANCE_LIGHT_NAVIGATION_BARS)
        } else {
            var flags = View.SYSTEM_UI_FLAG_LAYOUT_STABLE or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN or View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
            if (Build.VERSION.SDK_INT >= 26 && !isNight(a)) flags = flags or View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR
            w.decorView.systemUiVisibility = flags
        }
        if (Build.VERSION.SDK_INT < 35) {   // Android 15 makes both bars see-through itself
            w.statusBarColor = Color.TRANSPARENT
            // before Android 8 the navigation buttons are always white: keep a dark bar behind them
            w.navigationBarColor = if (Build.VERSION.SDK_INT >= 26) Color.TRANSPARENT else 0xFF202020.toInt()
        }
        val topPad = intArrayOf(top.paddingLeft, top.paddingTop, top.paddingRight, top.paddingBottom)
        val bottomPad = intArrayOf(bottom.paddingLeft, bottom.paddingTop, bottom.paddingRight, bottom.paddingBottom)
        root.setOnApplyWindowInsetsListener { v, insets ->
            val l: Int; val t: Int; val r: Int; val b: Int
            if (Build.VERSION.SDK_INT >= 30) {
                val i = insets.getInsets(WindowInsets.Type.systemBars() or WindowInsets.Type.displayCutout() or WindowInsets.Type.ime())
                l = i.left; t = i.top; r = i.right; b = i.bottom
            } else {
                l = insets.systemWindowInsetLeft; t = insets.systemWindowInsetTop; r = insets.systemWindowInsetRight; b = insets.systemWindowInsetBottom
            }
            v.setPadding(l, 0, r, 0)
            top.setPadding(topPad[0], topPad[1] + t, topPad[2], topPad[3])
            bottom.setPadding(bottomPad[0], bottomPad[1], bottomPad[2], bottomPad[3] + b)
            insets
        }
        root.requestApplyInsets()
    }
}
