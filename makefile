all: BDS1 BDC RDC IDS BDS FC FS

BDS1:BDS1.cpp Bserver1.h
	g++ BDS1.cpp -o BDS1
BDC:BDC.cpp client.h
	g++ BDC.cpp -o BDC
RDC:RDC.cpp client.h
	g++ RDC.cpp -o RDC -pthread
IDS:IDS.cpp Iserver.h
	g++ IDS.cpp -o IDS -pthread
BDS:BDS.cpp Bserver.h
	g++ BDS.cpp -o BDS
FC:FC.cpp client.h
	g++ FC.cpp -o FC
FS:FS.cpp Fserver.h
	g++ FS.cpp -o FS
clear: rm *.o
