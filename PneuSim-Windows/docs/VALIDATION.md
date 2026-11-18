# PneuSim validation (what is checked automatically)

Every push runs these on GitHub's Windows runner (`.github/workflows/pneusim.yml`); Linux sessions can
run the quick set with `bash tools/check_golden.sh`.

| Check | Tool | What it proves |
|---|---|---|
| Equivalence with 3.2.0 | `tools/equivalence-harness` | For all bundled and user circuits: static checks, dynamic checks, explanations, a 10 s simulation end state and saved-file size are identical to the original 3.2.0 program. Golden copy: `tools/golden/harness-linux-3.2.0.txt` (Linux run; the Windows run compares live against the extracted original). |
| Physics invariants | `tools/selftest` | On all 12 bundled examples, 10 s of simulation: no NaN, no fluid pressure below -1.1 bar or more than supply + 1 bar, cylinder positions stay between 0 and 1. |
| Speed | `tools/selftest` | Under 5 ms per simulation step (typically 0.03 ms). |
| Rated pressure | `tools/selftest` | A 6 bar circuit raises no warning, the same circuit at 12 bar does. |
| Sounds | `tools/selftest` | All six synthesised sounds are finite and within range. (Audible playback needs a real PC.) |
| Units | `tools/selftest` | 6 bar shows as 6 bar, 0.6 MPa, 600 kPa. |
| Hidden faults | `tools/selftest` | Encrypted, decrypt correctly, tampering is rejected. |
| Saving / loading | `tools/selftest` | Safe save keeps a .bak; damaged files and files from a newer version give friendly messages. |
| Updates | `tools/selftest` | Version comparison, SHA-256, manifest parsing (HTTPS only, bad data rejected). |
| GUI starts | `tools/gui-smoke.ps1` | The window opens with the right title and no error dialog, empty and with each example; screenshots in the artifact. |

Not covered yet: audible sound output, high-DPI layout, update download over the real network,
signed installer, long-run (hours) stability. See `HANDOFF.md`.
