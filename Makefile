# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>

default: all

build/build.ninja:
	@if test -d build/; then \
		meson setup build --reconfigure; \
	else \
		meson setup build; \
	fi

all: build/build.ninja
	@ninja -C build

clean: build/build.ninja
	@ninja -C build clean

reconfigure: build/build.ninja
	@ninja -C build reconfigure

install: build/build.ninja
	@ninja -C build install

uninstall: build/build.ninja
	@ninja -C build uninstall

devenv: all
	@meson devenv -C build

distclean:
	rm -rf build/

tests: all
	build/tests/unittest
	ninja -C build camorama-pot
	ninja -C build translation-report

dist:
	meson dist -C build $(if $(DRY_RUN),--allow-dirty --no-tests)
	python3 tests/check_translations.py --summary
