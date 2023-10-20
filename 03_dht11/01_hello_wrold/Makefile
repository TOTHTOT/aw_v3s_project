
# Chosse proper compiler for your PI
# NanoPi:             arm-linux-gnueabi-gcc
# Raspberry Pi Zero:  arm-linux-gnueabi-gcc

# Raspberry Pi 2:     arm-linux-gnueabihf-gcc
# OrangePi Zero:      arm-linux-gnueabihf-gcc
# NanoPi NEO:         arm-linux-gnueabihf-gcc
# NanoPi NEO Plus 2:  arm-linux-gnueabihf-gcc

# C-SKY Linux:  csky-linux-gcc

CC=arm-linux-gnueabihf-gcc

# IP Address of your PI
PI=root@192.168.0.110

TARGET=hello_world
IDIR= -I ../../../drivers -I ../../../csrc -I ../../../port
SRCDIR=../../../csrc
OBJDIR=../../../obj
OUTDIR=./bin
LDIR= -L ../../../lib
LIBS=  -lm
NFS_DIR=/home/yyh/Learn/aw_v3s/nfs/root
LDFLAGS = -pthread
CFLAGS= $(IDIR) -W -Wall -D __ARM_LINUX__

OBJ+=$(TARGET).o\
	../../../port/u8g2port.o\
	../../../drivers/gpio.o\
	../../../drivers/spi.o\
	../../../drivers/i2c.o\

OBJ+=$(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o, $(wildcard $(SRCDIR)/*.c))

all: directories $(TARGET) 

directories:
	@mkdir -p $(OBJDIR)
	@mkdir -p $(OUTDIR)

$(TARGET):$(OBJ)
	@echo Generating $(TARGET) ...
	@$(CC) $(LDFLAGS) -o $(OUTDIR)/$@ $(OBJ) $(LDIR) $(LIBS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) -c -o $@ $< $(CFLAGS) $(LDIR) $(LIBS)

clean:
	@echo RM -rf $(OBJDIR)/
	@rm -rf $(OBJ)
	@rm -rf $(OBJDIR)
	
	@echo RM -rf $(OUTDIR)/
	@rm -rf $(OUTDIR)

upload:
	scp $(OUTDIR)/$(TARGET) $(PI):~/

move:
	sudo mv ./bin/$(TARGET) $(NFS_DIR)

run:
	ssh $(PI)
