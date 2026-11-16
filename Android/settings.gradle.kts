// Feather PDF for Android. The engine is the repository's shared/ (built by app/src/main/cpp/CMakeLists.txt).
pluginManagement {
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
}
dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        google()
        mavenCentral()
    }
}
rootProject.name = "FeatherPDF"
include(":app")
