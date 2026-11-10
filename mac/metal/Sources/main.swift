// GeoNodus demo (Metal): shows a GeoNodus graph (.gng, gnSaveGraph) or a demo scene built in code and plays its
// animation. Command line: "GeoNodus Metal Demo" [graph.gng] [--scene <node type>] [--play]
import AppKit

let app = NSApplication.shared
let delegate = DemoApp()
app.delegate = delegate
app.setActivationPolicy(.regular)
app.run()
