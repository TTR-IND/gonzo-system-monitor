#!/usr/bin/env bash
#
# install.sh -- builds and installs Gonzo System Monitor, a fork of
# MATE System Monitor with live integration for detritusd
# (https://github.com/YOUR_GITHUB/detritusd).
#
# Design: staged, fail-loud, matching detritusd's own installer. Every
# run writes a full log to /var/log/gonzo-monitor-install-<timestamp>.log
# regardless of outcome.
#
# Usage:
#   sudo ./install.sh              (build + install)
#   sudo ./install.sh --uninstall  (remove and restore stock mate-system-monitor)
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
INSTALL_LOG="/var/log/gonzo-monitor-install-$(date +%Y%m%d-%H%M%S).log"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log()  { echo -e "${GREEN}[gonzo-monitor-install]${NC} $*"; }
warn() { echo -e "${YELLOW}[gonzo-monitor-install] WARNING:${NC} $*"; }
die()  { echo -e "${RED}[gonzo-monitor-install] ERROR:${NC} $*" >&2; exit 1; }

require_root() {
    if [ "$(id -u)" -ne 0 ]; then
        die "must be run as root (sudo ./install.sh)"
    fi
}

# Every dependency here was confirmed genuinely required by tracing
# meson.build's actual dependency() calls and by building against a
# real target system -- gettext and itstool specifically were both
# real, silent gaps that only surfaced when meson's i18n/yelp modules
# actually ran, not something a dependency() grep alone would catch.
check_dependencies() {
    log "checking build dependencies..."
    local missing=()
    local pkgs=(
        build-essential git gcc pkg-config gettext itstool
        libgtk-3-dev libgtop2-dev libglibmm-2.4-dev libgtkmm-3.0-dev
        libgirepository1.0-dev librsvg2-dev libxml2-dev
        meson ninja-build
    )
    for pkg in "${pkgs[@]}"; do
        dpkg -s "$pkg" >/dev/null 2>&1 || missing+=("$pkg")
    done
    if [ "${#missing[@]}" -gt 0 ]; then
        log "installing missing packages: ${missing[*]}"
        apt-get update || die "apt-get update failed"
        apt-get install -y "${missing[@]}" || die "apt-get install failed for: ${missing[*]}"
    else
        log "all build dependencies already present"
    fi
}

build_and_install() {
    log "cleaning any previous build directory..."
    rm -rf "$BUILD_DIR"

    # -Dsystemd is auto-detected, not hardcoded -- this repo isn't
    # OpenRC-specific the way detritusd is, so assuming every install
    # target lacks systemd would be wrong on plenty of real systems.
    # pkg-config --exists libsystemd is the real, direct check; the
    # meson option only gates systemd-logind session/seat columns in
    # the process list, nothing else.
    local systemd_flag=""
    if pkg-config --exists libsystemd 2>/dev/null; then
        log "libsystemd found -- building with systemd-logind support"
    else
        log "libsystemd not found -- building without systemd-logind support (-Dsystemd=false)"
        systemd_flag="-Dsystemd=false"
    fi

    log "configuring build (meson)..."
    # shellcheck disable=SC2086
    meson setup "$BUILD_DIR" --prefix=/usr $systemd_flag \
        || die "meson setup failed"

    log "building (this can take a few minutes)..."
    ninja -C "$BUILD_DIR" || die "ninja build failed"

    log "installing..."
    ninja -C "$BUILD_DIR" install || die "ninja install failed"

    if ! command -v mate-system-monitor >/dev/null 2>&1; then
        die "install completed but mate-system-monitor not found on PATH -- installation is inconsistent, do not treat this as installed"
    fi

    log "Gonzo System Monitor installed (binary: mate-system-monitor, display name: Gonzo System Monitor)"
    log "Note: this is a cosmetic rename over the mate-system-monitor package/binary --"
    log "  see the README for why, and for detritusd, which this UI is built to display."
}

uninstall_all() {
    log "removing build directory..."
    rm -rf "$BUILD_DIR"

    log "restoring stock mate-system-monitor from distro package (if available)..."
    if command -v apt-get >/dev/null 2>&1; then
        apt-get install --reinstall -y mate-system-monitor 2>/dev/null \
            || warn "could not reinstall stock mate-system-monitor from apt -- you may need to do this manually, or run 'sudo ninja -C build uninstall' from a prior build if it still exists"
    fi
    log "uninstall complete"
}

usage() {
    cat << EOF
Usage: sudo $0 [--uninstall]

  (no args)     build and install Gonzo System Monitor
  --uninstall   remove the build directory and restore stock mate-system-monitor

Every run writes a full log to /var/log/gonzo-monitor-install-<timestamp>.log.

Requires detritusd (https://github.com/YOUR_GITHUB/detritusd) installed
and running separately for the new UI elements to show real data.
EOF
}

main_inner() {
    require_root
    case "${1:-}" in
        --uninstall)
            uninstall_all
            exit 0
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        "")
            check_dependencies
            build_and_install
            ;;
        *)
            usage
            die "unrecognized argument: $1"
            ;;
    esac
    log "done."
    log "  run:  mate-system-monitor"
    log "  full install log: $INSTALL_LOG"
}

main() {
    touch "$INSTALL_LOG" 2>/dev/null || INSTALL_LOG="/tmp/gonzo-monitor-install-$(date +%Y%m%d-%H%M%S).log"
    main_inner "$@" 2>&1 | tee -a "$INSTALL_LOG"
    exit "${PIPESTATUS[0]}"
}

main "$@"
