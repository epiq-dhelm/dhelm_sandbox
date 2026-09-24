#!/bin/sh
set -eu

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
rfe_root=$(dirname -- "$here")
python=${PYTHON:-/usr/bin/python3}
architecture=$(uname -m)
build_root="$here/.build"
dist_root="$here/dist/linux-$architecture"

if [ "$(uname -s)" != Linux ]; then
    echo 'Build on Linux for Linux targets.' >&2
    exit 1
fi

"$python" -m venv "$build_root/venv"
export PYINSTALLER_CONFIG_DIR="$build_root/pyinstaller-cache"
export MPLCONFIGDIR="$build_root/matplotlib-cache"
mkdir -p "$PYINSTALLER_CONFIG_DIR" "$MPLCONFIGDIR" "$dist_root"
"$build_root/venv/bin/python" -m pip install -r "$here/requirements-build.txt"

for app in siggen sigann; do
    "$build_root/venv/bin/python" -m PyInstaller \
        --noconfirm --onedir --name "$app" \
        --distpath "$dist_root" \
        --workpath "$build_root/work/$app" \
        --specpath "$build_root/spec" \
        "$rfe_root/${app}_dev/$app"
    PYTHONNOUSERSITE=1 PYTHONPATH= "$dist_root/$app/$app" --check-runtime
done

tar -C "$dist_root" -czf "$here/dist/rfe-portable-linux-$architecture.tar.gz" siggen sigann
echo "Portable bundle: $here/dist/rfe-portable-linux-$architecture.tar.gz"
