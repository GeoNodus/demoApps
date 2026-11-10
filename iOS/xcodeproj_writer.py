"""Writing project.pbxproj (the OpenStep plist format) and shared Xcode schemes for make_xcodeproj.py of the iOS demo
(a copy of geonodus/ios/xcodeproj_writer.py of the library repository, which generates the library and test projects).

Object ids are stable (md5 of a key), so the project file does not change between runs. File paths are relative to the
project folder (sourceTree = SOURCE_ROOT): the project has no absolute paths.
"""
import hashlib
import os

FILE_TYPES = {
    ".swift": "sourcecode.swift",
    ".cpp": "sourcecode.cpp.cpp",
    ".mm": "sourcecode.cpp.objcpp",
    ".m": "sourcecode.c.objc",
    ".c": "sourcecode.c.c",
    ".h": "sourcecode.c.h",
    ".hpp": "sourcecode.cpp.h",
    ".plist": "text.plist.xml",
    ".modulemap": "sourcecode.module-map",
    ".json": "text.json",
    ".gng": "text.json",
    ".obj": "text",
    ".txt": "text",
    ".a": "archive.ar",
    ".xcframework": "wrapper.xcframework",
    ".framework": "wrapper.framework",
    ".storyboard": "file.storyboard",
    ".xcassets": "folder.assetcatalog",
}


def oid(key):
    return hashlib.md5(key.encode()).hexdigest()[:24].upper()


def q(s):
    s = str(s)
    if s and all(c.isalnum() or c in "._/$" for c in s):
        return s
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n") + '"'


def value(v):
    if isinstance(v, (list, tuple)):
        return "(" + "".join(f"{q(x)}, " for x in v) + ")"
    return q(v)


class Project:
    def __init__(self, root):
        self.root = root
        self.objs = {}
        self.refs = {}
        self.groups = {}
        self.targets = []
        self.products = []
        self.main_children = []

    def add(self, key, isa, body):
        self.objs[oid(key)] = (isa, f"isa = {isa}; " + body)
        return oid(key)

    # --- files and groups: the group tree follows the folders relative to the project root
    def group(self, path):
        if path in self.groups:
            return self.groups[path]
        gid = oid("group:" + path)
        self.groups[path] = [gid, [], path]
        parent = os.path.dirname(path)
        if parent:
            self.group(parent)[1].append(gid)
        else:
            self.main_children.append(gid)
        return self.groups[path]

    def file(self, path, group=None):
        """path — relative to the project root; group — the navigator group (by default — the folders of the path)."""
        if path in self.refs:
            return self.refs[path]
        ext = os.path.splitext(path)[1]
        ftype = FILE_TYPES.get(ext, "text")
        if os.path.isdir(os.path.join(self.root, path)) and ext not in (".xcframework", ".framework", ".xcassets"):
            ftype = "folder"  # a folder reference: copied into the resources as a whole
        rid = self.add("ref:" + path, "PBXFileReference",
                       f"lastKnownFileType = {ftype}; name = {q(os.path.basename(path))}; path = {q(path)}; "
                       "sourceTree = SOURCE_ROOT;")
        self.refs[path] = rid
        gpath = group if group is not None else self.display_dir(path)
        if gpath:
            self.group(gpath)[1].append(rid)
        else:
            self.main_children.append(rid)
        return rid

    @staticmethod
    def display_dir(path):
        parts = [p for p in os.path.dirname(path).split("/") if p not in ("..", ".")]
        return "/".join(parts)

    def build_file(self, key, path, settings=None):
        body = f"fileRef = {self.file(path)};"
        if settings:
            body += " settings = {" + settings + "};"
        return self.add(f"build:{key}:{path}", "PBXBuildFile", body)

    # --- targets
    def config_list(self, key, configs):
        ids = []
        for name, settings in configs.items():
            body = " ".join(f"{q(k)} = {value(v)};" for k, v in settings.items())
            ids.append(self.add(f"config:{key}:{name}", "XCBuildConfiguration",
                                f"buildSettings = {{{body}}}; name = {name};"))
        default = "Release" if "Release" in configs else list(configs)[-1]
        return self.add("configlist:" + key, "XCConfigurationList",
                        "buildConfigurations = (" + "".join(f"{i}, " for i in ids)
                        + f"); defaultConfigurationIsVisible = 0; defaultConfigurationName = {default};")

    def target(self, name, product_type, product_file, explicit_type, configs, sources=(), headers=(),
               link=(), embed=(), resources=(), deps=(), scripts=()):
        """sources — compiled files (or (path, compiler flags)); headers — (path, public); link — paths or
        ('target', product id); embed — xcframeworks / frameworks for Embed Frameworks; deps — target ids it depends on."""
        product = self.add("product:" + name, "PBXFileReference",
                           f"explicitFileType = {explicit_type}; includeInIndex = 0; path = {q(product_file)}; "
                           "sourceTree = BUILT_PRODUCTS_DIR;")
        self.products.append(product)
        phases = []
        for i, (sname, script) in enumerate(scripts):
            phases.append(self.add(f"phase:{name}:script{i}", "PBXShellScriptBuildPhase",
                                   "buildActionMask = 2147483647; files = (); inputPaths = (); "
                                   f"name = {q(sname)}; outputPaths = (); runOnlyForDeploymentPostprocessing = 0; "
                                   f"shellPath = /bin/sh; shellScript = {q(script)}; alwaysOutOfDate = 1;"))
        if headers:
            files = [self.build_file(name, p, "ATTRIBUTES = (Public, );" if pub else None) for p, pub in headers]
            phases.append(self.add(f"phase:{name}:headers", "PBXHeadersBuildPhase",
                                   "buildActionMask = 2147483647; files = (" + "".join(f"{f}, " for f in files)
                                   + "); runOnlyForDeploymentPostprocessing = 0;"))
        # an item of sources — a path or (path, compiler flags of this file)
        files = [self.build_file(name, p[0], f"COMPILER_FLAGS = {q(p[1])};") if isinstance(p, tuple)
                 else self.build_file(name, p) for p in sources]
        phases.append(self.add(f"phase:{name}:sources", "PBXSourcesBuildPhase",
                               "buildActionMask = 2147483647; files = (" + "".join(f"{f}, " for f in files)
                               + "); runOnlyForDeploymentPostprocessing = 0;"))
        lfiles = []
        for item in link:
            if isinstance(item, tuple):
                lfiles.append(self.add(f"build:{name}:link:{item[1]}", "PBXBuildFile", f"fileRef = {item[1]};"))
            else:
                lfiles.append(self.build_file(name + ":link", item))
        phases.append(self.add(f"phase:{name}:frameworks", "PBXFrameworksBuildPhase",
                               "buildActionMask = 2147483647; files = (" + "".join(f"{f}, " for f in lfiles)
                               + "); runOnlyForDeploymentPostprocessing = 0;"))
        if resources:
            rfiles = [self.build_file(name + ":res", p) for p in resources]
            phases.append(self.add(f"phase:{name}:resources", "PBXResourcesBuildPhase",
                                   "buildActionMask = 2147483647; files = (" + "".join(f"{f}, " for f in rfiles)
                                   + "); runOnlyForDeploymentPostprocessing = 0;"))
        if embed:
            efiles = [self.build_file(name + ":embed", p, "ATTRIBUTES = (CodeSignOnCopy, RemoveHeadersOnCopy, );")
                      for p in embed]
            phases.append(self.add(f"phase:{name}:embed", "PBXCopyFilesBuildPhase",
                                   "buildActionMask = 2147483647; dstPath = \"\"; dstSubfolderSpec = 10; files = ("
                                   + "".join(f"{f}, " for f in efiles)
                                   + "); name = \"Embed Frameworks\"; runOnlyForDeploymentPostprocessing = 0;"))
        dep_ids = []
        tid = oid("target:" + name)
        for d in deps:
            proxy = self.add(f"proxy:{name}:{d}", "PBXContainerItemProxy",
                             f"containerPortal = {oid('project')}; proxyType = 1; remoteGlobalIDString = {d}; "
                             "remoteInfo = dep;")
            dep_ids.append(self.add(f"dep:{name}:{d}", "PBXTargetDependency", f"target = {d}; targetProxy = {proxy};"))
        clist = self.config_list("target:" + name, configs)
        self.objs[tid] = ("PBXNativeTarget",
                          f"isa = PBXNativeTarget; buildConfigurationList = {clist}; buildPhases = ("
                          + "".join(f"{p}, " for p in phases) + "); buildRules = (); dependencies = ("
                          + "".join(f"{d}, " for d in dep_ids) + f"); name = {q(name)}; productName = {q(name)}; "
                          f"productReference = {product}; productType = {q(product_type)};")
        self.targets.append((name, tid, product, product_file))
        return tid, product

    def write(self, path, project_configs, attributes=""):
        for gpath, (gid, children, _) in self.groups.items():
            self.objs[gid] = ("PBXGroup", "isa = PBXGroup; children = (" + "".join(f"{c}, " for c in children)
                              + f"); name = {q(os.path.basename(gpath))}; sourceTree = \"<group>\";")
        products_group = self.add("group:Products", "PBXGroup",
                                  "children = (" + "".join(f"{p}, " for p in self.products)
                                  + "); name = Products; sourceTree = \"<group>\";")
        main_group = self.add("group:main", "PBXGroup",
                              "children = (" + "".join(f"{g}, " for g in self.main_children + [products_group])
                              + "); sourceTree = \"<group>\";")
        clist = self.config_list("project", project_configs)
        targets = "".join(f"{t[1]}, " for t in self.targets)
        self.objs[oid("project")] = (
            "PBXProject",
            "isa = PBXProject; attributes = {BuildIndependentTargetsInParallel = 1; LastUpgradeCheck = 1600; "
            f"{attributes}}}; buildConfigurationList = {clist}; compatibilityVersion = \"Xcode 14.0\"; "
            "developmentRegion = en; hasScannedForEncodings = 0; knownRegions = (en, Base, ); "
            f"mainGroup = {main_group}; productRefGroup = {products_group}; projectDirPath = \"\"; "
            f"projectRoot = \"\"; targets = ({targets});")
        lines = ["// !$*UTF8*$!", "{", "\tarchiveVersion = 1;", "\tclasses = {", "\t};", "\tobjectVersion = 56;",
                 "\tobjects = {"]
        for isa in sorted({i for i, _ in self.objs.values()}):
            lines.append(f"\n/* Begin {isa} section */")
            for k in sorted(self.objs):
                if self.objs[k][0] == isa:
                    lines.append(f"\t\t{k} = {{{self.objs[k][1]}}};")
            lines.append(f"/* End {isa} section */")
        lines += ["\t};", f"\trootObject = {oid('project')};", "}", ""]
        os.makedirs(path, exist_ok=True)
        with open(os.path.join(path, "project.pbxproj"), "w") as f:
            f.write("\n".join(lines))

    def scheme(self, project_path, name, target_name, run_config="Debug", test_target=None, launch=True,
               archive_config="Release"):
        """A shared scheme (xcshareddata): building the target, running it (an app) and testing (an XCTest target)."""
        proj = os.path.basename(project_path)

        def ref(tname):
            t = next(t for t in self.targets if t[0] == tname)
            return (f'<BuildableReference BuildableIdentifier = "primary" BlueprintIdentifier = "{t[1]}" '
                    f'BuildableName = "{t[3]}" BlueprintName = "{tname}" ReferencedContainer = "container:{proj}">'
                    "</BuildableReference>")

        testables = ""
        if test_target:
            testables = f'<TestableReference skipped = "NO">{ref(test_target)}</TestableReference>'
        runnable = f'<BuildableProductRunnable runnableDebuggingMode = "0">{ref(target_name)}</BuildableProductRunnable>'
        xml = f'''<?xml version="1.0" encoding="UTF-8"?>
<Scheme LastUpgradeVersion = "1600" version = "1.7">
   <BuildAction parallelizeBuildables = "YES" buildImplicitDependencies = "YES">
      <BuildActionEntries>
         <BuildActionEntry buildForTesting = "YES" buildForRunning = "YES" buildForProfiling = "YES" buildForArchiving = "YES" buildForAnalyzing = "YES">
            {ref(target_name)}
         </BuildActionEntry>
      </BuildActionEntries>
   </BuildAction>
   <TestAction buildConfiguration = "{run_config}" selectedDebuggerIdentifier = "Xcode.DebuggerFoundation.Debugger.LLDB" selectedLauncherIdentifier = "Xcode.DebuggerFoundation.Launcher.LLDB" shouldUseLaunchSchemeArgsEnv = "YES">
      <Testables>{testables}</Testables>
   </TestAction>
   <LaunchAction buildConfiguration = "{run_config}" selectedDebuggerIdentifier = "Xcode.DebuggerFoundation.Debugger.LLDB" selectedLauncherIdentifier = "Xcode.DebuggerFoundation.Launcher.LLDB" launchStyle = "0" useCustomWorkingDirectory = "NO" ignoresPersistentStateOnLaunch = "NO" debugDocumentVersioning = "YES" debugServiceExtension = "internal" allowLocationSimulation = "YES">
      {runnable if launch else ""}
   </LaunchAction>
   <ProfileAction buildConfiguration = "Release" shouldUseLaunchSchemeArgsEnv = "YES" savedToolIdentifier = "" useCustomWorkingDirectory = "NO" debugDocumentVersioning = "YES">
      {runnable if launch else ""}
   </ProfileAction>
   <AnalyzeAction buildConfiguration = "{run_config}"></AnalyzeAction>
   <ArchiveAction buildConfiguration = "{archive_config}" revealArchiveInOrganizer = "YES"></ArchiveAction>
</Scheme>
'''
        d = os.path.join(project_path, "xcshareddata", "xcschemes")
        os.makedirs(d, exist_ok=True)
        with open(os.path.join(d, name + ".xcscheme"), "w") as f:
            f.write(xml)
