#!/usr/bin/env bash
#
# Compile qnvim against Qt Creator's source headers (no qtcreator_dev archive
# needed) and optionally run the GUI tests.
#
#   tools/compilecheck/run.sh [--tests] [--nvim /path/to/nvim]
#
# Environment:
#   QT_PREFIX       Qt 6 installation (contains lib/cmake/Qt6). Default: the
#                   Qt found by CMake.
#   QTC_SOURCE_DIR  Qt Creator sources; cloned (shallow) at the version in
#                   external/qtcreator/version.cmake when unset.
#   WORK_DIR        where sources are cloned (default: ~/.cache/qnvim-compilecheck)
#   BUILD_DIR       build directory (default: build-check)
#
# Git is used for every download, so it also works where only git is allowed.

set -euo pipefail

repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
work="${WORK_DIR:-$HOME/.cache/qnvim-compilecheck}"
build="${BUILD_DIR:-$repo/build-check}"
run_tests=0
nvim_bin="nvim"

while [[ $# -gt 0 ]]; do
	case "$1" in
	--tests) run_tests=1 ;;
	--nvim) nvim_bin="$2"; shift ;;
	*) echo "unknown argument: $1" >&2; exit 2 ;;
	esac
	shift
done

version="$(sed -n 's/^set(QT_CREATOR_VERSION "\(.*\)")$/\1/p' "$repo/external/qtcreator/version.cmake")"
mkdir -p "$work"

clone() { # url tag dir
	if [[ ! -d "$3/.git" ]]; then
		echo "==> cloning $1 @ $2"
		git clone -q --depth 1 --branch "$2" "$1" "$3"
	fi
}

if [[ -z "${QTC_SOURCE_DIR:-}" ]]; then
	QTC_SOURCE_DIR="$work/qt-creator-$version"
	clone https://github.com/qt-creator/qt-creator.git "v$version" "$QTC_SOURCE_DIR"
fi
export QTC_SOURCE_DIR

# neovim-qt and msgpack-c from git (FetchContent's archive URLs may be blocked).
neovimqt_rev="$(sed -n 's/.*GIT_TAG \([0-9a-f]\{40\}\).*/\1/p' "$repo/external/neovim-qt/CMakeLists.txt" | head -1)"
if [[ ! -d "$work/neovim-qt/.git" ]]; then
	echo "==> cloning neovim-qt"
	git clone -q https://github.com/gikari/neovim-qt.git "$work/neovim-qt"
fi
git -C "$work/neovim-qt" checkout -q "$neovimqt_rev"
msgpack_ver="$(sed -n 's/^set(MSGPACK_VERSION \(.*\))$/\1/p' "$work/neovim-qt/third-party/CMakeLists.txt")"
clone https://github.com/msgpack/msgpack-c.git "cpp-$msgpack_ver" "$work/msgpack-c-$msgpack_ver"

prefix_args=()
if [[ -n "${QT_PREFIX:-}" ]]; then
	prefix_args=(-DCMAKE_PREFIX_PATH="$QT_PREFIX")
fi

generator=()
command -v ninja >/dev/null && generator=(-G Ninja)

echo "==> configuring $build (Qt Creator $version sources)"
cmake -S "$repo" -B "$build" "${generator[@]}" \
	-DCMAKE_BUILD_TYPE=RelWithDebInfo \
	-DFETCH_QTC=OFF \
	-DQtCreator_DIR="$repo/tools/compilecheck/QtCreator" \
	-DQTC_SOURCE_DIR="$QTC_SOURCE_DIR" \
	-DFETCHCONTENT_SOURCE_DIR_NEOVIMQT="$work/neovim-qt" \
	-DFETCHCONTENT_SOURCE_DIR_MSGPACKC="$work/msgpack-c-$msgpack_ver" \
	-DQNVIM_BUILD_TESTS=$([[ $run_tests == 1 ]] && echo ON || echo OFF) \
	-DQNVIM_TEST_NVIM="$nvim_bin" \
	"${prefix_args[@]}"

echo "==> building"
cmake --build "$build" --target QNVim

if [[ $run_tests == 1 ]]; then
	echo "==> protocol test"
	python3 "$repo/tests/gui/test_protocol.py" --nvim "$nvim_bin"
	echo "==> smoke test"
	cmake --build "$build" --target qnvim_gui_smoke_run
fi

echo "==> OK: compiled against Qt Creator $version headers"
echo "    (Qt Creator symbols are unresolved in this build; it is a compile check, not a release build)"
