CXX = x86_64-w64-mingw32-g++
CXXFLAGS = -O2 -mwindows -std=c++17 -static
LIBS = -lws2_32 -lgdi32 -lmsimg32

all: telegram.exe

telegram.exe: main.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LIBS)
	strip $@

clean:
	rm -f telegram.exe
