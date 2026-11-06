// GeoNodus demo for iOS / iPadOS (UIKit + Metal): shows a GeoNodus graph (.json, gnSaveGraph) or a demo scene built in
// code and plays its animation — the same app as the macOS (mac/metal) and Windows demos. The scene, the Metal renderer
// and the matrices are shared with the macOS demo (mac/metal/Sources: GnScene.swift, MetalRenderer.swift,
// DemoMath.swift), the demo scenes are the C++ code of ../scenes through the C bridge mac/metal/bridge.
// Launch arguments (Xcode scheme or xcrun simctl launch): [graph.json] [--scene <node type>] [--play]
import UIKit

@main
final class AppDelegate: UIResponder, UIApplicationDelegate {
    func application(_ application: UIApplication,
                     didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey: Any]?) -> Bool {
        true
    }

    func application(_ application: UIApplication, configurationForConnecting connectingSceneSession: UISceneSession,
                     options: UIScene.ConnectionOptions) -> UISceneConfiguration {
        let config = UISceneConfiguration(name: "Default", sessionRole: connectingSceneSession.role)
        config.delegateClass = SceneDelegate.self
        return config
    }
}

final class SceneDelegate: UIResponder, UIWindowSceneDelegate {
    var window: UIWindow?

    func scene(_ scene: UIScene, willConnectTo session: UISceneSession, options: UIScene.ConnectionOptions) {
        guard let windowScene = scene as? UIWindowScene else { return }
        let controller = DemoViewController()
        let window = UIWindow(windowScene: windowScene)
        window.rootViewController = controller
        window.makeKeyAndVisible()
        self.window = window
        // A .json opened with the app from Files / AirDrop
        if let url = options.urlContexts.first?.url { controller.openExternal(url) }
    }

    func scene(_ scene: UIScene, openURLContexts URLContexts: Set<UIOpenURLContext>) {
        guard let url = URLContexts.first?.url else { return }
        (window?.rootViewController as? DemoViewController)?.openExternal(url)
    }
}
