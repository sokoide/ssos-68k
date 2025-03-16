TARGET=makedisk
SRCS=main.c
OBJS=$(SRCS:.c=.o)

INCLUDE=
LIB=

CCFLAGS=$(INCLUDE) -g -O2
LDFLAGS=$(LIB)

OBJDUMP=objdump
#READELF=objdump -p
READELF=otool -hfal

.PHONY:
	all run clean dump readelf compiledb

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

run: $(TARGET)
	./$(TARGET) ../boot/BOOT.X.bin ../os/SSOS.X.bin ../ssos.xdf

%.o: %.c
	$(CC) -c $< -o $@ $(CCFLAGS)

clean:
	rm -rf $(TARGET) *.o

dump: $(TARGET)
	$(OBJDUMP) -D $(TARGET)


readelf: $(TARGET)
	$(READELF) $(TARGET)

compiledb: $(SRCS)
	compiledb make
