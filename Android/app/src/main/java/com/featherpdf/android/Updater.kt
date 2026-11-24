package com.featherpdf.android

import android.app.Activity
import android.app.AlertDialog
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.widget.LinearLayout
import android.widget.ProgressBar
import android.widget.TextView
import android.widget.Toast
import org.json.JSONObject
import java.io.File
import java.io.FileOutputStream
import java.net.HttpURLConnection
import java.net.URL
import java.security.MessageDigest
import kotlin.concurrent.thread

/**
 * New versions, as Feather PDF for Windows and macOS finds them: the releases page's latest.json says the newest
 * version, what is new, and for each download its address, size and SHA-256. A newer one is offered (Update now or
 * Later); "Update now" downloads the APK for this phone, checks its size and fingerprint, and hands it to Android's
 * installer, which asks before it installs (the same signing key, so it replaces this one and keeps everything).
 * Settings > Advanced: check when the app opens (at most once a day), or now. Installed from Google Play, updates come
 * from there and only its page is opened.
 */
object Updater {
    const val LATEST = "https://raw.githubusercontent.com/akshsim1991/FeatherPDF-releases/main/latest.json"
    private const val MIRROR = "https://cdn.jsdelivr.net/gh/akshsim1991/FeatherPDF-releases@main/latest.json"   // when GitHub cannot be reached
    private const val PLAY = "com.android.vending"

    class Release(val version: String, val notes: String, val url: String, val size: Long, val sha256: String)

    private val main = Handler(Looper.getMainLooper())
    private var busy = false

    fun version(c: Context): String = runCatching { c.packageManager.getPackageInfo(c.packageName, 0).versionName }.getOrNull() ?: "0"

    /** -1, 0 or 1 as a is older than, the same as or newer than b (numbers compared part by part: 2.25.10 > 2.25.9). */
    fun compare(a: String, b: String): Int {
        val x = a.split('.').map { it.toIntOrNull() ?: 0 }; val y = b.split('.').map { it.toIntOrNull() ?: 0 }
        for (i in 0 until maxOf(x.size, y.size)) {
            val d = x.getOrElse(i) { 0 } - y.getOrElse(i) { 0 }
            if (d != 0) return if (d > 0) 1 else -1
        }
        return 0
    }

    private fun fromPlay(c: Context): Boolean = runCatching {
        if (Build.VERSION.SDK_INT >= 30) c.packageManager.getInstallSourceInfo(c.packageName).installingPackageName == PLAY
        else @Suppress("DEPRECATION") c.packageManager.getInstallerPackageName(c.packageName) == PLAY
    }.getOrDefault(false)

    /** On a background thread: the newest release (the APK for this phone's kind: 64-bit, or the 32-bit one). */
    private fun fetch(): Release {
        val text = runCatching { get(LATEST) }.recoverCatching { get(MIRROR) }.getOrThrow().toString(Charsets.UTF_8)
        val j = JSONObject(text)
        val only32 = Build.SUPPORTED_64_BIT_ABIS.isEmpty()
        val a = if (only32 && j.has("android32")) j.getJSONObject("android32") else j.getJSONObject("android")
        return Release(j.getString("version"), j.optString("notes", ""), a.getString("url"), a.optLong("size", -1), a.optString("sha256", ""))
    }

    private fun get(address: String, progress: ((Long) -> Unit)? = null, into: File? = null): ByteArray {
        var url = URL(address)
        repeat(5) {   // (follows a few redirects, also between http and https)
            val c = url.openConnection() as HttpURLConnection
            c.connectTimeout = 15000; c.readTimeout = 30000
            c.instanceFollowRedirects = true
            c.setRequestProperty("User-Agent", "FeatherPDF-Android")
            val code = c.responseCode
            if (code in 300..399) { url = URL(url, c.getHeaderField("Location")); c.disconnect(); return@repeat }
            if (code != 200) { c.disconnect(); throw IllegalStateException("the server answered $code") }
            c.inputStream.use { input ->
                if (into == null) return input.readBytes()
                FileOutputStream(into).use { out ->
                    val buf = ByteArray(64 * 1024); var total = 0L
                    while (true) { val n = input.read(buf); if (n < 0) break; out.write(buf, 0, n); total += n; progress?.invoke(total) }
                }
                return ByteArray(0)
            }
        }
        throw IllegalStateException("too many redirects")
    }

    /** When the app opens: at most once a day, quietly unless there is a new version (Settings > Advanced). */
    fun checkOnOpen(a: Activity) {
        if (!Settings.checkUpdates(a)) return
        val now = System.currentTimeMillis()
        if (now - Settings.lastUpdateCheck(a) < 20 * 3600 * 1000L) return
        Settings.setLastUpdateCheck(a, now)
        check(a, manual = false)
    }

    /** Checks now; `manual` says so also when there is nothing new or the check failed. */
    fun check(a: Activity, manual: Boolean) {
        if (busy) return
        busy = true
        if (manual) Toast.makeText(a, "Checking for a new version…", Toast.LENGTH_SHORT).show()
        thread(name = "update-check") {
            val r = runCatching { fetch() }
            main.post {
                busy = false
                if (a.isFinishing || a.isDestroyed) return@post
                r.onSuccess { rel ->
                    if (compare(rel.version, version(a)) > 0) offer(a, rel)
                    else if (manual) AlertDialog.Builder(a).setTitle("Feather PDF is up to date")
                        .setMessage("Version ${version(a)} is the newest.").setPositiveButton("OK", null).show()
                }.onFailure {
                    if (manual) AlertDialog.Builder(a).setTitle("No answer from the releases page")
                        .setMessage("Is the phone online? (${it.message ?: it.javaClass.simpleName})").setPositiveButton("OK", null).show()
                }
            }
        }
    }

    private fun offer(a: Activity, r: Release) {
        AlertDialog.Builder(a).setTitle("Feather PDF ${r.version} is available")
            .setMessage((if (r.notes.isNotBlank()) r.notes + "\n\n" else "") + "You have ${version(a)}.")
            .setPositiveButton("Update now") { _, _ -> update(a, r) }
            .setNegativeButton("Later", null).show()
    }

    private fun update(a: Activity, r: Release) {
        if (fromPlay(a)) {   // installed from Google Play: it updates the app
            runCatching { a.startActivity(Intent(Intent.ACTION_VIEW, Uri.parse("market://details?id=${a.packageName}"))) }
            return
        }
        // Android 8 and later ask once whether Feather PDF may install apps (its own updates)
        if (Build.VERSION.SDK_INT >= 26 && !a.packageManager.canRequestPackageInstalls()) {
            AlertDialog.Builder(a).setTitle("Allow Feather PDF to install its updates")
                .setMessage("Android asks once: turn on “Allow from this source” for Feather PDF, come back, and tap Update now again.")
                .setPositiveButton("Open the setting") { _, _ ->
                    runCatching { a.startActivity(Intent(android.provider.Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES, Uri.parse("package:${a.packageName}"))) }
                }
                .setNegativeButton("Cancel", null).show()
            return
        }
        val bar = ProgressBar(a, null, android.R.attr.progressBarStyleHorizontal).apply { isIndeterminate = r.size <= 0; max = 1000 }
        val label = TextView(a)
        val pad = (20 * a.resources.displayMetrics.density).toInt()
        val box = LinearLayout(a).apply { orientation = LinearLayout.VERTICAL; setPadding(pad, pad / 2, pad, 0); addView(bar); addView(label) }
        val dlg = AlertDialog.Builder(a).setTitle("Downloading Feather PDF ${r.version}").setView(box).setCancelable(false).show()
        val dir = File(a.cacheDir, "share").apply { mkdirs() }
        val apk = File(dir, "FeatherPDF-${r.version}.apk")
        thread(name = "update-download") {
            val done = runCatching {
                get(r.url, progress = { n ->
                    main.post {
                        if (r.size > 0) bar.progress = (n * 1000 / r.size).toInt()
                        label.text = "%.1f of %.1f MB".format(n / 1048576.0, maxOf(r.size, n) / 1048576.0)
                    }
                }, into = apk)
                if (r.size > 0 && apk.length() != r.size) throw IllegalStateException("the download is incomplete")
                if (r.sha256.isNotBlank()) {
                    val md = MessageDigest.getInstance("SHA-256")
                    apk.inputStream().use { s -> val buf = ByteArray(64 * 1024); while (true) { val n = s.read(buf); if (n < 0) break; md.update(buf, 0, n) } }
                    val hex = md.digest().joinToString("") { "%02x".format(it) }
                    if (!hex.equals(r.sha256, ignoreCase = true)) throw IllegalStateException("the download is not the published file (its fingerprint differs)")
                }
            }
            main.post {
                runCatching { dlg.dismiss() }
                done.onSuccess { install(a, apk) }.onFailure {
                    apk.delete()
                    AlertDialog.Builder(a).setTitle("The update was not downloaded").setMessage(it.message ?: it.javaClass.simpleName)
                        .setPositiveButton("OK", null).show()
                }
            }
        }
    }

    /** Hands the APK to Android's installer (it asks before installing). */
    private fun install(a: Activity, apk: File) {
        val u = Uri.parse("content://${ShareProvider.AUTHORITY}/${Uri.encode(apk.name)}")
        val i = Intent(Intent.ACTION_VIEW).setDataAndType(u, ShareProvider.APK).addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        runCatching { a.startActivity(i) }.onFailure { Toast.makeText(a, "Android's installer could not be opened.", Toast.LENGTH_LONG).show() }
    }
}
