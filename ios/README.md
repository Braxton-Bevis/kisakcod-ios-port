# KisakCOD iOS shell — build, signing, and on-device launch runbook

Everything here is reproducible from a clean clone. The `.xcodeproj` is generated,
never hand-edited: [project.yml](project.yml) is the source of truth.

## What exists

- `Stub/` — minimal scene-less UIKit app: one window, one view controller whose view is
  backed by a `CAMetalLayer`. A `CADisplayLink` loop clears to an animated color and draws
  one triangle via a compiled `.metal` shader library. After the first successfully
  presented frame it writes `Documents/metal_first_frame.txt` inside its own sandbox
  (proof-of-run, and the seed of the Objective-3 filesystem-sandbox design).
- `project.yml` — XcodeGen spec. Bundle ID `dev.braxton.kisakstub`, deployment target iOS 15.0.
- `.github/workflows/ios-stub.yml` — CI on a macOS runner:
  - **simulator-launch-proof**: builds for `iphonesimulator`, boots a simulator, installs,
    launches, takes two screenshots seconds apart (animated clear color ⇒ they differ ⇒ live
    render loop), and fails the job unless the in-sandbox marker file exists.
  - **device-ipa-unsigned**: builds the real `arm64-apple-ios` binary with
    `CODE_SIGNING_ALLOWED=NO`, verifies the slice with `lipo`/`otool`, zips `Payload/` into
    `KisakStub-unsigned.ipa`, uploads as artifact.

## Path A — on a Mac (the finalization machine)

```bash
brew install xcodegen        # once
cd ios
xcodegen generate
open KisakStub.xcodeproj
```

1. In Xcode: select the `KisakStub` target → *Signing & Capabilities* → check
   **Automatically manage signing** → pick your **Team** (a free personal Apple ID works:
   Xcode → Settings → Accounts → add Apple ID).
2. If the bundle ID collides with someone else's, change `PRODUCT_BUNDLE_IDENTIFIER` in
   `project.yml` (e.g. `dev.<you>.kisakstub`) and re-run `xcodegen generate`.
3. Plug in the iPhone, enable **Developer Mode** on the phone
   (Settings → Privacy & Security → Developer Mode, iOS 16+; reboots the phone).
4. Select the phone as the run destination, press **Run**. First run: on the phone, trust
   the developer cert under Settings → General → VPN & Device Management.
5. Free-Apple-ID caveats: profile expires after **7 days** (re-run from Xcode to refresh),
   max 3 sideloaded apps, 10 App IDs per week.

## Path B — from Windows, no Mac (what CI + Sideloadly gives you)

1. Download the `KisakStub-unsigned-ipa` artifact from the GitHub Actions run.
2. Install [Sideloadly](https://sideloadly.io/) (needs iTunes drivers — already present on
   this machine, Apple Mobile Device Service is running).
3. Connect the iPhone via USB, drag the `.ipa` into Sideloadly, enter your Apple ID.
   Sideloadly obtains a free development certificate and re-signs the IPA (it rewrites the
   bundle ID to fit your personal App ID namespace — that is expected).
4. Trust the cert on the phone (same as Path A step 4), enable Developer Mode, launch.

## Simulator quickstart on a Mac

```bash
cd ios && xcodegen generate
xcodebuild -project KisakStub.xcodeproj -scheme KisakStub -sdk iphonesimulator \
  -derivedDataPath dd CODE_SIGNING_ALLOWED=NO build
xcrun simctl boot "iPhone 16"   # or any available iPhone simulator
xcrun simctl install booted dd/Build/Products/Debug-iphonesimulator/KisakStub.app
xcrun simctl launch booted dev.braxton.kisakstub
```

Expected on screen: dark blue slowly color-shifting background, an RGB-gradient triangle,
a system **virtual game controller overlay** (left thumbstick + A/B buttons —
GCVirtualController), and a white HUD reporting GPU, frame counter, **MetalFX status**
(`spatial WxH → WxH` on supported hardware, `unsupported — direct render` on the
simulator), and controller name + live stick values. The left stick moves the triangle
(physical controllers work identically via GCController), A switches the background
palette, B recenters. The scene renders at 0.5× resolution and MetalFX spatial-upscales
it to native when the GPU supports it (iOS 16+, `MTLFXSpatialScalerDescriptor.supportsDevice`).
