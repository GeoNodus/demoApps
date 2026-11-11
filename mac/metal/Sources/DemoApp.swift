// Shell of the demo app in AppKit, as the Windows demos (win/common/demo_app.cpp): the main window with a 3D view
// (Metal) and a bar under it — Open, the combo box of demo scenes by node type, Play, the frame slider, the status line.
// Orbit camera: LMB — rotate, wheel / RMB / Ctrl+LMB — zoom, MMB / Shift+LMB — pan, F — frame all, Space — Play,
// PgUp / PgDn — previous / next scene, ← / → — frame step, dropping a .gng onto the window opens it. Cooks are
// asynchronous: gnCookNode, then polling gnGetCookState
import AppKit
import MetalKit
import UniformTypeIdentifiers

// GeoNodus graph file (*.gng, gnSaveGraph): JSON, declared in Info.plist (UTExportedTypeDeclarations)
private let geonodusGraphType = UTType(exportedAs: "com.geonodus.graph", conformingTo: .json)

private let fovy: Float = 45 * .pi / 180
// Wheel: each notch changes the distance to the target by zoomPerNotch, as in the Qt client
private let zoomPerNotch: Float = 1.15

// The 3D view: forwards mouse, keys and dropped files to the app
final class DemoView: MTKView {
    weak var app: DemoApp?
    private var dragButton = 0 // 1 — rotate, 2 — pan, 3 — zoom

    override var acceptsFirstResponder: Bool { true }
    override func acceptsFirstMouse(for event: NSEvent?) -> Bool { true }

    private func beginDrag(_ button: Int, _ event: NSEvent) {
        window?.makeFirstResponder(self)
        dragButton = button
        // Shift + left — pan, Ctrl + left — zoom (for touchpads without a middle button)
        if button == 1 && event.modifierFlags.contains(.shift) {
            dragButton = 2
        } else if button == 1 && event.modifierFlags.contains(.control) {
            dragButton = 3
        }
    }

    override func mouseDown(with event: NSEvent) { beginDrag(1, event) }
    override func rightMouseDown(with event: NSEvent) { beginDrag(3, event) }
    override func otherMouseDown(with event: NSEvent) { beginDrag(2, event) }
    override func mouseUp(with event: NSEvent) { dragButton = 0 }
    override func rightMouseUp(with event: NSEvent) { dragButton = 0 }
    override func otherMouseUp(with event: NSEvent) { dragButton = 0 }
    override func mouseDragged(with event: NSEvent) { drag(event) }
    override func rightMouseDragged(with event: NSEvent) { drag(event) }
    override func otherMouseDragged(with event: NSEvent) { drag(event) }

    private func drag(_ event: NSEvent) {
        guard dragButton != 0 else { return }
        // deltaY grows downward, as the window coordinates of the Windows demos
        app?.dragCamera(button: dragButton, dx: Float(event.deltaX), dy: Float(event.deltaY))
    }

    override func scrollWheel(with event: NSEvent) {
        // A wheel notch — about one unit of deltaY; a touchpad gives precise deltas in points
        let notches = event.hasPreciseScrollingDeltas ? event.scrollingDeltaY / 30 : event.scrollingDeltaY
        app?.zoom(notches: Float(notches))
    }

    override func magnify(with event: NSEvent) {
        app?.zoom(notches: Float(event.magnification) * 8)
    }

    override func keyDown(with event: NSEvent) {
        if app?.handleKey(event) != true { super.keyDown(with: event) }
    }

    // Dropping a .gng onto the view opens it
    override func draggingEntered(_ sender: NSDraggingInfo) -> NSDragOperation {
        droppedFile(sender) != nil ? .copy : []
    }

    override func performDragOperation(_ sender: NSDraggingInfo) -> Bool {
        guard let url = droppedFile(sender) else { return false }
        app?.openFile(url.path)
        return true
    }

    private func droppedFile(_ sender: NSDraggingInfo) -> URL? {
        let urls = sender.draggingPasteboard.readObjects(forClasses: [NSURL.self],
                                                         options: [.urlReadingFileURLsOnly: true]) as? [URL]
        return urls?.first
    }
}

final class DemoApp: NSObject, NSApplicationDelegate, NSWindowDelegate, MTKViewDelegate {
    private let title = "GeoNodus Metal demo"
    private var window: NSWindow!
    private var view: DemoView!
    private var openButton: NSButton!
    private var scenePopup: NSPopUpButton!
    private var playButton: NSButton!
    private var slider: NSSlider!
    private var status: NSTextField!
    private var renderer: MetalRenderer!

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

    // Real-time playback (as Realtime in the Qt client): the frame follows the elapsed time, frames whose cook takes
    // longer than 1/fps are skipped
    private var playing = false
    private var playStartFrame = 0
    private var playStartTime = 0.0
    private var requestedFrame = 0
    private var timer: Timer?
    // A file from Finder that arrived before the window was created
    private var pendingFile: String?

    // --- Startup -----------------------------------------------------------------------------------------------------

    func applicationDidFinishLaunching(_ notification: Notification) {
        buildMenu()
        guard scene.isValid else {
            fail("Cannot create a GeoNodus session.")
            return
        }
        guard let device = MTLCreateSystemDefaultDevice() else {
            fail("No Metal device.")
            return
        }
        do {
            renderer = try MetalRenderer(device: device)
        } catch {
            fail("Cannot initialize Metal:\n\n\(error.localizedDescription)")
            return
        }
        createWindow(device: device)
        updateTitle()
        updateStatus()

        // Command line: [graph.gng] [--scene <node type>] [--play]. Without a graph — the first demo scene
        var play = false
        let args = Array(CommandLine.arguments.dropFirst())
        var i = 0
        while i < args.count {
            let arg = args[i]
            if arg == "--play" {
                play = true
            } else if arg == "--scene" && i + 1 < args.count {
                i += 1
                let index = DemoCatalog.find(args[i])
                if index >= 0 {
                    openScene(index)
                } else {
                    alert("No demo scene for the node type \(args[i])", style: .warning)
                }
            } else if arg.hasPrefix("-") {
                // AppKit's own arguments (-NSDocumentRevisionsDebugMode YES, ...) — skip the value too
                if i + 1 < args.count && !args[i + 1].hasPrefix("-") { i += 1 }
            } else {
                openFile(arg)
            }
            i += 1
        }
        if let path = pendingFile { openFile(path) }
        pendingFile = nil
        if !scene.isLoaded { openScene(0) }
        if play && scene.isLoaded { setPlaying(true) }

        window.makeKeyAndOrderFront(nil)
        window.makeFirstResponder(view)
        NSApp.activate(ignoringOtherApps: true)
    }

    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { true }

    // Files opened from Finder / dropped onto the Dock icon
    func application(_ application: NSApplication, open urls: [URL]) {
        guard let url = urls.first else { return }
        if window != nil { openFile(url.path) } else { pendingFile = url.path }
    }

    private func fail(_ text: String) {
        alert(text, style: .critical)
        NSApp.terminate(nil)
    }

    private func alert(_ text: String, style: NSAlert.Style) {
        let a = NSAlert()
        a.messageText = title
        a.informativeText = text
        a.alertStyle = style
        if let w = window, w.isVisible {
            a.beginSheetModal(for: w)
        } else {
            a.runModal()
        }
    }

    private func buildMenu() {
        let main = NSMenu()
        let appItem = NSMenuItem()
        main.addItem(appItem)
        let appMenu = NSMenu()
        appMenu.addItem(withTitle: "Hide \(title)", action: #selector(NSApplication.hide(_:)), keyEquivalent: "h")
        appMenu.addItem(.separator())
        appMenu.addItem(withTitle: "Quit \(title)", action: #selector(NSApplication.terminate(_:)), keyEquivalent: "q")
        appItem.submenu = appMenu

        let fileItem = NSMenuItem()
        main.addItem(fileItem)
        let fileMenu = NSMenu(title: "File")
        fileMenu.addItem(withTitle: "Open…", action: #selector(openDialog), keyEquivalent: "o").target = self
        fileMenu.addItem(withTitle: "Close Window", action: #selector(NSWindow.performClose(_:)), keyEquivalent: "w")
        fileItem.submenu = fileMenu

        let windowItem = NSMenuItem()
        main.addItem(windowItem)
        let windowMenu = NSMenu(title: "Window")
        windowMenu.addItem(withTitle: "Minimize", action: #selector(NSWindow.performMiniaturize(_:)),
                           keyEquivalent: "m")
        windowMenu.addItem(withTitle: "Zoom", action: #selector(NSWindow.performZoom(_:)), keyEquivalent: "")
        windowItem.submenu = windowMenu
        NSApp.windowsMenu = windowMenu
        NSApp.mainMenu = main
    }

    private func createWindow(device: MTLDevice) {
        window = NSWindow(contentRect: NSRect(x: 0, y: 0, width: 1280, height: 800),
                          styleMask: [.titled, .closable, .miniaturizable, .resizable], backing: .buffered,
                          defer: false)
        window.minSize = NSSize(width: 640, height: 360)
        window.delegate = self
        window.isReleasedWhenClosed = false
        window.center()

        view = DemoView(frame: .zero, device: device)
        view.app = self
        view.delegate = self
        view.colorPixelFormat = MetalRenderer.colorFormat
        view.depthStencilPixelFormat = MetalRenderer.depthFormat
        view.sampleCount = MetalRenderer.sampleCount
        // Drawn on demand: after new geometry, camera moves and resizes
        view.isPaused = true
        view.enableSetNeedsDisplay = true
        view.registerForDraggedTypes([.fileURL])

        openButton = NSButton(title: "Open...", target: self, action: #selector(openDialog))
        // Demo scenes built in code: one per supported node type
        scenePopup = NSPopUpButton(frame: .zero, pullsDown: false)
        for i in 0..<DemoCatalog.count {
            scenePopup.addItem(withTitle: DemoCatalog.name(i))
        }
        scenePopup.target = self
        scenePopup.action = #selector(sceneChosen)
        playButton = NSButton(title: "Play", target: self, action: #selector(playPressed))
        slider = NSSlider(value: 0, minValue: 0, maxValue: 1, target: self, action: #selector(sliderMoved))
        slider.isContinuous = true
        status = NSTextField(labelWithString: "")
        status.lineBreakMode = .byTruncatingTail
        status.font = NSFont.monospacedDigitSystemFont(ofSize: NSFont.systemFontSize, weight: .regular)
        playButton.isEnabled = false
        slider.isEnabled = false

        let bar = NSStackView(views: [openButton, scenePopup, playButton, slider, status])
        bar.orientation = .horizontal
        bar.spacing = 6
        bar.edgeInsets = NSEdgeInsets(top: 0, left: 6, bottom: 0, right: 6)
        bar.alignment = .centerY

        let content = NSView()
        for v in [view!, bar] as [NSView] {
            v.translatesAutoresizingMaskIntoConstraints = false
            content.addSubview(v)
        }
        for b in [openButton!, playButton!] { b.widthAnchor.constraint(equalToConstant: 84).isActive = true }
        scenePopup.widthAnchor.constraint(equalToConstant: 160).isActive = true
        status.widthAnchor.constraint(equalToConstant: 420).isActive = true
        slider.widthAnchor.constraint(greaterThanOrEqualToConstant: 60).isActive = true
        slider.setContentHuggingPriority(.defaultLow, for: .horizontal)
        NSLayoutConstraint.activate([
            view.topAnchor.constraint(equalTo: content.topAnchor),
            view.leadingAnchor.constraint(equalTo: content.leadingAnchor),
            view.trailingAnchor.constraint(equalTo: content.trailingAnchor),
            bar.topAnchor.constraint(equalTo: view.bottomAnchor),
            bar.leadingAnchor.constraint(equalTo: content.leadingAnchor),
            bar.trailingAnchor.constraint(equalTo: content.trailingAnchor),
            bar.bottomAnchor.constraint(equalTo: content.bottomAnchor),
            bar.heightAnchor.constraint(equalToConstant: 40),
        ])
        window.contentView = content
    }

    func windowWillClose(_ notification: Notification) {
        setPlaying(false)
        timer?.invalidate()
        timer = nil
    }

    // --- Commands ----------------------------------------------------------------------------------------------------

    @objc private func openDialog() {
        let panel = NSOpenPanel()
        panel.allowedContentTypes = [geonodusGraphType]
        panel.allowsMultipleSelection = false
        panel.beginSheetModal(for: window) { [weak self] response in
            if response == .OK, let url = panel.url { self?.openFile(url.path) }
            self?.focusView()
        }
    }

    @objc private func sceneChosen() {
        let index = scenePopup.indexOfSelectedItem
        if index >= 0 && index != sceneIndex { openScene(index) }
        focusView()
    }

    @objc private func playPressed() {
        setPlaying(!playing)
        focusView()
    }

    @objc private func sliderMoved() {
        requestFrame(Int(slider.doubleValue.rounded()))
        if playing { restartPlayClock() }
        if NSApp.currentEvent?.type == .leftMouseUp { focusView() }
        scheduleTick()
    }

    private func focusView() { window.makeFirstResponder(view) }

    func openFile(_ path: String) {
        setPlaying(false)
        if let error = scene.load(path: path) {
            alert("Cannot load \(path)\n\n\(error)", style: .critical)
            return
        }
        file = path
        sceneIndex = -1
        scenePopup.select(nil)
        sceneReplaced()
    }

    // Builds demo scene `index` of the registry (the scene combo box)
    private func openScene(_ index: Int) {
        guard index >= 0 && index < DemoCatalog.count else { return }
        let wasPlaying = playing
        setPlaying(false)
        let error = scene.build(sceneIndex: index)
        file = ""
        sceneIndex = index
        scenePopup.selectItem(at: index)
        sceneReplaced()
        if !error.isEmpty {
            let type = DemoCatalog.name(index)
            alert("The scene \"\(type)\" was built with problems:\n\n\(error)", style: .warning)
        }
        // Switching scenes while playing keeps playing
        if wasPlaying { setPlaying(true) }
    }

    // After a new graph: the timeline of the slider, the first frame, the camera is framed on the first geometry
    private func sceneReplaced() {
        framed = false
        geometry.clear()
        renderer.setGeometry(geometry)
        slider.minValue = Double(scene.firstFrame)
        slider.maxValue = Double(scene.lastFrame)
        playButton.isEnabled = true
        slider.isEnabled = true
        requestFrame(scene.startFrame)
        updateTitle()
        updateStatus()
        view.needsDisplay = true
        scheduleTick()
    }

    private func setPlaying(_ play: Bool) {
        if play && !scene.isLoaded { return }
        playing = play
        if play { restartPlayClock() }
        playButton?.title = play ? "Pause" : "Play"
        scheduleTick()
    }

    private func restartPlayClock() {
        playStartFrame = requestedFrame
        playStartTime = CACurrentMediaTime()
    }

    private func requestFrame(_ frame: Int) {
        requestedFrame = frame
        if Int(slider.doubleValue.rounded()) != frame { slider.doubleValue = Double(frame) }
    }

    // --- Keys and camera ---------------------------------------------------------------------------------------------

    func handleKey(_ event: NSEvent) -> Bool {
        let key = event.charactersIgnoringModifiers?.lowercased() ?? ""
        switch Int(event.keyCode) {
        case 116, 121: // Page Up / Page Down — the previous / next demo scene
            let count = DemoCatalog.count
            let next = event.keyCode == 121
            let current = sceneIndex < 0 ? (next ? -1 : 0) : sceneIndex
            openScene((current + (next ? 1 : count - 1)) % count)
            return true
        case 123, 124: // ← / → — a frame back / forward
            guard scene.isLoaded else { return true }
            setPlaying(false)
            let range = scene.lastFrame - scene.firstFrame + 1
            let f = requestedFrame - scene.firstFrame + (event.keyCode == 124 ? 1 : range - 1)
            requestFrame(scene.firstFrame + f % range)
            scheduleTick()
            return true
        case 49: // Space
            setPlaying(!playing)
            return true
        default:
            break
        }
        if key == "f" && event.modifierFlags.intersection([.command, .control, .option]).isEmpty {
            frameAll()
            view.needsDisplay = true
            return true
        }
        return false
    }

    private var eye: Vec3 {
        let dir = Vec3(cos(pitch) * sin(yaw), sin(pitch), cos(pitch) * cos(yaw))
        return target + dir * distance
    }

    func dragCamera(button: Int, dx: Float, dy: Float) {
        if button == 1 {
            yaw -= dx * 0.008
            pitch = min(max(pitch + dy * 0.008, -1.55), 1.55)
        } else if button == 2 {
            // Pan the target in the screen plane: a point is a fraction of the visible height at the target distance
            let perPoint = 2 * distance * tan(fovy * 0.5) / Float(max(1, view.bounds.height))
            let f = simd_normalize(target - eye)
            let right = simd_normalize(simd_cross(f, Vec3(0, 1, 0)))
            let up = simd_cross(right, f)
            target = target - right * (dx * perPoint) + up * (dy * perPoint)
        } else {
            distance *= pow(zoomPerNotch, dy / 40)
        }
        view.needsDisplay = true
    }

    func zoom(notches: Float) {
        distance *= pow(zoomPerNotch, -notches)
        view.needsDisplay = true
    }

    private func frameAll() {
        guard let a = geometry.boundsMin else { return }
        let b = geometry.boundsMax
        target = (a + b) * 0.5
        let radius = max(0.5 * simd_length(b - a), 1e-3)
        distance = radius / sin(fovy * 0.5) * 1.05
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
        view.needsDisplay = true
    }

    func draw(in view: MTKView) {
        renderer.pointSize = 4 * Float(view.window?.backingScaleFactor ?? 1)
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
            renderer.setGeometry(geometry)
            if !framed && geometry.boundsMin != nil {
                frameAll()
                framed = true
            }
            updateStatus()
            view.needsDisplay = true
        }
        // A new cook only after the previous one finished: with slow cooks the frames in between are skipped
        if !scene.isCooking && scene.cookedFrame != requestedFrame { scene.startCook(frame: requestedFrame) }
        scheduleTick()
    }

    private func updateTitle() {
        var t = title + (scene.isDemoEdition ? " (demo edition)" : "") + " — " + renderer.description
        if !file.isEmpty {
            t += " — " + URL(fileURLWithPath: file).lastPathComponent
        } else if sceneIndex >= 0 {
            let type = DemoCatalog.name(sceneIndex)
            let description = DemoCatalog.description(sceneIndex)
            t += " — \(type): \(description)"
        }
        window.title = t
    }

    private func updateStatus() {
        if !scene.isLoaded {
            status.stringValue = "Choose a node type or open / drop a graph .gng.  LMB rotate, wheel / RMB zoom, "
                + "MMB / Shift+LMB pan, F frame"
        } else {
            status.stringValue = String(format: "Frame %d   cook %.1f ms   read %.1f ms   %d tris   %d lines   %d points",
                                        scene.cookedFrame, scene.lastCookMs, scene.lastFetchMs,
                                        geometry.triangles.count / 3, geometry.lines.count / 2, geometry.points.count)
        }
    }
}
