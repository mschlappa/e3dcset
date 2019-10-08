CXX=g++
ROOT_VALUE=e3dcset

all: $(ROOT_VALUE)

$(ROOT_VALUE): clean
	$(CXX) -O3 e3dcset.cpp RscpProtocol.cpp AES.cpp SocketConnection.cpp -o $@


clean:
	-rm $(ROOT_VALUE) $(VECTOR)
