package com.featherpdf.android

import android.app.Activity
import android.app.AlertDialog
import android.content.Context

/** The app's settings (kept on the phone): where the tools are, and the page colours. */
object Settings {
    private const val PREFS = "settings"
    private const val TOOLS_ON_TOP = "toolsOnTop"

    fun toolsOnTop(c: Context) = c.getSharedPreferences(PREFS, Context.MODE_PRIVATE).getBoolean(TOOLS_ON_TOP, true)
    fun setToolsOnTop(c: Context, v: Boolean) { c.getSharedPreferences(PREFS, Context.MODE_PRIVATE).edit().putBoolean(TOOLS_ON_TOP, v).apply() }

    fun show(a: Activity) {
        val viewer = a as? ViewerActivity
        val items = mutableListOf("Tools: " + if (toolsOnTop(a)) "at the top" else "at the bottom")
        if (viewer != null) items.add("Page colours…")
        AlertDialog.Builder(a).setTitle("Settings").setItems(items.toTypedArray()) { _, i ->
            when (i) {
                0 -> chooseToolPlace(a)
                1 -> viewer?.chooseColours()
            }
        }.setPositiveButton("Close", null).show()
    }

    private fun chooseToolPlace(a: Activity) {
        val choices = arrayOf("At the top, under the title", "At the bottom of the screen")
        AlertDialog.Builder(a).setTitle("Tools (Read, Edit text, Highlight …)")
            .setSingleChoiceItems(choices, if (toolsOnTop(a)) 0 else 1) { d, i ->
                setToolsOnTop(a, i == 0)
                (a as? ViewerActivity)?.placeTools(i == 0)
                d.dismiss()
            }.show()
    }
}
