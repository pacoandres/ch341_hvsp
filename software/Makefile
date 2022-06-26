# minimal makefile
# for ch341 gpio library
# (c) Paco Andrés GPL3
 
 
CC=g++

CFLAGS=-Wall -Wextra -O2 -g `pkg-config --cflags libusb-1.0`
#CFLAGS+= -ggdb 
SOURCES=main.cpp ch341i.cpp intelhex/intelhexclass.cpp
LIBS=-lusb-1.0
OBJECTS=$(SOURCES:.cpp=.o)
TARGET=ch341_hvsp

all: $(TARGET) 

$(TARGET): $(OBJECTS)
	$(CC) -o $@ $(OBJECTS) $(LIBS)

%.o : %.cpp
	$(CC) $(CFLAGS) -o $@ -c $<
	
clean:
	rm -f $(OBJECTS) $(TARGET)


