#!/bin/zsh
# Usage: ./bump-version.sh [major|minor|patch]   (default: patch)
# Raises the release number in VERSION and in the iPhone project, so every release carries a new version.
set -euo pipefail
cd "${0:A:h}"
IFS=. read -r MAJOR MINOR PATCH < <(tr -d "[:space:]" < VERSION; echo) || true
case "${1:-patch}" in
  major) MAJOR=$((MAJOR+1)); MINOR=0; PATCH=0 ;;
  minor) MINOR=$((MINOR+1)); PATCH=0 ;;
  *) PATCH=$((PATCH+1)) ;;
esac
NEW="$MAJOR.$MINOR.$PATCH"
echo "$NEW" > VERSION
IOS="../FileGlade-iOS/FileGlade.xcodeproj/project.pbxproj"
if [[ -f "$IOS" ]]; then
  sed -i '' "s/MARKETING_VERSION = [0-9.]*;/MARKETING_VERSION = $NEW;/" "$IOS"
  CUR=$(grep -o 'CURRENT_PROJECT_VERSION = [0-9]*' "$IOS" | head -1 | grep -o '[0-9]*$')
  sed -i '' "s/CURRENT_PROJECT_VERSION = [0-9]*;/CURRENT_PROJECT_VERSION = $((CUR+1));/" "$IOS"
fi
echo "Version is now $NEW"
