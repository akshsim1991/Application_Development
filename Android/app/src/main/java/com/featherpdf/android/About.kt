package com.featherpdf.android

import android.app.Activity
import android.app.AlertDialog
import android.content.Intent
import android.content.pm.PackageManager
import android.graphics.Typeface
import android.net.Uri
import android.text.method.LinkMovementMethod
import android.text.util.Linkify
import android.widget.ScrollView
import android.widget.TextView
import android.widget.Toast
import com.featherpdf.engine.Engine

/** About Feather PDF: who made it (the same words as the desktop apps), privacy, the open-source licences. */
object About {
    const val PRIVACY_URL = "https://github.com/akshsim1991/FeatherPDF-releases/blob/main/PRIVACY.md"

    private fun dp(a: Activity, v: Int) = (v * a.resources.displayMetrics.density).toInt()

    fun version(a: Activity): String {
        val v = runCatching { a.packageManager.getPackageInfo(a.packageName, 0).versionName }.getOrNull() ?: ""
        val name = runCatching {
            @Suppress("DEPRECATION")
            a.packageManager.getApplicationInfo(a.packageName, PackageManager.GET_META_DATA).metaData?.getString("com.featherpdf.codename")
        }.getOrNull().orEmpty()
        return if (name.isEmpty()) v else "$v “$name”"
    }

    private fun scrollingText(a: Activity, text: CharSequence, mono: Boolean = false): ScrollView {
        val t = TextView(a).apply {
            this.text = text
            setTextIsSelectable(true)
            textSize = if (mono) 12f else 15f
            if (mono) typeface = Typeface.MONOSPACE
            setPadding(dp(a, 22), dp(a, 12), dp(a, 22), dp(a, 8))
            setLineSpacing(0f, 1.15f)
            if (!mono) { Linkify.addLinks(this, Linkify.EMAIL_ADDRESSES or Linkify.WEB_URLS); movementMethod = LinkMovementMethod.getInstance() }
        }
        return ScrollView(a).apply { addView(t) }
    }

    fun show(a: Activity) {
        val text = "Version ${version(a)} for Android\n\n" +
            "A fast, lightweight PDF reader and editor for Android. Read and search documents; change their text; add text, " +
            "highlights, drawings and notes; turn and delete pages; save and share. Everything happens on your phone, with " +
            "no account, cloud service or tracking.\n\n" +
            Engine.aboutCredits() + "\n\n" +
            "PRIVACY\n" +
            "Feather PDF does not collect, store or send any personal data. It has no permission to use the internet; your " +
            "documents are opened only when you choose them, and stay on your phone. The privacy policy: $PRIVACY_URL"
        AlertDialog.Builder(a).setTitle("Feather PDF")
            .setView(scrollingText(a, text))
            .setPositiveButton("Close", null)
            .setNeutralButton("Open-source licences") { _, _ -> licences(a) }
            .setNegativeButton("Write to the developer") { _, _ -> write(a) }
            .show()
    }

    private fun write(a: Activity) {
        val i = Intent(Intent.ACTION_SENDTO, Uri.parse("mailto:" + Engine.feedbackAddress()))
            .putExtra(Intent.EXTRA_SUBJECT, "Feather PDF ${version(a)} (Android)")
        runCatching { a.startActivity(i) }.onFailure { Toast.makeText(a, "No e-mail app: write to ${Engine.feedbackAddress()}", Toast.LENGTH_LONG).show() }
    }

    /** The licences of the open-source parts inside the app (assets/licenses, from the PDFium package and the NDK). */
    fun licences(a: Activity) {
        val files = runCatching { a.assets.list("licenses")?.sorted() }.getOrNull().orEmpty()
        val intro = "Feather PDF uses these open-source parts, under the licences listed (tap one to read it):\n" +
            "PDFium (BSD-3-Clause, Apache-2.0) with FreeType, libjpeg-turbo, libpng, zlib, OpenJPEG, Little CMS, " +
            "Anti-Grain Geometry, ICU, Abseil, fast_float, simdutf; and the C++ library of the Android NDK (libc++, Apache-2.0 " +
            "with LLVM exception); and fonts made from Latin Modern Math for equations in TeX documents (GUST Font License)."
        val names = files.map { it.substringBeforeLast('.').replace('_', ' ') }.toTypedArray()
        AlertDialog.Builder(a).setTitle("Open-source licences")
            .setMessage(if (files.isEmpty()) "$intro\n\n(The licence texts are not in this build.)" else null)
            .apply {
                if (files.isNotEmpty()) setItems(names) { _, i -> showLicence(a, files[i]) }
                if (files.isNotEmpty()) setCustomTitle(TextView(a).apply {
                    this.text = intro; setPadding(dp(a, 22), dp(a, 18), dp(a, 22), dp(a, 8)); textSize = 14f
                })
            }
            .setPositiveButton("Close", null).show()
    }

    private fun showLicence(a: Activity, file: String) {
        val text = runCatching { a.assets.open("licenses/$file").bufferedReader().use { it.readText() } }.getOrDefault("(not readable)")
        AlertDialog.Builder(a).setTitle(file).setView(scrollingText(a, text, mono = true)).setPositiveButton("Close", null).show()
    }
}
