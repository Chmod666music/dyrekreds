# Signed macOS releases

Gatekeeper permits a downloaded plug-in or standalone application only after
it has a valid Developer ID signature and has been notarised by Apple. A
universal binary alone is not sufficient. The beta.7 release workflow signs,
notarises and staples every macOS bundle before it is packaged.

Configure these repository Actions secrets before pushing a `v*` release tag:

| Secret | Value |
| --- | --- |
| `MACOS_CERTIFICATE_BASE64` | Base64-encoded Developer ID Application `.p12` certificate |
| `MACOS_CERTIFICATE_PASSWORD` | Password used when exporting that `.p12` |
| `MACOS_SIGNING_IDENTITY` | Exact Developer ID Application identity shown by `security find-identity -v -p codesigning` |
| `APPLE_ID` | Apple ID authorised for the Developer team |
| `APPLE_APP_SPECIFIC_PASSWORD` | App-specific password created for that Apple ID |
| `APPLE_TEAM_ID` | Ten-character Apple Developer Team ID |

The workflow intentionally fails a tagged macOS release if any of these are
missing. This prevents accidentally publishing another unsigned archive.

For a local release build, configure CMake as usual. The project defaults to a
universal `arm64;x86_64` build on macOS; inspect each result with `lipo -archs`
and install the entire `.vst3` or `.component` bundle, rather than only its
inner executable.
