package com.featherpdf.android

import android.app.Activity
import android.app.AlertDialog
import android.content.Intent
import android.content.res.Configuration
import android.graphics.Typeface
import android.net.Uri
import android.os.Bundle
import android.text.format.DateUtils
import android.view.Menu
import android.view.MenuItem
import android.view.View
import android.view.ViewGroup
import android.widget.BaseAdapter
import android.widget.Button
import android.widget.LinearLayout
import android.widget.ListView
import android.widget.TextView
import android.widget.Toast

/** The start screen: open a PDF, or one of the recent ones. */
class MainActivity : Activity() {
    private lateinit var list: ListView
    private lateinit var empty: TextView
    private var items: List<Recents.Entry> = emptyList()
    private fun dp(v: Int) = (v * resources.displayMetrics.density).toInt()
    private fun isNight() = (resources.configuration.uiMode and Configuration.UI_MODE_NIGHT_MASK) == Configuration.UI_MODE_NIGHT_YES

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val root = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; setPadding(dp(16), dp(16), dp(16), 0) }
        val open = Button(this).apply {
            text = "Open a PDF"
            isAllCaps = false
            textSize = 17f
            setOnClickListener { pick() }
        }
        root.addView(open, LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(56)))
        root.addView(TextView(this).apply {
            text = "Recent"
            typeface = Typeface.DEFAULT_BOLD
            setPadding(dp(4), dp(20), 0, dp(6))
        })
        empty = TextView(this).apply {
            text = "Documents you open appear here."
            setPadding(dp(4), dp(8), 0, 0)
            alpha = 0.7f
        }
        root.addView(empty)
        list = ListView(this).apply {
            setOnItemClickListener { _, _, pos, _ -> openRecent(items[pos]) }
            setOnItemLongClickListener { _, _, pos, _ -> forget(items[pos]); true }
        }
        root.addView(list, LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f))
        setContentView(root)
        FeatherApp.startError?.let { AlertDialog.Builder(this).setTitle("Feather PDF").setMessage(it).setPositiveButton("OK", null).show() }
    }

    override fun onResume() {
        super.onResume()
        items = Recents.list(this)
        empty.visibility = if (items.isEmpty()) View.VISIBLE else View.GONE
        list.adapter = object : BaseAdapter() {
            override fun getCount() = items.size
            override fun getItem(p: Int) = items[p]
            override fun getItemId(p: Int) = p.toLong()
            override fun getView(p: Int, convert: View?, parent: ViewGroup?): View {
                val e = items[p]
                val row = (convert as? LinearLayout) ?: LinearLayout(this@MainActivity).apply {
                    orientation = LinearLayout.VERTICAL
                    setPadding(dp(4), dp(10), dp(4), dp(10))
                    addView(TextView(context).apply { textSize = 16f; isSingleLine = true })
                    addView(TextView(context).apply { textSize = 12f; alpha = 0.65f })
                }
                (row.getChildAt(0) as TextView).text = e.name
                (row.getChildAt(1) as TextView).text = "Page ${e.page + 1} · " +
                    DateUtils.getRelativeTimeSpanString(e.time, System.currentTimeMillis(), DateUtils.MINUTE_IN_MILLIS)
                return row
            }
        }
    }

    override fun onCreateOptionsMenu(menu: Menu): Boolean {
        menu.add(0, 1, 0, "Clear the recent list")
        menu.add(0, 2, 1, "About Feather PDF")
        return true
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        when (item.itemId) {
            1 -> { Recents.clear(this); onResume() }
            2 -> {
                val v = packageManager.getPackageInfo(packageName, 0).versionName
                AlertDialog.Builder(this).setTitle("Feather PDF $v")
                    .setMessage("Free, for ever. Reads, finds, edits text, highlights, draws, adds notes, turns and deletes pages, and saves.")
                    .setPositiveButton("OK", null).show()
            }
            else -> return super.onOptionsItemSelected(item)
        }
        return true
    }

    private fun pick() {
        val i = Intent(Intent.ACTION_OPEN_DOCUMENT).addCategory(Intent.CATEGORY_OPENABLE).setType("application/pdf")
        runCatching { startActivityForResult(i, REQ_OPEN) }.onFailure { Toast.makeText(this, "No app to choose files with.", Toast.LENGTH_LONG).show() }
    }

    @Deprecated("Activity result API of the platform")
    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        @Suppress("DEPRECATION") super.onActivityResult(requestCode, resultCode, data)
        if (requestCode != REQ_OPEN || resultCode != RESULT_OK) return
        val u = data?.data ?: return
        // kept across restarts, so the recent list can open it again (and saving can write it back)
        val flags = Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_GRANT_WRITE_URI_PERMISSION
        runCatching { contentResolver.takePersistableUriPermission(u, flags) }
            .onFailure { runCatching { contentResolver.takePersistableUriPermission(u, Intent.FLAG_GRANT_READ_URI_PERMISSION) } }
        view(u)
    }

    private fun view(u: Uri) {
        startActivity(Intent(this, ViewerActivity::class.java).setAction(Intent.ACTION_VIEW).setDataAndType(u, "application/pdf")
            .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_GRANT_WRITE_URI_PERMISSION))
    }

    private fun openRecent(e: Recents.Entry) {
        val u = Uri.parse(e.uri)
        val allowed = contentResolver.persistedUriPermissions.any { it.uri == u && it.isReadPermission }
        if (!allowed) {
            AlertDialog.Builder(this).setTitle(e.name).setMessage("Feather PDF may no longer open this file. Open it again from its folder.")
                .setPositiveButton("Open a PDF") { _, _ -> pick() }.setNeutralButton("Remove from the list") { _, _ -> forget(e) }
                .setNegativeButton("Cancel", null).show()
            return
        }
        view(u)
    }

    private fun forget(e: Recents.Entry) {
        Recents.remove(this, e.uri)
        runCatching { contentResolver.releasePersistableUriPermission(Uri.parse(e.uri), Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_GRANT_WRITE_URI_PERMISSION) }
        onResume()
    }


    companion object { private const val REQ_OPEN = 1 }
}
