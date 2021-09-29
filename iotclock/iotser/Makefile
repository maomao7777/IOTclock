#make sure your cross compiler path
CROSS = ../../tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian/bin/arm-linux-gnueabihf-gcc 

all:	clean eloop.c iotser.c
	cc -Os -Wall  eloop.c iotser.c -lpthread -o iotser

cross:	clean eloop.c iotser.c
	$(CROSS) -Os -Wall  eloop.c iotser.c -lpthread -o iotser
clean:	
	rm -f iotclt

