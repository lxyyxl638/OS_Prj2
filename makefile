all: BDC BDS RDC IDS FC FS

BDC:BDC.cpp
	g++ BDC.cpp -o BDC
BDS:BDS.cpp
	g++ BDS.cpp -o BDS
RDC:RDC.cpp
	g++ RDC.cpp -o RDC -pthread
IDS:IDS.cpp server.h
	g++ IDS.cpp -o IDS -pthread
FC:FC.cpp client.h
	g++ FC.cpp -o FC
FS:FS.cpp FServer.h
	g++ FS.cpp -o FS
