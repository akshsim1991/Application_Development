// Compiles the Android app's code (Kotlin and Java) against the Android framework's classes (Robolectric's android-all,
// from Maven Central), to catch mistakes without the Android SDK. The APK itself is built by .github/workflows/android.yml.
pluginManagement { repositories { gradlePluginPortal(); mavenCentral() } }
dependencyResolutionManagement { repositories { mavenCentral() } }
rootProject.name = "android-compile-check"
