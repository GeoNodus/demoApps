#ifndef GEONODUS_API_H
#define GEONODUS_API_H

// Helper macros.
#if defined( __GNUC__ )
#define GN_IS_GCC_GE( MAJOR, MINOR ) \
( __GNUC__ > MAJOR || (__GNUC__ == MAJOR && __GNUC_MINOR__ >= MINOR) )
#else
#define GN_IS_GCC_GE( MAJOR, MINOR ) 0
#endif // defined( __GNUC__ )
#define GC_TO_STRING_( a ) # a
#define GC_TO_STRING( a ) GN_TO_STRING_( a )

#if defined( WIN32 ) && !defined( MAKING_STATIC )
#define GN_VISIBILITY_EXPORT __declspec( dllexport )
#define GN_VISIBILITY_IMPORT __declspec( dllimport )
#define GN_VISIBILITY_EXPORT_TINST GN_VISIBILITY_EXPORT
#define GN_VISIBILITY_IMPORT_TINST GN_VISIBILITY_IMPORT
#elif defined(__GNUC__) && GN_IS_GCC_GE( 4, 2 ) && !defined( MAKING_STATIC )
#define GN_VISIBILITY_EXPORT __attribute__( ( visibility( "default" ) ) )
#define GN_VISIBILITY_IMPORT __attribute__( ( visibility( "default" ) ) )
#define GN_VISIBILITY_EXPORT_TINST
#define GN_VISIBILITY_IMPORT_TINST
#else
#define GN_VISIBILITY_EXPORT
#define GN_VISIBILITY_IMPORT
#define GN_VISIBILITY_EXPORT_TINST
#define GN_VISIBILITY_IMPORT_TINST
#endif

#ifdef GN_EXPORTS
#define GN_API GN_VISIBILITY_EXPORT
#else
#define GN_API GN_VISIBILITY_IMPORT
#endif // GN_EXPORTS

#if defined( WIN32 )
#define GN_CALL __cdecl
#else
#define GN_CALL
#endif // defined( WIN32 )

#ifdef __cplusplus
#define GN_EXTERN_C extern "C"
#else
#define GN_EXTERN_C
#endif // __cplusplus

#define GN_RETURN GN_Result GN_CALL

#define GN_DECL_RETURN( r ) GN_EXTERN_C GN_API r GN_CALL

#define GN_DECL GN_EXTERN_C GN_API GN_RETURN
#define GN_DEF GN_EXTERN_C GN_RETURN

#ifdef __cplusplus
#define GN_C_ENUM_TYPEDEF(enum_name)
#define GN_C_STRUCT_TYPEDEF(struct_name)
#else
#define GN_C_ENUM_TYPEDEF(enum_name) \
typedef enum enum_name enum_name;
#define GN_C_STRUCT_TYPEDEF(struct_name) \
typedef struct struct_name struct_name;
#endif // __cplusplus

#ifdef __cplusplus
typedef bool GN_Bool;
#else
typedef char GN_Bool;
#endif

#include <stdint.h>

typedef uint8_t GN_UInt8;
typedef int8_t GN_Int8;
typedef int16_t GN_Int16;
typedef int64_t GN_Int64;

typedef unsigned int GN_SessionId;
typedef int GN_NodeId;
typedef int GN_StringHandle;
typedef int GN_ParmId;
typedef int GN_PartId;

enum GN_Result {
    GN_RESULT_SUCCESS = 0,
    GN_RESULT_FAILURE = 1,
    GN_RESULT_ALREADY_INITIALIZED = 2,
    GN_RESULT_NOT_INITIALIZED = 3,
    GN_RESULT_CANT_LOADFILE = 4,
    GN_RESULT_PARM_SET_FAILED = 5,
    GN_RESULT_INVALID_ARGUMENT = 6,
    GN_RESULT_CANT_LOAD_GEO = 7,
    GN_RESULT_CANT_GENERATE_PRESET = 8,
    GN_RESULT_CANT_LOAD_PRESET = 9,
    GN_RESULT_ASSET_DEF_ALREADY_LOADED = 10,

    GN_RESULT_ASSET_INVALID = 200,
    GN_RESULT_NODE_INVALID = 210,

    GN_RESULT_USER_INTERRUPTED = 300,

    GN_RESULT_INVALID_SESSION = 400,

    // Not in HAPI: a limit of the demo edition of the library (node limit, graph saving); the text is in
    // GN_STATUS_CALL_RESULT
    GN_RESULT_DEMO_LIMIT = 500
};
GN_C_ENUM_TYPEDEF(GN_Result)

enum GN_AttributeOwner {
    GN_ATTROWNER_INVALID = -1,
    GN_ATTROWNER_VERTEX,
    GN_ATTROWNER_POINT,
    GN_ATTROWNER_PRIM,
    GN_ATTROWNER_DETAIL,
    GN_ATTROWNER_MAX
};
GN_C_ENUM_TYPEDEF(GN_AttributeOwner)

// HAPI_GroupType: point, primitive and edge groups are visible through the API (vertex groups — only inside the library)
enum GN_GroupType {
    GN_GROUPTYPE_INVALID = -1,
    GN_GROUPTYPE_POINT,
    GN_GROUPTYPE_PRIM,
    GN_GROUPTYPE_EDGE,
    GN_GROUPTYPE_MAX
};
GN_C_ENUM_TYPEDEF(GN_GroupType)

enum GN_NodeType {
    GN_NODETYPE_ANY = -1,
    GN_NODETYPE_NONE = 0,
    GN_NODETYPE_OBJ = 1 << 0,
    GN_NODETYPE_SURF_OP = 1 << 1,
};
GN_C_ENUM_TYPEDEF(GN_NodeType)
typedef int GN_NodeTypeBits;

enum GN_NodeFlags {
    GN_NODEFLAGS_ANY = -1,
    GN_NODEFLAGS_NONE = 0,
    GN_NODEFLAGS_DISPLAY = 1 << 0,
    GN_NODEFLAGS_RENDER = 1 << 1,
    GN_NODEFLAGS_TEMPLATED = 1 << 2,
    GN_NODEFLAGS_LOCKED = 1 << 3,
    GN_NODEFLAGS_EDITABLE = 1 << 4,
    GN_NODEFLAGS_BYPASS = 1 << 5,
    GN_NODEFLAGS_NETWORK = 1 << 6,

    GN_NODEFLAGS_OBJ_GEOMETRY = 1 << 7,
    GN_NODEFLAGS_OBJ_CAMERA = 1 << 8,
    GN_NODEFLAGS_OBJ_LIGHT = 1 << 9,
    GN_NODEFLAGS_OBJ_SUBNET = 1 << 10,

    GN_NODEFLAGS_SOP_CURVE = 1 << 11,
    GN_NODEFLAGS_SOP_GUIDE = 1 << 12,

    // As HAPI_NODEFLAGS_NON_BYPASS: nodes without the Bypass flag (in a filter)
    GN_NODEFLAGS_NON_BYPASS = 1 << 14,

    // Not in HAPI: an internal input node of a SOP subnet (HDK: OP_SubnetIndirectInput, in Houdini — "1".."4" inside
    // the subnet). Input nodes are created together with the subnet, they cannot be deleted or renamed, they have no
    // parameters and flags. gnComposeChildNodeList and childNodeCount do not list them (as in HAPI), except with a
    // filter with this flag
    GN_NODEFLAGS_SOP_SUBNET_INPUT = 1 << 15,
};
GN_C_ENUM_TYPEDEF(GN_NodeFlags)
typedef int GN_NodeFlagsBits;

enum GN_StatusType { GN_STATUS_CALL_RESULT, GN_STATUS_COOK_RESULT, GN_STATUS_COOK_STATE, GN_STATUS_MAX };
GN_C_ENUM_TYPEDEF(GN_StatusType)

// Verbosity of a status string (like HAPI_StatusVerbosity).
enum GN_StatusVerbosity {
    GN_STATUSVERBOSITY_0,
    GN_STATUSVERBOSITY_1,
    GN_STATUSVERBOSITY_2,

    GN_STATUSVERBOSITY_ERRORS = GN_STATUSVERBOSITY_0,
    GN_STATUSVERBOSITY_WARNINGS = GN_STATUSVERBOSITY_1,
    GN_STATUSVERBOSITY_MESSAGES = GN_STATUSVERBOSITY_2,
    GN_STATUSVERBOSITY_ALL = GN_STATUSVERBOSITY_2,
    GN_STATUSVERBOSITY_MAX
};
GN_C_ENUM_TYPEDEF(GN_StatusVerbosity)

enum GN_State {
    GN_STATE_READY,
    GN_STATE_READY_WITH_FATAL_ERRORS,
    GN_STATE_READY_WITH_COOK_ERRORS,
    GN_STATE_STARTING_COOK,
    GN_STATE_COOKING,
    GN_STATE_STARTING_LOAD,
    GN_STATE_LOADING,
    GN_STATE_MAX,

    GN_STATE_MAX_READY_STATE = GN_STATE_READY_WITH_COOK_ERRORS
};
GN_C_ENUM_TYPEDEF(GN_State)

// The cook state of a node — like the is_cooking flag / cook code of a node in Houdini HDK and HAPI_NodeCookState in
// Houdini HAPI.
enum GN_CookState {
    GN_COOKSTATE_INVALID = -1, // the node does not exist (or the session is closed)
    GN_COOKSTATE_IDLE = 0,     // not cooked yet
    GN_COOKSTATE_QUEUED,       // waiting in the session's cook queue
    GN_COOKSTATE_COOKING,      // cooking (on the session's job thread)
    GN_COOKSTATE_SUCCESS,      // cooked successfully
    GN_COOKSTATE_ERROR         // cooked with errors
};
GN_C_ENUM_TYPEDEF(GN_CookState)

enum GN_PartType {
    GN_PARTTYPE_INVALID = -1,
    GN_PARTTYPE_MESH,
    GN_PARTTYPE_CURVE,
    GN_PARTTYPE_VOLUME,
    GN_PARTTYPE_INSTANCER,
    GN_PARTTYPE_BOX,
    GN_PARTTYPE_SPHERE,
    GN_PARTTYPE_MAX
};
GN_C_ENUM_TYPEDEF(GN_PartType)

enum GN_ParmType {
    GN_PARMTYPE_INT = 0,
    GN_PARMTYPE_MULTIPARMLIST,
    GN_PARMTYPE_TOGGLE,
    GN_PARMTYPE_BUTTON,

    GN_PARMTYPE_FLOAT,
    GN_PARMTYPE_COLOR,

    GN_PARMTYPE_STRING,
    GN_PARMTYPE_PATH_FILE,
    GN_PARMTYPE_PATH_FILE_GEO,
    GN_PARMTYPE_PATH_FILE_IMAGE,

    GN_PARMTYPE_NODE,

    GN_PARMTYPE_FOLDERLIST,
    GN_PARMTYPE_FOLDERLIST_RADIO,

    GN_PARMTYPE_FOLDER,
    GN_PARMTYPE_LABEL,
    GN_PARMTYPE_SEPARATOR,
    GN_PARMTYPE_PATH_FILE_DIR,

    GN_PARMTYPE_MAX
};
GN_C_ENUM_TYPEDEF(GN_ParmType)

// The parameter type in Houdini terms (HAPI_PrmScriptType, GN_ParmInfo::scriptType). For folders — the folder kind:
// tabs, collapsible, a simple frame with a title, radio buttons.
enum GN_PrmScriptType {
    GN_PRM_SCRIPT_TYPE_INT = 0,
    GN_PRM_SCRIPT_TYPE_FLOAT,
    GN_PRM_SCRIPT_TYPE_ANGLE,
    GN_PRM_SCRIPT_TYPE_STRING,
    GN_PRM_SCRIPT_TYPE_FILE,
    GN_PRM_SCRIPT_TYPE_DIRECTORY,
    GN_PRM_SCRIPT_TYPE_IMAGE,
    GN_PRM_SCRIPT_TYPE_GEOMETRY,
    GN_PRM_SCRIPT_TYPE_TOGGLE,
    GN_PRM_SCRIPT_TYPE_BUTTON,
    GN_PRM_SCRIPT_TYPE_VECTOR2,
    GN_PRM_SCRIPT_TYPE_VECTOR3,
    GN_PRM_SCRIPT_TYPE_VECTOR4,
    GN_PRM_SCRIPT_TYPE_INTVECTOR2,
    GN_PRM_SCRIPT_TYPE_INTVECTOR3,
    GN_PRM_SCRIPT_TYPE_INTVECTOR4,
    GN_PRM_SCRIPT_TYPE_UV,
    GN_PRM_SCRIPT_TYPE_UVW,
    GN_PRM_SCRIPT_TYPE_DIR,
    GN_PRM_SCRIPT_TYPE_COLOR,
    GN_PRM_SCRIPT_TYPE_COLOR4,
    GN_PRM_SCRIPT_TYPE_HUECIRCLE,
    GN_PRM_SCRIPT_TYPE_OPPATH,
    GN_PRM_SCRIPT_TYPE_OPLIST,
    GN_PRM_SCRIPT_TYPE_OBJECT,
    GN_PRM_SCRIPT_TYPE_OBJECTLIST,
    GN_PRM_SCRIPT_TYPE_RENDER,
    GN_PRM_SCRIPT_TYPE_SEPARATOR,
    GN_PRM_SCRIPT_TYPE_GEOMETRY_DATA,
    GN_PRM_SCRIPT_TYPE_KEY_VALUE_DICT,
    GN_PRM_SCRIPT_TYPE_LABEL,
    GN_PRM_SCRIPT_TYPE_RGBAMASK,
    GN_PRM_SCRIPT_TYPE_ORDINAL,
    GN_PRM_SCRIPT_TYPE_RAMP_FLT,
    GN_PRM_SCRIPT_TYPE_RAMP_RGB,
    GN_PRM_SCRIPT_TYPE_FLOAT_LOG,
    GN_PRM_SCRIPT_TYPE_INT_LOG,
    GN_PRM_SCRIPT_TYPE_DATA,
    GN_PRM_SCRIPT_TYPE_FLOAT_MINMAX,
    GN_PRM_SCRIPT_TYPE_INT_MINMAX,
    GN_PRM_SCRIPT_TYPE_INT_STARTEND,
    GN_PRM_SCRIPT_TYPE_BUTTONSTRIP,
    GN_PRM_SCRIPT_TYPE_ICONSTRIP,

    // For GN_PARMTYPE_FOLDER (and its GN_PARMTYPE_FOLDERLIST)
    GN_PRM_SCRIPT_TYPE_GROUPRADIO = 1000,
    GN_PRM_SCRIPT_TYPE_GROUPCOLLAPSIBLE,
    GN_PRM_SCRIPT_TYPE_GROUPSIMPLE,
    GN_PRM_SCRIPT_TYPE_GROUP
};
GN_C_ENUM_TYPEDEF(GN_PrmScriptType)

// HAPI_RampType: the kind of a ramp — a multiparm whose instances are pos / value / interp keys; INVALID for other
// parameters
enum GN_RampType {
    GN_RAMPTYPE_INVALID = -1,
    GN_RAMPTYPE_FLOAT,
    GN_RAMPTYPE_COLOR,
    GN_RAMPTYPE_MAX
};
GN_C_ENUM_TYPEDEF(GN_RampType)

enum GN_ChoiceListType {
    GN_CHOICELISTTYPE_NONE,
    GN_CHOICELISTTYPE_NORMAL,
    GN_CHOICELISTTYPE_MINI,
    GN_CHOICELISTTYPE_REPLACE,
    GN_CHOICELISTTYPE_TOGGLE
};
GN_C_ENUM_TYPEDEF(GN_ChoiceListType)

struct GN_API GN_ParmChoiceInfo
{
    GN_ParmId parentParmId;
    GN_StringHandle labelSH;
    GN_StringHandle valueSH;
};
GN_C_STRUCT_TYPEDEF(GN_ParmChoiceInfo)

struct GN_API GN_TimelineOptions
{
    double fps;
    double startTime;
    double endTime;
};
GN_C_STRUCT_TYPEDEF(GN_TimelineOptions)

struct GN_API GN_CookOptions
{
    GN_Bool splitGeosByGroup;
    GN_StringHandle splitGroupSH;

    GN_Bool splitGeosByAttribute;
    GN_StringHandle splitAttrSH;

    GN_Bool refineCurveToLinear;

    float curveRefineLOD;

    int maxVerticesPerPrimitive;
    GN_Bool clearErrorsAndWarnings;

    GN_Bool splitPointsByVertexAttributes;
};
GN_C_STRUCT_TYPEDEF(GN_CookOptions)

// Parameter description (HAPI_ParmInfo). The panel layout is as in HAPI:
//  - GN_PARMTYPE_FOLDERLIST — a folder set, size — the number of folders; its folders are GN_PARMTYPE_FOLDER (parentId —
//    the set), a folder's size — the number of its parameters; the parentId of a folder's parameters is the folder, -1 —
//    the root; childIndex — the index among the parent's children. The list order is depth-first (set, folder, its
//    parameters, next folder, ...);
//  - scriptType of a folder and a set — the folder kind (GN_PRM_SCRIPT_TYPE_GROUP — tabs, GROUPSIMPLE — a frame, ...);
//  - joinNext — the next parameter is on the same row, labelNone — do not show the label;
//  - a file path string — type GN_PARMTYPE_PATH_FILE / _GEO / _IMAGE / _DIR (scriptType FILE / GEOMETRY / IMAGE /
//    DIRECTORY), the value is read and written as for GN_PARMTYPE_STRING (stringValuesIndex);
//  - disabled / invisible — the DisableWhen / HideWhen conditions evaluated for the current values (the parameter's own
//    and those of the folders it is in); disabledConditionSH / visibilityConditionSH — the conditions themselves in
//    Houdini syntax. Re-read the description after changing values.
struct GN_API GN_ParmInfo
{
    GN_ParmId id;
    GN_ParmId parentId;

    int childIndex;

    GN_ParmType type;
    GN_PrmScriptType scriptType;

    GN_StringHandle typeInfoSH;

    int size;

    GN_ChoiceListType choiceListType;

    int choiceCount;

    GN_StringHandle nameSH;
    GN_StringHandle labelSH;
    // HAPI: for a multiparm instance parameter — the name with '#' ("applyattribs#"), for others — as nameSH
    GN_StringHandle templateNameSH;
    GN_StringHandle helpSH;

    GN_Bool hasMin;
    GN_Bool hasMax;
    GN_Bool hasUIMin;
    GN_Bool hasUIMax;

    float min;
    float max;
    float UIMin;
    float UIMax;

    GN_Bool invisible;
    GN_Bool disabled;

    GN_Bool spare;

    GN_Bool joinNext;
    ///
    GN_Bool labelNone;

    int intValuesIndex;
    int floatValuesIndex;
    int stringValuesIndex;
    int choiceIndex;

    GN_NodeType inputNodeType;
    GN_NodeFlags inputNodeFlag;

    GN_Bool isChildOfMultiParm;

    // HAPI_ParmInfo: the multiparm instance number (from instanceStartOffset), the number of parameters per instance and
    // the number of instances (for GN_PARMTYPE_MULTIPARMLIST and the parameters of its instances), the number of the
    // first instance (usually 1, 0 for the Add SOP, as the multistartoffset tag in Houdini)
    int instanceNum;
    int instanceLength;
    int instanceCount;
    int instanceStartOffset;

    // HAPI_ParmInfo::rampType: for a ramp (GN_PARMTYPE_MULTIPARMLIST with scriptType RAMP_FLT) — GN_RAMPTYPE_FLOAT, for
    // other parameters — GN_RAMPTYPE_INVALID
    GN_RampType rampType;

    GN_StringHandle visibilityConditionSH;
    GN_StringHandle disabledConditionSH;

    GN_Bool useMenuItemTokenAsValue;
};
GN_C_STRUCT_TYPEDEF(GN_ParmInfo)

// Not in HAPI: a user (spare) parameter description for gnAddSpareParm (HDK: hou.ParmTemplate — IntParmTemplate,
// FloatParmTemplate, StringParmTemplate, ToggleParmTemplate, MenuParmTemplate, FolderParmTemplate, LabelParmTemplate,
// SeparatorParmTemplate). The struct can be zeroed with only type, name and label set: one component, values 0 / "",
// no range, menu or conditions.
struct GN_API GN_SpareParmTemplate
{
    // GN_PARMTYPE_INT (with menuItemCount > 0 — a menu), GN_PARMTYPE_FLOAT, GN_PARMTYPE_TOGGLE, GN_PARMTYPE_STRING,
    // GN_PARMTYPE_PATH_FILE / _GEO / _IMAGE / _DIR, GN_PARMTYPE_FOLDER, GN_PARMTYPE_LABEL, GN_PARMTYPE_SEPARATOR
    GN_ParmType type;
    // Name: Latin letters, digits and '_', not starting with a digit. Unique among the node's parameters, including the
    // component names of vector parameters (tx of t) and multiparm instances (value3 of value#); for a folder the name
    // "<name>_folders" must be free too — that is the name of its set when it does not join a neighbouring tab set
    const char* name;
    const char* label; // nullptr — the name (a separator has no label)
    int size;          // components of INT (without a menu), FLOAT, STRING: 1..4 (0 — 1); unused for the others

    double defaults[4];            // INT, FLOAT, TOGGLE (≠ 0 — on), menu — the item index from 0
    const char* stringDefaults[4]; // STRING and paths (nullptr — "")

    // Range (HDK: PRM_Range; hou: min, max, min_is_strict, max_is_strict) of INT and FLOAT: the slider goes from min to
    // max (GN_ParmInfo::UIMin / UIMax); a strict bound also clamps the value (GN_ParmInfo::hasMin / hasMax)
    GN_Bool hasRange;
    double min;
    double max;
    GN_Bool minIsStrict;
    GN_Bool maxIsStrict;

    // Menu of an INT (hou.MenuParmTemplate): item tokens (non-empty, distinct) and labels (nullptr — labels = tokens).
    // The value is the item index from 0, written to the graph as the token
    int menuItemCount;
    const char* const* menuItems;
    const char* const* menuLabels;

    // Folder kind (for FOLDER): GN_PRM_SCRIPT_TYPE_GROUP — a tab (0 — too), GROUPSIMPLE — a frame with a title,
    // GROUPCOLLAPSIBLE — collapsible
    GN_PrmScriptType folderType;

    GN_Bool joinNext;
    GN_Bool labelNone;
    // Conditions in Houdini syntax (GN_ParmInfo::disabledConditionSH / visibilityConditionSH), nullptr — none
    const char* disableWhen;
    const char* hideWhen;
};
GN_C_STRUCT_TYPEDEF(GN_SpareParmTemplate)

struct GN_API GN_NodeInfo
{
    GN_NodeId id;
    GN_NodeId parentId;
    GN_StringHandle nameSH;
    GN_NodeType type;

    GN_Bool isValid;

    int totalCookCount;

    // HAPI_NodeInfo::uniqueHoudiniNodeId — for gnIsNodeValid. Node ids are never reused, so it equals id
    int uniqueNodeId;

    GN_StringHandle internalNodePathSH;

    int parmCount;

    int parmIntValueCount;
    int parmFloatValueCount;
    int parmStringValueCount;

    int parmChoiceCount;

    int childNodeCount;

    int inputCount;
    int outputCount;

    GN_Bool isTimeDependent;
};
GN_C_STRUCT_TYPEDEF(GN_NodeInfo)

struct GN_API GN_PartInfo
{
    GN_PartId id;
    GN_StringHandle nameSH;
    GN_PartType type;

    int faceCount;
    int vertexCount;

    int pointCount;

    int attributeCounts[GN_ATTROWNER_MAX];

    GN_Bool hasChanged;
};
GN_C_STRUCT_TYPEDEF(GN_PartInfo)

struct GN_API GN_BoundingBox
{
    GN_Bool valid;
    float min[3];
    float max[3];
};
GN_C_STRUCT_TYPEDEF(GN_BoundingBox)

// HAPI_RSTOrder: the order of scale, rotation and translation (row vectors, as in Houdini: GN_SRT — p' = p * S * R * T)
enum GN_RSTOrder {
    GN_TRS = 0,
    GN_TSR,
    GN_RTS,
    GN_RST,
    GN_STR,
    GN_SRT,

    GN_RSTORDER_DEFAULT = GN_SRT
};
GN_C_ENUM_TYPEDEF(GN_RSTOrder)

// HAPI_Transform: translation, rotation as a quaternion (x, y, z, w), scale and shear (xy, xz, yz; the scale block is
// S * Sh, as in hou.hmath.buildTransform) in rstOrder
struct GN_API GN_Transform
{
    float position[3];
    float rotationQuaternion[4];
    float scale[3];
    float shear[3];

    GN_RSTOrder rstOrder;
};
GN_C_STRUCT_TYPEDEF(GN_Transform)

enum GN_CurveType {
    GN_CURVETYPE_INVALID = -1,
    GN_CURVETYPE_LINEAR,
    GN_CURVETYPE_NURBS,
    GN_CURVETYPE_BEZIER,
    GN_CURVETYPE_MAX
};
GN_C_ENUM_TYPEDEF(GN_CurveType)

struct GN_API GN_CurveInfo
{
    GN_CurveType curveType;

    int curveCount;
    int vertexCount;
    int knotCount;

    GN_Bool isPeriodic;
    GN_Bool isRational;

    int order;

    GN_Bool hasKnots;
    GN_Bool isClosed;
};
GN_C_STRUCT_TYPEDEF(GN_CurveInfo)

enum GN_GeoType { GN_GEOTYPE_INVALID = -1, GN_GEOTYPE_DEFAULT, GN_GEOTYPE_INPUT, GN_GEOTYPE_CURVE, GN_GEOTYPE_MAX };
GN_C_ENUM_TYPEDEF(GN_GeoType)

struct GN_API GN_GeoInfo
{
    GN_GeoType type;
    GN_StringHandle nameSH;

    GN_NodeId nodeId;

    GN_Bool isEditable;

    GN_Bool isTemplated;

    GN_Bool isDisplayGeo;

    GN_Bool hasGeoChanged;

    GN_Bool hasMaterialChanged;

    int pointGroupCount;
    int primitiveGroupCount;
    int edgeGroupCount;

    int partCount;
};
GN_C_STRUCT_TYPEDEF(GN_GeoInfo)

enum GN_AttributeTypeInfo {
    HAPI_ATTRIBUTE_TYPE_INVALID = -1,
    HAPI_ATTRIBUTE_TYPE_NONE,
    HAPI_ATTRIBUTE_TYPE_POINT,
    HAPI_ATTRIBUTE_TYPE_HPOINT,
    HAPI_ATTRIBUTE_TYPE_VECTOR,
    HAPI_ATTRIBUTE_TYPE_NORMAL,
    HAPI_ATTRIBUTE_TYPE_COLOR,
    HAPI_ATTRIBUTE_TYPE_QUATERNION,
    HAPI_ATTRIBUTE_TYPE_MATRIX3,
    HAPI_ATTRIBUTE_TYPE_MATRIX,
    HAPI_ATTRIBUTE_TYPE_ST,
    HAPI_ATTRIBUTE_TYPE_HIDDEN,
    HAPI_ATTRIBUTE_TYPE_BOX2,
    HAPI_ATTRIBUTE_TYPE_BOX,
    HAPI_ATTRIBUTE_TYPE_TEXTURE,
    HAPI_ATTRIBUTE_TYPE_MAX
};
GN_C_ENUM_TYPEDEF(GN_AttributeTypeInfo)

// HAPI_StorageType: the storage type of attribute values (same order as in HAPI)
enum GN_StorageType {
    GN_STORAGETYPE_INVALID = -1,

    GN_STORAGETYPE_INT,
    GN_STORAGETYPE_INT64,
    GN_STORAGETYPE_FLOAT,
    GN_STORAGETYPE_FLOAT64,
    GN_STORAGETYPE_STRING,
    GN_STORAGETYPE_UINT8,
    GN_STORAGETYPE_INT8,
    GN_STORAGETYPE_INT16,
    GN_STORAGETYPE_DICTIONARY,

    GN_STORAGETYPE_INT_ARRAY,
    GN_STORAGETYPE_INT64_ARRAY,
    GN_STORAGETYPE_FLOAT_ARRAY,
    GN_STORAGETYPE_FLOAT64_ARRAY,
    GN_STORAGETYPE_STRING_ARRAY,
    GN_STORAGETYPE_UINT8_ARRAY,
    GN_STORAGETYPE_INT8_ARRAY,
    GN_STORAGETYPE_INT16_ARRAY,
    GN_STORAGETYPE_DICTIONARY_ARRAY,

    GN_STORAGETYPE_MAX
};
GN_C_ENUM_TYPEDEF(GN_StorageType)

struct GN_API GN_AttributeInfo
{
    GN_Bool exists;

    GN_AttributeOwner owner;
    GN_StorageType storage;
    GN_AttributeOwner originalOwner;

    int count;
    int tupleSize;

    GN_Int64 totalArrayElements;
    GN_AttributeTypeInfo typeInfo;
};
GN_C_STRUCT_TYPEDEF(GN_AttributeInfo)

#endif // GEONODUS_GLOBAL_H
