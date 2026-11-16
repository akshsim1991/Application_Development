plugins { kotlin("jvm") version "2.0.21" }
val app = file("../../../Android/app/src/main/java")
sourceSets { main { kotlin.srcDir(app); java.srcDir(app) } }
dependencies { compileOnly("org.robolectric:android-all:14-robolectric-10818077") }
kotlin { jvmToolchain(21) }
tasks.withType<org.jetbrains.kotlin.gradle.tasks.KotlinCompile>().configureEach {
    compilerOptions { jvmTarget.set(org.jetbrains.kotlin.gradle.dsl.JvmTarget.JVM_17); freeCompilerArgs.add("-Xjdk-release=17") }
}
tasks.withType<JavaCompile>().configureEach { options.release.set(17) }
