package com.featherpdf.android

import android.graphics.Bitmap
import android.graphics.BitmapFactory
import com.featherpdf.engine.Engine
import java.io.ByteArrayOutputStream
import java.nio.ByteBuffer
import java.security.MessageDigest
import java.security.SecureRandom
import javax.crypto.Cipher
import javax.crypto.spec.IvParameterSpec
import javax.crypto.spec.SecretKeySpec

/** The system services the engine asks for: Android's cryptography and picture classes. */
class AndroidHooks : Engine.SystemHooks {
    private val random = SecureRandom()

    override fun hash(alg: Int, data: ByteArray): ByteArray? =
        runCatching { MessageDigest.getInstance("SHA-$alg").digest(data) }.getOrNull()

    override fun aes(cbc: Boolean, key: ByteArray, iv: ByteArray?, input: ByteArray): ByteArray? = runCatching {
        val c = Cipher.getInstance(if (cbc) "AES/CBC/NoPadding" else "AES/ECB/NoPadding")
        if (cbc) c.init(Cipher.ENCRYPT_MODE, SecretKeySpec(key, "AES"), IvParameterSpec(iv))
        else c.init(Cipher.ENCRYPT_MODE, SecretKeySpec(key, "AES"))
        c.doFinal(input)
    }.getOrNull()

    override fun random(n: Int): ByteArray = ByteArray(n).also { random.nextBytes(it) }

    override fun encodeImage(bgra: ByteArray, w: Int, h: Int, jpeg: Boolean, quality: Int, alpha: Boolean): ByteArray? = runCatching {
        val rgba = ByteArray(w * h * 4)
        var i = 0
        while (i < rgba.size) {
            val a = if (alpha && !jpeg) bgra[i + 3].toInt() and 255 else 255
            // a Bitmap holds premultiplied colour
            rgba[i] = ((bgra[i + 2].toInt() and 255) * a / 255).toByte()
            rgba[i + 1] = ((bgra[i + 1].toInt() and 255) * a / 255).toByte()
            rgba[i + 2] = ((bgra[i].toInt() and 255) * a / 255).toByte()
            rgba[i + 3] = a.toByte()
            i += 4
        }
        val bmp = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888)
        bmp.copyPixelsFromBuffer(ByteBuffer.wrap(rgba))
        val out = ByteArrayOutputStream()
        bmp.compress(if (jpeg) Bitmap.CompressFormat.JPEG else Bitmap.CompressFormat.PNG, quality.coerceIn(1, 100), out)
        bmp.recycle()
        out.toByteArray()
    }.getOrNull()

    override fun decodeImage(path: String): ByteArray? = runCatching {
        val opts = BitmapFactory.Options().apply { inPreferredConfig = Bitmap.Config.ARGB_8888 }
        val bmp = BitmapFactory.decodeFile(path, opts) ?: return null
        val w = bmp.width
        val h = bmp.height
        val rgba = ByteBuffer.allocate(w * h * 4)
        bmp.copyPixelsToBuffer(rgba)
        bmp.recycle()
        val src = rgba.array()
        val out = ByteArray(8 + w * h * 4)
        for (k in 0 until 4) { out[k] = (w ushr (24 - 8 * k)).toByte(); out[4 + k] = (h ushr (24 - 8 * k)).toByte() }
        var i = 0
        while (i < src.size) {
            val a = src[i + 3].toInt() and 255
            fun un(v: Byte): Byte = if (a == 0 || a == 255) v else ((v.toInt() and 255) * 255 / a).coerceAtMost(255).toByte()
            out[8 + i] = un(src[i + 2]); out[9 + i] = un(src[i + 1]); out[10 + i] = un(src[i]); out[11 + i] = src[i + 3]
            i += 4
        }
        out
    }.getOrNull()
}
