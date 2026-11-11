// A thin helper over the GeoNodus C API for building scenes in Swift — the Swift counterpart of the SceneBuilder of the
// C++ demo scenes (../../../scenes/scene_builder.h), with the same calls. Every call is a plain gn* call: nodes are
// created with gnCreateNode, connected with gnConnectNodeInput, parameters are found by name in gnGetParameters and
// written with gnSetParm*Values / gnSetParmStringValue / gnSetParmExpression, the Display flag is gnSetNodeDisplay.
// Errors (an unknown parameter, a failed call) do not stop the build: they are collected in SceneBuilder.errors.
//
//   let geo = b.object("shapes")
//   let ball = b.sop(geo, "sphere", "ball").set("type", "polymesh").set("rows", 24).expr("rad", 0, "1 + 0.2 * sin($T)")
//   b.sop(geo, "mountain").input(0, ball).set("height", 0.5).display()
//
// Scenes written with it are listed in SwiftScenes.swift (the scene list of the apps shows them after the C++ ones).
import Foundation

// A user (spare) parameter of any kind for SceneNode.addSpareParm (GN_SpareParmTemplate):
//   node.addSpareParm(SpareParm(GN_PARMTYPE_INT, "count", "Count").range(1, 10).joinNext())
struct SpareParm {
    var type: GN_ParmType
    var name: String
    var label: String?
    var size: Int32 = 1
    var defaults: [Double] = []
    var stringDefaults: [String] = []
    var range: (min: Double, max: Double, minStrict: Bool, maxStrict: Bool)? = nil
    var menuItems: [(token: String, label: String)] = []
    var folderType: GN_PrmScriptType? = nil
    var parentFolder: String? = nil
    var joinNextFlag = false, labelNoneFlag = false
    var disableCondition: String? = nil, hideCondition: String? = nil

    init(_ type: GN_ParmType, _ name: String, _ label: String? = nil) {
        self.type = type
        self.name = name
        self.label = label
    }

    // Components of INT, FLOAT, STRING (1..4)
    func size(_ n: Int32) -> SpareParm { var p = self; p.size = n; return p }
    // INT, FLOAT, TOGGLE (≠ 0 — on), menu — the item index
    func defaults(_ v: [Double]) -> SpareParm { var p = self; p.defaults = v; return p }
    func stringDefaults(_ v: [String]) -> SpareParm { var p = self; p.stringDefaults = v; return p }
    // Slider range; a strict bound also clamps the value
    func range(_ min: Double, _ max: Double, minStrict: Bool = false, maxStrict: Bool = false) -> SpareParm {
        var p = self
        p.range = (min, max, minStrict, maxStrict)
        return p
    }
    // Menu items of an INT: token and label; the value is the item index
    func menu(_ items: [(String, String)]) -> SpareParm { var p = self; p.menuItems = items.map { ($0.0, $0.1) }; return p }
    // FOLDER: GN_PRM_SCRIPT_TYPE_GROUP (a tab, the default), GROUPSIMPLE, GROUPCOLLAPSIBLE
    func folder(_ t: GN_PrmScriptType) -> SpareParm { var p = self; p.folderType = t; return p }
    // The folder (by name) the parameter goes into; without it — the root
    func parent(_ folder: String) -> SpareParm { var p = self; p.parentFolder = folder; return p }
    func joinNext() -> SpareParm { var p = self; p.joinNextFlag = true; return p }
    func labelNone() -> SpareParm { var p = self; p.labelNoneFlag = true; return p }
    func disableWhen(_ c: String) -> SpareParm { var p = self; p.disableCondition = c; return p }
    func hideWhen(_ c: String) -> SpareParm { var p = self; p.hideCondition = c; return p }
}

// A node of the scene being built. Setters return the node, so a node is configured in one expression
struct SceneNode {
    unowned let builder: SceneBuilder
    let id: GN_NodeId

    var valid: Bool { id >= 0 }
    var name: String { builder.nodeName(id) }

    // Numeric values of the first components of a parameter: int, toggle (0 / 1), menu (item index), float, multiparm
    // (number of instances)
    @discardableResult func set(_ parm: String, _ values: Double...) -> SceneNode {
        if valid { builder.setValues(id, parm, 0, values) }
        return self
    }

    @discardableResult func set(_ parm: String, _ on: Bool) -> SceneNode { set(parm, on ? 1 : 0) }

    // One component of a parameter
    @discardableResult func setComponent(_ parm: String, _ component: Int, _ value: Double) -> SceneNode {
        if valid { builder.setValues(id, parm, component, [value]) }
        return self
    }

    // A string parameter or, for a menu, the item token ("polymesh", "subtract", ...)
    @discardableResult func set(_ parm: String, _ value: String) -> SceneNode { setString(parm, 0, value) }

    // The same for one component of a string parameter
    @discardableResult func setString(_ parm: String, _ component: Int, _ value: String) -> SceneNode {
        if valid { builder.setString(id, parm, component, value) }
        return self
    }

    // An expression on a parameter component (muParser: $T — time in seconds, $F — frame, sin / cos in radians,
    // ch("path") — the value of another parameter, a path relative to this node or absolute)
    @discardableResult func expr(_ parm: String, _ component: Int, _ expression: String) -> SceneNode {
        if valid { builder.setExpression(id, parm, component, expression) }
        return self
    }

    @discardableResult func expr(_ parm: String, _ expression: String) -> SceneNode { expr(parm, 0, expression) }

    // Expressions on several components at once; nil skips a component
    @discardableResult func expr(_ parm: String, _ expressions: [String?]) -> SceneNode {
        for (component, e) in expressions.enumerated() {
            if let e = e { expr(parm, component, e) }
        }
        return self
    }

    // Connects input `input` of this node to output `output` of `from`
    @discardableResult func input(_ input: Int, _ from: SceneNode, _ output: Int = 0) -> SceneNode {
        if valid && from.valid {
            builder.check(gnConnectNodeInput(builder.session, id, Int32(input), from.id, Int32(output)), id,
                          "gnConnectNodeInput(\(input), \(from.name))")
        }
        return self
    }

    // The Display flag: for a SOP — the node shown as the network's geometry, for an object — visibility
    @discardableResult func display(_ on: Bool = true) -> SceneNode {
        if valid { builder.check(gnSetNodeDisplay(builder.session, id, on ? 1 : 0), id, "gnSetNodeDisplay") }
        return self
    }

    // A user (spare) float parameter: a control that other nodes read with ch()
    @discardableResult func addFloatParm(_ name: String, _ label: String, _ value: Double, _ min: Double,
                                         _ max: Double) -> SceneNode {
        addSpareParm(SpareParm(GN_PARMTYPE_FLOAT, name, label).defaults([value]).range(min, max))
    }

    // A user parameter of any kind (gnAddSpareParm), see SpareParm
    @discardableResult func addSpareParm(_ parm: SpareParm) -> SceneNode {
        if valid { builder.addSpareParm(id, parm) }
        return self
    }

    // --- Geometry given by the application (gnSetPartInfo ... gnCommitGeo): one part of an input node or any SOP;
    // describe it, then commit() locks the node with it

    // Polygons: the vertex count of each face and the point of each vertex, face after face
    @discardableResult func mesh(points: Int32, faceCounts: [Int32], vertices: [Int32]) -> SceneNode {
        guard valid else { return self }
        var part = GN_PartInfo()
        part.type = GN_PARTTYPE_MESH
        part.pointCount = points
        part.vertexCount = Int32(vertices.count)
        part.faceCount = Int32(faceCounts.count)
        builder.beginPart(id, &part)
        let s = builder.session
        if !faceCounts.isEmpty {
            builder.check(gnSetFaceCounts(s, id, 0, faceCounts, 0, part.faceCount), id, "gnSetFaceCounts")
        }
        if !vertices.isEmpty {
            builder.check(gnSetVertexList(s, id, 0, vertices, 0, part.vertexCount), id, "gnSetVertexList")
        }
        return self
    }

    // Curves of one type (GN_CURVETYPE_LINEAR / NURBS / BEZIER): orders — one for all or one per curve (not used by
    // LINEAR), knots — of open NURBS (order + CV count per curve), empty — uniform
    @discardableResult func curves(_ type: GN_CurveType, points: Int32, curveCounts: [Int32], vertices: [Int32],
                                   orders: [Int32], closed: Bool, knots: [Float] = []) -> SceneNode {
        guard valid else { return self }
        var part = GN_PartInfo()
        part.type = GN_PARTTYPE_CURVE
        part.pointCount = points
        part.vertexCount = Int32(vertices.count)
        part.faceCount = Int32(curveCounts.count)
        builder.beginPart(id, &part)
        var info = GN_CurveInfo()
        info.curveType = type
        info.curveCount = part.faceCount
        info.vertexCount = part.vertexCount
        info.knotCount = Int32(knots.count)
        info.order = orders.count == 1 ? orders[0] : 0
        info.hasKnots = knots.isEmpty ? 0 : 1
        info.isClosed = closed ? 1 : 0
        let s = builder.session
        builder.check(gnSetCurveInfo(s, id, 0, &info), id, "gnSetCurveInfo")
        builder.check(gnSetCurveCounts(s, id, 0, curveCounts, 0, part.faceCount), id, "gnSetCurveCounts")
        if orders.count > 1 {
            builder.check(gnSetCurveOrders(s, id, 0, orders, 0, Int32(orders.count)), id, "gnSetCurveOrders")
        }
        if !knots.isEmpty { builder.check(gnSetCurveKnots(s, id, 0, knots, 0, info.knotCount), id, "gnSetCurveKnots") }
        if !vertices.isEmpty {
            builder.check(gnSetVertexList(s, id, 0, vertices, 0, part.vertexCount), id, "gnSetVertexList")
        }
        return self
    }

    // Attributes: `size` values per element of the owner, element after element
    @discardableResult func attrib(_ owner: GN_AttributeOwner, _ name: String, size: Int32,
                                   _ typeInfo: GN_AttributeTypeInfo, _ values: [Float]) -> SceneNode {
        if valid, var info = builder.addAttribute(id, owner, name, size, typeInfo, GN_STORAGETYPE_FLOAT, values.count) {
            builder.check(gnSetAttributeFloatData(builder.session, id, 0, name, &info, values, 0, info.count), id,
                          "gnSetAttributeFloatData(\(name))")
        }
        return self
    }

    @discardableResult func attribFloat64(_ owner: GN_AttributeOwner, _ name: String, size: Int32,
                                          _ typeInfo: GN_AttributeTypeInfo, _ values: [Double]) -> SceneNode {
        if valid, var info = builder.addAttribute(id, owner, name, size, typeInfo, GN_STORAGETYPE_FLOAT64, values.count) {
            builder.check(gnSetAttributeFloat64Data(builder.session, id, 0, name, &info, values, 0, info.count), id,
                          "gnSetAttributeFloat64Data(\(name))")
        }
        return self
    }

    @discardableResult func attribInt(_ owner: GN_AttributeOwner, _ name: String, size: Int32,
                                      _ typeInfo: GN_AttributeTypeInfo, _ values: [Int32]) -> SceneNode {
        if valid, var info = builder.addAttribute(id, owner, name, size, typeInfo, GN_STORAGETYPE_INT, values.count) {
            builder.check(gnSetAttributeIntData(builder.session, id, 0, name, &info, values, 0, info.count), id,
                          "gnSetAttributeIntData(\(name))")
        }
        return self
    }

    @discardableResult func attribInt64(_ owner: GN_AttributeOwner, _ name: String, size: Int32,
                                        _ typeInfo: GN_AttributeTypeInfo, _ values: [Int64]) -> SceneNode {
        if valid, var info = builder.addAttribute(id, owner, name, size, typeInfo, GN_STORAGETYPE_INT64, values.count) {
            builder.check(gnSetAttributeInt64Data(builder.session, id, 0, name, &info, values, 0, info.count), id,
                          "gnSetAttributeInt64Data(\(name))")
        }
        return self
    }

    @discardableResult func attribString(_ owner: GN_AttributeOwner, _ name: String, size: Int32,
                                         _ values: [String]) -> SceneNode {
        guard valid, var info = builder.addAttribute(id, owner, name, size, HAPI_ATTRIBUTE_TYPE_INVALID,
                                                     GN_STORAGETYPE_STRING, values.count) else { return self }
        var strings: [UnsafePointer<CChar>?] = values.map { UnsafePointer(strdup($0)) }
        builder.check(gnSetAttributeStringData(builder.session, id, 0, name, &info, &strings, 0, info.count), id,
                      "gnSetAttributeStringData(\(name))")
        strings.forEach { free(UnsafeMutablePointer(mutating: $0)) }
        return self
    }

    // A point or primitive group (the numbers of its elements) or an edge group (pairs of point numbers)
    @discardableResult func group(_ type: GN_GroupType, _ name: String, _ elements: [Int32]) -> SceneNode {
        guard valid, builder.check(gnAddGroup(builder.session, id, 0, type, name), id, "gnAddGroup(\(name))") else {
            return self
        }
        if type == GN_GROUPTYPE_EDGE {
            builder.check(gnSetGroupMembership(builder.session, id, 0, type, name, elements, 0, Int32(elements.count / 2)),
                          id, "gnSetGroupMembership(\(name))")
            return self
        }
        let count = builder.partNode != id ? 0 : type == GN_GROUPTYPE_POINT ? builder.partPoints : builder.partPrims
        var membership = [Int32](repeating: 0, count: count)
        for e in elements where e >= 0 && Int(e) < count { membership[Int(e)] = 1 }
        builder.check(gnSetGroupMembership(builder.session, id, 0, type, name, membership, 0, Int32(count)), id,
                      "gnSetGroupMembership(\(name))")
        return self
    }

    // Builds the described geometry and locks the node with it (gnCommitGeo)
    @discardableResult func commit() -> SceneNode {
        if valid { builder.check(gnCommitGeo(builder.session, id), id, "gnCommitGeo") }
        return self
    }
}

final class SceneBuilder {
    let session: GN_SessionId
    private(set) var errors: [String] = []

    init(_ session: GN_SessionId) { self.session = session }

    // Removes every node under /obj (the scene is built from an empty session)
    func clear() {
        for child in children(0, GN_NODEFLAGS_ANY.rawValue) { gnDeleteNode(session, child) }
    }

    // Timeline in seconds (as the "Global Animation Options" of Houdini); the scene starts at `start`
    func timeline(start: Double, end: Double, fps: Double = 24) {
        var tl = GN_TimelineOptions()
        tl.fps = fps
        tl.startTime = start
        tl.endTime = end
        if gnSetTimelineOptions(session, &tl) != GN_RESULT_SUCCESS { error(-1, "gnSetTimelineOptions failed") }
        gnSetTime(session, start)
    }

    // The current time in seconds ($T)
    func time(_ seconds: Double) {
        if gnSetTime(session, seconds) != GN_RESULT_SUCCESS { error(-1, "gnSetTime failed") }
    }

    // An object on /obj: "geo" (a SOP network) or "null"
    func object(_ name: String, _ op: String = "geo") -> SceneNode { create(0, op, name) }

    // A SOP inside a geo object or a SOP subnet
    func sop(_ parent: SceneNode, _ op: String, _ name: String? = nil) -> SceneNode {
        parent.valid ? create(parent.id, op, name) : SceneNode(builder: self, id: -1)
    }

    // A node inside a SOP subnet: its input "1".."4" (the subnet's input `index` from 1)
    func subnetInput(_ subnet: SceneNode, _ index: Int) -> SceneNode {
        if subnet.valid {
            for child in children(subnet.id, GN_NODEFLAGS_SOP_SUBNET_INPUT.rawValue) where nodeName(child) == String(index) {
                return SceneNode(builder: self, id: child)
            }
            error(subnet.id, "no subnet input \(index)")
        }
        return SceneNode(builder: self, id: -1)
    }

    // An input node (gnCreateInputNode) in a geo object or a SOP subnet, named `name` (the library names it
    // "input_<name>"; it is renamed); its geometry is set with SceneNode.mesh / curves ... commit
    func inputNode(_ parent: SceneNode, _ name: String) -> SceneNode {
        var id: GN_NodeId = -1
        guard parent.valid, check(gnCreateInputNode(session, parent.id, &id, name), parent.id, "gnCreateInputNode") else {
            return SceneNode(builder: self, id: -1)
        }
        check(gnRenameNode(session, id, name), id, "gnRenameNode")
        return SceneNode(builder: self, id: id)
    }

    // Checks the result of a gn* call: false — the call failed, the error (with the GN_STATUS_CALL_RESULT text) goes
    // to errors
    @discardableResult func check(_ result: GN_Result, _ node: GN_NodeId, _ call: @autoclosure () -> String) -> Bool {
        if result == GN_RESULT_SUCCESS { return true }
        var text = "\(call()) — error \(result.rawValue)"
        var length: Int32 = 0
        if gnGetStatusStringBufLength(session, GN_STATUS_CALL_RESULT, GN_STATUSVERBOSITY_ERRORS, &length)
            == GN_RESULT_SUCCESS && length > 1
        {
            var buffer = [CChar](repeating: 0, count: Int(length) + 1)
            gnGetStatusString(session, GN_STATUS_CALL_RESULT, &buffer, length)
            text += ": " + String(cString: buffer)
        }
        error(node, text)
        return false
    }

    // --- For SceneNode ---------------------------------------------------------------------------------------------

    fileprivate var partNode: GN_NodeId = -1
    fileprivate var partPoints = 0, partPrims = 0

    fileprivate func string(_ handle: GN_StringHandle) -> String {
        var length: Int32 = 0
        guard gnGetStringBufLength(session, handle, &length) == GN_RESULT_SUCCESS, length > 0 else { return "" }
        var buffer = [CChar](repeating: 0, count: Int(length) + 1)
        gnGetString(session, handle, &buffer, length)
        return String(cString: buffer)
    }

    fileprivate func nodeName(_ node: GN_NodeId) -> String {
        var info = GN_NodeInfo()
        guard node >= 0, gnGetNodeInfo(session, node, &info) == GN_RESULT_SUCCESS else { return "?" }
        return string(info.nameSH)
    }

    private func error(_ node: GN_NodeId, _ text: String) {
        errors.append(node >= 0 ? "\(nodeName(node)): \(text)" : text)
    }

    private func create(_ parent: GN_NodeId, _ op: String, _ name: String?) -> SceneNode {
        var id: GN_NodeId = -1
        let r = gnCreateNode(session, parent, op, name, 0, &id)
        if r != GN_RESULT_SUCCESS {
            check(r, parent, "gnCreateNode(\(op), \(name ?? ""))")
            return SceneNode(builder: self, id: -1)
        }
        return SceneNode(builder: self, id: id)
    }

    private func children(_ parent: GN_NodeId, _ flags: Int32) -> [GN_NodeId] {
        var count: Int32 = 0
        guard gnComposeChildNodeList(session, parent, GN_NodeTypeBits(GN_NODETYPE_ANY.rawValue),
                                     GN_NodeFlagsBits(flags), 0, &count) == GN_RESULT_SUCCESS, count > 0
        else { return [] }
        var ids = [GN_NodeId](repeating: 0, count: Int(count))
        gnGetComposedChildNodeList(session, parent, &ids, count)
        return ids
    }

    // The list is read anew every time: inserting multiparm instances shifts the ids of the parameters after them
    private func parameters(_ node: GN_NodeId) -> [GN_ParmInfo] {
        var info = GN_NodeInfo()
        guard gnGetNodeInfo(session, node, &info) == GN_RESULT_SUCCESS, info.parmCount > 0 else { return [] }
        var parms = [GN_ParmInfo](repeating: GN_ParmInfo(), count: Int(info.parmCount))
        guard gnGetParameters(session, node, &parms, 0, info.parmCount) == GN_RESULT_SUCCESS else { return [] }
        return parms
    }

    private func findParm(_ node: GN_NodeId, _ name: String) -> GN_ParmInfo? {
        for p in parameters(node) {
            if p.type == GN_PARMTYPE_FOLDER || p.type == GN_PARMTYPE_FOLDERLIST || p.type == GN_PARMTYPE_LABEL
                || p.type == GN_PARMTYPE_SEPARATOR { continue }
            if string(p.nameSH) == name { return p }
        }
        error(node, "no parameter '\(name)'")
        return nil
    }

    private static func storesInt(_ t: GN_ParmType) -> Bool {
        t == GN_PARMTYPE_INT || t == GN_PARMTYPE_TOGGLE || t == GN_PARMTYPE_BUTTON || t == GN_PARMTYPE_MULTIPARMLIST
            || t == GN_PARMTYPE_FOLDERLIST_RADIO
    }

    private static func storesString(_ t: GN_ParmType) -> Bool {
        t == GN_PARMTYPE_STRING || t == GN_PARMTYPE_PATH_FILE || t == GN_PARMTYPE_PATH_FILE_GEO
            || t == GN_PARMTYPE_PATH_FILE_IMAGE || t == GN_PARMTYPE_PATH_FILE_DIR || t == GN_PARMTYPE_NODE
    }

    fileprivate func setValues(_ node: GN_NodeId, _ parm: String, _ first: Int, _ values: [Double]) {
        guard let p = findParm(node, parm) else { return }
        let size = p.type == GN_PARMTYPE_FOLDERLIST_RADIO || p.type == GN_PARMTYPE_MULTIPARMLIST ? 1 : Int(p.size)
        if first + values.count > size {
            error(node, "parameter '\(parm)' has \(size) components")
            return
        }
        if SceneBuilder.storesInt(p.type) {
            let v = values.map { Int32($0.rounded()) }
            check(gnSetParmIntValues(session, node, v, p.intValuesIndex + Int32(first), Int32(v.count)), node,
                  "setting '\(parm)'")
        } else if p.floatValuesIndex >= 0 {
            let v = values.map { Float($0) }
            check(gnSetParmFloatValues(session, node, v, p.floatValuesIndex + Int32(first), Int32(v.count)), node,
                  "setting '\(parm)'")
        }
    }

    fileprivate func setString(_ node: GN_NodeId, _ parm: String, _ component: Int, _ value: String) {
        guard let p = findParm(node, parm) else { return }
        if SceneBuilder.storesString(p.type) {
            check(gnSetParmStringValue(session, node, value, p.id, Int32(component)), node, "setting '\(parm)'")
            return
        }
        guard p.choiceCount > 0 else {
            error(node, "parameter '\(parm)' is neither a string nor a menu")
            return
        }
        // A menu: the value is the item index; valueSH of an item is its token
        var choices = [GN_ParmChoiceInfo](repeating: GN_ParmChoiceInfo(), count: Int(p.choiceCount))
        guard gnGetParmChoiceLists(session, node, &choices, p.choiceIndex, p.choiceCount) == GN_RESULT_SUCCESS,
            let index = choices.firstIndex(where: { string($0.valueSH) == value })
        else {
            error(node, "menu '\(parm)' has no item '\(value)'")
            return
        }
        var v = Int32(index)
        check(gnSetParmIntValues(session, node, &v, p.intValuesIndex + Int32(component), 1), node, "setting '\(parm)'")
    }

    fileprivate func setExpression(_ node: GN_NodeId, _ parm: String, _ component: Int, _ expression: String) {
        guard let p = findParm(node, parm) else { return }
        check(gnSetParmExpression(session, node, expression, p.id, Int32(component)), node,
              "expression on '\(parm)[\(component)]' = \(expression)")
    }

    fileprivate func addSpareParm(_ node: GN_NodeId, _ parm: SpareParm) {
        var owned: [UnsafeMutablePointer<CChar>] = []
        func c(_ s: String?) -> UnsafePointer<CChar>? {
            guard let s = s, let p = strdup(s) else { return nil }
            owned.append(p)
            return UnsafePointer(p)
        }
        defer { owned.forEach { free($0) } }
        var t = GN_SpareParmTemplate()
        t.type = parm.type
        t.name = c(parm.name)
        t.label = c(parm.label)
        t.size = parm.size
        let d = parm.defaults + [Double](repeating: 0, count: 4)
        t.defaults = (d[0], d[1], d[2], d[3])
        let sd = parm.stringDefaults.map { c($0) } + [UnsafePointer<CChar>?](repeating: nil, count: 4)
        t.stringDefaults = (sd[0], sd[1], sd[2], sd[3])
        if let r = parm.range {
            t.hasRange = 1
            t.min = r.min
            t.max = r.max
            t.minIsStrict = r.minStrict ? 1 : 0
            t.maxIsStrict = r.maxStrict ? 1 : 0
        }
        if let f = parm.folderType { t.folderType = f }
        t.joinNext = parm.joinNextFlag ? 1 : 0
        t.labelNone = parm.labelNoneFlag ? 1 : 0
        t.disableWhen = c(parm.disableCondition)
        t.hideWhen = c(parm.hideCondition)
        let tokens = parm.menuItems.map { c($0.token) }, labels = parm.menuItems.map { c($0.label) }

        // The parent folder by name (findParm skips folders)
        var parent: GN_ParmId = -1
        if let folder = parm.parentFolder {
            for p in parameters(node) where p.type == GN_PARMTYPE_FOLDER && string(p.nameSH) == folder { parent = p.id }
            if parent < 0 { error(node, "no folder '\(folder)' for '\(parm.name)'") }
        }
        var id: GN_ParmId = -1
        tokens.withUnsafeBufferPointer { tp in
            labels.withUnsafeBufferPointer { lp in
                if !tokens.isEmpty {
                    t.menuItemCount = Int32(tokens.count)
                    t.menuItems = tp.baseAddress
                    t.menuLabels = lp.baseAddress
                }
                check(gnAddSpareParm(session, node, &t, parent, &id), node, "gnAddSpareParm(\(parm.name))")
            }
        }
    }

    fileprivate func beginPart(_ node: GN_NodeId, _ part: inout GN_PartInfo) {
        partNode = node
        partPoints = Int(part.pointCount)
        partPrims = Int(part.faceCount)
        check(gnSetPartInfo(session, node, 0, &part), node, "gnSetPartInfo")
    }

    fileprivate func addAttribute(_ node: GN_NodeId, _ owner: GN_AttributeOwner, _ name: String, _ size: Int32,
                                  _ typeInfo: GN_AttributeTypeInfo, _ storage: GN_StorageType,
                                  _ valueCount: Int) -> GN_AttributeInfo? {
        var info = GN_AttributeInfo()
        info.exists = 1
        info.owner = owner
        info.storage = storage
        info.originalOwner = GN_ATTROWNER_INVALID
        info.tupleSize = size
        info.count = size > 0 ? Int32(valueCount) / size : 0
        info.typeInfo = typeInfo
        return check(gnAddAttribute(session, node, 0, name, &info), node, "gnAddAttribute(\(name))") ? info : nil
    }
}
