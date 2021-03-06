# Various flags
#CXX  = clang++
CXX = g++-6
LINK = $(CXX)
#CXXFLAGS = -std=c++11 -Wall -g 
#CXXFLAGS = -std=c++11 -Wall -O3
CXXFLAGS = -std=c++11 -Wall -O3 -fopenmp
#LFLAGS = -lm
LFLAGS = -lm -fopenmp
# ifneq "$(findstring noomp, $(MAKECMDGOALS))" "noomp"
# 	CXXFLAGS += -fopenmp
# 	LFLAGS += -fopenmp
# endif

TARGET  = hpc

HEADER  = hpc.h
FILES = hpc.cc

OBJECTS = $(FILES:.cc=.o)

$(TARGET): ${OBJECTS}
	$(LINK) $^ $(LFLAGS) -o $@

all: $(TARGET)

clean:
	rm -f $(OBJECTS)

distclean:
	rm -f $(OBJECTS) $(TARGET)

# Compile and dependency
$(OBJECTS): $(HEADER) Makefile



