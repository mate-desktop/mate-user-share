#!/usr/bin/bash

set -eo pipefail

# Use grouped output messages
infobegin() {
	echo "::group::${1}"
}
infoend() {
	echo "::endgroup::"
}

# Required packages on Fedora
requires=(
	ccache # Use ccache to speed up build
)

requires+=(
	autoconf-archive
	caja-devel
	dbus-glib-devel
	desktop-file-utils
	gcc
	git
	gtk2-devel
	httpd
	libICE-devel
	libSM-devel
	libcanberra-devel
	libnotify-devel
	libselinux-devel
	make
	mate-common
	redhat-rpm-config
	yelp-tools
)

infobegin "Update system"
dnf update -y
infoend

infobegin "Install dependency packages"
dnf install -y ${requires[@]}
infoend
