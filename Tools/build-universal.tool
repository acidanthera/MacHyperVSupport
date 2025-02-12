#!/bin/sh
#
#  build-universal.tool
#  Combines x86_64 and i386 binaries into one universal binary.
#
# Usage: build-universal.tool "x86_64 slice" "i386 slice" "output"
# 

if [ "${TARGET_BUILD_DIR}" = "" ]; then
  echo "This must not be run outside of Xcode"
  exit 1
fi

cd "${TARGET_BUILD_DIR}"

# Xcode 10.2 build system seems to have a race condition between
# dependency scheduling, and for some reason does not complete
# the compilation of dependencies even though it should have been.
# Adding a delay here "fixes" it. TODO: bugreport.
if [ ! -f libaistat.dylib ]; then
  sleep 5
fi

lipo -create -arch x86_64 "$1" -arch i386 "$2" -output "$3"
