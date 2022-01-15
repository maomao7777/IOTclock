#make sure your cross compiler path
CROSS = ../../tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian/bin/arm-linux-gnueabihf-gcc 

all:	clean eloop.c iotser.c
	cc -Os -Wall  eloop.c iotser.c -lpthread -o iotser

crossall:	clean eloop.c iotser.c
	$(CROSS) -Os -Wall  eloop.c iotser.c -lpthread -o iotser

clean:	
	rm -f iotser
test :	eloop.c test.c
	rm -f testser
	cc -Os -Wall  eloop.c test.c -lpthread -o testser
