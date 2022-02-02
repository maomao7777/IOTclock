all:	
	cd iotser && make all 
	cd iotclt && make all
clean:
	cd iotser && make clean
	cd iotclt && make clean
crossall:	
	cd iotser && make crossall 
	cd iotclt && make crossall
install:
	cd iotser && install -m 0755 iotser ../../
	cd iotclt && install -m 0755 iotclt ../../
	cd iotclt && install -m 0755 remoteIot ../../
