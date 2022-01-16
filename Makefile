all:	
	cd iotser && make all 
	cd iotclt && make all

crossall:	
	cd iotser && make crossall 
	cd iotclt && make crossall
install:
	cd iotser && install -m 0755 iotser ../../
	cd iotclt && install -m 0755 iotclt ../../
	cd iotclt && install -m 0755 remoteIot ../../
