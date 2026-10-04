#!/usr/bin/env bash
# Sign, notarise and staple the three distributable macOS bundles.
# This is deliberately used only by tagged GitHub releases; credentials are
# supplied as Actions secrets and are never written to the repository.
set -euo pipefail

stage="${1:?usage: sign-and-notarise-macos.sh <Release artefacts directory>}"
: "${MACOS_SIGNING_IDENTITY:?Missing Developer ID Application signing identity}"
: "${APPLE_ID:?Missing Apple ID for notarisation}"
: "${APPLE_APP_SPECIFIC_PASSWORD:?Missing app-specific password for notarisation}"
: "${APPLE_TEAM_ID:?Missing Apple Developer Team ID}"

bundles=(
    "$stage/VST3/Dyrekreds.vst3"
    "$stage/AU/Dyrekreds.component"
    "$stage/Standalone/Dyrekreds.app"
)

temporary_directory="$(mktemp -d)"
trap 'rm -rf "$temporary_directory"' EXIT

for bundle in "${bundles[@]}"; do
    test -d "$bundle"

    # JUCE links its code into the bundle executable.  Signing the bundle root
    # gives every distributable an independent Developer ID signature.
    codesign --force --deep --options runtime --timestamp \
        --sign "$MACOS_SIGNING_IDENTITY" "$bundle"
    codesign --verify --deep --strict --verbose=2 "$bundle"

    archive="$temporary_directory/$(basename "$bundle").zip"
    ditto -c -k --keepParent "$bundle" "$archive"
    xcrun notarytool submit "$archive" --wait \
        --apple-id "$APPLE_ID" \
        --password "$APPLE_APP_SPECIFIC_PASSWORD" \
        --team-id "$APPLE_TEAM_ID"
    xcrun stapler staple "$bundle"
    xcrun stapler validate "$bundle"

    # Gatekeeper's executable assessment is for applications and command-line
    # tools.  VST3 and Audio Unit bundles are still notarised and stapled above,
    # but are not applications and are rejected by `spctl --type execute`.
    if [[ "$bundle" == *.app ]]; then
        spctl --assess --type execute --verbose=4 "$bundle"
    fi
done
