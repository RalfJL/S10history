CXX=/usr/bin/g++
ROOT_VALUE=S10history
LDFLAGS=-lrlog
CCFLAGS=-Irlog  -O2

all: $(ROOT_VALUE)

$(ROOT_VALUE): clean
	$(CXX) $(LDFLAGS) $(CCFLAGS)  -Wall   S10history.cpp RscpReader.cpp RscpProtocol.cpp AES.cpp SocketConnection.cpp -o $@


clean:
	-rm $(ROOT_VALUE) $(VECTOR)
