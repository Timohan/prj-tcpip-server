#/*!
#* \file Makefile
#* \brief Makefile for compiling prj-tcpip-server library
#* and generating pkgconfig file
#* and making install & uninstall
#*
#* Copyright of Timo Hannukkala. All rights reserved.
#*
#* \author Timo Hannukkala <timohannukkala@hotmail.com>
#*/
TARGET:=libprj-tcpip-server.so
VERSION:=0.1
PREFIX:=/usr
LIBDIR:= $(PREFIX)/lib
PKGCONFIGDIR:=$(LIBDIR)/pkgconfig
PKGCONFIGFILE:=prj-tcpip-server
CXX:=$(if $(CXX),$(CXX),g++)
CXXFLAGS+=-Wuninitialized -Wconversion -Wshadow -Wpointer-arith \
	-Wswitch-default -Wswitch-enum -Wcast-align \
	-Wundef -Wcast-qual -Wunreachable-code -Wfloat-equal \
	-Wredundant-decls -Wextra -Wall
CXXFLAGS+=-Werror
CXXFLAGS+=-O2
CXXFLAGS+=-g -fPIC -std=c++17
LDFLAGS+=-shared
CURRENT_DIR=$(dir $(abspath $(lastword $(MAKEFILE_LIST))))
# writes a .d file next to each object listing the headers it includes,
# so that changing a header rebuilds the objects that include it
DEPFLAGS:=-MMD -MP
# holds the last compile command, so that changing CXXFLAGS (e.g. the log level) rebuilds everything
FLAGSFILE:=$(CURRENT_DIR).compile_flags

src_SRCDIR:=$(CURRENT_DIR)src
src_SRCS:=$(wildcard $(src_SRCDIR)/*.cpp)
src_OBJS:=$(src_SRCS:.cpp=.o)

src_private_SRCDIR:=$(CURRENT_DIR)src/private
src_private_SRCS:=$(wildcard $(src_private_SRCDIR)/*.cpp)
src_private_OBJS:=$(src_private_SRCS:.cpp=.o)

default: $(src_OBJS) $(src_private_OBJS)
	$(CXX) $(src_OBJS) $(src_private_OBJS) $(LDFLAGS) -o $(TARGET)

$(src_OBJS):%.o: %.cpp $(FLAGSFILE)
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -c $< -o $@

$(src_private_OBJS):%.o: %.cpp $(FLAGSFILE)
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -c $< -o $@

install: $(PKGCONFIGDIR)/$(PKGCONFIGFILE).pc
	mkdir -p $(LIBDIR)/prj-tcpip-server
	cp lib*.so $(LIBDIR)/prj-tcpip-server
	mkdir -p $(PREFIX)/include/prj-tcpip-server
	cp src/prj_tcpip_server_lib.h $(PREFIX)/include/prj-tcpip-server

clean:
	rm -f src/*.o
	rm -f src/private/*.o
	rm -f src/*.d
	rm -f src/private/*.d
	rm -f $(FLAGSFILE)
	rm -f $(TARGET)

# rewritten only when the compile command changes, so that its timestamp shows when it did
$(FLAGSFILE): FORCE
	@echo '$(CXX) $(CXXFLAGS)' | cmp -s - $@ || echo '$(CXX) $(CXXFLAGS)' > $@

FORCE:


$(PKGCONFIGDIR)/$(PKGCONFIGFILE).pc:
	@echo "installing $(PKGCONFIGDIR)/$(PKGCONFIGFILE).pc"
	@mkdir -p $(PKGCONFIGDIR)
	@echo "prefix=$(PREFIX)" > $@
	@echo "exec_prefix=\$${prefix}" >> $@
	@echo "libdir=\$${prefix}/lib/prj-tcpip-server" >> $@
	@echo "includedir=\$${prefix}/include/prj-tcpip-server" >> $@
	@echo "" >> $@
	@echo "Name: prj-tcpip-server" >> $@
	@echo "Description: prj-tcpip-server lib makes tcpip-server" >> $@
	@echo "Version: $(VERSION)" >> $@
	@echo "Cflags: -I\$${includedir}" >> $@
	@echo "Libs: -L\$${libdir} -lprj-tcpip-server -Wl,--rpath=\$${libdir}" >> $@

uninstall:
	rm -rf $(LIBDIR)/prj-tcpip-server
	rm -rf $(PREFIX)/include/prj-tcpip-server
	rm -f $(PKGCONFIGDIR)/$(PKGCONFIGFILE).pc

-include $(src_OBJS:.o=.d) $(src_private_OBJS:.o=.d)
