#ifndef GEONODUS_H
#define GEONODUS_H

#include "geonodus_API.h"

GN_DECL gnCreateSession(GN_SessionId* sessionID);
GN_DECL gnCloseSession(GN_SessionId sessionID);

GN_DECL gnGetTimelineOptions(const GN_SessionId, GN_TimelineOptions* timeline_options);
GN_DECL gnSetTimelineOptions(const GN_SessionId, const GN_TimelineOptions* timeline_options);
GN_DECL gnGetTime(const GN_SessionId, double* time);
GN_DECL gnSetTime(const GN_SessionId, double time);

GN_DECL gnCreateNode(const GN_SessionId, GN_NodeId parent, const char* operat, const char* lbl, GN_Bool withCook,
    GN_NodeId* res);

GN_DECL gnDeleteNode(const GN_SessionId, GN_NodeId node_id);

GN_DECL gnRenameNode(const GN_SessionId, GN_NodeId node_id, const char* new_name);

GN_DECL gnConnectNodeInput(const GN_SessionId, GN_NodeId node_id, int input_ind, GN_NodeId node_to_conn,
    int output_ind);

GN_DECL gnDisconnectNodeInput(const GN_SessionId, GN_NodeId node_id, int input_ind);

// Whether the node is dirty (HDK: the node's isDirty)
GN_DECL gnIsNodeDirty(const GN_SessionId, GN_NodeId node_id, GN_Bool* is_dirty);

GN_DECL_RETURN(GN_CookOptions) gnCookOptions_Create();

// HDK model: a non-blocking call. The cook is queued in the session's cook queue and runs on its job thread. Track the
// result by the node's cook state (see gnGetCookState / gnWaitCook).
GN_DECL gnCookNode(const GN_SessionId, GN_NodeId node_id, const GN_CookOptions* cook_options);

// The node's cook state (like HAPI_Node_GetCookStatus)
GN_DECL gnGetCookState(const GN_SessionId, GN_NodeId node_id, GN_CookState* cook_state);

// Waits until the node's cook finishes (as in HDK: waits for the cooking flag to clear).
// timeout_ms < 0 — wait until done (no limit).
// GN_RESULT_SUCCESS — the cook finished; GN_RESULT_USER_INTERRUPTED — timeout (the cook is still running).
GN_DECL gnWaitCook(const GN_SessionId, GN_NodeId node_id, GN_Int64 timeout_ms);

// Session status (HAPI-style): GN_STATUS_COOK_STATE returns a GN_State.
// A client can wait for the cook to finish with:
//   while (status == GN_STATE_COOKING) { gnGetStatus(...); }
GN_DECL gnGetStatus(const GN_SessionId, GN_StatusType status_type, int* status);

// Session status string. As in HAPI: call *BufLength() first, then *String().
// *BufLength() caches the string, so the length and the contents match between the two calls.
GN_DECL gnGetStatusStringBufLength(const GN_SessionId, GN_StatusType status_type, GN_StatusVerbosity verbosity,
    int* buffer_length);

GN_DECL gnGetStatusString(const GN_SessionId, GN_StatusType status_type, char* string_value, int length);

// Not in HAPI: library memory diagnostics (MemoryTracker over scalable_malloc). allocated_total — bytes allocated
// since the library was loaded (only grows: its increase over a cook shows whether the cook allocated memory), in_use —
// bytes in use now. Either pointer may be nullptr
GN_DECL gnGetMemoryStats(GN_Int64* allocated_total, GN_Int64* in_use);

GN_DECL gnGetStringBufLength(const GN_SessionId, GN_StringHandle string_handle, int* buffer_length);

GN_DECL gnGetString(const GN_SessionId, GN_StringHandle string_handle, char* string_value, int length);

GN_DECL gnGetNodePath(const GN_SessionId, GN_NodeId node_id, GN_NodeId relative_to_node_id, GN_StringHandle* path);

GN_DECL gnGetNodeInfo(const GN_SessionId, GN_NodeId node_id, GN_NodeInfo* node_info);

// HAPI_IsNodeValid: whether the node still exists. As in Houdini, a node id stays the same while the node is alive
// (deleting other nodes does not change it), and the id of a deleted node is never given to a new one.
// unique_node_id — GN_NodeInfo::uniqueNodeId. Always GN_RESULT_SUCCESS (except an invalid session / answer == NULL),
// the answer is in *answer.
GN_DECL gnIsNodeValid(const GN_SessionId, GN_NodeId node_id, int unique_node_id, GN_Bool* answer);

GN_DECL gnComposeChildNodeList(const GN_SessionId, GN_NodeId parent, GN_NodeTypeBits type_filter,
    GN_NodeFlagsBits flags_filter, GN_Bool recursive, int* count);

// The node's Display flag (HAPI_SetNodeDisplay). For a SOP it selects which node of the network is output (to the
// viewport and as the object's geometry): the flag is exclusive among the SOPs of a network, turning it on for one node
// turns it off for the others; it cannot be turned off on a SOP (the call changes nothing) — only turned on for another
// node. The first SOP of a network gets Display and Render itself; when the node with the flag is deleted, the flag
// moves to its input (or to the last SOP of the network). For an object (a network on /obj) it is visibility, on at
// creation; objects have no Render flag. To read the flags use the GN_NODEFLAGS_DISPLAY / GN_NODEFLAGS_RENDER filter of
// gnComposeChildNodeList, GN_GeoInfo::isDisplayGeo, gnGetDisplayGeoInfo.
GN_DECL gnSetNodeDisplay(const GN_SessionId, GN_NodeId node_id, int onOff);

GN_DECL gnGetComposedChildNodeList(const GN_SessionId, GN_NodeId parent, GN_NodeId* child_array, int count);

GN_DECL gnGetGeoInfo(const GN_SessionId, GN_NodeId node_id, GN_GeoInfo* geo_info);

// HAPI_GetDisplayGeoInfo: the geometry of the object's (network's) Display SOP. For a SOP with the Display flag — its own;
// for a SOP without it and a network without SOPs — an error (GN_RESULT_FAILURE). Read the geometry by
// geo_info->nodeId.
GN_DECL gnGetDisplayGeoInfo(const GN_SessionId, GN_NodeId object_node_id, GN_GeoInfo* geo_info);

GN_DECL gnGetPartInfo(const GN_SessionId, GN_NodeId node_id, GN_PartId part_id, GN_PartInfo* part_info);

// Local bbox of the Detail over all point positions "P". As in HDK/GU_Detail, it is recomputed on every request,
// without a cache.
GN_DECL gnGetDetailBoundingBox(const GN_SessionId, GN_NodeId node_id, GN_BoundingBox* bbox);

// HAPI_GetObjectTransform: the transform of an object (geo, null on /obj). As in HAPI, SOP geometry is read in the space
// of its object, and this matrix takes it to world space. relative_to_node_id: -1 or /obj (0) — world (this is how HAPI
// behaves in Houdini 21, although its documentation calls it the local transform), another object — relative to it
// (world(node) * world(relative)^-1; relative to the parent on input 0 — local, the object itself — identity). Not an
// object — GN_RESULT_INVALID_ARGUMENT. The decomposition is exact, shear included
GN_DECL gnGetObjectTransform(const GN_SessionId, GN_NodeId node_id, GN_NodeId relative_to_node_id,
    GN_RSTOrder rst_order, GN_Transform* transform);

// HAPI_ConvertTransformQuatToMatrix: a 4x4 matrix of 16 floats in rows, row vectors (p' = p * M, translation —
// matrix[12..14]; the same layout is an OpenGL column-major matrix). Unlike HAPI, shear is applied (HAPI in
// Houdini 21 uses it as a pivot)
GN_DECL gnConvertTransformQuatToMatrix(const GN_SessionId, const GN_Transform* transform, float* matrix);

// HAPI_ConvertMatrixToQuat: decomposes a matrix (layout as in gnConvertTransformQuatToMatrix) in the given order
GN_DECL gnConvertMatrixToQuat(const GN_SessionId, const float* matrix, GN_RSTOrder rst_order,
    GN_Transform* transform_out);

GN_DECL gnGetAttributeInfo(const GN_SessionId, GN_NodeId node_id, GN_PartId part_id, const char* name,
    GN_AttributeOwner owner, GN_AttributeInfo* attr_info);

// Attribute data of a part (HAPI_GetAttribute*Data). attr_info is an input, as in HAPI: the owner is taken from it
// (usually the struct from gnGetAttributeInfo); stride — the step between tuples in data_array, -1 — tupleSize.
// Numeric attributes are read by any of the numeric functions with type conversion; string attributes — only by
// gnGetAttributeStringData (handles of the session's StringTable, valid while the node's geometry is alive).
GN_DECL gnGetAttributeFloatData(const GN_SessionId, GN_NodeId node_id, GN_PartId part_id, const char* name,
    GN_AttributeInfo* attr_info, int stride, float* data_array, int start, int length);

GN_DECL gnGetAttributeFloat64Data(const GN_SessionId, GN_NodeId node_id, GN_PartId part_id, const char* name,
    GN_AttributeInfo* attr_info, int stride, double* data_array, int start, int length);

GN_DECL gnGetAttributeIntData(const GN_SessionId, GN_NodeId node_id, GN_PartId part_id, const char* name,
    GN_AttributeInfo* attr_info, int stride, int* data_array, int start, int length);

GN_DECL gnGetAttributeInt64Data(const GN_SessionId, GN_NodeId node_id, GN_PartId part_id, const char* name,
    GN_AttributeInfo* attr_info, int stride, GN_Int64* data_array, int start, int length);

// HAPI_GetAttributeStringData: tupleSize handles per element, no stride
GN_DECL gnGetAttributeStringData(const GN_SessionId, GN_NodeId node_id, GN_PartId part_id, const char* name,
    GN_AttributeInfo* attr_info, GN_StringHandle* data_array, int start, int length);

GN_DECL gnGetVertexList(const GN_SessionId, GN_NodeId node, GN_PartId part, int* vertex_array, int start, int length);

GN_DECL gnGetFaceCounts(const GN_SessionId, GN_NodeId node, GN_PartId part, int* counts_array, int start, int length);

GN_DECL gnGetCurveCounts(const GN_SessionId, GN_NodeId node, GN_PartId part, int* counts_array, int start, int length);

GN_DECL gnGetCurveInfo(const GN_SessionId, GN_NodeId node, GN_PartId part, GN_CurveInfo* info);

GN_DECL gnGetCurveOrders(const GN_SessionId, GN_NodeId node, GN_PartId part, int* orders_array, int start, int length);

// HAPI_GetCurveKnots: the knots of the part's curves one after another, GN_CurveInfo::knotCount of them (order + the
// number of CVs per curve); start and length are knot indices. NURBS and Bezier curves are unrolled B-splines: closed
// ones repeat their first CVs at the end, the curve is evaluated on [knots[order − 1], knots[number of CVs]]
GN_DECL gnGetCurveKnots(const GN_SessionId, GN_NodeId node, GN_PartId part, float* knots_array, int start, int length);

// Node parameters (HDK: ParmList/NodeParms, declared by the node through a Template in its constructor).
// start — for gnGetParameters the parameter index; for gnGetParm*Values the index in the HAPI-style flat array of
// values of that type (GN_ParmInfo::*ValuesIndex).

GN_DECL gnGetParameters(const GN_SessionId, GN_NodeId node, GN_ParmInfo* parm_infos_array, int start, int length);

GN_DECL gnGetParmIntValues(const GN_SessionId, GN_NodeId node, int* values_array, int start, int length);

GN_DECL gnSetParmIntValues(const GN_SessionId, GN_NodeId node, const int* values_array, int start, int length);

GN_DECL gnGetParmFloatValues(const GN_SessionId, GN_NodeId node, float* values_array, int start, int length);

GN_DECL gnSetParmFloatValues(const GN_SessionId, GN_NodeId node, const float* values_array, int start, int length);

GN_DECL gnParmHasExpression(const GN_SessionId, GN_NodeId node, const char* parm_name, int ind, GN_Bool* has_expr);

GN_DECL gnGetParmExpression(const GN_SessionId, GN_NodeId node, const char* parm_name, int ind, GN_StringHandle* value);

GN_DECL gnRemoveParmExpression(const GN_SessionId, GN_NodeId node, GN_ParmId parm_id, int index);

GN_DECL gnSetParmExpression(const GN_SessionId, GN_NodeId node, const char* value, GN_ParmId parm_id, int index);

GN_DECL gnGetParmChoiceLists(const GN_SessionId, GN_NodeId node, GN_ParmChoiceInfo* parm_choices_array, int start,
    int length);

GN_DECL gnGetParmStringValues(const GN_SessionId, GN_NodeId node, GN_Bool evaluate, GN_StringHandle* vals_array,
    int start, int length);

GN_DECL gnSetParmStringValue(const GN_SessionId, GN_NodeId node, const char* value, GN_ParmId parm_id, int index);

// Multiparm instances (HAPI_InsertMultiparmInstance / HAPI_RemoveMultiparmInstance): parm_id — a
// GN_PARMTYPE_MULTIPARMLIST parameter, instance_position — the instance number starting at
// GN_ParmInfo::instanceStartOffset (usually 1, 0 for the Add SOP); inserting at instanceStartOffset + count appends.
// The number of instances can also be set by writing the multiparm's int value (gnSetParmIntValues).
// Instance parameters after the insert/remove position are renumbered, the ids of the parameters after them shift
GN_DECL gnInsertMultiparmInstance(const GN_SessionId, GN_NodeId node, GN_ParmId parm_id, int instance_position);
GN_DECL gnRemoveMultiparmInstance(const GN_SessionId, GN_NodeId node, GN_ParmId parm_id, int instance_position);

// Not in HAPI: user (spare) parameters of a node (HDK: OP_Node::addSpareParms; hou.Node.addSpareParmTuple,
// removeSpareParmTuple, removeSpareParms). Every node has them except the /obj root and SOP subnet input nodes; they do
// not affect the node's cook, expressions read their values: ch("/obj/geo1/null1/amp") in other nodes works as with
// node parameters. GN_ParmInfo::spare marks such a parameter. Errors — GN_RESULT_INVALID_ARGUMENT, the text is in the
// GN_STATUS_CALL_RESULT status string.
//
// gnAddSpareParm: a parameter from a template — the last child of parent_id (-1 — the root, otherwise a
// GN_PARMTYPE_FOLDER, except the folders of multiparm instances; the node's own folders are allowed too). A tab folder
// joins the tab set if that set comes last (for an object — next to Transform), otherwise it gets its own set.
// *parm_id — the id of the new parameter (for a folder — the folder's id, its set is parentId). The ids of the
// parameters after the insert position shift; values, expressions and references to them stay. The node does not get
// dirty. ch() references to a not yet existing parameter with this path get resolved.
GN_DECL gnAddSpareParm(const GN_SessionId, GN_NodeId node, const GN_SpareParmTemplate* parm_template,
    GN_ParmId parent_id, GN_ParmId* parm_id);
// Removes a user parameter (a folder — with its contents; a set — with all its folders; a set left without folders is
// removed too). Node parameters cannot be removed (GN_RESULT_INVALID_ARGUMENT). The removed parameter's expressions are
// dropped; ch() to it in other parameters becomes unresolved — gives 0, and those parameters (and their nodes) get
// dirty; if a parameter with the same path is created later, the references resolve again. The ids of the parameters
// after the removed one shift
GN_DECL gnRemoveSpareParm(const GN_SessionId, GN_NodeId node, GN_ParmId parm_id);
// All user parameters of the node (hou.Node.removeSpareParms)
GN_DECL gnRemoveSpareParms(const GN_SessionId, GN_NodeId node);
// A new name for a user parameter (as editing the name in Houdini's Edit Parameter Interface): the position, values and
// expressions stay with it, and ch() references to it in the expressions of any node (the node itself included) are
// rewritten to the new name — ch("../ctrl/amp") → ch("../ctrl/gain"), ch("../ctrl/ampx") → ch("../ctrl/gainx"); a
// folder with its own set renames the set too ("<name>_folders"). The name is checked as in gnAddSpareParm; a node
// parameter and a folder set cannot be renamed (GN_RESULT_INVALID_ARGUMENT). Parameter ids do not change
GN_DECL gnRenameSpareParm(const GN_SessionId, GN_NodeId node, GN_ParmId parm_id, const char* new_name);

GN_DECL gnGetAttributeNames(const GN_SessionId, GN_NodeId node, GN_PartId part, GN_AttributeOwner owner,
    GN_StringHandle* attr_names, int count);

// Groups (as HAPI_GetGroupNames / HAPI_GetGroupMembership / HAPI_GetEdgeCountOfEdgeGroup). The number of groups of each
// type is in GN_GeoInfo (pointGroupCount, primitiveGroupCount, edgeGroupCount); internal groups are hidden. Names are in
// group creation order.
GN_DECL gnGetGroupNames(const GN_SessionId, GN_NodeId node, GN_GroupType group_type,
    GN_StringHandle* group_names_array, int group_count);

// Group membership of the part's elements: 1 — the element is in the group, 0 — it is not. For points and primitives —
// in the part's element order (a piece of a split face — as its primitive; points of a curve part — as the detail
// points they repeat), the length is GN_PartInfo::pointCount / faceCount. For edges — pairs of the part's point indices
// (2 numbers per edge, gnGetEdgeCountOfEdgeGroup edges) in group order; an edge whose points are not in the part is
// skipped. membership_array_all_equal (optional, not used for edges) — all elements of the part are equally in or out
// of the group.
GN_DECL gnGetGroupMembership(const GN_SessionId, GN_NodeId node, GN_PartId part, GN_GroupType group_type,
    const char* group_name, GN_Bool* membership_array_all_equal, int* membership_array, int start, int length);

// The number of edges of the group with both points in the part
GN_DECL gnGetEdgeCountOfEdgeGroup(const GN_SessionId, GN_NodeId node, GN_PartId part, const char* group_name,
    int* edge_count);

// --- Input geometry (HAPI_CreateInputNode and the HAPI geometry setters) ---------------------------------------------
// Geometry given by the caller: points, vertices, faces or curves, attributes and groups. The setters describe one part
// (a mesh or curves) of a SOP; gnCommitGeo builds the geometry and locks the SOP with it (as a hard-locked node in
// Houdini): the node no longer cooks its operator or its inputs, it outputs the committed geometry, and the nodes
// connected to it cook with it. The graph (gnSaveGraph) stores the geometry of locked nodes ("geometry" of the node,
// the format is in geometry/geo_json.h of the library), so such a graph opens in any client. The description stays
// after the commit, as in HAPI: committing again gives the same geometry, and setting one attribute (an animated P)
// and committing again changes only it. part_id is not used (one part): pass 0. Errors — GN_RESULT_INVALID_ARGUMENT
// (GN_RESULT_NODE_INVALID — no such node), the text is in the GN_STATUS_CALL_RESULT status string.
//
// A typical mesh:
//   gnCreateInputNode(s, -1, &node, "mesh");             // /obj/input_mesh/input_mesh
//   GN_PartInfo part = {0}; part.type = GN_PARTTYPE_MESH;
//   part.pointCount = 4; part.vertexCount = 6; part.faceCount = 2;
//   gnSetPartInfo(s, node, 0, &part);
//   GN_AttributeInfo p = {0}; p.exists = 1; p.owner = GN_ATTROWNER_POINT; p.storage = GN_STORAGETYPE_FLOAT;
//   p.count = 4; p.tupleSize = 3; p.typeInfo = HAPI_ATTRIBUTE_TYPE_POINT;
//   gnAddAttribute(s, node, 0, "P", &p);
//   gnSetAttributeFloatData(s, node, 0, "P", &p, positions, 0, 4);
//   gnSetVertexList(s, node, 0, vertices, 0, 6);         // the point of each vertex, face after face
//   gnSetFaceCounts(s, node, 0, faceCounts, 0, 2);       // vertices per face
//   gnCommitGeo(s, node);

// HAPI_CreateInputNode: a Null SOP for input geometry, named "input_<name>" (without a name "input_"; a taken name gets
// a number). parent_node_id — a geo object or a SOP subnet to create it in, -1 — a new geo object on /obj with the same
// name. GN_GeoInfo::type of the node is GN_GEOTYPE_INPUT. The geometry can also be set on any other SOP (except a SOP
// subnet), as in HAPI
GN_DECL gnCreateInputNode(const GN_SessionId, GN_NodeId parent_node_id, GN_NodeId* node_id, const char* name);

// HAPI_SetPartInfo: the part type (GN_PARTTYPE_MESH or GN_PARTTYPE_CURVE) and its counts — pointCount, vertexCount and
// faceCount (faces of a mesh, curves of a curve part); attributeCounts are not used. Starts a new description: the
// attributes, groups, vertex list and counts set before are dropped (the committed geometry stays until the next
// commit). P always exists (points at the origin until it is set)
GN_DECL gnSetPartInfo(const GN_SessionId, GN_NodeId node_id, GN_PartId part_id, const GN_PartInfo* part_info);

// HAPI_SetFaceCounts: the number of vertices of each face (at least 1), start and length — face indices
GN_DECL gnSetFaceCounts(const GN_SessionId, GN_NodeId node_id, GN_PartId part_id, const int* face_counts_array,
    int start, int length);

// HAPI_SetVertexList: the point of each vertex (faces one after another, in the order of their vertices). A curve
// part may go without it: vertex i is then point i
GN_DECL gnSetVertexList(const GN_SessionId, GN_NodeId node_id, GN_PartId part_id, const int* vertex_list_array,
    int start, int length);

// HAPI_AddAttribute: an attribute of attr_info->owner with attr_info->storage (INT, INT64, FLOAT, FLOAT64, STRING;
// UINT8 / INT8 / INT16 are stored as INT; arrays and dictionaries are not supported), attr_info->tupleSize components
// and attr_info->typeInfo (HAPI_ATTRIBUTE_TYPE_INVALID — guessed from the name: N — normal, Cd — color, uv —
// texture, ...). The values start at 0 ("" for strings). An attribute with the same name and owner is replaced. P is
// always a float of size 3
GN_DECL gnAddAttribute(const GN_SessionId, GN_NodeId node_id, GN_PartId part_id, const char* name,
    const GN_AttributeInfo* attr_info);

// HAPI_DeleteAttribute: removes an attribute of attr_info->owner (P cannot be removed)
GN_DECL gnDeleteAttribute(const GN_SessionId, GN_NodeId node_id, GN_PartId part_id, const char* name,
    const GN_AttributeInfo* attr_info);

// HAPI_SetAttribute{Int,Int64,Float,Float64}Data: the values of elements [start, start + length) of attr_info->owner,
// attr_info->tupleSize numbers per element (it must match the attribute). The numbers are converted to the storage of
// the attribute
GN_DECL gnSetAttributeIntData(const GN_SessionId, GN_NodeId node_id, GN_PartId part_id, const char* name,
    const GN_AttributeInfo* attr_info, const int* data_array, int start, int length);

GN_DECL gnSetAttributeInt64Data(const GN_SessionId, GN_NodeId node_id, GN_PartId part_id, const char* name,
    const GN_AttributeInfo* attr_info, const GN_Int64* data_array, int start, int length);

GN_DECL gnSetAttributeFloatData(const GN_SessionId, GN_NodeId node_id, GN_PartId part_id, const char* name,
    const GN_AttributeInfo* attr_info, const float* data_array, int start, int length);

GN_DECL gnSetAttributeFloat64Data(const GN_SessionId, GN_NodeId node_id, GN_PartId part_id, const char* name,
    const GN_AttributeInfo* attr_info, const double* data_array, int start, int length);

// HAPI_SetAttributeStringData: tupleSize strings per element (NULL — an empty string)
GN_DECL gnSetAttributeStringData(const GN_SessionId, GN_NodeId node_id, GN_PartId part_id, const char* name,
    const GN_AttributeInfo* attr_info, const char** data_array, int start, int length);

// HAPI_AddGroup / HAPI_DeleteGroup: a point, primitive or edge group
GN_DECL gnAddGroup(const GN_SessionId, GN_NodeId node_id, GN_PartId part_id, GN_GroupType group_type,
    const char* group_name);

GN_DECL gnDeleteGroup(const GN_SessionId, GN_NodeId node_id, GN_PartId part_id, GN_GroupType group_type,
    const char* group_name);

// HAPI_SetGroupMembership: points and primitives — 1 (in the group) or 0 per element of [start, start + length).
// Edges (unlike HAPI, the same way as gnGetGroupMembership returns them) — pairs of point indices, 2 numbers per edge;
// start and length are edge indices
GN_DECL gnSetGroupMembership(const GN_SessionId, GN_NodeId node_id, GN_PartId part_id, GN_GroupType group_type,
    const char* group_name, const int* membership_array, int start, int length);

// Curves of a curve part (HAPI_SetCurveInfo / Counts / Orders / Knots). info->curveCount must equal the faceCount of the
// part; info->order — 2..11 for every curve, or 0 with per-curve orders from gnSetCurveOrders (ignored for LINEAR).
// As in HAPI (checked in Houdini 21):
//  - LINEAR — polylines; isClosed — closed polygons (they are read back as a mesh part); isPeriodic — an open polyline
//    that repeats its first point at the end;
//  - NURBS — open, with the knots of gnSetCurveKnots when hasKnots is set (order + CV count knots per curve, one curve
//    after another), otherwise the uniform HAPI knots 0, 0, 1, 2, ..., K, K (the curve does not reach its end CVs);
//    isClosed — a closed periodic curve with uniform knots (given knots are not used); isPeriodic does nothing;
//  - BEZIER — the knots are not used; an open curve needs k * (order - 1) + 1 CVs, a closed one (isClosed)
//    k * (order - 1); isPeriodic — an open curve that repeats its first CV.
// A curve Houdini would skip (too few CVs for its order, a CV count that does not fit Bezier segments) fails
// gnCommitGeo. Rational curves: a point attribute Pw (the CV weights), as in Houdini
GN_DECL gnSetCurveInfo(const GN_SessionId, GN_NodeId node_id, GN_PartId part_id, const GN_CurveInfo* info);

// The number of CVs of each curve (as gnSetFaceCounts)
GN_DECL gnSetCurveCounts(const GN_SessionId, GN_NodeId node_id, GN_PartId part_id, const int* counts_array, int start,
    int length);

GN_DECL gnSetCurveOrders(const GN_SessionId, GN_NodeId node_id, GN_PartId part_id, const int* orders_array, int start,
    int length);

// Set the counts and orders first; start and length are knot indices
GN_DECL gnSetCurveKnots(const GN_SessionId, GN_NodeId node_id, GN_PartId part_id, const float* knots_array, int start,
    int length);

// HAPI_CommitGeo: builds the geometry of the description and locks the node with it; the node and the nodes after it
// become dirty (cook them to see it). An incomplete description (a face count or a vertex not set, the counts not
// adding up to vertexCount) or an invalid curve — GN_RESULT_INVALID_ARGUMENT, the node keeps its geometry
GN_DECL gnCommitGeo(const GN_SessionId, GN_NodeId node_id);

// HAPI_RevertGeo: unlocks the node — it cooks its operator again (an input node, a Null SOP, passes its input on);
// the description is dropped
GN_DECL gnRevertGeo(const GN_SessionId, GN_NodeId node_id);

// The session graph (as HAPI_SaveHIPFile / HAPI_LoadHIPFile): all /obj nodes with parameters, expressions, connections
// and time — as JSON. Parameters are saved by name (menus — by item token), only those that differ from their defaults.
// Loading replaces the session graph (the children of /obj are deleted); the whole file is parsed before the session
// changes — on a parse error the graph stays as it was (GN_RESULT_FAILURE). Unknown parameters are skipped; nodes,
// connections that could not be created and invalid expressions are warnings in the GN_STATUS_CALL_RESULT status
// string, the result is success. cook_on_load — queue a cook of all nodes. File paths are UTF-8.
// Demo edition: saving (gnSaveGraph, gnGetGraphString*) is disabled, gnCreateNode creates at most 20 nodes, a graph with
// more than 20 nodes opens only from a demo scene (by file name); a refusal returns GN_RESULT_DEMO_LIMIT, the text is in
// GN_STATUS_CALL_RESULT
GN_DECL gnSaveGraph(const GN_SessionId, const char* file_path);
GN_DECL gnLoadGraph(const GN_SessionId, const char* file_path, GN_Bool cook_on_load);
// The same through a string: first the buffer length (with the terminating zero), then the string of the last
// serialization itself
GN_DECL gnGetGraphStringBufLength(const GN_SessionId, int* buffer_length);
GN_DECL gnGetGraphString(const GN_SessionId, char* string_value, int length);
GN_DECL gnLoadGraphString(const GN_SessionId, const char* json, GN_Bool cook_on_load);

#endif // GEONODUS_H
