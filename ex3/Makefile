obj-m := message_slot.o
KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

compile: message_reader message_sender

message_reader: message_reader.c
	gcc -O3 -Wall -std=c11 message_reader.c -o reader

message_sender: message_sender.c
	gcc -O3 -Wall -std=c11 message_sender.c -o sender
