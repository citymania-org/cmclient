# Auto-generated file from 'Makefile.settings.in' -- DO NOT EDIT
# This file is part of OpenTTD.
# OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
# OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.

SETTINGSGEN      = settings_gen
SRC_DIR          = /home/pavels/Projects/cmbase/src
CXX_BUILD        = g++
CFLAGS_BUILD     =  -Wall -Wno-multichar -Wsign-compare -Wundef -Wwrite-strings -Wpointer-arith -W -Wno-unused-parameter -Wredundant-decls -Wformat=2 -Wformat-security -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-but-set-parameter -Winit-self -fno-strict-aliasing -Wcast-qual -fno-strict-overflow -Wnon-virtual-dtor -Wno-free-nonheap-object -rdynamic -DUNIX -D_FORTIFY_SOURCE=2 -O1 -DNDEBUG 
CXXFLAGS_BUILD   =  -flifetime-dse=1 -std=c++11 
LDFLAGS_BUILD    =  -rdynamic 
STAGE            = [SETTING]
SETTING_OBJS_DIR = /home/pavels/Projects/cmbase/objs/setting

# Check if we want to show what we are doing
ifdef VERBOSE
	Q =
	E = @true
else
	Q = @
	E = @echo
endif

all: table/settings.h

settingsgen.o: $(SRC_DIR)/settingsgen/settingsgen.cpp $(SRC_DIR)/string_func.h $(SRC_DIR)/strings_type.h $(SRC_DIR)/misc/getoptdata.h $(SRC_DIR)/ini_type.h $(SRC_DIR)/core/smallvec_type.hpp $(SRC_DIR)/safeguards.h
	$(E) '$(STAGE) Compiling $(<:$(SRC_DIR)/%.cpp=%.cpp)'
	$(Q)$(CXX_BUILD) $(CFLAGS_BUILD) $(CXXFLAGS_BUILD) -DSETTINGSGEN -c -o $@ $<

alloc_func.o: $(SRC_DIR)/core/alloc_func.cpp $(SRC_DIR)/safeguards.h
	$(E) '$(STAGE) Compiling $(<:$(SRC_DIR)/%.cpp=%.cpp)'
	$(Q)$(CXX_BUILD) $(CFLAGS_BUILD) $(CXXFLAGS_BUILD) -DSETTINGSGEN -c -o $@ $<

getoptdata.o: $(SRC_DIR)/misc/getoptdata.cpp $(SRC_DIR)/misc/getoptdata.h $(SRC_DIR)/safeguards.h
	$(E) '$(STAGE) Compiling $(<:$(SRC_DIR)/misc/%.cpp=%.cpp)'
	$(Q)$(CXX_BUILD) $(CFLAGS_BUILD) $(CXXFLAGS_BUILD) -DSETTINGSGEN -c -o $@ $<

string.o: $(SRC_DIR)/string.cpp $(SRC_DIR)/safeguards.h
	$(E) '$(STAGE) Compiling $(<:$(SRC_DIR)/%.cpp=%.cpp)'
	$(Q)$(CXX_BUILD) $(CFLAGS_BUILD) $(CXXFLAGS_BUILD) -DSETTINGSGEN -c -o $@ $<

ini_load.o: $(SRC_DIR)/ini_load.cpp $(SRC_DIR)/core/alloc_func.hpp $(SRC_DIR)/core/mem_func.hpp $(SRC_DIR)/ini_type.h $(SRC_DIR)/string_func.h $(SRC_DIR)/safeguards.h
	$(E) '$(STAGE) Compiling $(<:$(SRC_DIR)/%.cpp=%.cpp)'
	$(Q)$(CXX_BUILD) $(CFLAGS_BUILD) $(CXXFLAGS_BUILD) -DSETTINGSGEN -c -o $@ $<

$(SETTINGSGEN): alloc_func.o string.o ini_load.o settingsgen.o getoptdata.o
	$(E) '$(STAGE) Compiling and Linking $@'
	$(Q)$(CXX_BUILD) $(CFLAGS_BUILD) $(CXXFLAGS_BUILD) $(LDFLAGS_BUILD) $^ -o $@

table/settings.h: $(SETTINGSGEN) $(SRC_DIR)/table/settings.h.preamble $(SRC_DIR)/table/settings.h.postamble $(SRC_DIR)/table/*.ini
	$(E) '$(STAGE) Generating $@'
	@mkdir -p table
	$(Q)./$(SETTINGSGEN) -o table/settings.h -b $(SRC_DIR)/table/settings.h.preamble -a $(SRC_DIR)/table/settings.h.postamble $(SRC_DIR)/table/*.ini

depend:

clean:
	$(E) '$(STAGE) Cleaning up settings files'
	$(Q)rm -f settingsgen.o alloc_func.o getoptdata.o string.o ini_load.o $(SETTINGSGEN) table/settings.h

mrproper: clean

.PHONY: all mrproper depend clean
