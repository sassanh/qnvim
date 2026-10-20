#!/usr/bin/env bash
#
# Retarget a Qt Creator plugin to the Qt bundled in Qt Creator (macOS).
#
# When qnvim is built against a Qt whose frameworks are linked by absolute path
# (e.g. Homebrew's /opt/homebrew/opt/qtbase/lib/QtCore.framework/...), the built
# plugin records those paths. At load time they resolve next to Qt Creator's own
# Qt, two Qt versions end up in one process, and Qt Creator reports
#
#     Cannot load library ...: Symbol not found: ...QtPrivate_...
#
# and refuses to load the plugin.
#
# The recommended fix is building against the official Qt (Qt installer or
# https://github.com/miurahr/aqtinstall), whose frameworks use @rpath install
# names and bind to the Qt bundled in the Qt Creator the plugin is installed
# into. This script is the fallback for builds that can't do that: it rewrites
# the absolute non-system framework references of an already-built plugin to
# @rpath/<name>.framework/<suffix>, where <suffix> is whatever follows
# ".framework/" in the original path (normally Versions/A/<name>). Since the
# plugin already carries Qt Creator's "@loader_path/../../Frameworks" rpath, it
# then uses the Qt bundled in the Qt Creator it is installed into.
#
# Usage:
#     tools/RetargetPluginQt.sh [path/to/libQNVim.dylib]
#
# The path defaults to the freshly built plugin in this repository's build
# directory; it can also point at an installed copy, e.g.
# "/Applications/Qt Creator.app/Contents/PlugIns/qtcreator/libQNVim.dylib".
#
# Notes:
#   * Only works while the plugin doesn't use symbols missing from the Qt
#     bundled in the target Qt Creator.
#   * Run it again after every rebuild.
#   * Absolute rpaths recorded in the plugin (e.g. Homebrew's /opt/homebrew/lib)
#     are left alone. The @loader_path rpath is searched first, so the
#     retargeted frameworks still resolve inside Qt Creator's bundle.
#   * The plugin is re-signed ad-hoc afterwards, which arm64 macOS requires.

set -euo pipefail

if [[ "$(uname -s)" != "Darwin" ]]; then
	echo "This script only applies to macOS." >&2
	exit 1
fi

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
plugin="${1:-$repo_dir/build/Qt Creator.app/Contents/PlugIns/qtcreator/libQNVim.dylib}"

if [[ ! -f "$plugin" ]]; then
	echo "Plugin not found: $plugin" >&2
	echo "Usage: $(basename "$0") [path/to/libQNVim.dylib]" >&2
	exit 1
fi

frameworks="$(otool -L "$plugin" |
	awk '$1 ~ /^\// && $1 !~ /^\/System\// && $1 ~ /\.framework\// { print $1 }' |
	sort -u)"

if [[ -z "$frameworks" ]]; then
	echo "Nothing to do: no frameworks linked by absolute (non-system) path in"
	echo "$plugin"
	exit 0
fi

echo "Retargeting $plugin"
while read -r lib; do
	prefix="${lib%%.framework/*}"
	fw="${prefix##*/}"
	suffix="${lib#"$prefix.framework/"}"
	dest="@rpath/$fw.framework/$suffix"
	echo "  $lib"
	echo "    -> $dest"
	install_name_tool -change "$lib" "$dest" "$plugin"
done <<<"$frameworks"

codesign --force --sign - "$plugin"
echo "Done. Reinstall the plugin and restart Qt Creator."
