import UIKit
import Metal
import GameController
import QuartzCore
#if canImport(MetalFX)
// The iphonesimulator SDK (as of Xcode 16.4) does not ship the MetalFX module
// at all — only the device SDK does. Everything MetalFX is compile-gated.
import MetalFX
#endif

// A UIView whose backing layer IS a CAMetalLayer — the iOS equivalent of the
// HWND the engine's D3D9 device would have been created against.
final class MetalView: UIView {
    override class var layerClass: AnyClass { CAMetalLayer.self }
    var metalLayer: CAMetalLayer { layer as! CAMetalLayer }
}

final class MetalViewController: UIViewController {
    private var device: MTLDevice!
    private var commandQueue: MTLCommandQueue!
    private var pipelineState: MTLRenderPipelineState!
    private var displayLink: CADisplayLink?
    private var frameIndex: UInt64 = 0
    private var wroteFirstFrameMarker = false
    private let hudLabel = UILabel()

    // Graphics settings — persisted in UserDefaults (the stub-scale preview of the
    // engine's future r_* dvar surface: r_metalfx / r_renderScale / com_maxfps).
    private var fxEnabled = UserDefaults.standard.object(forKey: "kisak.fxEnabled") as? Bool ?? true
    private var renderScalePct = UserDefaults.standard.object(forKey: "kisak.renderScalePct") as? Int ?? 50
    private var frameCap = UserDefaults.standard.object(forKey: "kisak.frameCap") as? Int ?? 0   // 0 = uncapped

    // MetalFX spatial upscaling: the scene renders at renderScalePct of the drawable,
    // then MTLFXSpatialScaler upscales into the drawable. Unsupported GPUs (and the
    // CI simulator, whose SDK lacks the module) fall back to direct rendering; the
    // HUD and the sandbox marker file record which path is live.
    #if canImport(MetalFX)
    private var spatialScaler: MTLFXSpatialScaler?
    #endif
    private var sceneColorTexture: MTLTexture?
    private var lastScalerDrawableSize: CGSize = .zero
    private var metalFXStatus = "init"

    // Controller state: one code path serves both the on-screen GCVirtualController
    // and any physical controller — both surface as GCController instances.
    private var virtualController: GCVirtualController?
    private var controllerStatus = "none"
    private var stick = SIMD2<Float>(0, 0)
    private var triangleOffset = SIMD2<Float>(0, 0)
    private var paletteFlash = false

    // Settings UI
    private let settingsPanel = UIVisualEffectView(effect: UIBlurEffect(style: .systemUltraThinMaterialDark))
    private let fxSwitch = UISwitch()
    private let scaleControl = UISegmentedControl(items: ["50%", "75%", "100%"])
    private let capControl = UISegmentedControl(items: ["30", "60", "Max"])
    private let statusFootnote = UILabel()

    override func loadView() { view = MetalView() }
    private var metalView: MetalView { view as! MetalView }

    override func viewDidLoad() {
        super.viewDidLoad()

        guard let device = MTLCreateSystemDefaultDevice(),
              let queue = device.makeCommandQueue() else {
            fatalError("Metal unavailable on this device")
        }
        self.device = device
        self.commandQueue = queue

        let metalLayer = metalView.metalLayer
        metalLayer.device = device
        metalLayer.pixelFormat = .bgra8Unorm
        metalLayer.framebufferOnly = false   // MetalFX writes the drawable as scaler output

        guard let library = device.makeDefaultLibrary(),
              let vertexFn = library.makeFunction(name: "stub_vertex"),
              let fragmentFn = library.makeFunction(name: "stub_fragment") else {
            fatalError("Shaders.metal did not compile into the default library")
        }
        let desc = MTLRenderPipelineDescriptor()
        desc.vertexFunction = vertexFn
        desc.fragmentFunction = fragmentFn
        desc.colorAttachments[0].pixelFormat = metalLayer.pixelFormat
        pipelineState = try! device.makeRenderPipelineState(descriptor: desc)

        hudLabel.textColor = .white
        hudLabel.font = .monospacedSystemFont(ofSize: 13, weight: .semibold)
        hudLabel.numberOfLines = 0
        hudLabel.textAlignment = .center
        hudLabel.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(hudLabel)
        NSLayoutConstraint.activate([
            hudLabel.centerXAnchor.constraint(equalTo: view.centerXAnchor),
            hudLabel.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor, constant: 12),
        ])

        setUpSettingsUI()
        setUpControllers()

        let link = CADisplayLink(target: self, selector: #selector(renderFrame))
        link.add(to: .main, forMode: .common)
        displayLink = link
        applyFrameCap()
    }

    override func viewDidLayoutSubviews() {
        super.viewDidLayoutSubviews()
        let scale = view.window?.screen.scale ?? UIScreen.main.scale
        metalView.metalLayer.drawableSize = CGSize(width: view.bounds.width * scale,
                                                   height: view.bounds.height * scale)
    }

    // MARK: - Graphics settings UI

    private func setUpSettingsUI() {
        var gearConfig = UIButton.Configuration.plain()
        gearConfig.image = UIImage(systemName: "gearshape.fill")
        gearConfig.baseForegroundColor = .white
        let gear = UIButton(configuration: gearConfig)
        gear.addAction(UIAction { [weak self] _ in self?.toggleSettings() }, for: .touchUpInside)
        gear.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(gear)

        let title = UILabel()
        title.text = "GRAPHICS"
        title.textColor = .white
        title.font = .monospacedSystemFont(ofSize: 12, weight: .bold)

        fxSwitch.isOn = fxEnabled
        fxSwitch.addAction(UIAction { [weak self] _ in
            guard let self else { return }
            self.fxEnabled = self.fxSwitch.isOn
            UserDefaults.standard.set(self.fxEnabled, forKey: "kisak.fxEnabled")
            self.lastScalerDrawableSize = .zero   // force scaler rebuild next frame
        }, for: .valueChanged)

        scaleControl.selectedSegmentIndex = [50: 0, 75: 1, 100: 2][renderScalePct] ?? 0
        scaleControl.addAction(UIAction { [weak self] _ in
            guard let self else { return }
            self.renderScalePct = [0: 50, 1: 75, 2: 100][self.scaleControl.selectedSegmentIndex] ?? 50
            UserDefaults.standard.set(self.renderScalePct, forKey: "kisak.renderScalePct")
            self.lastScalerDrawableSize = .zero
        }, for: .valueChanged)

        capControl.selectedSegmentIndex = [30: 0, 60: 1, 0: 2][frameCap] ?? 2
        capControl.addAction(UIAction { [weak self] _ in
            guard let self else { return }
            self.frameCap = [0: 30, 1: 60, 2: 0][self.capControl.selectedSegmentIndex] ?? 0
            UserDefaults.standard.set(self.frameCap, forKey: "kisak.frameCap")
            self.applyFrameCap()
        }, for: .valueChanged)

        statusFootnote.textColor = .lightGray
        statusFootnote.font = .monospacedSystemFont(ofSize: 10, weight: .regular)
        statusFootnote.numberOfLines = 0

        let stack = UIStackView(arrangedSubviews: [
            title,
            Self.row("MetalFX upscaling", fxSwitch),
            Self.row("Render scale", scaleControl),
            Self.row("Frame cap", capControl),
            statusFootnote,
        ])
        stack.axis = .vertical
        stack.spacing = 10
        stack.translatesAutoresizingMaskIntoConstraints = false

        settingsPanel.layer.cornerRadius = 14
        settingsPanel.clipsToBounds = true
        settingsPanel.translatesAutoresizingMaskIntoConstraints = false
        settingsPanel.contentView.addSubview(stack)
        view.addSubview(settingsPanel)

        NSLayoutConstraint.activate([
            gear.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor, constant: 8),
            gear.trailingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.trailingAnchor, constant: -8),
            settingsPanel.topAnchor.constraint(equalTo: gear.bottomAnchor, constant: 6),
            settingsPanel.trailingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.trailingAnchor, constant: -12),
            settingsPanel.widthAnchor.constraint(equalToConstant: 300),
            stack.topAnchor.constraint(equalTo: settingsPanel.contentView.topAnchor, constant: 12),
            stack.bottomAnchor.constraint(equalTo: settingsPanel.contentView.bottomAnchor, constant: -12),
            stack.leadingAnchor.constraint(equalTo: settingsPanel.contentView.leadingAnchor, constant: 12),
            stack.trailingAnchor.constraint(equalTo: settingsPanel.contentView.trailingAnchor, constant: -12),
        ])

        // Visible on first-ever launch (so the CI screenshot proves the menu exists,
        // and users discover it); hidden afterwards until the gear is tapped.
        let seen = UserDefaults.standard.bool(forKey: "kisak.seenSettings")
        settingsPanel.isHidden = seen
        UserDefaults.standard.set(true, forKey: "kisak.seenSettings")
    }

    private static func row(_ text: String, _ control: UIView) -> UIStackView {
        let label = UILabel()
        label.text = text
        label.textColor = .white
        label.font = .monospacedSystemFont(ofSize: 12, weight: .regular)
        let row = UIStackView(arrangedSubviews: [label, control])
        row.axis = .horizontal
        row.distribution = .equalSpacing
        row.alignment = .center
        return row
    }

    private func toggleSettings() {
        settingsPanel.isHidden.toggle()
    }

    private func applyFrameCap() {
        guard let link = displayLink else { return }
        if frameCap > 0 {
            link.preferredFrameRateRange = CAFrameRateRange(minimum: Float(frameCap),
                                                            maximum: Float(frameCap),
                                                            preferred: Float(frameCap))
        } else {
            link.preferredFrameRateRange = CAFrameRateRange(minimum: 30, maximum: 120, preferred: 120)
        }
    }

    // MARK: - Controllers

    private func setUpControllers() {
        NotificationCenter.default.addObserver(forName: .GCControllerDidConnect,
                                               object: nil, queue: .main) { [weak self] note in
            guard let self, let controller = note.object as? GCController else { return }
            self.bind(controller)
        }
        NotificationCenter.default.addObserver(forName: .GCControllerDidDisconnect,
                                               object: nil, queue: .main) { [weak self] _ in
            self?.controllerStatus = "disconnected"
            self?.stick = .zero
        }

        // On-screen controller (left thumbstick + A/B) — doubles as the Objective-5
        // touch overlay. A physical controller connecting uses the same bind path.
        let config = GCVirtualController.Configuration()
        config.elements = [GCInputLeftThumbstick, GCInputButtonA, GCInputButtonB]
        let vc = GCVirtualController(configuration: config)
        vc.connect { [weak self] error in
            if let error { self?.controllerStatus = "virtual connect failed: \(error.localizedDescription)" }
        }
        virtualController = vc
    }

    private func bind(_ controller: GCController) {
        controllerStatus = controller.vendorName ?? "controller"
        guard let gamepad = controller.extendedGamepad else { return }
        gamepad.leftThumbstick.valueChangedHandler = { [weak self] _, x, y in
            self?.stick = SIMD2<Float>(x, y)
        }
        gamepad.buttonA.pressedChangedHandler = { [weak self] _, _, pressed in
            if pressed { self?.paletteFlash.toggle() }
        }
        gamepad.buttonB.pressedChangedHandler = { [weak self] _, _, pressed in
            if pressed { self?.triangleOffset = .zero }
        }
    }

    // MARK: - MetalFX

    private func ensureScaler(for drawableSize: CGSize) {
        guard drawableSize != lastScalerDrawableSize else { return }
        lastScalerDrawableSize = drawableSize
        sceneColorTexture = nil
        #if !canImport(MetalFX)
        metalFXStatus = "module absent in this SDK (simulator) — direct render"
        #else
        spatialScaler = nil

        guard fxEnabled else {
            metalFXStatus = "off (settings) — direct render"
            return
        }
        guard renderScalePct < 100 else {
            metalFXStatus = "off (100% scale) — direct render"
            return
        }
        guard MTLFXSpatialScalerDescriptor.supportsDevice(device) else {
            metalFXStatus = "unsupported (\(device.name)) — direct render"
            return
        }
        let effectiveScale = CGFloat(renderScalePct) / 100.0
        let inW = max(1, Int(drawableSize.width * effectiveScale))
        let inH = max(1, Int(drawableSize.height * effectiveScale))
        let outW = Int(drawableSize.width), outH = Int(drawableSize.height)

        let texDesc = MTLTextureDescriptor.texture2DDescriptor(pixelFormat: .bgra8Unorm,
                                                               width: inW, height: inH,
                                                               mipmapped: false)
        texDesc.usage = [.renderTarget, .shaderRead]
        texDesc.storageMode = .private
        sceneColorTexture = device.makeTexture(descriptor: texDesc)

        let scalerDesc = MTLFXSpatialScalerDescriptor()
        scalerDesc.inputWidth = inW
        scalerDesc.inputHeight = inH
        scalerDesc.outputWidth = outW
        scalerDesc.outputHeight = outH
        scalerDesc.colorTextureFormat = .bgra8Unorm
        scalerDesc.outputTextureFormat = .bgra8Unorm
        scalerDesc.colorProcessingMode = .perceptual
        spatialScaler = scalerDesc.makeSpatialScaler(device: device)
        metalFXStatus = spatialScaler != nil
            ? "spatial \(inW)x\(inH) → \(outW)x\(outH)"
            : "scaler creation failed — direct render"
        if spatialScaler == nil { sceneColorTexture = nil }
        #endif
    }

    // MARK: - Frame loop

    @objc private func renderFrame() {
        let metalLayer = metalView.metalLayer
        guard metalLayer.drawableSize.width > 0,
              let drawable = metalLayer.nextDrawable(),
              let commandBuffer = commandQueue.makeCommandBuffer() else { return }

        frameIndex += 1
        // Integrate the thumbstick each frame (clamped to keep the triangle on screen).
        triangleOffset += stick * 0.02
        triangleOffset.x = max(-0.9, min(0.9, triangleOffset.x))
        triangleOffset.y = max(-0.9, min(0.9, triangleOffset.y))

        ensureScaler(for: metalLayer.drawableSize)

        // Slowly cycling clear color (A button switches palette): two screenshots
        // taken seconds apart differ, proving a live render loop.
        let t = Double(frameIndex) / 240.0
        let clear = paletteFlash
            ? MTLClearColor(red: 0.22 + 0.08 * sin(t), green: 0.10, blue: 0.12, alpha: 1.0)
            : MTLClearColor(red: 0.10 + 0.08 * sin(t), green: 0.12, blue: 0.22 + 0.08 * cos(t), alpha: 1.0)

        let sceneTarget = sceneColorTexture ?? drawable.texture
        let pass = MTLRenderPassDescriptor()
        pass.colorAttachments[0].texture = sceneTarget
        pass.colorAttachments[0].loadAction = .clear
        pass.colorAttachments[0].storeAction = .store
        pass.colorAttachments[0].clearColor = clear

        if let encoder = commandBuffer.makeRenderCommandEncoder(descriptor: pass) {
            encoder.setRenderPipelineState(pipelineState)
            var offset = triangleOffset
            encoder.setVertexBytes(&offset, length: MemoryLayout<SIMD2<Float>>.size, index: 0)
            encoder.drawPrimitives(type: .triangle, vertexStart: 0, vertexCount: 3)
            encoder.endEncoding()
        }

        #if canImport(MetalFX)
        if let scaler = spatialScaler, let scene = sceneColorTexture {
            scaler.colorTexture = scene
            scaler.outputTexture = drawable.texture
            scaler.encode(commandBuffer: commandBuffer)
        }
        #endif

        commandBuffer.present(drawable)
        commandBuffer.commit()

        // Frame 1: prove the first present. Frame 300 (~5s): rewrite with settled
        // MetalFX/controller state (virtual-controller connect is asynchronous);
        // CI pulls the marker after a 10s sleep, so it reads the enriched version.
        if !wroteFirstFrameMarker || frameIndex == 300 {
            wroteFirstFrameMarker = true
            writeFirstFrameMarker()
        }
        if frameIndex % 30 == 0 {
            statusFootnote.text = metalFXStatus
            hudLabel.text = """
            KisakCOD iOS stub — Metal live
            GPU: \(device.name)  frame \(frameIndex)
            MetalFX: \(metalFXStatus)
            Controller: \(controllerStatus)  stick (\(String(format: "%.2f", stick.x)), \(String(format: "%.2f", stick.y)))
            """
        }
    }

    // Proof-of-run artifact: CI (and a curious human) can pull this out of the app
    // sandbox container to verify Metal actually presented a frame, and which
    // MetalFX/controller/settings paths were live. Doubles as the first demonstration
    // of the Documents-directory write path that Objective 3 routes the engine through.
    private func writeFirstFrameMarker() {
        let docs = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
        let marker = docs.appendingPathComponent("metal_first_frame.txt")
        let contents = """
        KisakCOD iOS stub: first Metal frame presented OK
        gpu=\(device.name)
        drawableSize=\(metalView.metalLayer.drawableSize)
        metalfx=\(metalFXStatus)
        controller=\(controllerStatus)
        settings=fx:\(fxEnabled ? "on" : "off") scale:\(renderScalePct)% cap:\(frameCap == 0 ? "max" : String(frameCap))
        date=\(ISO8601DateFormatter().string(from: Date()))
        """
        try? contents.write(to: marker, atomically: true, encoding: .utf8)
        NSLog("KISAK_STUB_FIRST_FRAME_OK gpu=%@ metalfx=%@ controller=%@", device.name, metalFXStatus, controllerStatus)
    }
}
