CXX = g++
CXXWFLAGS =  -Wall -Wextra -Wdeprecated-declarations -Wpointer-arith -Wwrite-strings -Wmissing-noreturn -Wno-unused-parameter -Wsuggest-attribute=format -Wsuggest-attribute=noreturn -Wunused-variable -Winit-self -Wshadow -Wlogical-op -Wconversion -Wold-style-cast
CXXFLAGS = -c -O3 -std=c++23
INCLUDES = 
LD = g++
LDFLAGS = -shared -Wl,--dll,--enable-stdcall-fixup
LIBS = -static
RM = rm -f
SH = bash

TARGET = la_smooth_ks.auf
OBJ = la_smooth_ks.o
DEF = la_smooth_ks.def

.SUFFIXES: .cpp .o

all: $(TARGET)

$(TARGET): $(OBJ) $(DEF)
	$(LD) $(LDFLAGS) -o $(TARGET) $(OBJ) $(RES) $(DEF) $(LIBS)

.cpp.o:
	$(CXX) $(CXXWFLAGS) $(CXXFLAGS) $(INCLUDES) -o $@ $<

la_smooth_ks.o: filter.hpp

clean:
	rm -f $(TARGET) $(OBJ)

release:
	make all
	strip $(TARGET)
	cp -f $(TARGET) ../
	$(SH) makezip.sh la_smooth_ks
	make clean
