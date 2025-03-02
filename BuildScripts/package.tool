#!/bin/bash

if [ "${TARGET_BUILD_DIR}" = "" ]; then
  echo "This must not be run outside of Xcode"
  exit 1
fi

cd "${TARGET_BUILD_DIR}"
archive="MacHyperVSupport-${MODULE_VERSION}-$(echo $CONFIGURATION | tr /a-z/ /A-Z/).zip"

# clean / build
if [ "$1" != "analyze" ]; then
  rm -rf package "${archive}" || exit 1
fi

if [ "$1" != "" ]; then
  echo "Got action $1, skipping!"
  exit 0
fi

mkdir -p package/Tools || exit 1
mkdir -p package/Kexts || exit 1

cd package || exit 1

# Xcode 10.2 build system seems to have a race condition between
# dependency scheduling, and for some reason does not complete
# the compilation of dependencies even though it should have been.
# Adding a delay here "fixes" it. TODO: bugreport.
if [ ! -f ../libaistat.dylib ]; then
  sleep 5
fi

cp "${PROJECT_DIR}/Tools/Daemons/hvfilecopyd/fish.goldfish64.hvfilecopyd.plist" Tools/fish.goldfish64.hvfilecopyd.plist || exit 1
if [[ -f "../hvfilecopyd-universal" ]] && [[ "../hvfilecopyd-universal" -nt "../hvfilecopyd" ]]; then
  cp ../hvfilecopyd-universal Tools/hvfilecopyd || exit 1
else
  cp ../hvfilecopyd Tools/hvfilecopyd || exit 1
fi
if [[ -f "../hvfilecopyd-tiger" ]]; then
  cp ../hvfilecopyd-tiger Tools/ || exit 1
fi

cp "${PROJECT_DIR}/Tools/Daemons/hvfilecopyd/fish.goldfish64.hvfilecopyd.plist" Tools/fish.goldfish64.hvshutdownd.plist || exit 1
if [[ -f "../hvshutdownd-universal" ]] && [[ "../hvshutdownd-universal" -nt "../hvshutdownd" ]]; then
  cp ../hvshutdownd-universal Tools/hvshutdownd || exit 1
else
  cp ../hvshutdownd Tools/hvshutdownd || exit 1
fi
if [[ -f "../hvshutdownd-tiger" ]]; then
  cp ../hvshutdownd-tiger Tools/ || exit 1
fi

cp "${PROJECT_DIR}/Tools/Daemons/hvfilecopyd/fish.goldfish64.hvfilecopyd.plist" Tools/fish.goldfish64.hvtimesyncd.plist || exit 1
if [[ -f "../hvtimesyncd-universal" ]] && [[ "../hvtimesyncd-universal" -nt "../hvtimesyncd" ]]; then
  cp ../hvtimesyncd-universal Tools/hvtimesyncd || exit 1
else
  cp ../hvtimesyncd Tools/hvtimesyncd || exit 1
fi
if [[ -f "../hvtimesyncd-tiger" ]]; then
  cp ../hvtimesyncd-tiger Tools/ || exit 1
fi

for kext in ../*.kext; do
  echo "$kext"
  cp -a "$kext" Kexts/ || exit 1
done

# Workaround Xcode 10 bug
if [ "$CONFIGURATION" = "" ]; then
  if [ "$(basename "$TARGET_BUILD_DIR")" = "Debug" ]; then
    CONFIGURATION="Debug"
  elif [ "$(basename "$TARGET_BUILD_DIR")" = "Sanitize" ]; then
    CONFIGURATION="Sanitize"
  else
    CONFIGURATION="Release"
  fi
fi

if [ "$CONFIGURATION" = "Release" ]; then
  mkdir -p dSYM || exit 1
  for dsym in ../*.dSYM; do
    if [ "$dsym" = "../*.dSYM" ]; then
      continue
    fi
    echo "$dsym"
    cp -a "$dsym" dSYM/ || exit 1
  done
fi

zip -qry -FS ../"${archive}" * || exit 1
