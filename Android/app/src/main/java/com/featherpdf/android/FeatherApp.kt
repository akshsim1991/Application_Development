package com.featherpdf.android

import android.app.Application
import com.featherpdf.engine.Engine

/** Starts the engine once for the whole app. */
class FeatherApp : Application() {
    override fun onCreate() {
        super.onCreate()
        startError = Engine.start(AndroidHooks())
        getExternalFilesDir(null)   // the app's own folder exists (the build check puts its test document there)
    }

    companion object {
        /** Why the engine could not start, or null when it is ready. */
        @Volatile var startError: String? = null
            private set
    }
}
