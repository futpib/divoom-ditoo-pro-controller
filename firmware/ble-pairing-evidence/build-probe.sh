#!/usr/bin/env bash
set -euo pipefail

# A temporary diagnostic APK, pinned to the device and firmware in this record.
sdk_root=${1:?Usage: build-probe.sh ANDROID_SDK_ROOT}
evidence_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(cd -- "$evidence_dir/../.." && pwd)
build_dir="$repo_root/target/tv-pairing-probe"
android_jar="$sdk_root/platforms/android-34/android.jar"
build_tools="$sdk_root/build-tools/35.0.0"
mkdir -p "$build_dir/classes" "$build_dir/dex"
javac -source 8 -target 8 -classpath "$android_jar" \
    -d "$build_dir/classes" "$evidence_dir"/probe/*.java
"$build_tools/d8" --lib "$android_jar" --output "$build_dir/dex" \
    "$build_dir"/classes/dev/ditoo/tvprobe/*.class
"$build_tools/aapt2" link -I "$android_jar" \
    --manifest "$evidence_dir/probe/AndroidManifest.xml" \
    -o "$build_dir/unsigned.apk"
python3 - "$build_dir" <<'PY'
import sys
import zipfile
from pathlib import Path
build = Path(sys.argv[1])
with zipfile.ZipFile(build / 'unsigned.apk', 'a') as apk:
    apk.write(build / 'dex/classes.dex', 'classes.dex')
PY
if [[ ! -f "$build_dir/test.keystore" ]]; then
    keytool -genkeypair -keystore "$build_dir/test.keystore" \
        -storepass android -keypass android -alias test -keyalg RSA \
        -validity 365 -dname 'CN=Ditoo temporary TV probe' >/dev/null 2>&1
fi
"$build_tools/apksigner" sign --ks "$build_dir/test.keystore" \
    --ks-pass pass:android --out "$build_dir/probe.apk" "$build_dir/unsigned.apk"
echo "$build_dir/probe.apk"
