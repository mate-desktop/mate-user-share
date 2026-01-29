#!/usr/bin/bash

set -eo pipefail

# Use grouped output messages
infobegin() {
	echo "::group::${1}"
}
infoend() {
	echo "::endgroup::"
}

# Required packages on Ubuntu
requires=(
	ccache # Use ccache to speed up build
)

requires+=(
	autoconf-archive
	autopoint
	gcc
	git
	intltool
	libcaja-extension-dev
	libcanberra-gtk3-dev
	libdbus-glib-1-dev
	libglib2.0-dev
	libgtk-3-dev
	libnotify-dev
	libselinux1-dev
	libxt-dev
	make
	mate-common
	pkg-config
	quilt
	yelp-tools
)

infobegin "Update system"
apt-get update -y
infoend

infobegin "Install dependency packages"
env DEBIAN_FRONTEND=noninteractive \
	apt-get install --assume-yes \
	${requires[@]}
infoend
