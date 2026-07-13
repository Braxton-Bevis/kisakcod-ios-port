# KisakCOD iOS shell — build, signing, and on-device launch runbook

Everything here is reproducible from a clean clone. The `.xcodeproj` is generated,
never hand-edited: [project.yml](project.yml) is the source of truth.

## What exists

- `Stub/` — minimal scene-less UIKit app: one window, one view controller whose view is
  backed by a `CAMetalLayer`. A `CADisplayLink` loop clears to an animated color and draws
  one triangle via a compiled `.metal` shader library. After the first successfully
  presented frame it writes `Documents/metal_first_frame.txt` inside its own sandbox
  (proof-of-run, and the seed of the Objective-3 filesystem-sandbox design).
- `project.yml` — XcodeGen spec. Bundle ID `dev.braxton.kisakstub`, deployment target iOS 16.0.
- `.github/workflows/ios-stub.yml` — CI on a macOS runner:
  - **simulator-launch-proof**: builds for `iphonesimulator`, boots a simulator, installs,
    launches, takes two screenshots seconds apart (animated clear color ⇒ they differ ⇒ live
    render loop), and fails unless the in-sandbox marker contains the exact
    staged-boot and real-pmove behavioral proof lines.
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
simulator), controller name + live stick values, and the real pmove sandbox's
origin/velocity. The left stick moves both the triangle and the synthetic-world
player (physical controllers work identically via GCController). A queues a
one-frame jump and switches the background palette; B holds sprint and recenters.
The scene renders at 0.5× resolution and MetalFX spatial-upscales it to native
when the GPU supports it (iOS 16+, `MTLFXSpatialScalerDescriptor.supportsDevice`).

M14 hosted run `29267514067` built, launched, and captured this exact movement
proof while retaining the M13 staged-boot line:

```
pmove=real bg_pmove OK: walk+jump+land+friction on synthetic z=0
pmoveLive=org=(0.0,0.0,0.0) vel=(0.0,0.0,0.0) speed=0 ground=1
```

That is simulator runtime evidence. The unsigned arm64 build from the same run
is compilation/linkage evidence only. For the pending physical M13/M14
addendum, install a signed build, pull `metal_first_frame.txt` with the command
below, require both exact boot and pmove proof lines, then feel-test left-stick
movement, A jump, and held-B sprint. Do not record physical proof from an IPA
artifact alone.

---

## Verified physical-device flow (2026-07-11, iPad Pro 13" M5)

The whole path below was exercised end-to-end from the command line — the only
finger-on-device steps are Developer Mode and one trust tap.

```bash
# One-time device prep (on the iPad):
#   1. Settings > Privacy & Security > Developer Mode -> on (reboots)
#   2. After first install: Settings > General > VPN & Device Management
#      -> trust the "Apple Development: <your appleid>" profile

xcodegen generate
xcrun devicectl list devices             # grab the device identifier

# Build signed (automatic signing; first run mints your dev certificate):
xcodebuild -project KisakStub.xcodeproj -scheme KisakStub -configuration Debug \
  -destination 'platform=iOS,id=<DEVICE-ID>' -derivedDataPath build/dd \
  -allowProvisioningUpdates build

xcrun devicectl device install app --device <DEVICE-ID> \
  build/dd/Build/Products/Debug-iphoneos/KisakStub.app
xcrun devicectl device process launch --device <DEVICE-ID> dev.braxton.kisakstub

# Pull the proof marker straight out of the app sandbox:
xcrun devicectl device copy from --device <DEVICE-ID> \
  --domain-type appDataContainer --domain-identifier dev.braxton.kisakstub \
  --source Documents/metal_first_frame.txt --destination ./
```

Result on the M5 iPad: `metalfx=spatial 1600x1200 → 3200x2400` (the real
MetalFX path — the simulator SDK doesn't ship the module), `fps=120.0`,
`raytracing=supported`, and the engine smoke line from libkisaksmoke.a.

**Wireless installs:** with iPadOS 17+, the CoreDevice pairing created over
USB automatically serves a network tunnel (the `*.coredevice.local` hostname
in `devicectl list devices`). Keep Mac + iPad on the same Wi-Fi, unplug, and
the same `devicectl` commands keep working — no toggle needed.
