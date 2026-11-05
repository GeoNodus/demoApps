#!/usr/bin/env python3
"""Generates GeoNodusMetalDemo.xcodeproj next to this script: the Metal demo (metal/) as one Xcode app target with
relative paths, without CMake. Swift sources (metal/Sources), the C bridge (metal/bridge) and the demo scenes
(../scenes/*.cpp) are compiled into the app; lib/libgeonodus.dylib is linked and copied into Contents/Frameworks.

Run it again after adding source files (a new scenes_*.cpp, a new .swift):  python3 make_xcodeproj.py
"""
import glob
import hashlib
import os

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.join(HERE, "GeoNodusMetalDemo.xcodeproj")
PRODUCT = "GeoNodus Metal Demo"


def oid(key):
    # Stable 24-hex object ids: the project file does not change between runs
    return hashlib.md5(key.encode()).hexdigest()[:24].upper()


def q(s):
    # Quoting of strings in the OpenStep plist
    if s and all(c.isalnum() or c in "._/$" for c in s):
        return s
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def rel(pattern):
    return sorted(os.path.relpath(p, HERE) for p in glob.glob(os.path.join(HERE, pattern)))


swift = rel("metal/Sources/*.swift")
bridge = rel("metal/bridge/*.cpp") + rel("metal/bridge/*.h")
scenes = rel("../scenes/*.cpp") + rel("../scenes/*.h")
others = ["metal/BridgingHeader.h", "metal/Info.plist"]
dylib = "lib/libgeonodus.dylib"

groups = [
    ("Sources", swift),
    ("bridge", bridge),
    ("scenes", scenes),
    ("Support", others),
    ("lib", [dylib]),
]


def file_type(path):
    ext = os.path.splitext(path)[1]
    return {
        ".swift": "sourcecode.swift",
        ".cpp": "sourcecode.cpp.cpp",
        ".h": "sourcecode.c.h",
        ".plist": "text.plist.xml",
        ".dylib": "compiled.mach-o.dylib",
    }[ext]


compiled = [p for p in swift + bridge + scenes if p.endswith((".swift", ".cpp"))]

objs = {}  # id → (isa, text)


def add(key, isa, body):
    objs[oid(key)] = (isa, body)
    return oid(key)


file_refs = {}
for _, files in groups:
    for p in files:
        file_refs[p] = add("ref:" + p, "PBXFileReference",
                           f"isa = PBXFileReference; lastKnownFileType = {file_type(p)}; "
                           f"name = {q(os.path.basename(p))}; path = {q(p)}; sourceTree = SOURCE_ROOT;")
product_ref = add("ref:product", "PBXFileReference",
                  f"isa = PBXFileReference; explicitFileType = wrapper.application; includeInIndex = 0; "
                  f"path = {q(PRODUCT + '.app')}; sourceTree = BUILT_PRODUCTS_DIR;")

source_builds = [add("build:" + p, "PBXBuildFile", f"isa = PBXBuildFile; fileRef = {file_refs[p]};")
                 for p in compiled]
link_build = add("build:link", "PBXBuildFile", f"isa = PBXBuildFile; fileRef = {file_refs[dylib]};")
embed_build = add("build:embed", "PBXBuildFile",
                  f"isa = PBXBuildFile; fileRef = {file_refs[dylib]}; "
                  "settings = {ATTRIBUTES = (CodeSignOnCopy, ); };")

group_ids = []
for name, files in groups:
    children = "".join(f"{file_refs[p]}, " for p in files)
    group_ids.append(add("group:" + name, "PBXGroup",
                         f"isa = PBXGroup; children = ({children}); name = {q(name)}; sourceTree = \"<group>\";"))
products_group = add("group:Products", "PBXGroup",
                     f"isa = PBXGroup; children = ({product_ref}, ); name = Products; sourceTree = \"<group>\";")
main_group = add("group:main", "PBXGroup",
                 "isa = PBXGroup; children = (" + "".join(f"{g}, " for g in group_ids + [products_group])
                 + "); sourceTree = \"<group>\";")

sources_phase = add("phase:sources", "PBXSourcesBuildPhase",
                    "isa = PBXSourcesBuildPhase; buildActionMask = 2147483647; files = ("
                    + "".join(f"{b}, " for b in source_builds) + "); runOnlyForDeploymentPostprocessing = 0;")
frameworks_phase = add("phase:frameworks", "PBXFrameworksBuildPhase",
                       f"isa = PBXFrameworksBuildPhase; buildActionMask = 2147483647; files = ({link_build}, ); "
                       "runOnlyForDeploymentPostprocessing = 0;")
embed_phase = add("phase:embed", "PBXCopyFilesBuildPhase",
                  f"isa = PBXCopyFilesBuildPhase; buildActionMask = 2147483647; dstPath = \"\"; dstSubfolderSpec = 10; "
                  f"files = ({embed_build}, ); name = \"Embed Libraries\"; runOnlyForDeploymentPostprocessing = 0;")

common = {
    "PRODUCT_NAME": PRODUCT,
    "PRODUCT_BUNDLE_IDENTIFIER": "com.geonodus.demo.metal",
    "INFOPLIST_FILE": "metal/Info.plist",
    "MACOSX_DEPLOYMENT_TARGET": "12.0",
    "ARCHS": "arm64",
    "SDKROOT": "macosx",
    "SWIFT_VERSION": "5.0",
    "SWIFT_OBJC_BRIDGING_HEADER": "metal/BridgingHeader.h",
    "HEADER_SEARCH_PATHS": "(\"$(SRCROOT)/lib/include\", \"$(SRCROOT)/../scenes\", \"$(SRCROOT)/metal\", )",
    "LIBRARY_SEARCH_PATHS": "(\"$(SRCROOT)/lib\", )",
    "LD_RUNPATH_SEARCH_PATHS": "(\"@executable_path/../Frameworks\", )",
    "CLANG_CXX_LANGUAGE_STANDARD": "c++20",
    "CLANG_CXX_LIBRARY": "libc++",
    "CODE_SIGN_IDENTITY": "-",
    "CODE_SIGN_STYLE": "Manual",
    "COMBINE_HIDPI_IMAGES": "YES",
    "ALWAYS_SEARCH_USER_PATHS": "NO",
    "ENABLE_USER_SCRIPT_SANDBOXING": "YES",
}
variants = {
    "Debug": {"SWIFT_OPTIMIZATION_LEVEL": "-Onone", "GCC_OPTIMIZATION_LEVEL": "0", "DEBUG_INFORMATION_FORMAT": "dwarf",
              "ONLY_ACTIVE_ARCH": "YES", "SWIFT_ACTIVE_COMPILATION_CONDITIONS": "DEBUG",
              "GCC_PREPROCESSOR_DEFINITIONS": "(\"DEBUG=1\", )"},
    "Release": {"SWIFT_OPTIMIZATION_LEVEL": "-O", "SWIFT_COMPILATION_MODE": "wholemodule",
                "GCC_OPTIMIZATION_LEVEL": "s", "DEBUG_INFORMATION_FORMAT": "dwarf-with-dsym",
                "GCC_PREPROCESSOR_DEFINITIONS": "(\"NDEBUG=1\", )"},
}


def settings(d):
    out = []
    for k, v in d.items():
        out.append(f"{k} = {v if v.startswith('(') else q(v)};")
    return "buildSettings = {" + " ".join(out) + "};"


def config_list(key, extra):
    ids = []
    for name, v in variants.items():
        ids.append(add(f"config:{key}:{name}", "XCBuildConfiguration",
                       f"isa = XCBuildConfiguration; {settings(extra(v))} name = {name};"))
    return add("configlist:" + key, "XCConfigurationList",
               "isa = XCConfigurationList; buildConfigurations = (" + "".join(f"{i}, " for i in ids)
               + "); defaultConfigurationIsVisible = 0; defaultConfigurationName = Release;")


target_configs = config_list("target", lambda v: {**common, **v})
project_configs = config_list("project", lambda v: {"SDKROOT": "macosx", "MACOSX_DEPLOYMENT_TARGET": "12.0"})

target = add("target", "PBXNativeTarget",
             f"isa = PBXNativeTarget; buildConfigurationList = {target_configs}; "
             f"buildPhases = ({sources_phase}, {frameworks_phase}, {embed_phase}, ); buildRules = (); "
             f"dependencies = (); name = GeoNodusMetalDemo; productName = GeoNodusMetalDemo; "
             f"productReference = {product_ref}; productType = \"com.apple.product-type.application\";")
project = add("project", "PBXProject",
              f"isa = PBXProject; attributes = {{BuildIndependentTargetsInParallel = 1; LastUpgradeCheck = 1600; }}; "
              f"buildConfigurationList = {project_configs}; compatibilityVersion = \"Xcode 14.0\"; "
              f"developmentRegion = en; hasScannedForEncodings = 0; knownRegions = (en, Base, ); "
              f"mainGroup = {main_group}; productRefGroup = {products_group}; projectDirPath = \"\"; "
              f"projectRoot = \"\"; targets = ({target}, );")

lines = ["// !$*UTF8*$!", "{", "\tarchiveVersion = 1;", "\tclasses = {", "\t};", "\tobjectVersion = 56;",
         "\tobjects = {"]
for isa in sorted({i for i, _ in objs.values()}):
    lines.append(f"\n/* Begin {isa} section */")
    for k in sorted(objs):
        if objs[k][0] == isa:
            lines.append(f"\t\t{k} = {{{objs[k][1]}}};")
    lines.append(f"/* End {isa} section */")
lines += ["\t};", f"\trootObject = {project};", "}", ""]

os.makedirs(PROJECT, exist_ok=True)
with open(os.path.join(PROJECT, "project.pbxproj"), "w") as f:
    f.write("\n".join(lines))
print("Written", os.path.relpath(PROJECT, os.getcwd()), "—", len(compiled), "sources")
