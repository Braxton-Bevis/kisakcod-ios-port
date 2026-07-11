import UIKit

// Deliberately scene-less (no UIApplicationSceneManifest in Info.plist):
// a game engine wants one window it owns, same shape as the Win32 WinMain flow.
@main
final class AppDelegate: UIResponder, UIApplicationDelegate {
    var window: UIWindow?

    func application(_ application: UIApplication,
                     didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey: Any]?) -> Bool {
        let window = UIWindow(frame: UIScreen.main.bounds)
        window.rootViewController = MetalViewController()
        window.makeKeyAndVisible()
        self.window = window
        return true
    }
}
