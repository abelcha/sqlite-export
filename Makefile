CC := clang
XX := clang++
#  
CXXFLAGS := -O3 -march=native
INCLUDES := -I/opt/homebrew/include -I/usr/local/include
LDFLAGS := -L/opt/homebrew/lib -L/usr/local/lib -lsqlite3

# Check if we're on macOS and adjust flags if necessary
UNAME_S := $(shell uname -s)

# TARGET := sqlite-export litexp
TARGET := cpp-litexp litexp

litexp: main.c
	$(CC) $(CXXFLAGS) $(INCLUDES) -o $@ $< $(LDFLAGS) 

cpp-litexp: std-litexp.cpp 
	$(XX) -std=c++17 $(CXXFLAGS) $(INCLUDES) -o $@ $< $(LDFLAGS) 

all: litexp cpp-litexp
re: clean all
clean:
	trash $(TARGET)

.PHONY: all clean