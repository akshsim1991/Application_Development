# Reporting a security issue
Please do not post vulnerabilities in public issues.
Report privately using GitHub's **Report a vulnerability** button (repository → Security → Advisories) on https://github.com/akshsim1991/Application_Development, or contact the developer, Akshaya Simha, through the profile at https://github.com/akshsim1991.
Include the Feather version (Settings → Version), Windows version, what you did and what you expected. You will get a reply as soon as possible; fixes ship as a normal update.

Scope: the Feather application and installer, the update mechanism, and the recommendation list. Design rules the project follows: elevate only when needed, never execute inspected files, validate every path before a file operation (no raw paths come from the UI), skip links/junctions, back up before changing, verify downloads by SHA-256 (and signature when installers are signed), and log every change.
