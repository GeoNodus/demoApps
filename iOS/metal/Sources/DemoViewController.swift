// Shell of the iOS demo, as DemoApp.swift of the macOS demo: a full-screen 3D view (Metal) with the title on top and a
// bar at the bottom — Open (the graphs bundled with the app or Files), previous / next scene, the menu of demo scenes by
// node type, Play, the frame slider, the status line.
// Touch: one finger — rotate, pinch — zoom, two fingers — pan, double tap — frame all. A hardware keyboard (iPad,
// simulator): Space — Play, F — frame all, ← / → — frame step, PgUp / PgDn (or ↑ / ↓) — previous / next scene.
// Cooks are asynchronous: gnCookNode, then polling gnGetCookState
import MetalKit
import UIKit
import UniformTypeIdentifiers

private let fovy: Float = 45 * .pi / 180
// Pinch and wheel-like zoom: as zoomPerNotch of the macOS demo
private let zoomPerNotch: Float = 1.15
// Demo graphs bundled with the app (copies of ../scenes/*.json; the demo edition opens them without the node limit)
private let bundledGraphs = ["animated_scene", "castle_scene", "gear_mechanism_scene", "log_house_scene",
                             "polyextrude_tube_scene"]

final class DemoViewController: UIViewController, MTKViewDelegate, UIGestureRecognizerDelegate,
    UIDocumentPickerDelegate {
    private let appTitle = "GeoNodus Metal demo"
    private var metalView: MTKView!
    private var renderer: MetalRenderer?
    private let titleLabel = UILabel()
    private let statusLabel = UILabel()
    private let openButton = UIButton(type: .system)
    private let prevButton = UIButton(type: .system)
    private let nextButton = UIButton(type: .system)
    private let sceneButton = UIButton(type: .system)
    private let playButton = UIButton(type: .system)
    private let slider = UISlider()
    private let frameLabel = UILabel()

    private let scene = GnScene()
    private var geometry = SceneGeometry()
    private var file = ""
    private var sceneIndex = -1 // the demo scene shown, -1 — a graph file (file) or nothing

    // Camera: an orbit around the target, angles in radians
    private var target = Vec3(0, 0, 0)
    private var distance: Float = 10
    private var yaw: Float = 0.785
    private var pitch: Float = 0.45
    private var framed = false
    private var lastPan = CGPoint.zero
    private var lastPan2 = CGPoint.zero

    // Real-time playback: the frame follows the elapsed time, frames whose cook takes longer than 1/fps are skipped
    private var playing = false
    private var playStartFrame = 0
    private var playStartTime = 0.0
    private var requestedFrame = 0
    private var timer: Timer?
    private var started = false
    private var pendingURL: URL?

    // --- Startup -----------------------------------------------------------------------------------------------------

    override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = .black
        guard let device = MTLCreateSystemDefaultDevice() else {
            showFatal("No Metal device.")
            return
        }
        do {
            renderer = try MetalRenderer(device: device)
        } catch {
            showFatal("Cannot initialize Metal:\n\n\(error.localizedDescription)")
            return
        }
        guard scene.isValid else {
            showFatal("Cannot create a GeoNodus session.")
            return
        }
        createViews(device: device)
        updateTitle()
        updateStatus()
    }

    override func viewDidAppear(_ animated: Bool) {
        super.viewDidAppear(animated)
        becomeFirstResponder()
        guard !started, renderer != nil, scene.isValid else { return }
        started = true

        // Launch arguments: [graph.json] [--scene <node type>] [--play]. Without a graph — the first demo scene
        var play = false
        let args = Array(ProcessInfo.processInfo.arguments.dropFirst())
        var i = 0
        while i < args.count {
            let arg = args[i]
            if arg == "--play" {
                play = true
            } else if arg == "--scene" && i + 1 < args.count {
                i += 1
                let index = Int(demoSceneFind(args[i]))
                if index >= 0 { openScene(index) } else { alert("No demo scene for the node type \(args[i])") }
            } else if arg.hasPrefix("-") {
                // system arguments (-AppleLanguages (ru), ...) — skip the value too
                if i + 1 < args.count && !args[i + 1].hasPrefix("-") { i += 1 }
            } else {
                openFile(resolveGraph(arg))
            }
            i += 1
        }
        if let url = pendingURL { openExternal(url) }
        pendingURL = nil
        if !scene.isLoaded { openScene(0) }
        if play && scene.isLoaded { setPlaying(true) }
    }

    override var canBecomeFirstResponder: Bool { true }
    override var prefersHomeIndicatorAutoHidden: Bool { true }

    // A bundled graph by name ("castle_scene" / "castle_scene.json") or a path
    private func resolveGraph(_ arg: String) -> String {
        let name = (arg as NSString).deletingPathExtension
        if !arg.contains("/"), let path = Bundle.main.path(forResource: name, ofType: "json") { return path }
        return arg
    }

    private func showFatal(_ text: String) {
        let label = UILabel()
        label.text = text
        label.textColor = .white
        label.numberOfLines = 0
        label.textAlignment = .center
        label.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(label)
        NSLayoutConstraint.activate([
            label.centerXAnchor.constraint(equalTo: view.centerXAnchor),
            label.centerYAnchor.constraint(equalTo: view.centerYAnchor),
            label.widthAnchor.constraint(lessThanOrEqualTo: view.widthAnchor, constant: -40),
        ])
    }

    private func alert(_ text: String) {
        let a = UIAlertController(title: appTitle, message: text, preferredStyle: .alert)
        a.addAction(UIAlertAction(title: "OK", style: .default))
        if presentedViewController == nil { present(a, animated: true) }
    }

    private func createViews(device: MTLDevice) {
        metalView = MTKView(frame: view.bounds, device: device)
        metalView.delegate = self
        metalView.colorPixelFormat = MetalRenderer.colorFormat
        metalView.depthStencilPixelFormat = MetalRenderer.depthFormat
        metalView.sampleCount = MetalRenderer.sampleCount
        // Drawn on demand: after new geometry, camera moves and resizes
        metalView.isPaused = true
        metalView.enableSetNeedsDisplay = true
        metalView.autoResizeDrawable = true

        // Gestures: one finger — rotate, two fingers — pan, pinch — zoom, double tap — frame all
        let rotate = UIPanGestureRecognizer(target: self, action: #selector(rotateGesture(_:)))
        rotate.maximumNumberOfTouches = 1
        let pan = UIPanGestureRecognizer(target: self, action: #selector(panGesture(_:)))
        pan.minimumNumberOfTouches = 2
        pan.maximumNumberOfTouches = 2
        let pinch = UIPinchGestureRecognizer(target: self, action: #selector(pinchGesture(_:)))
        let doubleTap = UITapGestureRecognizer(target: self, action: #selector(doubleTapGesture))
        doubleTap.numberOfTapsRequired = 2
        for g in [rotate, pan, pinch, doubleTap] as [UIGestureRecognizer] {
            g.delegate = self
            metalView.addGestureRecognizer(g)
        }

        titleLabel.textColor = .white
        titleLabel.font = .preferredFont(forTextStyle: .footnote)
        titleLabel.numberOfLines = 4
        titleLabel.shadowColor = UIColor.black.withAlphaComponent(0.6)
        titleLabel.shadowOffset = CGSize(width: 0, height: 1)

        statusLabel.font = .monospacedDigitSystemFont(ofSize: 11, weight: .regular)
        statusLabel.textColor = .secondaryLabel
        statusLabel.adjustsFontSizeToFitWidth = true
        statusLabel.minimumScaleFactor = 0.7

        openButton.setImage(UIImage(systemName: "folder"), for: .normal)
        openButton.menu = openMenu()
        openButton.showsMenuAsPrimaryAction = true
        prevButton.setImage(UIImage(systemName: "chevron.left"), for: .normal)
        prevButton.addTarget(self, action: #selector(prevScene), for: .touchUpInside)
        nextButton.setImage(UIImage(systemName: "chevron.right"), for: .normal)
        nextButton.addTarget(self, action: #selector(nextScene), for: .touchUpInside)
        sceneButton.setTitle("Scenes", for: .normal)
        sceneButton.titleLabel?.font = .preferredFont(forTextStyle: .headline)
        sceneButton.titleLabel?.lineBreakMode = .byTruncatingTail
        sceneButton.menu = sceneMenu()
        sceneButton.showsMenuAsPrimaryAction = true
        sceneButton.setContentCompressionResistancePriority(.defaultLow, for: .horizontal)
        playButton.setImage(UIImage(systemName: "play.fill"), for: .normal)
        playButton.addTarget(self, action: #selector(playPressed), for: .touchUpInside)
        playButton.isEnabled = false
        slider.addTarget(self, action: #selector(sliderMoved), for: .valueChanged)
        slider.isEnabled = false
        frameLabel.font = .monospacedDigitSystemFont(ofSize: 13, weight: .regular)
        frameLabel.textAlignment = .right
        frameLabel.text = "0"
        for b in [openButton, prevButton, nextButton, playButton] {
            b.widthAnchor.constraint(equalToConstant: 40).isActive = true
        }
        frameLabel.widthAnchor.constraint(equalToConstant: 44).isActive = true

        let row1 = UIStackView(arrangedSubviews: [openButton, prevButton, sceneButton, nextButton])
        row1.spacing = 4
        let row2 = UIStackView(arrangedSubviews: [playButton, slider, frameLabel])
        row2.spacing = 8
        let bar = UIStackView(arrangedSubviews: [row1, row2, statusLabel])
        bar.axis = .vertical
        bar.spacing = 4
        bar.isLayoutMarginsRelativeArrangement = true
        bar.directionalLayoutMargins = NSDirectionalEdgeInsets(top: 6, leading: 8, bottom: 6, trailing: 8)
        let barBackground = UIVisualEffectView(effect: UIBlurEffect(style: .systemThinMaterial))

        for v in [metalView!, barBackground, bar, titleLabel] as [UIView] {
            v.translatesAutoresizingMaskIntoConstraints = false
            view.addSubview(v)
        }
        let safe = view.safeAreaLayoutGuide
        NSLayoutConstraint.activate([
            metalView.topAnchor.constraint(equalTo: view.topAnchor),
            metalView.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            metalView.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            metalView.bottomAnchor.constraint(equalTo: view.bottomAnchor),
            titleLabel.topAnchor.constraint(equalTo: safe.topAnchor, constant: 4),
            titleLabel.leadingAnchor.constraint(equalTo: safe.leadingAnchor, constant: 12),
            titleLabel.trailingAnchor.constraint(equalTo: safe.trailingAnchor, constant: -12),
            bar.leadingAnchor.constraint(equalTo: safe.leadingAnchor),
            bar.trailingAnchor.constraint(equalTo: safe.trailingAnchor),
            bar.bottomAnchor.constraint(equalTo: safe.bottomAnchor),
            bar.widthAnchor.constraint(lessThanOrEqualToConstant: 900),
            barBackground.topAnchor.constraint(equalTo: bar.topAnchor),
            barBackground.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            barBackground.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            barBackground.bottomAnchor.constraint(equalTo: view.bottomAnchor),
        ])
    }

    // Open: the graphs bundled with the app and any .json from Files
    private func openMenu() -> UIMenu {
        var items: [UIMenuElement] = bundledGraphs.compactMap { name in
            guard let path = Bundle.main.path(forResource: name, ofType: "json") else { return nil }
            return UIAction(title: name + ".json", image: UIImage(systemName: "doc")) { [weak self] _ in
                self?.openFile(path)
            }
        }
        items.append(UIAction(title: "Browse Files…", image: UIImage(systemName: "folder")) { [weak self] _ in
            self?.browseFiles()
        })
        return UIMenu(title: "Open a graph", children: items)
    }

    // Demo scenes built in code: one per supported node type
    private func sceneMenu() -> UIMenu {
        let actions = (0..<Int(demoSceneCount())).map { i -> UIAction in
            let type = String(cString: demoSceneNodeType(Int32(i)))
            let action = UIAction(title: type, subtitle: String(cString: demoSceneDescription(Int32(i)))) {
                [weak self] _ in self?.openScene(i)
            }
            action.state = i == sceneIndex ? .on : .off
            return action
        }
        return UIMenu(title: "Demo scene by node type", children: actions)
    }

    // --- Commands ----------------------------------------------------------------------------------------------------

    private func browseFiles() {
        let picker = UIDocumentPickerViewController(forOpeningContentTypes: [.json], asCopy: true)
        picker.delegate = self
        picker.allowsMultipleSelection = false
        present(picker, animated: true)
    }

    func documentPicker(_ controller: UIDocumentPickerViewController, didPickDocumentsAt urls: [URL]) {
        // A copy in the app's sandbox, under the original name: the demo edition recognizes the demo scenes by name
        if let url = urls.first { openFile(url.path) }
    }

    // A file from outside the app (Files, AirDrop): read while access is granted
    func openExternal(_ url: URL) {
        guard started else {
            pendingURL = url
            return
        }
        let access = url.startAccessingSecurityScopedResource()
        defer { if access { url.stopAccessingSecurityScopedResource() } }
        openFile(url.path)
    }

    @objc private func prevScene() { stepScene(next: false) }
    @objc private func nextScene() { stepScene(next: true) }

    private func stepScene(next: Bool) {
        let count = Int(demoSceneCount())
        let current = sceneIndex < 0 ? (next ? -1 : 0) : sceneIndex
        openScene((current + (next ? 1 : count - 1)) % count)
    }

    @objc private func playPressed() { setPlaying(!playing) }

    @objc private func sliderMoved() {
        requestFrame(Int(slider.value.rounded()))
        if playing { restartPlayClock() }
        scheduleTick()
    }

    func openFile(_ path: String) {
        setPlaying(false)
        if let error = scene.load(path: path) {
            alert("Cannot load \((path as NSString).lastPathComponent)\n\n\(error)")
            return
        }
        file = path
        sceneIndex = -1
        sceneReplaced()
    }

    // Builds demo scene `index` of the registry (the scene menu)
    private func openScene(_ index: Int) {
        guard index >= 0 && index < Int(demoSceneCount()) else { return }
        let wasPlaying = playing
        setPlaying(false)
        let error = scene.build(sceneIndex: index)
        file = ""
        sceneIndex = index
        sceneReplaced()
        if !error.isEmpty {
            let type = String(cString: demoSceneNodeType(Int32(index)))
            alert("The scene \"\(type)\" was built with problems:\n\n\(error)")
        }
        // Switching scenes while playing keeps playing
        if wasPlaying { setPlaying(true) }
    }

    // After a new graph: the timeline of the slider, the first frame, the camera is framed on the first geometry
    private func sceneReplaced() {
        framed = false
        geometry.clear()
        renderer?.setGeometry(geometry)
        slider.minimumValue = Float(scene.firstFrame)
        slider.maximumValue = Float(max(scene.lastFrame, scene.firstFrame + 1))
        playButton.isEnabled = true
        slider.isEnabled = true
        sceneButton.setTitle(sceneIndex >= 0 ? String(cString: demoSceneNodeType(Int32(sceneIndex)))
                             : (file as NSString).lastPathComponent, for: .normal)
        sceneButton.menu = sceneMenu()
        requestFrame(scene.startFrame)
        updateTitle()
        updateStatus()
        metalView.setNeedsDisplay()
        scheduleTick()
    }

    private func setPlaying(_ play: Bool) {
        if play && !scene.isLoaded { return }
        playing = play
        if play { restartPlayClock() }
        playButton.setImage(UIImage(systemName: play ? "pause.fill" : "play.fill"), for: .normal)
        // the screen stays on while the animation plays
        UIApplication.shared.isIdleTimerDisabled = play
        scheduleTick()
    }

    private func restartPlayClock() {
        playStartFrame = requestedFrame
        playStartTime = CACurrentMediaTime()
    }

    private func requestFrame(_ frame: Int) {
        requestedFrame = frame
        if Int(slider.value.rounded()) != frame && !slider.isTracking { slider.value = Float(frame) }
        frameLabel.text = "\(frame)"
    }

    private func stepFrame(forward: Bool) {
        guard scene.isLoaded else { return }
        setPlaying(false)
        let range = scene.lastFrame - scene.firstFrame + 1
        let f = requestedFrame - scene.firstFrame + (forward ? 1 : range - 1)
        requestFrame(scene.firstFrame + f % range)
        scheduleTick()
    }

    // --- Hardware keyboard ---------------------------------------------------------------------------------------------

    override var keyCommands: [UIKeyCommand]? {
        let commands: [(String, String)] = [
            (" ", "Play / Pause"), ("f", "Frame All"), (UIKeyCommand.inputLeftArrow, "Previous Frame"),
            (UIKeyCommand.inputRightArrow, "Next Frame"), (UIKeyCommand.inputPageUp, "Previous Scene"),
            (UIKeyCommand.inputPageDown, "Next Scene"), (UIKeyCommand.inputUpArrow, "Previous Scene"),
            (UIKeyCommand.inputDownArrow, "Next Scene"),
        ]
        return commands.map { input, title in
            let c = UIKeyCommand(title: title, action: #selector(keyCommand(_:)), input: input)
            c.wantsPriorityOverSystemBehavior = true
            return c
        }
    }

    @objc private func keyCommand(_ command: UIKeyCommand) {
        switch command.input {
        case " ": setPlaying(!playing)
        case "f":
            frameAll()
            metalView.setNeedsDisplay()
        case UIKeyCommand.inputLeftArrow: stepFrame(forward: false)
        case UIKeyCommand.inputRightArrow: stepFrame(forward: true)
        case UIKeyCommand.inputPageUp, UIKeyCommand.inputUpArrow: stepScene(next: false)
        case UIKeyCommand.inputPageDown, UIKeyCommand.inputDownArrow: stepScene(next: true)
        default: break
        }
    }

    // --- Camera --------------------------------------------------------------------------------------------------------

    func gestureRecognizer(_ g: UIGestureRecognizer, shouldRecognizeSimultaneouslyWith other: UIGestureRecognizer)
        -> Bool {
        // pinch and two-finger pan together; one-finger rotate alone
        (g is UIPinchGestureRecognizer || other is UIPinchGestureRecognizer)
            && !((g as? UIPanGestureRecognizer)?.maximumNumberOfTouches == 1
                || (other as? UIPanGestureRecognizer)?.maximumNumberOfTouches == 1)
    }

    private var eye: Vec3 {
        let dir = Vec3(cos(pitch) * sin(yaw), sin(pitch), cos(pitch) * cos(yaw))
        return target + dir * distance
    }

    @objc private func rotateGesture(_ g: UIPanGestureRecognizer) {
        let p = g.translation(in: metalView)
        if g.state == .began { lastPan = .zero }
        let dx = Float(p.x - lastPan.x), dy = Float(p.y - lastPan.y)
        lastPan = p
        yaw -= dx * 0.008
        pitch = min(max(pitch + dy * 0.008, -1.55), 1.55)
        metalView.setNeedsDisplay()
    }

    @objc private func panGesture(_ g: UIPanGestureRecognizer) {
        let p = g.translation(in: metalView)
        if g.state == .began { lastPan2 = .zero }
        let dx = Float(p.x - lastPan2.x), dy = Float(p.y - lastPan2.y)
        lastPan2 = p
        // Pan the target in the screen plane: a point is a fraction of the visible height at the target distance
        let perPoint = 2 * distance * tan(fovy * 0.5) / Float(max(1, metalView.bounds.height))
        let f = simd_normalize(target - eye)
        let right = simd_normalize(simd_cross(f, Vec3(0, 1, 0)))
        let up = simd_cross(right, f)
        target = target - right * (dx * perPoint) + up * (dy * perPoint)
        metalView.setNeedsDisplay()
    }

    @objc private func pinchGesture(_ g: UIPinchGestureRecognizer) {
        guard g.scale > 0 else { return }
        distance /= Float(g.scale)
        g.scale = 1
        metalView.setNeedsDisplay()
    }

    @objc private func doubleTapGesture() {
        frameAll()
        metalView.setNeedsDisplay()
    }

    private func frameAll() {
        guard let a = geometry.boundsMin else { return }
        let b = geometry.boundsMax
        target = (a + b) * 0.5
        let radius = max(0.5 * simd_length(b - a), 1e-3)
        // the narrower side of a portrait screen fits the scene too
        let size = metalView.bounds.size
        let aspect = Float(size.width / max(size.height, 1))
        let fov = aspect < 1 ? 2 * atan(tan(fovy * 0.5) * aspect) : fovy
        distance = radius / sin(fov * 0.5) * 1.05
    }

    private func viewParams() -> ViewParams {
        let e = eye
        // Near and far planes from the scene bounds, so that nothing is clipped at any distance
        var radius = distance
        var centerDist = distance
        if let a = geometry.boundsMin {
            let b = geometry.boundsMax
            radius = max(0.5 * simd_length(b - a), 1e-3)
            centerDist = simd_length((a + b) * 0.5 - e)
        }
        let zFar = max(centerDist + radius * 1.1, 1e-2)
        let zNear = max(centerDist - radius * 1.1, zFar * 1e-4)
        return ViewParams(view: lookAt(eye: e, target: target, up: Vec3(0, 1, 0)), fovy: fovy, zNear: zNear,
                          zFar: zFar)
    }

    // --- MTKViewDelegate ---------------------------------------------------------------------------------------------

    func mtkView(_ view: MTKView, drawableSizeWillChange size: CGSize) {
        view.setNeedsDisplay()
    }

    func draw(in view: MTKView) {
        guard let renderer else { return }
        renderer.pointSize = 4 * Float(view.contentScaleFactor)
        renderer.render(in: view, params: viewParams())
    }

    // --- Cooking and playback ----------------------------------------------------------------------------------------

    // Time until the next playback frame, s
    private func playWait() -> Double {
        let frameTime = 1 / scene.fps
        let elapsed = CACurrentMediaTime() - playStartTime
        let next = ((elapsed / frameTime).rounded(.down) + 1) * frameTime
        return max(0.001, next - elapsed)
    }

    // The next tick: while cooking — poll its state every millisecond, while playing — when the frame changes,
    // otherwise none until something changes
    private func scheduleTick() {
        timer?.invalidate()
        timer = nil
        guard scene.isLoaded else { return }
        let wait: Double
        if scene.isCooking {
            wait = 0.001
        } else if scene.cookedFrame != requestedFrame {
            wait = 0
        } else if playing {
            wait = playWait()
        } else {
            return
        }
        let t = Timer(timeInterval: wait, repeats: false) { [weak self] _ in self?.tick() }
        // .common — also while the slider is dragged or a menu is open
        RunLoop.main.add(t, forMode: .common)
        timer = t
    }

    private func tick() {
        timer = nil
        guard scene.isLoaded else { return }
        if playing {
            let first = scene.firstFrame
            let range = scene.lastFrame - first + 1
            let elapsed = Int((CACurrentMediaTime() - playStartTime) * scene.fps)
            let frame = first + (playStartFrame - first + elapsed) % range
            if frame != requestedFrame { requestFrame(frame) }
        }

        if scene.isCooking && scene.pollCook(&geometry) {
            renderer?.setGeometry(geometry)
            if !framed && geometry.boundsMin != nil {
                frameAll()
                framed = true
            }
            updateStatus()
            metalView.setNeedsDisplay()
        }
        // A new cook only after the previous one finished: with slow cooks the frames in between are skipped
        if !scene.isCooking && scene.cookedFrame != requestedFrame { scene.startCook(frame: requestedFrame) }
        scheduleTick()
    }

    private func updateTitle() {
        var t = appTitle + (scene.isDemoEdition ? " (demo edition)" : "")
        if let renderer { t += " — " + renderer.description }
        if !file.isEmpty {
            t += "\n" + (file as NSString).lastPathComponent
        } else if sceneIndex >= 0 {
            let type = String(cString: demoSceneNodeType(Int32(sceneIndex)))
            let description = String(cString: demoSceneDescription(Int32(sceneIndex)))
            t += "\n\(type): \(description)"
        }
        titleLabel.text = t
    }

    private func updateStatus() {
        if !scene.isLoaded {
            statusLabel.text = "Choose a node type or open a graph .json.  Drag rotate, pinch zoom, two fingers pan, "
                + "double tap frame"
        } else {
            statusLabel.text = String(format: "Frame %d   cook %.1f ms   read %.1f ms   %d tris   %d lines   %d points",
                                      scene.cookedFrame, scene.lastCookMs, scene.lastFetchMs,
                                      geometry.triangles.count / 3, geometry.lines.count / 2, geometry.points.count)
        }
    }
}
