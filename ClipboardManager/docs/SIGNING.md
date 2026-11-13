# Code signing

An unsigned installer makes Windows SmartScreen say "Windows protected your PC / unknown publisher". Signing it with a code-signing certificate shows **your name** as the publisher instead. **I could not get a certificate for you**: they are issued to a verified person or company after an identity check, and cost money. Everything else is ready; this is what is left.

## What signing does and does not do
* It shows a verified publisher name and proves the file was not changed after you signed it.
* It does **not** instantly remove every SmartScreen warning. SmartScreen also uses a *reputation* that builds as many people install a signed file. Rules about how quickly reputation is granted have changed over the years (extended-validation certificates used to give an immediate boost); check the certificate seller's current statement instead of relying on this note.
* Sign **both** the program (`FeatherClipboardManager.exe`) and the installer. The small `Uninstall.exe` that the installer writes stays unsigned.
* Always sign **before** you run `scripts/publish-release.sh`: the update file records the SHA-256 of the final installer, and the in-app updater checks it.

## Getting a certificate (choose one)
1. **A cloud signing service run by Microsoft (Artifact Signing, formerly "Trusted Signing")** — low monthly price, no hardware, signs from a script. It validates your identity; **availability depends on country and on whether you are an individual or an organisation, so check the current terms for India** before relying on it.
2. **A code-signing certificate from a certificate authority** (for example DigiCert, Sectigo, SSL.com, Certum). Since mid-2023 the private key for new certificates must live on a **hardware token or in the seller's cloud service**, so you will **not** receive a `.pfx` file. You sign on Windows with `signtool` and the token plugged in (or with the seller's cloud tool).
3. An older certificate that you already have as a `.pfx` file works with `scripts/sign.sh` on Linux/macOS.

A self-signed certificate does **not** help real users: Windows does not trust it. It is only good for testing that the signing mechanism works (that is how it was tested here).

## Signing on Windows (token or cloud certificate)
Install the Windows SDK (it contains `signtool.exe`), then after building:
```bat
signtool sign /fd SHA256 /tr http://timestamp.digicert.com /td SHA256 /a build\FeatherClipboardManager.exe
signtool sign /fd SHA256 /tr http://timestamp.digicert.com /td SHA256 /a releases\FeatherClipboardManager-Setup-2.2.0.exe
signtool verify /pa /v releases\FeatherClipboardManager-Setup-2.2.0.exe
```
(`/a` picks the best certificate; your certificate seller's instructions may add options for their token or cloud service.) Build the unsigned installer first with `scripts/make_installer.sh` (it needs NSIS and MinGW, so on Windows run it in WSL), sign the program **before** the installer is built if you want the signed program inside it, then sign the installer.

## Signing on Linux/macOS/WSL with a .pfx
```sh
export SIGN_PFX=/path/to/certificate.pfx SIGN_PASS='the password'
scripts/make_installer.sh          # signs the program, builds the installer, signs the installer
```
`scripts/sign.sh file.exe` signs any single file. It adds a trusted timestamp (default DigiCert's server) so the signature stays valid after the certificate expires; set `SIGN_TIMESTAMP` to use another server.

## Check the result
`signtool verify /pa /v file.exe` (Windows) or `osslsigncode verify -in file.exe` (Linux). In Windows: right-click the file > Properties > Digital Signatures.
