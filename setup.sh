#!/usr/bin/env bash
# =============================================================================
# setup.sh - Automated Toolchain Verification & Installer for WSL / Linux
# =============================================================================

set -e

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

echo "============================================================"
echo "  Mini-OS Toolchain Check & Setup (Ubuntu / Debian / WSL)"
echo "============================================================"

MISSING_PKGS=()

check_cmd() {
    local cmd=$1
    local pkg=$2
    if command -v "$cmd" >/dev/null 2>&1; then
        echo -e "[${GREEN}OK${NC}] $cmd is installed ($(command -v "$cmd"))"
    else
        echo -e "[${YELLOW}MISSING${NC}] $cmd is not installed (package: $pkg)"
        MISSING_PKGS+=("$pkg")
    fi
}

check_cmd "gcc" "build-essential"
check_cmd "make" "make"
check_cmd "nasm" "nasm"
check_cmd "ld" "binutils"
check_cmd "qemu-system-i386" "qemu-system-x86"

# Check 32-bit multilib support for gcc on x86_64 hosts
if [ "$(uname -m)" = "x86_64" ]; then
    if dpkg -s gcc-multilib >/dev/null 2>&1; then
        echo -e "[${GREEN}OK${NC}] gcc-multilib (32-bit support) is installed"
    else
        echo -e "[${YELLOW}MISSING${NC}] gcc-multilib is not installed (needed for -m32)"
        MISSING_PKGS+=("gcc-multilib")
    fi
fi

if [ ${#MISSING_PKGS[@]} -eq 0 ]; then
    echo ""
    echo -e "${GREEN}All required tools are installed!${NC}"
    echo "You can now build and run Mini-OS using:"
    echo "  make build"
    echo "  make run"
    exit 0
fi

echo ""
echo -e "${YELLOW}Missing packages detected:${NC} ${MISSING_PKGS[*]}"
read -r -p "Would you like to install them now via apt? [Y/n] " response
response=${response:-Y}

if [[ "$response" =~ ^([yY][eE][sS]|[yY])$ ]]; then
    sudo apt-get update
    sudo apt-get install -y "${MISSING_PKGS[@]}"
    echo ""
    echo -e "${GREEN}Installation complete!${NC} Run 'make run' to boot Mini-OS."
else
    echo -e "${RED}Aborted.${NC} Please install the missing packages manually:"
    echo "  sudo apt update && sudo apt install -y ${MISSING_PKGS[*]}"
    exit 1
fi

