# Makefile for chat project

# Toolchain
CXX ?= /usr/bin/g++

# Paths
SRCDIR := src
BINDIR := bin
BUILDDIR := build
INCLUDEDIR := include

# Build mode: debug or release (use MODE=release)
MODE ?= debug

# Flag categories
CXXFLAGS_COMMON := -fdiagnostics-color=always -std=c++20 -I$(INCLUDEDIR)
CXXFLAGS_DEBUG := -g
CXXFLAGS_RELEASE := -O2 -DNDEBUG

ifeq ($(MODE),release)
CXXFLAGS := $(CXXFLAGS_COMMON) $(CXXFLAGS_RELEASE)
else
CXXFLAGS := $(CXXFLAGS_COMMON) $(CXXFLAGS_DEBUG)
endif

LDFLAGS := -lpthread

.PHONY: all server client clean dirs

all: dirs server client

dirs:
	mkdir -p $(BINDIR) $(BUILDDIR)

# Per-binary source lists (explicit so we can avoid accidental main collisions)
SERVER_SRCS := $(SRCDIR)/Server.cpp $(SRCDIR)/JSONTranslator.cpp $(SRCDIR)/Room.cpp $(SRCDIR)/RoomManager.cpp $(SRCDIR)/GameRoom.cpp

SERVER_OBJS := $(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/%.o,$(SERVER_SRCS))

# Client sources only if ChatClient.cpp exists
ifneq ($(wildcard $(SRCDIR)/ChatClient.cpp),)
CLIENT_SRCS := $(SRCDIR)/ChatClient.cpp $(SRCDIR)/JSONTranslator.cpp
CLIENT_OBJS := $(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/%.o,$(CLIENT_SRCS))
else
CLIENT_SRCS :=
CLIENT_OBJS :=
endif

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp | dirs
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@ -MF $(BUILDDIR)/$*.d

# Dependency files (generated with -MMD -MP)
DEPFILES := $(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/%.d,$(SERVER_SRCS) $(CLIENT_SRCS))

# Include dependency files if they exist (prevents stale builds when headers change)
-include $(DEPFILES)

server: $(BINDIR)/Server

client:
	@if [ -z "$(CLIENT_SRCS)" ]; then \
		echo "Skipping Client build: $(SRCDIR)/ChatClient.cpp not found"; \
	else \
		$(MAKE) $(BINDIR)/Client; \
	fi

$(BINDIR)/Server: $(SERVER_OBJS)
	$(CXX) $(CXXFLAGS) $(SERVER_OBJS) -o $@ $(LDFLAGS)

$(BINDIR)/Client: $(CLIENT_OBJS)
	$(CXX) $(CXXFLAGS) $(CLIENT_OBJS) -o $@ $(LDFLAGS)

clean:
	rm -rf $(BUILDDIR) $(BINDIR)
