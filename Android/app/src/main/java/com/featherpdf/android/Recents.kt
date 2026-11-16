package com.featherpdf.android

import android.content.Context
import org.json.JSONArray
import org.json.JSONObject

/** The documents opened lately (their content:// address, name, and the page last read), newest first. */
object Recents {
    data class Entry(val uri: String, val name: String, val page: Int, val time: Long)

    private const val PREFS = "recents"
    private const val KEY = "list"
    private const val MAX = 20

    fun list(ctx: Context): List<Entry> {
        val raw = ctx.getSharedPreferences(PREFS, Context.MODE_PRIVATE).getString(KEY, "[]") ?: "[]"
        return runCatching {
            val a = JSONArray(raw)
            (0 until a.length()).map { i ->
                val o = a.getJSONObject(i)
                Entry(o.getString("uri"), o.optString("name", "Document"), o.optInt("page", 0), o.optLong("time", 0))
            }
        }.getOrDefault(emptyList())
    }

    private fun write(ctx: Context, items: List<Entry>) {
        val a = JSONArray()
        items.take(MAX).forEach { a.put(JSONObject().put("uri", it.uri).put("name", it.name).put("page", it.page).put("time", it.time)) }
        // commit, not apply: the list is on disk before the app can be closed
        ctx.getSharedPreferences(PREFS, Context.MODE_PRIVATE).edit().putString(KEY, a.toString()).commit()
    }

    @Synchronized fun add(ctx: Context, uri: String, name: String, page: Int = -1) {
        val old = list(ctx)
        val keepPage = if (page >= 0) page else old.firstOrNull { it.uri == uri }?.page ?: 0
        write(ctx, listOf(Entry(uri, name, keepPage, System.currentTimeMillis())) + old.filter { it.uri != uri })
    }

    @Synchronized fun setPage(ctx: Context, uri: String, page: Int) {
        val old = list(ctx)
        if (old.none { it.uri == uri }) return
        write(ctx, old.map { if (it.uri == uri) it.copy(page = page) else it })
    }

    @Synchronized fun remove(ctx: Context, uri: String) = write(ctx, list(ctx).filter { it.uri != uri })
    @Synchronized fun clear(ctx: Context) = write(ctx, emptyList())
    fun pageOf(ctx: Context, uri: String): Int = list(ctx).firstOrNull { it.uri == uri }?.page ?: 0
}
