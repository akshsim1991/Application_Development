# Feather PDF on Google Play: what to upload

Everything here is made for the Play Console (https://play.google.com/console). Each Android build on GitHub puts the
app bundle in `releases/FeatherPDF-<version>-android.aab` and the phone screenshots in `store/screenshots/`.

## The files

| Play Console asks for | File |
|---|---|
| App bundle (Production / Testing > Create new release) | `releases/FeatherPDF-<version>-android.aab` |
| App icon, 512 x 512 | `store/icon-512.png` |
| Feature graphic, 1024 x 500 | `store/feature-graphic.png` |
| Phone screenshots (2 to 8; 1080 x 1920) | `store/screenshots/*.jpg` |
| Privacy policy | https://github.com/akshsim1991/FeatherPDF-releases/blob/main/PRIVACY.md |

## The texts

**App name** (30 characters at most): `Feather PDF`

**Short description** (80 at most):
`Free PDF reader and editor: edit text, highlight, draw. No ads, no account.`

**Full description:**

```
Feather PDF is a fast, lightweight PDF reader and editor. Everything happens on your phone: no account, no cloud, no ads, no tracking, and no permission to use the internet.

READ
• Smooth scrolling, pinch and double-tap zoom with sharp pages
• Find words, with every match highlighted
• Bookmarks and go to page
• Page colours for reading at night: night, dimmed, and pure black (AMOLED) blackboard
• Password-protected PDFs
• Recent files reopen at the page you last read

EDIT
• Change the text of any paragraph: tap it, type, choose the size, bold or italic. The text wraps in its column and moves what is below it, like a word processor
• Add new text anywhere on a page
• Highlight, underline or strike out text
• Draw with a pen in your colour and width
• Add notes, and remove highlights, drawings and notes
• Turn and delete pages
• Undo and redo every change

SAVE AND SHARE
• Save back to the same file, save a copy anywhere, or share the PDF
• Open PDFs from any app ("Open with" and "Share to")

Feather PDF uses the same PDF engine as Feather PDF for Windows and macOS. It is made by Akshaya Simha, and it is free.

Suggestions and feedback: akshsimha@gmail.com
```

**Category:** Productivity. **Tags:** PDF, Document, Editor.
**Contact e-mail:** akshsimha@gmail.com

## App content answers (Policy > App content)

- **Privacy policy:** the address above.
- **Ads:** No ads.
- **App access:** All functionality is available without special access (no sign-in).
- **Content rating:** answer the questionnaire as a utility/productivity app with no user-generated sharing, no
  violence, no gambling: it comes out as "Everyone".
- **Target audience:** 13 and over (it is not designed for children).
- **Data safety:** "Does your app collect or share any of the required user data types?" **No.** Data is not
  collected (the app has no internet permission). Encrypted in transit: not applicable.
- **Government app / financial features / health:** No.
- **News app:** No.

## Signing

The bundle is signed with the Feather PDF key (`app/featherpdf-release.jks`). On Play, this key becomes the **upload
key**, and Google signs what it delivers with an app signing key of its own ("Play App Signing").

- Choose this if Play is the only way you will hand out the app: let Google create the app signing key.
- Copies installed from the APK on the releases page cannot be updated from Play (and the other way round), because
  the keys differ. To keep both working, choose "Use my own key" when setting up Play App Signing and upload this
  same key with the tool the Console offers (PEPK); then every copy is signed alike.

Keep a backup of `featherpdf-release.jks` outside GitHub: without it no update can be uploaded (Google can reset an
upload key, but only on request).

## Each new version

1. The version comes from `CMakeLists.txt` (version code = major x 10000 + minor x 100 + patch, e.g. 2.22.1 -> 22201);
   Play accepts only a higher version code than the last one.
2. Push; wait for the Android build on GitHub to pass (it writes the new `.aab` and screenshots).
3. Play Console > your release track > Create new release > upload the `.aab` > write what is new > roll out.
