#!/usr/bin/env bash
#
# Build qnvim and run it in the development Qt Creator.
#
# This is the one-command development loop: configure, build, retarget the
# plugin to the Qt bundled in Qt Creator (macOS), install it into the Qt
# Creator under external/qtcreator/, and start that Qt Creator with qnvim's
# logging enabled. A test Qt Creator from an earlier run is stopped before the
# new build is installed, so the new build is always the one that loads.
#
# Usage:
#     ./quick-run.sh [Qt Creator arguments...]
#
# Arguments are passed to Qt Creator, e.g.:
#     ./quick-run.sh src/qnvimcore.cpp
#
# Environment:
#     QNVIM_QUICK_RUN_JOBS=<n>         build parallelism (default: CPU count)
#     QNVIM_QUICK_RUN_BUILD_DIR=<dir>  build directory (default: build)
#     QT_LOGGING_RULES=<rules>         replaces the default qnvim logging rules
#
# Only the Qt Creator downloaded for development is touched; a Qt Creator
# installed in /Applications is not.

set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
build_dir="${QNVIM_QUICK_RUN_BUILD_DIR:-$repo_dir/build}"

say() { printf '==> %s\n' "$*"; }
die() {
	printf 'quick-run: %s\n' "$*" >&2
	exit 1
}

version_file="$repo_dir/external/qtcreator/version.cmake"
version="$(sed -n 's/^set(QT_CREATOR_VERSION "\(.*\)")$/\1/p' "$version_file")"
[[ -n "$version" ]] || die "could not read QT_CREATOR_VERSION from $version_file"

platform="$(uname -s)"
dist_dir="$repo_dir/external/qtcreator/dist-${platform}-${version}"

case "$platform" in
Darwin)
	qtc_app="$dist_dir/Qt Creator.app"
	qtc_bin="$qtc_app/Contents/MacOS/Qt Creator"
	built_plugin="$build_dir/Qt Creator.app/Contents/PlugIns/qtcreator/libQNVim.dylib"
	installed_plugin="$qtc_app/Contents/PlugIns/qtcreator/libQNVim.dylib"
	;;
Linux)
	qtc_bin="$dist_dir/bin/qtcreator"
	built_plugin="$build_dir/lib/qtcreator/plugins/libQNVim.so"
	installed_plugin="$dist_dir/lib/qtcreator/plugins/libQNVim.so"
	;;
*)
	die "unsupported platform '$platform' (only macOS and Linux are supported)"
	;;
esac

# PIDs of Qt Creator processes running from our development bundle. Match the
# executable path at the start of the command line so a Qt Creator installed
# elsewhere (or another qnvim checkout) is left alone.
qtc_pids() {
	ps -axww -o pid=,command= | awk -v exe="$qtc_bin" '
		{
			command = $0
			sub(/^[[:space:]]*[0-9]+[[:space:]]+/, "", command)
			if (command == exe || index(command, exe " ") == 1)
				print $1
		}'
}

stop_running_qtc() {
	local pids pid
	pids="$(qtc_pids)"
	[[ -n "$pids" ]] || return 0

	say "Stopping the running test Qt Creator..."
	while IFS= read -r pid; do
		[[ -n "$pid" ]] && kill "$pid" 2>/dev/null || true
	done <<<"$pids"

	for _ in $(seq 1 150); do
		[[ -z "$(qtc_pids)" ]] && break
		sleep 0.1
	done
	[[ -z "$(qtc_pids)" ]] ||
		die "the test Qt Creator did not exit (unsaved changes?); close it and run again"
}

say "Configuring..."
if [[ ! -f "$build_dir/CMakeCache.txt" ]]; then
	say "No build directory yet; the first configure downloads the test Qt Creator."
fi
cmake -S "$repo_dir" -B "$build_dir"

[[ -x "$qtc_bin" ]] || die "Qt Creator not found at $qtc_bin (check the download in $dist_dir)"

if [[ -n "${QNVIM_QUICK_RUN_JOBS:-}" ]]; then
	jobs="$QNVIM_QUICK_RUN_JOBS"
elif [[ "$platform" == "Darwin" ]]; then
	jobs="$(sysctl -n hw.ncpu)"
else
	jobs="$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"
fi

say "Building QNVim with $jobs jobs..."
cmake --build "$build_dir" --target QNVim --parallel "$jobs"
[[ -f "$built_plugin" ]] || die "the build did not produce $built_plugin"

if [[ "$platform" == "Darwin" ]]; then
	say "Retargeting the plugin to the Qt bundled in Qt Creator..."
	"$repo_dir/tools/RetargetPluginQt.sh" "$built_plugin"
fi

# Stop the old instance right before replacing the plugin: if the build failed,
# the user still has their session.
stop_running_qtc

say "Installing the plugin into the test Qt Creator..."
mkdir -p "$(dirname "$installed_plugin")"
install -m 755 "$built_plugin" "$installed_plugin"
if [[ "$platform" == "Darwin" ]]; then
	codesign --force --sign - "$installed_plugin"
fi

export QT_LOGGING_RULES="${QT_LOGGING_RULES:-qtcreator.plugin.qnvim.*=true}"
say "Starting Qt Creator..."
exec "$qtc_bin" "$@"
