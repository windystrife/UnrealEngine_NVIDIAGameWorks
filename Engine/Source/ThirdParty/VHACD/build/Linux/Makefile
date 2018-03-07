# Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#
# Makefile for building VHACD on Linux (or Cygwin, if cross-building)
#

ROOT_DIR = ../../src
LIB_SRCS = $(wildcard $(ROOT_DIR)/*.cpp)

LIB_OBJS := $(LIB_SRCS:.cpp=.o)

LIB_FPIC_OBJS := $(LIB_SRCS:.cpp=.of)

# expecting TARGET_ARCH to be set
LIB = ../../Lib/Linux/$(TARGET_ARCH)/libVHACD.a
LIB_FPIC = ../../Lib/Linux/$(TARGET_ARCH)/libVHACD_fPIC.a

all: lib 
lib: $(LIB) $(LIB_FPIC)

DEBUG = 0
CC = clang
CXX = clang++
CPPFLAGS = -Wno-switch -Wno-unused-value -Wno-unused-variable -nostdinc++ -I../../../Linux/LibCxx/include/c++/v1 -I../../inc/ -fvisibility=hidden
CXXFLAGS = -std=c++11

ifeq ($(DEBUG),1)
CPPFLAGS += -g -O0
else
CPPFLAGS += -DNDEBUG -O2
endif


%.o: %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ -c $^

%.of: %.cpp
	$(CXX) $(CPPFLAGS) -fPIC $(CXXFLAGS) -o $@ -c $^

$(LIB): $(LIB_OBJS)
	ar cr $@ $^

$(LIB_FPIC): $(LIB_FPIC_OBJS)
	ar cr $@ $^

clean:
	rm -f $(LIB_OBJS) $(LIB) $(LIB_FPIC_OBJS) $(LIB_FPIC)

.PHONY: all lib clean
