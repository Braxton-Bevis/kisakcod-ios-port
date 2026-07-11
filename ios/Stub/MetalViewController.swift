import UIKit
import Metal
import QuartzCore

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
        metalLayer.framebufferOnly = true

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
        hudLabel.font = .monospacedSystemFont(ofSize: 14, weight: .semibold)
        hudLabel.numberOfLines = 0
        hudLabel.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(hudLabel)
        NSLayoutConstraint.activate([
            hudLabel.centerXAnchor.constraint(equalTo: view.centerXAnchor),
            hudLabel.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor, constant: 12),
        ])

        let link = CADisplayLink(target: self, selector: #selector(renderFrame))
        link.add(to: .main, forMode: .common)
        displayLink = link
    }

    override func viewDidLayoutSubviews() {
        super.viewDidLayoutSubviews()
        let scale = view.window?.screen.scale ?? UIScreen.main.scale
        metalView.metalLayer.drawableSize = CGSize(width: view.bounds.width * scale,
                                                   height: view.bounds.height * scale)
    }

    @objc private func renderFrame() {
        let metalLayer = metalView.metalLayer
        guard metalLayer.drawableSize.width > 0,
              let drawable = metalLayer.nextDrawable(),
              let commandBuffer = commandQueue.makeCommandBuffer() else { return }

        frameIndex += 1
        // Slowly cycling clear color: two screenshots taken seconds apart will differ,
        // proving a live render loop rather than a single presented frame.
        let t = Double(frameIndex) / 240.0
        let pass = MTLRenderPassDescriptor()
        pass.colorAttachments[0].texture = drawable.texture
        pass.colorAttachments[0].loadAction = .clear
        pass.colorAttachments[0].storeAction = .store
        pass.colorAttachments[0].clearColor = MTLClearColor(
            red: 0.10 + 0.08 * sin(t), green: 0.12, blue: 0.22 + 0.08 * cos(t), alpha: 1.0)

        if let encoder = commandBuffer.makeRenderCommandEncoder(descriptor: pass) {
            encoder.setRenderPipelineState(pipelineState)
            encoder.drawPrimitives(type: .triangle, vertexStart: 0, vertexCount: 3)
            encoder.endEncoding()
        }
        commandBuffer.present(drawable)
        commandBuffer.commit()

        if !wroteFirstFrameMarker {
            wroteFirstFrameMarker = true
            writeFirstFrameMarker()
        }
        if frameIndex % 30 == 0 {
            hudLabel.text = "KisakCOD iOS stub — Metal live\nGPU: \(device.name)  frame \(frameIndex)"
        }
    }

    // Proof-of-run artifact: CI (and a curious human) can pull this out of the app
    // sandbox container to verify Metal actually presented a frame. Doubles as the
    // first demonstration of the Documents-directory write path that Objective 3
    // (filesystem sandboxing) will route the whole engine through.
    private func writeFirstFrameMarker() {
        let docs = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
        let marker = docs.appendingPathComponent("metal_first_frame.txt")
        let contents = """
        KisakCOD iOS stub: first Metal frame presented OK
        gpu=\(device.name)
        drawableSize=\(metalView.metalLayer.drawableSize)
        date=\(ISO8601DateFormatter().string(from: Date()))
        """
        try? contents.write(to: marker, atomically: true, encoding: .utf8)
        NSLog("KISAK_STUB_FIRST_FRAME_OK gpu=%@", device.name)
    }
}
