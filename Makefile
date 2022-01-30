CC=clang
LD=lld

INCS=-I/usr/include/freetype2/ -I./includes
CFLAGS=-O3 $(INCS)
LIBS=-lfreetype -lwayland-client 
LFLAGS=-fuse-ld=$(LD) $(LIBS)

TARGET=client 

BUILD=./build
CSRC=$(wildcard src/*.c)
COBJ=$(patsubst src/%.c, $(BUILD)/%.o, $(CSRC))

.PHONY: clean 

$(TARGET): $(BUILD) $(COBJ)
	$(CC) $(CFLAGS) $(LFLAGS) -o $(TARGET) $(COBJ) 

$(BUILD)/%.o: src/%.c 
	$(CC) $(CFLAGS) -c $^ -o $@

$(BUILD):
	mkdir -p $@

clean:
	rm -rf $(COBJ) $(BUILD) $(TARGET)
