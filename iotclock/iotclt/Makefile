#make sure your cross compiler path
CROSS = ../../tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian/bin/arm-linux-gnueabihf-gcc 

all:	clean iotclt.c
	cc -Os -Wall -Wextra iotclt.c -o iotclt

cross:	clean iotclt.c
	$(CROSS) -Os -Wall -Wextra iotclt.c -o iotclt
clean:	
	rm -f iotclt
