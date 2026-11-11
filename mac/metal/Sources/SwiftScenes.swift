// Scenes built in Swift with SceneBuilder (SceneBuilder.swift) — for example the code of GeoNodus Graph Builder's
// Generate Swift Code. The scene list of the apps (macOS and iOS) shows them after the C++ demo scenes. To add one,
// put its function into a .swift file of this folder and list it below:
//   SwiftDemoScene(name: "gear_mechanism_scene", description: "Gears and a cam", build: buildGearMechanismScene),
// The function gets a cleared session with the timeline 0..10 s at 24 fps (a scene may set its own)

struct SwiftDemoScene {
    let name: String
    let description: String
    let build: (SceneBuilder) -> Void
}

let swiftDemoScenes: [SwiftDemoScene] = [
]

// The scenes of the list: the C++ demo scenes (../../../scenes, through bridge/demo_scenes_c.h), then swiftDemoScenes
enum DemoCatalog {
    static var cppCount: Int { Int(demoSceneCount()) }
    static var count: Int { cppCount + swiftDemoScenes.count }

    static func name(_ index: Int) -> String {
        index < cppCount ? String(cString: demoSceneNodeType(Int32(index))) : swiftDemoScenes[index - cppCount].name
    }

    static func description(_ index: Int) -> String {
        index < cppCount ? String(cString: demoSceneDescription(Int32(index)))
            : swiftDemoScenes[index - cppCount].description
    }

    // The scene of a node type / name (case-insensitive), -1 — none
    static func find(_ name: String) -> Int {
        let index = Int(demoSceneFind(name))
        if index >= 0 { return index }
        if let i = swiftDemoScenes.firstIndex(where: { $0.name.lowercased() == name.lowercased() }) {
            return cppCount + i
        }
        return -1
    }
}
