#!/usr/bin/env python3
#
# build-toolspkg.py
# Builds installer packages for MacHyperVSupport userspace daemons and tools.
#

import os
import plistlib
import tempfile
import shutil
import subprocess
import sys
import xml.etree.cElementTree as ET

from pathlib import Path

if os.environ.get("TARGET_BUILD_DIR") is None:
    print("This tool must not be run outside of Xcode")
    sys.exit(-1)

gBuildDir   = os.environ.get("TARGET_BUILD_DIR")
gProjDir    = os.environ.get("PROJECT_DIR")
gProjConfig = os.environ.get("CONFIGURATION")

if gProjConfig is None:
    if Path(gBuildDir).parent.name is "Debug":
        gProjConfig = "Debug"
    elif Path(gBuildDir).parent.name is "Sanitize":
        gProjConfig = "Sanitize"
    else:
        gProjConfig = "Release"

# Global package identification.
# Only support / as the install location.
gPkgIdentifierBase  = "fish.goldfish64.pkg.MacHyperVSupport."
gPkgVersion         = os.environ.get("MODULE_VERSION")
gPkgInstallLocation = "/"

gPkgBuildDir = gBuildDir + "/pkg_build"
gAppSupportDir = "Library/Application Support/MacHyperVSupport"
gLaunchDaemonsDir = "Library/LaunchDaemons"

def getDirectorySize(path):
    total = 0
    with os.scandir(path) as it:
        for entry in it:
            if entry.is_file():
                total += entry.stat().st_size
            elif entry.is_dir():
                total += getDirectorySize(entry.path)
    return total

def createDaemonPayloadLayout(pkgName, daemonPath, daemonUniversalPath, plistPath):
    print("Creating payload layout for " + pkgName)
    payloadDir              = gPkgBuildDir + "/" + pkgName
    payloadAppSupportDir    = payloadDir + "/" + gAppSupportDir
    payloadLaunchDaemonsDir = payloadDir + "/" + gLaunchDaemonsDir

    # Create payload tree.
    Path(payloadAppSupportDir).mkdir(parents=True, exist_ok=True)
    Path(payloadLaunchDaemonsDir).mkdir(parents=True, exist_ok=True)
    print("Payload layout at " + payloadDir)

    # Copy daemon binary and plist.
    if Path(daemonUniversalPath).is_file() and (os.path.getmtime(daemonUniversalPath) > os.path.getmtime(daemonPath)):
        shutil.copyfile(daemonUniversalPath, payloadAppSupportDir + "/" + os.path.basename(daemonPath))
        shutil.copymode(daemonUniversalPath, payloadAppSupportDir + "/" + os.path.basename(daemonPath))
    else:
        shutil.copyfile(daemonPath, payloadAppSupportDir + "/" + os.path.basename(daemonPath))
        shutil.copymode(daemonPath, payloadAppSupportDir + "/" + os.path.basename(daemonPath))
    print("Created " + payloadAppSupportDir + "/" + os.path.basename(daemonPath))
    shutil.copyfile(plistPath, payloadLaunchDaemonsDir + "/" + os.path.basename(plistPath))
    print("Created " + payloadLaunchDaemonsDir + "/" + os.path.basename(plistPath))

    return payloadDir

def createDaemonTigerPayloadLayout(pkgName, daemonPath, plistPath):
    print("Creating payload layout for " + pkgName)
    payloadDir              = gPkgBuildDir + "/" + pkgName
    payloadAppSupportDir    = payloadDir + "/" + gAppSupportDir
    payloadLaunchDaemonsDir = payloadDir + "/" + gLaunchDaemonsDir

    # Create payload tree.
    Path(payloadAppSupportDir).mkdir(parents=True, exist_ok=True)
    Path(payloadLaunchDaemonsDir).mkdir(parents=True, exist_ok=True)
    print("Payload layout at " + payloadDir)

    # Copy daemon binary and plist.
    shutil.copyfile(daemonPath, payloadAppSupportDir + "/" + os.path.basename(daemonPath).replace("-tiger", ""))
    shutil.copymode(daemonPath, payloadAppSupportDir + "/" + os.path.basename(daemonPath).replace("-tiger", ""))
    print("Created " + payloadAppSupportDir + "/" + os.path.basename(daemonPath))
    shutil.copyfile(plistPath, payloadLaunchDaemonsDir + "/" + os.path.basename(plistPath))
    print("Created " + payloadLaunchDaemonsDir + "/" + os.path.basename(plistPath))

    return payloadDir

def buildFlatPkg(moduleName, payloadPath, scriptsPath = None):
    pkgIdentifier = gPkgIdentifierBase + moduleName
    pkgPath       = gPkgBuildDir + "/" + moduleName + ".pkg"

    print("Building flat package " + pkgIdentifier + ", payload path: " + payloadPath)
    if scriptsPath is None:
        result = subprocess.run(["pkgbuild",
                                "--root", payloadPath,
                                "--identifier", pkgIdentifier,
                                "--version", gPkgVersion,
                                "--install-location", gPkgInstallLocation,
                                pkgPath],
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    else:
        result = subprocess.run(["pkgbuild",
                                "--root", payloadPath,
                                "--identifier", pkgIdentifier,
                                "--version", gPkgVersion,
                                "--install-location", gPkgInstallLocation,
                                "--scripts", scriptsPath,
                                pkgPath],
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if result.returncode != 0:
        print("Failed to build flat package: " + result.stderr)
        sys.exit(-1)

    print("Flat package built at " + pkgPath)
    return pkgPath, pkgIdentifier

def buildBundlePkg(moduleName, moduleFriendlyName, payloadPath, postInstallScriptPath = None):
    pkgIdentifier       = gPkgIdentifierBase + moduleName
    pkgPath             = gPkgBuildDir + "/" + moduleName + ".pkg"
    pkgContentsPath     = pkgPath + "/Contents"
    pkgResourcesPath    = pkgContentsPath + "/Resources"
    pkgEnglishProjPath  = pkgResourcesPath + "/English.lproj"

    print("Building bundle package " + pkgIdentifier + ", payload path: " + payloadPath)
    pkgTempFile, pkgTempPath = tempfile.mkstemp()
    pkgTempPath = Path(pkgTempPath)
    os.close(pkgTempFile)

    # Build flat package.
    print("Building temporary flat package to " + str(pkgTempPath))
    result = subprocess.run(["pkgbuild",
                        "--root", payloadPath,
                        "--identifier", pkgIdentifier,
                        "--version", gPkgVersion,
                        "--install-location", gPkgInstallLocation,
                        pkgTempPath],
                        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if result.returncode != 0:
        print("Failed to build package: " + result.stderr)
        pkgTempPath.unlink()
        sys.exit(-1)

    # Expand built package.
    pkgTempExpandPath = Path(tempfile.mkdtemp())
    if pkgTempExpandPath.exists():
        shutil.rmtree(pkgTempExpandPath)

    print("Expanding temporary package to " + str(pkgTempExpandPath))
    result = subprocess.run(["pkgutil", "--expand", pkgTempPath, pkgTempExpandPath],
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    pkgTempPath.unlink()
    if result.returncode != 0:
        print("Failed to expand package: " + result.stderr)
        pkgTempExpandPath.unlink()
        sys.exit(-1)

    # Create bundle package structure.
    Path(pkgContentsPath).mkdir(parents=True, exist_ok=True)
    Path(pkgResourcesPath).mkdir(parents=True, exist_ok=True)
    Path(pkgEnglishProjPath).mkdir(parents=True, exist_ok=True)

    # Copy BOM and archive from previously expanded package.
    shutil.copyfile(str(pkgTempExpandPath) + "/Bom", pkgContentsPath + "/Archive.bom")
    shutil.copyfile(str(pkgTempExpandPath) + "/Payload", pkgContentsPath + "/Archive.pax.gz")
    shutil.rmtree(pkgTempExpandPath)

    # Create Info.plist.
    infoPlist = {
        "CFBundleIdentifier" : pkgIdentifier,
        "CFBundleShortVersionString" : gPkgVersion,
        "IFPkgFlagAllowBackRev" : False,
        "IFPkgFlagAuthorizationAction" : "RootAuthorization",
        "IFPkgFlagDefaultLocation" : gPkgInstallLocation,
        "IFPkgFlagFollowLinks" : True,
        "IFPkgFlagInstallFat" : False,
        "IFPkgFlagIsRequired" : True,
        "IFPkgFlagOverwritePermissions" : False,
        "IFPkgFlagRelocatable" : False,
        "IFPkgFlagRestartAction" : "NoRestart",
        "IFPkgFlagRootVolumeOnly" : True,
        "IFPkgFlagUpdateInstalledLanguages" : False,
        "IFPkgFormatVersion" : 0.1
    }
    infoPlistPath = Path(pkgContentsPath + "/Info.plist").open("wb")
    plistlib.dump(infoPlist, infoPlistPath)

    # Create Description.plist.
    descriptionPlist = {
        "IFPkgDescriptionDescription" : "This package contains " + moduleFriendlyName + " " + gPkgVersion + ".",
        "IFPkgDescriptionTitle" : moduleFriendlyName,
        "IFPkgDescriptionVersion" : gPkgVersion
    }
    descriptionPlistPath = Path(pkgEnglishProjPath + "/Description.plist").open("wb")
    plistlib.dump(descriptionPlist, descriptionPlistPath)

    # Copy postinstall script if present.
    if postInstallScriptPath is not None:
        shutil.copyfile(postInstallScriptPath, pkgResourcesPath + "/postinstall")
        shutil.copymode(postInstallScriptPath, pkgResourcesPath + "/postinstall")

    print("Bundle package built at " + pkgPath)
    return pkgPath, pkgIdentifier
    
# Create package scratch directory.
if os.path.exists(gPkgBuildDir):
    shutil.rmtree(gPkgBuildDir)
os.makedirs(gPkgBuildDir)

moduleFileCopyTool          = "FileCopyTool"
moduleFileCopyToolTiger     = moduleFileCopyTool + "Tiger"
moduleFileCopyToolFriendly  = "Hyper-V File Copy Daemon"
moduleShutdownTool          = "ShutdownTool"
moduleShutdownToolTiger     = moduleShutdownTool + "Tiger"
moduleShutdownToolFriendly  = "Hyper-V Shutdown Daemon"
moduleSnapshotTool          = "SnapshotTool"
moduleSnapshotToolTiger     = moduleSnapshotTool + "Tiger"
moduleSnapshotToolFriendly  = "Hyper-V Snapshot Daemon"
moduleTimeSyncTool          = "TimeSyncTool"
moduleTimeSyncToolTiger     = moduleTimeSyncTool + "Tiger"
moduleTimeSyncToolFriendly  = "Hyper-V Time Synchronization Daemon"

# Build hvfilecopyd package.
fileCopyPath = createDaemonPayloadLayout(moduleFileCopyTool, gBuildDir + "/hvfilecopyd", gBuildDir + "/hvfilecopyd-universal",
                                         gProjDir + "/Tools/Daemons/hvfilecopyd/fish.goldfish64.hvfilecopyd.plist")
fileCopyPkg, fileCopyPkgId = buildFlatPkg(moduleFileCopyTool, fileCopyPath, gProjDir + "/Tools/Daemons/hvfilecopyd")

# Build hvfilecopyd package for Tiger.
if Path(gBuildDir + "/hvfilecopyd-tiger").is_file():
    fileCopyTigerPath = createDaemonTigerPayloadLayout(moduleFileCopyToolTiger, gBuildDir + "/hvfilecopyd-tiger",
                                                       gProjDir + "/Tools/Daemons/hvfilecopyd/fish.goldfish64.hvfilecopyd.plist")
    fileCopyTigerPkg, fileCopyTigerPkgId = buildBundlePkg(moduleFileCopyToolTiger, moduleFileCopyToolFriendly,
                                                          fileCopyTigerPath, gProjDir + "/Tools/Daemons/hvfilecopyd/postinstall")

# Build hvshutdownd package.
shutdownPath = createDaemonPayloadLayout(moduleShutdownTool, gBuildDir + "/hvshutdownd", gBuildDir + "/hvshutdownd-universal",
                                         gProjDir + "/Tools/Daemons/hvshutdownd/fish.goldfish64.hvshutdownd.plist")
shutdownPkg, shutdownPkgId = buildFlatPkg(moduleShutdownTool, shutdownPath, gProjDir + "/Tools/Daemons/hvshutdownd")

# Build hvshutdownd package for Tiger.
if Path(gBuildDir + "/hvshutdownd-tiger").is_file():
    shutdownTigerPath = createDaemonTigerPayloadLayout(moduleShutdownToolTiger, gBuildDir + "/hvshutdownd-tiger",
                                                       gProjDir + "/Tools/Daemons/hvshutdownd/fish.goldfish64.hvshutdownd.plist")
    shutdownTigerPkg, shutdownTigerPkgId = buildBundlePkg(moduleShutdownToolTiger, moduleShutdownToolFriendly,
                                                          shutdownTigerPath, gProjDir + "/Tools/Daemons/hvshutdownd/postinstall")
                                                          
# Build hvsnapshotd package.
snapshotPath = createDaemonPayloadLayout(moduleSnapshotTool, gBuildDir + "/hvsnapshotd", gBuildDir + "/hvsnapshotd-universal",
                                         gProjDir + "/Tools/Daemons/hvsnapshotd/fish.goldfish64.hvsnapshotd.plist")
snapshotPkg, snapshotPkgId = buildFlatPkg(moduleSnapshotTool, snapshotPath, gProjDir + "/Tools/Daemons/hvsnapshotd")

# Build hvsnapshotd package for Tiger.
if Path(gBuildDir + "/hvsnapshotd-tiger").is_file():
    snapshotTigerPath = createDaemonTigerPayloadLayout(moduleSnapshotToolTiger, gBuildDir + "/hvsnapshotd-tiger",
                                                       gProjDir + "/Tools/Daemons/hvsnapshotd/fish.goldfish64.hvsnapshotd.plist")
    snapshotTigerPkg, snapshotTigerPkgId = buildBundlePkg(moduleSnapshotToolTiger, moduleSnapshotToolFriendly,
                                                          snapshotTigerPath, gProjDir + "/Tools/Daemons/hvsnapshotd/postinstall")

# Build hvtimesyncd package.
timeSyncPath = createDaemonPayloadLayout(moduleTimeSyncTool, gBuildDir + "/hvtimesyncd", gBuildDir + "/hvtimesyncd-universal",
                                         gProjDir + "/Tools/Daemons/hvtimesyncd/fish.goldfish64.hvtimesyncd.plist")
timeSyncPkg, timeSyncPkgId = buildFlatPkg(moduleTimeSyncTool, timeSyncPath, gProjDir + "/Tools/Daemons/hvtimesyncd")

# Build hvtimesyncd package for Tiger.
if Path(gBuildDir + "/hvtimesyncd-tiger").is_file():
    timeSyncTigerPath = createDaemonTigerPayloadLayout(moduleTimeSyncToolTiger, gBuildDir + "/hvtimesyncd-tiger",
                                                       gProjDir + "/Tools/Daemons/hvtimesyncd/fish.goldfish64.hvtimesyncd.plist")
    timeSyncTigerPkg, timeSyncTigerPkgId = buildBundlePkg(moduleTimeSyncToolTiger, moduleTimeSyncToolFriendly,
                                                          timeSyncTigerPath, gProjDir + "/Tools/Daemons/hvtimesyncd/postinstall")

# Assemble mpkg
mpkgName            = "MacHyperVSupportTools"
mpkgFileName        = mpkgName + "-" + gPkgVersion + "-" + gProjConfig + ".mpkg"
mpkgIdentifier      = "fish.goldfish64.mpkg." + mpkgName
mpkgPath            = gPkgBuildDir + "/" + mpkgFileName
mpkgContentsPath    = mpkgPath + "/Contents"
mpkgPackagesPath    = mpkgContentsPath + "/Packages"
mpkgResourcesPath   = mpkgContentsPath + "/Resources"
mpkgEnglishProjPath = mpkgResourcesPath + "/English.lproj"

print("Building mpkg at ", mpkgPath)
Path(mpkgContentsPath).mkdir(parents=True, exist_ok=True)
Path(mpkgPackagesPath).mkdir(parents=True, exist_ok=True)
Path(mpkgResourcesPath).mkdir(parents=True, exist_ok=True)
Path(mpkgEnglishProjPath).mkdir(parents=True, exist_ok=True)

# Copy component packages.
shutil.copyfile(fileCopyPkg, mpkgPackagesPath + "/" + os.path.basename(fileCopyPkg))
if fileCopyTigerPkg is not None:
    shutil.copytree(fileCopyTigerPkg, mpkgPackagesPath + "/" + os.path.basename(fileCopyTigerPkg), dirs_exist_ok=True)
shutil.copyfile(shutdownPkg, mpkgPackagesPath + "/" + os.path.basename(shutdownPkg))
if shutdownTigerPkg is not None:
    shutil.copytree(shutdownTigerPkg, mpkgPackagesPath + "/" + os.path.basename(shutdownTigerPkg), dirs_exist_ok=True)
shutil.copyfile(snapshotPkg, mpkgPackagesPath + "/" + os.path.basename(snapshotPkg))
if snapshotTigerPkg is not None:
    shutil.copytree(snapshotTigerPkg, mpkgPackagesPath + "/" + os.path.basename(snapshotTigerPkg), dirs_exist_ok=True)
shutil.copyfile(timeSyncPkg, mpkgPackagesPath + "/" + os.path.basename(timeSyncPkg))
if timeSyncTigerPkg is not None:
    shutil.copytree(timeSyncTigerPkg, mpkgPackagesPath + "/" + os.path.basename(timeSyncTigerPkg), dirs_exist_ok=True)

# Copy license and strings.
shutil.copyfile(gProjDir + "/LICENSE.txt", mpkgEnglishProjPath + "/License.txt")
shutil.copyfile(gProjDir + "/Tools/package/Localizable_EN.strings", mpkgEnglishProjPath + "/Localizable.strings")

# Get scripts JS.
mpkgScriptsJSFile = open(gProjDir + "/Tools/package/scripts.js")
mpkgScriptsJS = mpkgScriptsJSFile.read()
mpkgScriptsJSFile.close()

# Create distribution XML.
distInstallerGuiScript = ET.Element("installer-gui-script", { "minSpecVersion" : "1" })

ET.SubElement(distInstallerGuiScript, "title").text = "MacHyperVSupportTools_title"
ET.SubElement(distInstallerGuiScript, "license", { "file" : "License.txt" })
ET.SubElement(distInstallerGuiScript, "options", {
    "allow-external-scripts" : "no",
    "customize"              : "allow",
    "rootVolumeOnly"         : "true"
    })
ET.SubElement(distInstallerGuiScript, "script").text = mpkgScriptsJS
ET.SubElement(distInstallerGuiScript, "installation-check", { "script" : "installCheckScript()" })

# Choices outline.
distChoices = ET.SubElement(distInstallerGuiScript, "choices-outline")

ET.SubElement(distChoices, "line", { "choice": moduleFileCopyTool })
if fileCopyTigerPkg is not None:
    ET.SubElement(distChoices, "line", { "choice": moduleFileCopyToolTiger })

ET.SubElement(distChoices, "line", { "choice": moduleShutdownTool })
if shutdownTigerPkg is not None:
    ET.SubElement(distChoices, "line", { "choice": moduleShutdownToolTiger })

ET.SubElement(distChoices, "line", { "choice": moduleSnapshotTool })
if snapshotTigerPkg is not None:
    ET.SubElement(distChoices, "line", { "choice": moduleSnapshotToolTiger })

ET.SubElement(distChoices, "line", { "choice": moduleTimeSyncTool })
if timeSyncTigerPkg is not None:
    ET.SubElement(distChoices, "line", { "choice": moduleTimeSyncToolTiger })

# Choices list.
distChoiceFileCopy = ET.SubElement(distInstallerGuiScript, "choice", {
    "id" : moduleFileCopyTool,
    "title" : moduleFileCopyTool + "_title",
    "description" : moduleFileCopyTool + "_description",
    "start_selected" : "!checkIfTiger()",
    "visible" : "!checkIfTiger()",
})
ET.SubElement(distChoiceFileCopy, "pkg-ref", { "id" : fileCopyPkgId })
if fileCopyTigerPkg is not None:
    distChoiceFileCopyTiger = ET.SubElement(distInstallerGuiScript, "choice", {
        "id" : moduleFileCopyToolTiger,
        "title" : moduleFileCopyToolTiger + "_title",
        "description" : moduleFileCopyToolTiger + "_description",
        "start_selected" : "checkIfTiger()",
        "visible" : "checkIfTiger()",
    })
    ET.SubElement(distChoiceFileCopyTiger, "pkg-ref", { "id" : fileCopyTigerPkgId })

distChoiceShutdown = ET.SubElement(distInstallerGuiScript, "choice", {
    "id" : moduleShutdownTool,
    "title" : moduleShutdownTool + "_title",
    "description" : moduleShutdownTool + "_description",
    "start_selected" : "!checkIfTiger()",
    "visible" : "!checkIfTiger()",
})
ET.SubElement(distChoiceShutdown, "pkg-ref", { "id" : shutdownPkgId })
if shutdownTigerPkg is not None:
    distChoiceShutdownTiger = ET.SubElement(distInstallerGuiScript, "choice", {
        "id" : moduleShutdownToolTiger,
        "title" : moduleShutdownToolTiger + "_title",
        "description" : moduleShutdownToolTiger + "_description",
        "start_selected" : "checkIfTiger()",
        "visible" : "checkIfTiger()",
    })
    ET.SubElement(distChoiceShutdownTiger, "pkg-ref", { "id" : shutdownTigerPkgId })

distChoiceSnapshot = ET.SubElement(distInstallerGuiScript, "choice", {
    "id" : moduleSnapshotTool,
    "title" : moduleSnapshotTool + "_title",
    "description" : moduleSnapshotTool + "_description",
    "start_selected" : "!checkIfTiger()",
    "visible" : "!checkIfTiger()",
})
ET.SubElement(distChoiceSnapshot, "pkg-ref", { "id" : snapshotPkgId })
if snapshotTigerPkg is not None:
    distChoiceSnapshotTiger = ET.SubElement(distInstallerGuiScript, "choice", {
        "id" : moduleSnapshotToolTiger,
        "title" : moduleSnapshotToolTiger + "_title",
        "description" : moduleSnapshotToolTiger + "_description",
        "start_selected" : "checkIfTiger()",
        "visible" : "checkIfTiger()",
    })
    ET.SubElement(distChoiceSnapshotTiger, "pkg-ref", { "id" : snapshotTigerPkgId })

distChoiceTimeSync = ET.SubElement(distInstallerGuiScript, "choice", {
    "id" : moduleTimeSyncTool,
    "title" : moduleTimeSyncTool + "_title",
    "description" : moduleTimeSyncTool + "_description",
    "start_selected" : "!checkIfTiger()",
    "visible" : "!checkIfTiger()",
})
ET.SubElement(distChoiceTimeSync, "pkg-ref", { "id" : timeSyncPkgId })
if timeSyncTigerPkg is not None:
    distChoiceTimeSyncTiger = ET.SubElement(distInstallerGuiScript, "choice", {
        "id" : moduleTimeSyncToolTiger,
        "title" : moduleTimeSyncToolTiger + "_title",
        "description" : moduleTimeSyncToolTiger + "_description",
        "start_selected" : "checkIfTiger()",
        "visible" : "checkIfTiger()",
    })
    ET.SubElement(distChoiceTimeSyncTiger, "pkg-ref", { "id" : timeSyncTigerPkgId })

# Create package references.
ET.SubElement(distInstallerGuiScript, "pkg-ref", {
    "id" : fileCopyPkgId,
    "auth" : "root",
    "installKBytes" : str(round(getDirectorySize(fileCopyPath) / 1000))
}).text = "file:./Contents/Packages/" + os.path.basename(fileCopyPkg)
if fileCopyTigerPkg is not None:
    ET.SubElement(distInstallerGuiScript, "pkg-ref", {
        "id" : fileCopyTigerPkgId,
        "auth" : "root",
        "installKBytes" : str(round(getDirectorySize(fileCopyTigerPath) / 1000))
    }).text = "file:./Contents/Packages/" + os.path.basename(fileCopyTigerPkg)

ET.SubElement(distInstallerGuiScript, "pkg-ref", {
    "id" : shutdownPkgId,
    "auth" : "root",
    "installKBytes" : str(round(getDirectorySize(shutdownPath) / 1000))
}).text = "file:./Contents/Packages/" + os.path.basename(shutdownPkg)
if shutdownTigerPkg is not None:
    ET.SubElement(distInstallerGuiScript, "pkg-ref", {
        "id" : shutdownTigerPkgId,
        "auth" : "root",
        "installKBytes" : str(round(getDirectorySize(shutdownTigerPath) / 1000))
    }).text = "file:./Contents/Packages/" + os.path.basename(shutdownTigerPkg)

ET.SubElement(distInstallerGuiScript, "pkg-ref", {
    "id" : snapshotPkgId,
    "auth" : "root",
    "installKBytes" : str(round(getDirectorySize(snapshotPath) / 1000))
}).text = "file:./Contents/Packages/" + os.path.basename(snapshotPkg)
if snapshotTigerPkg is not None:
    ET.SubElement(distInstallerGuiScript, "pkg-ref", {
        "id" : snapshotTigerPkgId,
        "auth" : "root",
        "installKBytes" : str(round(getDirectorySize(snapshotTigerPath) / 1000))
    }).text = "file:./Contents/Packages/" + os.path.basename(snapshotTigerPkg)

ET.SubElement(distInstallerGuiScript, "pkg-ref", {
    "id" : timeSyncPkgId,
    "auth" : "root",
    "installKBytes" : str(round(getDirectorySize(timeSyncPath) / 1000))
}).text = "file:./Contents/Packages/" + os.path.basename(timeSyncPkg)
if timeSyncTigerPkg is not None:
    ET.SubElement(distInstallerGuiScript, "pkg-ref", {
        "id" : timeSyncTigerPkgId,
        "auth" : "root",
        "installKBytes" : str(round(getDirectorySize(timeSyncTigerPath) / 1000))
    }).text = "file:./Contents/Packages/" + os.path.basename(timeSyncTigerPkg)

# Write out distribution XML.
distTree = ET.ElementTree(distInstallerGuiScript)
ET.indent(distTree, space="\t", level=0)
distTree.write(mpkgContentsPath + "/distribution.dist", xml_declaration=True, encoding="utf-8")

# Compress mpkg and copy to main build folder.
shutil.make_archive(gBuildDir + "/" + mpkgFileName, "zip", gPkgBuildDir, mpkgFileName)
