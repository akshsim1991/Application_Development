# Code signing and releasing
Unsigned programs trigger Windows SmartScreen warnings and cannot prove who published an update. Feather is ready to be signed but needs **your** certificate (an OV or EV code-signing certificate from a certificate authority; EV removes SmartScreen warnings fastest).
1. Put the certificate (.pfx) somewhere safe, outside the repository. Never commit it.
2. Build, then sign the exes **before** building the installer, then sign the installer:
   `PFX=cert.pfx PFX_PASS=... tools/sign.sh build/FeatherSystemUtilities.exe` (same for the x86 exe), `./build.sh installer`, then sign the Setup exe. On Windows use `tools/sign.ps1`.
3. Set `SIGNER="Your certificate's subject name"` when running `tools/release.sh`. The manifest `latest.json` then contains `"signer"`, and the app refuses to install an update that is not validly signed by that name.
4. Keep the license private key (`tools/license_keygen.py`) as secret as the certificate.
`tools/release.sh VERSION "notes"` builds, packages, writes the SBOM and the update manifest.
