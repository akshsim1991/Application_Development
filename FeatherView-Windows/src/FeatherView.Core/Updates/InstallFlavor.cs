namespace FeatherView.Core.Updates;

public static class InstallFlavor
{
    /// <summary>
    /// True when the app was installed without its own .NET runtime (small installer). A self-contained app lists
    /// "includedFrameworks" in FeatherView.runtimeconfig.json; a framework-dependent one lists a "framework" it needs.
    /// </summary>
    public static bool IsFrameworkDependent(string? runtimeConfigJson) =>
        !string.IsNullOrEmpty(runtimeConfigJson) && !runtimeConfigJson.Contains("includedFrameworks", StringComparison.Ordinal)
        && runtimeConfigJson.Contains("\"framework\"", StringComparison.Ordinal);
}
