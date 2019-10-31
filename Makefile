# Auto-generated file from 'Makefile.in' -- DO NOT EDIT
# $Id$

# This file is part of OpenTTD.
# OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
# OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.

# Check if we want to show what we are doing
ifdef VERBOSE
	Q =
else
	Q = @
endif

include Makefile.am

CONFIG_CACHE_PWD         = config.cache.pwd
CONFIG_CACHE_SOURCE_LIST = config.cache.source.list
BIN_DIR        = /home/pavels/Downloads/openttd-1.10.0-beta1/bin
ICON_THEME_DIR = /usr/local/share/icons/hicolor
MAN_DIR        = /usr/local/share/man/man6
MENU_DIR       = /usr/local/share/applications
SRC_DIR        = /home/pavels/Downloads/openttd-1.10.0-beta1/src
ROOT_DIR       = /home/pavels/Downloads/openttd-1.10.0-beta1
BUNDLE_DIR     = "$(ROOT_DIR)/bundle"
BUNDLES_DIR    = "$(ROOT_DIR)/bundles"
INSTALL_DIR    = /
INSTALL_BINARY_DIR     = "$(INSTALL_DIR)/"/usr/local/games
INSTALL_MAN_DIR        = "$(INSTALL_DIR)/$(MAN_DIR)"
INSTALL_MENU_DIR       = "$(INSTALL_DIR)/$(MENU_DIR)"
INSTALL_ICON_DIR       = "$(INSTALL_DIR)/"/usr/local/share/pixmaps
INSTALL_ICON_THEME_DIR = "$(INSTALL_DIR)/$(ICON_THEME_DIR)"
INSTALL_DATA_DIR       = "$(INSTALL_DIR)/"/usr/local/share/games/openttd
INSTALL_DOC_DIR        = "$(INSTALL_DIR)/"/usr/local/share/doc/openttd
SOURCE_LIST     = /home/pavels/Downloads/openttd-1.10.0-beta1/source.list
CONFIGURE_FILES = /home/pavels/Downloads/openttd-1.10.0-beta1/configure /home/pavels/Downloads/openttd-1.10.0-beta1/config.lib /home/pavels/Downloads/openttd-1.10.0-beta1/Makefile.in /home/pavels/Downloads/openttd-1.10.0-beta1/Makefile.grf.in /home/pavels/Downloads/openttd-1.10.0-beta1/Makefile.lang.in /home/pavels/Downloads/openttd-1.10.0-beta1/Makefile.src.in /home/pavels/Downloads/openttd-1.10.0-beta1/Makefile.bundle.in /home/pavels/Downloads/openttd-1.10.0-beta1/Makefile.setting.in
BINARY_NAME = openttd
STRIP       =  
TTD         = openttd
TTDS        = $(SRC_DIRS:%=%/$(TTD))
OS          = UNIX
OSXAPP      = 
LIPO        = 
AWK         = awk
SORT        = sort -u
DISTCC      = 

RES := $(shell if [ ! -f $(CONFIG_CACHE_PWD) ] || [ "`pwd`" != "`cat $(CONFIG_CACHE_PWD)`" ]; then echo "`pwd`" > $(CONFIG_CACHE_PWD); fi )
RES := $(shell if [ ! -f $(CONFIG_CACHE_SOURCE_LIST) ] || [ -n "`cmp $(CONFIG_CACHE_SOURCE_LIST) $(SOURCE_LIST) 2>/dev/null`" ]; then cp $(SOURCE_LIST) $(CONFIG_CACHE_SOURCE_LIST); fi )

all: config.pwd config.cache
ifdef DISTCC
	@if [ -z "`echo '$(MFLAGS)' | grep '\-j'`" ]; then echo; echo "WARNING: you enabled distcc support, but you don't seem to be using the -jN parameter"; echo; fi
endif
	@for dir in $(DIRS); do \
		$(MAKE) -C $$dir all || exit 1; \
	done
ifdef LIPO
# Lipo is an OSX thing. If it is defined, it means we are building for universal,
# and so we have have to combine the binaries into one big binary

# Remove the last binary made by the last compiled target
	$(Q)rm -f $(BIN_DIR)/$(TTD)
# Make all the binaries into one
	$(Q)$(LIPO) -create -output $(BIN_DIR)/$(TTD) $(TTDS)
endif

help:
	@echo "Available make commands:"
	@echo ""
	@echo "Compilation:"
	@echo "  all           compile the executable and the lang files"
	@echo "  lang          compile the lang files only"
	@echo "Clean up:"
	@echo "  clean         remove the files generated during compilation"
	@echo "  mrproper      remove the files generated during configuration and compilation"
	@echo "Run after compilation:"
	@echo "  run           execute openttd after the compilation"
	@echo "  run-gdb       execute openttd in debug mode after the compilation"
	@echo "  run-prof      execute openttd in profiling mode after the compilation"
	@echo "Installation:"
	@echo "  install       install the compiled files and the data-files after the compilation"
	@echo "  bundle        create the base for an installation bundle"
	@echo "  bundle_zip    create the zip installation bundle"
	@echo "  bundle_gzip   create the gzip installation bundle"
	@echo "  bundle_bzip2  create the bzip2 installation bundle"
	@echo "  bundle_lha    create the lha installation bundle"
	@echo "  bundle_dmg    create the dmg installation bundle"

config.pwd: $(CONFIG_CACHE_PWD)
	$(MAKE) reconfigure

config.cache: $(CONFIG_CACHE_SOURCE_LIST) $(CONFIGURE_FILES)
	$(MAKE) reconfigure

reconfigure:
ifeq ($(shell if test -f config.cache; then echo 1; fi), 1)
	@echo "----------------"
	@echo "The system detected that source.list or any configure file is altered."
	@echo " Going to reconfigure with last known settings..."
	@echo "----------------"
# Make sure we don't lock config.cache
	@$(shell cat config.cache | sed 's@\\ @\\\\ @g') || exit 1
	@echo "----------------"
	@echo "Reconfig done. Please re-execute make."
	@echo "----------------"
else
	@echo "----------------"
	@echo "Have not found a configuration, please run configure first."
	@echo "----------------"
	@exit 1
endif

clean:
	@for dir in $(DIRS); do \
		$(MAKE) -C $$dir clean; \
	done
	$(Q)rm -rf $(BUNDLE_TARGET)

lang:
	@for dir in $(LANG_DIRS); do \
		$(MAKE) -C $$dir all; \
	done

mrproper:
	@for dir in $(DIRS); do \
		$(MAKE) -C $$dir mrproper; \
	done
# Don't be tempted to merge these two for loops. Doing that breaks make
# --dry-run, since make has this "feature" that it always runs commands
# containing $(MAKE), even when --dry-run is passed. The objective is of
# course to also get a dry-run of submakes, but make is not smart enough
# to see that a for loop runs both a submake and an actual command.
	@for dir in $(DIRS); do \
		rm -f $$dir/Makefile; \
	done
	$(Q)rm -rf objs
	$(Q)rm -f Makefile Makefile.am Makefile.bundle
	$(Q)rm -f media/openttd.desktop media/openttd.desktop.install
	$(Q)rm -f $(CONFIG_CACHE_SOURCE_LIST) config.cache config.pwd config.log $(CONFIG_CACHE_PWD)
# directories for bundle generation
	$(Q)rm -rf $(BUNDLE_DIR)
	$(Q)rm -rf $(BUNDLES_DIR)
# output of profiling
	$(Q)rm -f $(BIN_DIR)/gmon.out
# output of generating 'API' documentation
	$(Q)rm -rf $(ROOT_DIR)/docs/source
	$(Q)rm -rf $(ROOT_DIR)/docs/aidocs
	$(Q)rm -rf $(ROOT_DIR)/docs/gamedocs
# directories created by OpenTTD on regression testing
	$(Q)rm -rf $(BIN_DIR)/ai/regression/content_download $(BIN_DIR)/ai/regression/save $(BIN_DIR)/ai/regression/scenario
distclean: mrproper

maintainer-clean: distclean
	$(Q)rm -f $(BIN_DIR)/baseset/openttd.grf $(BIN_DIR)/baseset/orig_extra.grf $(BIN_DIR)/baseset/*.obg $(BIN_DIR)/baseset/*.obs $(BIN_DIR)/baseset/*.obm

depend:
	@for dir in $(SRC_DIRS); do \
		$(MAKE) -C $$dir depend; \
	done

run: all
	$(Q)cd /home/pavels/Downloads/openttd-1.10.0-beta1/bin && ./openttd $(OPENTTD_ARGS)

run-gdb: all
	$(Q)cd /home/pavels/Downloads/openttd-1.10.0-beta1/bin && gdb --ex run --args ./openttd $(OPENTTD_ARGS)

run-prof: all
	$(Q)cd /home/pavels/Downloads/openttd-1.10.0-beta1/bin && ./openttd $(OPENTTD_ARGS) && gprof openttd | less

regression: all
	$(Q)cd /home/pavels/Downloads/openttd-1.10.0-beta1/bin && sh ai/regression/run.sh
test: regression

%.o:
	@for dir in $(SRC_DIRS); do \
		$(MAKE) -C $$dir $(@:src/%=%); \
	done

%.lng:
	@for dir in $(LANG_DIRS); do \
		$(MAKE) -C $$dir $@; \
	done

.PHONY: test distclean mrproper clean

include Makefile.bundle
