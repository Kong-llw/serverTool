CXX = g++
CXXFLAGS = -fdignostics-color=always -g -std=c++20 -Wall -Wextra
INCLUDES = -I/root/vscodeProject/include
LDFLAGS = -lpthread

SRCDIR = .
OBJDIR = obj
BINDIR = /root/vscodeProject/bin

all: build

build: $(BINDIR)/$(basename $(notdir $(FILE)))
	@echo "build complite."

$(BINDIR)/%: $.cpp
	@mkdir -p $(BINDIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< -o $@ $(LDFLAGS)

clean:
	rm -f $(BINDIR)/*