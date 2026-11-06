#!/usr/bin/env python3
"""Generates GeoNodusMetalDemo.xcodeproj next to this script: the iOS / iPadOS Metal demo as one Xcode app target with
relative paths, without CMake. Builds for devices and for the simulator.

Compiled into the app:
  metal/Sources/*.swift                 — the UIKit shell (AppDelegate, DemoViewController);
  ../mac/metal/Sources/{GnScene,MetalRenderer,DemoMath}.swift — the scene, the renderer and the matrices, shared with
                                          the macOS demo (no AppKit in them);
  ../mac/metal/bridge/*.cpp, ../scenes/*.cpp — the demo scenes built in code (C++) and their C interface for Swift.
Linked and embedded: lib/geonodus.xcframework — the demo edition of the library (device arm64, simulator arm64 and
x86_64), put there by geonodus/ios/build_xcframework.sh of the library repository.
Resources: the demo graphs ../scenes/*.json and ../scenes/assets (crystal.obj of the File SOP).

Signing: automatic; for a device choose your team in Signing & Capabilities (or pass DEVELOPMENT_TEAM=<id> to
xcodebuild). Run it again after adding source files (a new scenes_*.cpp, a new .swift):  python3 make_xcodeproj.py
"""
import glob
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from xcodeproj_writer import Project  # noqa: E402

PROJECT = os.path.join(HERE, "GeoNodusMetalDemo.xcodeproj")
PRODUCT = "GeoNodus Metal Demo"
IOS_TARGET = "15.0"
LIB = "lib/geonodus.xcframework"


def rel(pattern):
    return sorted(os.path.relpath(p, HERE) for p in glob.glob(os.path.join(HERE, pattern)))


swift = rel("metal/Sources/*.swift") + ["../mac/metal/Sources/GnScene.swift",
                                        "../mac/metal/Sources/MetalRenderer.swift",
                                        "../mac/metal/Sources/DemoMath.swift"]
bridge = rel("../mac/metal/bridge/*.cpp")
scenes = rel("../scenes/*.cpp")
headers = rel("../mac/metal/bridge/*.h") + rel("../scenes/*.h") + ["metal/BridgingHeader.h"] + rel("lib/include/*.h")
graphs = rel("../scenes/*.json")
resources = graphs + ["../scenes/assets"]

p = Project(HERE)
for f in headers + ["metal/Info.plist", "make_xcodeproj.py", "xcodeproj_writer.py"]:
    p.file(f)

common = {
    "PRODUCT_NAME": PRODUCT,
    "PRODUCT_BUNDLE_IDENTIFIER": "com.geonodus.demo.ios",
    "INFOPLIST_FILE": "metal/Info.plist",
    "SDKROOT": "iphoneos",
    "SUPPORTED_PLATFORMS": "iphoneos iphonesimulator",
    "SUPPORTS_MACCATALYST": "NO",
    "SUPPORTS_MAC_DESIGNED_FOR_IPHONE_IPAD": "YES",
    "IPHONEOS_DEPLOYMENT_TARGET": IOS_TARGET,
    "TARGETED_DEVICE_FAMILY": "1,2",
    "SWIFT_VERSION": "5.0",
    "SWIFT_OBJC_BRIDGING_HEADER": "metal/BridgingHeader.h",
    "HEADER_SEARCH_PATHS": ["$(SRCROOT)/lib/include", "$(SRCROOT)/../scenes", "$(SRCROOT)/../mac/metal"],
    "LD_RUNPATH_SEARCH_PATHS": ["$(inherited)", "@executable_path/Frameworks"],
    "CLANG_CXX_LANGUAGE_STANDARD": "c++20",
    "CLANG_CXX_LIBRARY": "libc++",
    "CLANG_ENABLE_MODULES": "YES",
    "ALWAYS_SEARCH_USER_PATHS": "NO",
    "CODE_SIGN_STYLE": "Automatic",
    "CODE_SIGN_IDENTITY": "Apple Development",
    "DEVELOPMENT_TEAM": "",
    "ENABLE_BITCODE": "NO",
    "MARKETING_VERSION": "1.0",
    "CURRENT_PROJECT_VERSION": "1",
}
variants = {
    "Debug": {"SWIFT_OPTIMIZATION_LEVEL": "-Onone", "GCC_OPTIMIZATION_LEVEL": "0", "DEBUG_INFORMATION_FORMAT": "dwarf",
              "ONLY_ACTIVE_ARCH": "YES", "SWIFT_ACTIVE_COMPILATION_CONDITIONS": "DEBUG",
              "GCC_PREPROCESSOR_DEFINITIONS": ["DEBUG=1", "$(inherited)"], "ENABLE_TESTABILITY": "YES"},
    "Release": {"SWIFT_OPTIMIZATION_LEVEL": "-O", "SWIFT_COMPILATION_MODE": "wholemodule",
                "GCC_OPTIMIZATION_LEVEL": "s", "DEBUG_INFORMATION_FORMAT": "dwarf-with-dsym",
                "GCC_PREPROCESSOR_DEFINITIONS": ["NDEBUG=1", "$(inherited)"]},
}

p.target("GeoNodusMetalDemo", "com.apple.product-type.application", PRODUCT + ".app", "wrapper.application",
         {name: {**common, **v} for name, v in variants.items()},
         sources=swift + bridge + scenes, link=[LIB], embed=[LIB], resources=resources)
p.write(PROJECT, {name: {"SDKROOT": "iphoneos", "IPHONEOS_DEPLOYMENT_TARGET": IOS_TARGET} for name in variants})
# Run in Release: reading big scenes in Swift -Onone is ~10x slower
p.scheme(PROJECT, "GeoNodusMetalDemo", "GeoNodusMetalDemo", run_config="Release")
print("Written", os.path.relpath(PROJECT, os.getcwd()), "—", len(swift + bridge + scenes), "sources")
