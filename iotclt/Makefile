#make sure your cross compiler path
CROSS = ../../tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian/bin/arm-linux-gnueabihf-gcc 

all:	clean iotclt.c remote.c
	cc -Os -Wall -Wextra iotclt.c -o iotclt
	cc -Os -Wall -Wextra remote.c -o remoteIot
crossall:	clean iotclt.c remote.c
	$(CROSS) -Os -Wall -Wextra iotclt.c -o iotclt
	$(CROSS) -Os -Wall -Wextra remote.c -o remoteIot
clean:	
	rm -f iotclt
	rm -f remoteIot
test :	test.c
	rm -f testclt
	cc -Os -Wall test.c -o testclt
