ifeq ($(origin CXX),default)
CXX = g++
endif

CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2
TARGET = rkcfgtool
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin

all: $(TARGET)

$(TARGET): rkcfgtool.cpp rkcfg.cpp version.hpp rkcfg.hpp
	$(CXX) $(CXXFLAGS) -o $@ rkcfgtool.cpp rkcfg.cpp

install: $(TARGET)
		install -d $(DESTDIR)$(BINDIR)
		install -m 0755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)

lint:
	        $(CXX) $(CXXFLAGS) -fsyntax-only rkcfgtool.cpp rkcfg.cpp

test: $(TARGET)
	./test.sh

clean:
	rm -f $(TARGET)