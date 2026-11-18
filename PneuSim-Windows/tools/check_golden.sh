#!/usr/bin/env bash
set -o pipefail
# Quick regression check for Linux/cloud sessions: builds PneuSim, runs the headless harness on all
# example circuits and compares with the checked-in golden file made from the ORIGINAL 3.2.0 program.
# Usage (from PneuSim-Windows): bash tools/check_golden.sh      Exit 0 = identical.
set -e
export DOTNET_CLI_TELEMETRY_OPTOUT=1
dotnet build src/PneuSim/PneuSim.csproj -c Release -nologo -p:EnableWindowsTargeting=true -v q | tail -2
dotnet build tools/equivalence-harness/Harness.csproj -c Release -nologo -p:EnableWindowsTargeting=true -v q | tail -1
dotnet build tools/selftest/SelfTest.csproj -c Release -nologo -p:EnableWindowsTargeting=true -v q | tail -1
H=$(ls tools/equivalence-harness/bin/Release/*/Harness.dll | head -1)
dotnet "$H" src/PneuSim/bin/Release/net472/PneuSim.exe $(find examples -name '*.pneu' | sort) > /tmp/pneusim-harness.txt 2>&1
diff tools/golden/harness-linux-3.2.0.txt /tmp/pneusim-harness.txt && echo "GOLDEN: identical to 3.2.0"
dotnet tools/selftest/bin/Release/net8.0/SelfTest.dll src/PneuSim/bin/Release/net472/PneuSim.exe | tail -1
