# Auto-generated file from 'Makefile.lang.in' -- DO NOT EDIT
# This file is part of OpenTTD.
# OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
# OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.

STRGEN        = strgen
SRC_DIR       = /home/pavels/Projects/cmbase/src
LANG_DIR      = /home/pavels/Projects/cmbase/src/lang
BIN_DIR       = /home/pavels/Projects/cmbase/bin
LANGS_SRC     = $(shell ls $(LANG_DIR)/*.txt)
LANGS         = $(LANGS_SRC:$(LANG_DIR)/%.txt=%.lng)
CXX_BUILD     = g++
CFLAGS_BUILD  =  -Wall -Wno-multichar -Wsign-compare -Wundef -Wwrite-strings -Wpointer-arith -W -Wno-unused-parameter -Wredundant-decls -Wformat=2 -Wformat-security -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-but-set-parameter -Winit-self -fno-strict-aliasing -Wcast-qual -fno-strict-overflow -Wnon-virtual-dtor -Wno-free-nonheap-object -rdynamic -DUNIX -D_FORTIFY_SOURCE=2 -O1 -DNDEBUG 
CXXFLAGS_BUILD=  -flifetime-dse=1 -std=c++11 
LDFLAGS_BUILD =  -rdynamic 
STRGEN_FLAGS  = 
STAGE         = [LANG]
LANG_SUPPRESS = 
LANG_OBJS_DIR = /home/pavels/Projects/cmbase/objs/lang

ifeq ($(LANG_SUPPRESS), yes)
LANG_ERRORS = >/dev/null 2>&1
endif

# Check if we want to show what we are doing
ifdef VERBOSE
	Q =
	E = @true
else
	Q = @
	E = @echo
endif

RES := $(shell mkdir -p $(BIN_DIR)/lang )

all: table/strings.h $(LANGS)

strgen_base.o: $(SRC_DIR)/strgen/strgen_base.cpp $(SRC_DIR)/strgen/strgen.h $(SRC_DIR)/table/control_codes.h $(SRC_DIR)/table/strgen_tables.h $(SRC_DIR)/safeguards.h
	$(E) '$(STAGE) Compiling $(<:$(SRC_DIR)/%.cpp=%.cpp)'
	$(Q)$(CXX_BUILD) $(CFLAGS_BUILD) $(CXXFLAGS_BUILD) -DSTRGEN -c -o $@ $<

strgen.o: $(SRC_DIR)/strgen/strgen.cpp $(SRC_DIR)/strgen/strgen.h $(SRC_DIR)/table/control_codes.h $(SRC_DIR)/table/strgen_tables.h $(SRC_DIR)/safeguards.h
	$(E) '$(STAGE) Compiling $(<:$(SRC_DIR)/%.cpp=%.cpp)'
	$(Q)$(CXX_BUILD) $(CFLAGS_BUILD) $(CXXFLAGS_BUILD) -DSTRGEN -c -o $@ $<

string.o: $(SRC_DIR)/string.cpp $(SRC_DIR)/safeguards.h
	$(E) '$(STAGE) Compiling $(<:$(SRC_DIR)/%.cpp=%.cpp)'
	$(Q)$(CXX_BUILD) $(CFLAGS_BUILD) $(CXXFLAGS_BUILD) -DSTRGEN -c -o $@ $<

alloc_func.o: $(SRC_DIR)/core/alloc_func.cpp $(SRC_DIR)/safeguards.h
	$(E) '$(STAGE) Compiling $(<:$(SRC_DIR)/%.cpp=%.cpp)'
	$(Q)$(CXX_BUILD) $(CFLAGS_BUILD) $(CXXFLAGS_BUILD) -DSTRGEN -c -o $@ $<

getoptdata.o: $(SRC_DIR)/misc/getoptdata.cpp $(SRC_DIR)/misc/getoptdata.h $(SRC_DIR)/safeguards.h
	$(E) '$(STAGE) Compiling $(<:$(SRC_DIR)/misc/%.cpp=%.cpp)'
	$(Q)$(CXX_BUILD) $(CFLAGS_BUILD) $(CXXFLAGS_BUILD) -DSTRGEN -c -o $@ $<

lang/english.txt: $(LANG_DIR)/english.txt
	$(Q)mkdir -p lang
	$(Q)cp $(LANG_DIR)/english.txt lang/english.txt

$(STRGEN): alloc_func.o string.o strgen_base.o strgen.o getoptdata.o
	$(E) '$(STAGE) Compiling and Linking $@'
	$(Q)$(CXX_BUILD) $(CFLAGS_BUILD) $(CXXFLAGS_BUILD) $(LDFLAGS_BUILD) $^ -o $@

table/strings.h: lang/english.txt $(STRGEN)
	$(E) '$(STAGE) Generating $@'
	@mkdir -p table
	$(Q)./$(STRGEN) -s $(LANG_DIR) -d table

$(LANGS): %.lng: $(LANG_DIR)/%.txt $(STRGEN) lang/english.txt
	$(E) '$(STAGE) Compiling language $(*F)'
	$(Q)./$(STRGEN) $(STRGEN_FLAGS) -s $(LANG_DIR) -d $(LANG_OBJS_DIR) $< $(LANG_ERRORS) && cp $@ $(BIN_DIR)/lang || true # Do not fail all languages when one fails

depend:

clean:
	$(E) '$(STAGE) Cleaning up language files'
	$(Q)rm -f strgen_base.o strgen.o string.o alloc_func.o getoptdata.o table/strings.h $(STRGEN) $(LANGS) $(LANGS:%=$(BIN_DIR)/lang/%) lang/english.*

mrproper: clean
	$(Q)rm -rf $(BIN_DIR)/lang

%.lng:
	@echo '$(STAGE) No such language: $(@:%.lng=%)'

.PHONY: all mrproper depend clean
