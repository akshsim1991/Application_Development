package com.featherpdf.android

import android.content.ContentProvider
import android.content.ContentValues
import android.database.Cursor
import android.database.MatrixCursor
import android.net.Uri
import android.os.ParcelFileDescriptor
import android.provider.OpenableColumns
import java.io.File
import java.io.FileNotFoundException

/**
 * Lends the copies in cache/share to the app a document is shared with (content://com.featherpdf.android.share/<name>),
 * read only, for the permission the share grants.
 */
class ShareProvider : ContentProvider() {
    private fun fileOf(uri: Uri): File {
        val ctx = context ?: throw FileNotFoundException()
        val dir = File(ctx.cacheDir, "share")
        val name = uri.lastPathSegment ?: throw FileNotFoundException()
        val f = File(dir, name)
        if (f.parentFile?.canonicalPath != dir.canonicalPath || !f.isFile) throw FileNotFoundException(name)
        return f
    }

    override fun onCreate() = true
    override fun getType(uri: Uri) = "application/pdf"
    override fun openFile(uri: Uri, mode: String): ParcelFileDescriptor =
        ParcelFileDescriptor.open(fileOf(uri), ParcelFileDescriptor.MODE_READ_ONLY)

    override fun query(uri: Uri, projection: Array<out String>?, selection: String?, selectionArgs: Array<out String>?, sortOrder: String?): Cursor {
        val f = fileOf(uri)
        val cols = projection ?: arrayOf(OpenableColumns.DISPLAY_NAME, OpenableColumns.SIZE)
        val c = MatrixCursor(cols)
        c.addRow(cols.map { if (it == OpenableColumns.DISPLAY_NAME) f.name else if (it == OpenableColumns.SIZE) f.length() else null }.toTypedArray())
        return c
    }

    override fun insert(uri: Uri, values: ContentValues?): Uri? = null
    override fun delete(uri: Uri, selection: String?, selectionArgs: Array<out String>?) = 0
    override fun update(uri: Uri, values: ContentValues?, selection: String?, selectionArgs: Array<out String>?) = 0

    companion object {
        const val AUTHORITY = "com.featherpdf.android.share"
    }
}
