#!/bin/bash

abort() {
  echo "ERROR: $1!"
  exit 1
}

# Get Xcode SDK directory
OSX_PLAT=`xcrun --sdk macosx --show-sdk-platform-path`

# Lower minimum SDK version
sudo plutil -replace MinimumSDKVersion -string 10.6 "$OSX_PLAT"/Info.plist

# Grab macOS 10.13 SDK and extract
wget -P /tmp https://github.com/acidanthera/ocbuild/releases/download/macos13-sdk/Xcode1013SDK.tar.gz
sudo tar -xf /tmp/Xcode1013SDK.tar.gz -C "$OSX_PLAT"/Developer
rm /tmp/Xcode1013SDK.tar.gz

xcrun --sdk macosx10.13 --show-sdk-path
