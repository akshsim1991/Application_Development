import java.io.File

plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

// One version for every app: the repository's CMakeLists.txt.
val repoRoot: File = rootDir.parentFile
val featherVersion: String = Regex("project\\(FeatherPDF VERSION ([0-9.]+)")
    .find(File(repoRoot, "CMakeLists.txt").readText())!!.groupValues[1]
val featherCodename: String = Regex("set\\(FEATHER_CODENAME \"([^\"]+)\"\\)")
    .find(File(repoRoot, "CMakeLists.txt").readText())?.groupValues?.get(1) ?: ""
val parts = featherVersion.split(".").map { it.toInt() }

// PDFium for each ABI (scripts/fetch-pdfium.sh android), laid out as the APK's native libraries.
val abis = listOf("arm64-v8a", "armeabi-v7a", "x86_64")
val pdfiumLibs = layout.buildDirectory.dir("pdfiumLibs")
val copyPdfium by tasks.registering(Copy::class) {
    for (abi in abis) {
        from(File(repoRoot, "third_party/pdfium-android/$abi/lib/libpdfium.so")) { into(abi) }
    }
    into(pdfiumLibs)
}

// The open-source licences shown in About (the PDFium package's texts, and the NDK's C++ library's).
val licenceAssets = layout.buildDirectory.dir("licenceAssets")
val copyLicences by tasks.registering(Copy::class) {
    from(File(repoRoot, "third_party/pdfium-android/arm64-v8a/licenses"))
    from(File(rootDir, "app/src/licences"))
    into(licenceAssets.map { it.dir("licenses") })
}

android {
    namespace = "com.featherpdf.android"
    compileSdk = 35
    ndkVersion = "27.2.12479018"

    defaultConfig {
        applicationId = "com.featherpdf.android"
        minSdk = 24
        targetSdk = 35
        versionCode = parts[0] * 10000 + parts[1] * 100 + parts[2]
        versionName = featherVersion
        manifestPlaceholders["codename"] = featherCodename   // the release's name, read by the About box
        externalNativeBuild { cmake { arguments += listOf("-DANDROID_STL=c++_shared") } }
    }

    // The release key is kept with the code so every build can update the last one (see Android/README.md); a key in
    // FEATHER_KEYSTORE (with FEATHER_KEYSTORE_PASSWORD) takes its place when set.
    signingConfigs {
        create("release") {
            storeFile = System.getenv("FEATHER_KEYSTORE")?.let { File(it) } ?: file("featherpdf-release.jks")
            storePassword = System.getenv("FEATHER_KEYSTORE_PASSWORD") ?: "featherpdf-release"
            keyAlias = "featherpdf"
            keyPassword = System.getenv("FEATHER_KEYSTORE_PASSWORD") ?: "featherpdf-release"
        }
    }
    buildTypes {
        release {
            isMinifyEnabled = false
            signingConfig = signingConfigs.getByName("release")
        }
    }

    // One APK for each kind of phone, so a download carries only its own libraries (about a third of one APK for all):
    // arm64-v8a (phones since about 2017), armeabi-v7a (older 32-bit phones), x86_64 (emulators, Chromebooks).
    splits {
        abi {
            isEnable = true
            reset()
            include(*abis.toTypedArray())
            isUniversalApk = false
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }
    sourceSets["main"].jniLibs.srcDir(pdfiumLibs)
    sourceSets["main"].assets.srcDir(licenceAssets)
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions { jvmTarget = "17" }
    packaging { jniLibs { useLegacyPackaging = false } }
}

tasks.named("preBuild") { dependsOn(copyPdfium, copyLicences) }
