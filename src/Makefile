CC = gcc
CXX = g++
CFLAGS = -Wall -Wextra -Wdeprecated-declarations -Wimplicit-function-declaration -Wimplicit-int -Wpointer-arith -Wwrite-strings -Wmissing-noreturn -Wno-unused-parameter -Wsuggest-attribute=format -Wsuggest-attribute=noreturn -Wunused-variable -Winit-self -Wshadow -Wlogical-op -Wconversion -c -O3
CXXFLAGS = -Wall -Wextra -Wdeprecated-declarations -Wpointer-arith -Wwrite-strings -Wmissing-noreturn -Wno-unused-parameter -Wsuggest-attribute=format -Wsuggest-attribute=noreturn -Wunused-variable -Winit-self -Wshadow -Wlogical-op -Wconversion -c -O3
INCLUDES = 
LD = gcc
LDFLAGS = -shared -Wl,--dll,--enable-stdcall-fixup -static
LIBS = 
RM = rm -f

TARGET = la_smooth_ks.auf
OBJ = la_smooth_ks.o
DEF = la_smooth_ks.def

.SUFFIXES: .cpp .o
.SUFFIXES: .c .o

all: $(TARGET)

$(TARGET): $(OBJ) $(DEF)
	$(LD) $(LDFLAGS) -o $(TARGET) $(OBJS) $(RES) $(DEF) $(LIBS)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $<

.cpp.o:
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $<

la_smooth_ks.o: filter.h

clean:
	rm -f $(TARGET) $(OBJ)

release:
	make all
	strip $(TARGET)
	cp -f $(TARGET) ../
	make clean
