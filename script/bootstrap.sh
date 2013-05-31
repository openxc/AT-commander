#!/usr/bin/env bash

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
pushd $DIR/..

KERNEL=`uname`
if [ ${KERNEL:0:7} == "MINGW32" ]; then
    OS="windows"
elif [ ${KERNEL:0:6} == "CYGWIN" ]; then
    OS="cygwin"
elif [ $KERNEL == "Darwin" ]; then
    OS="mac"
else
    OS="linux"
    if ! command -v lsb_release >/dev/null 2>&1; then
        # Arch Linux
        if command -v pacman>/dev/null 2>&1; then
            sudo pacman -S lsb-release
        fi
    fi

    DISTRO=`lsb_release -si`
fi

_cygwin_error() {
    echo
    echo "${bldred}Missing \"$1\"${txtrst} - run the Cygwin installer again and select the base package set:"
    echo "    $CYGWIN_PACKAGES"
    echo "After installing the packages, re-run this bootstrap script."
    die
}

if [ $OS == "cygwin" ] && ! command -v tput >/dev/null 2>&1; then
    _cygwin_error "ncurses"
fi

txtrst=$(tput sgr0) # reset
bldred=${txtbld}$(tput setaf 1)
bldgreen=${txtbld}$(tput setaf 2)

die() {
    echo >&2 "${bldred}$@${txtrst}"
    exit 1
}

_wait() {
    if [ -z $CI ]; then
        echo "Press Enter when done"
        read
    fi
}

_install() {
    if [ $OS == "cygwin" ]; then
        _cygwin_error $1
    elif [ $OS == "mac" ]; then
        # brew exists with 1 if it's already installed
        set +e
        brew install $1
        set -e
    else
        if ! command -v lsb_release >/dev/null 2>&1; then
            echo
            echo "Missing $1 - install it using your distro's package manager or build from source"
            _wait
        else
            if [ $DISTRO == "arch" ]; then
                sudo pacman -S $1
            elif [ $DISTRO == "Ubuntu" ]; then
                sudo apt-get update -qq
                sudo apt-get install $1 -y
            else
                echo
                echo "Missing $1 - install it using your distro's package manager or build from source"
                _wait
            fi
        fi
    fi
}

CYGWIN_PACKAGES="git, curl, libsasl2, ca-certificates, ncurses"

if [ `id -u` == 0 ]; then
    die "Error: running as root - don't use 'sudo' with this script"
fi

CYGWIN_PACKAGES="make, gcc4, check"

if [ $OS == "windows" ]; then
    die "Sorry, the bootstrap script for compiling from source doesn't support the Windows console - try Cygwin."
fi

if [ $OS == "mac" ] && ! command -v brew >/dev/null 2>&1; then
    echo "Installing Homebrew..."
    ruby -e "$(curl -fsSkL raw.github.com/mxcl/homebrew/go)"
fi

if ! command -v make >/dev/null 2>&1; then
    if [ $OS == "cygwin" ]; then
        _cygwin_error "make"
    elif [ $OS == "mac" ]; then
            echo "Missing 'make' - install the Xcode CLI tools"
	    die
    else
        if [ $DISTRO == "arch" ]; then
            sudo pacman -S base-devel
        elif [ $DISTRO == "Ubuntu" ]; then
            sudo apt-get update -qq
            sudo apt-get install build-essential -y
        fi
    fi
fi

echo "Installing dependencies for running test suite..."

if [ -z $CI ] && ! command -v lcov >/dev/null 2>&1; then
    if [ $OS == "cygwin" ]; then
        echo "Missing lcov - Cygwin doesn't have a packaged version of lcov, and it's only required to calculate test suite coverage. We'll skip it."
    elif [ $OS == "mac" ]; then
        brew install lcov
    else
        if [ $DISTRO == "arch" ]; then
            echo "Missing lcov - install from the AUR."
            _wait
        elif [ $DISTRO == "Ubuntu" ]; then
            sudo apt-get update -qq
            sudo apt-get install lcov -y
        fi
    fi
fi
if [ $OS == "cygwin" ] && ! command -v ld >/dev/null 2>&1; then
    _cygwin_error "gcc4"
fi

if ! ld -lcheck -o /tmp/checkcheck 2>/dev/null; then
    echo "Installing the check unit testing library..."

    _install "check"
fi

popd

echo
echo "${bldgreen}All developer dependencies installed, ready to compile.$txtrst"
