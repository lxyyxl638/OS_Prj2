all: BDC BDS RDC IDS

BDC:BDC.cpp
	g++ BDC.cpp -o BDC
BDS:BDS.cpp
	g++ BDS.cpp -o BDS
RDC:RDC.cpp
	g++ RDC.cpp -o RDC -pthread
IDS:IDS.cpp
	g++ IDS.cpp -o IDS -pthread
