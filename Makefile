CXX=g++
CXXFLAGS=-O3
ROOT_VALUE=e3dcset

all: $(ROOT_VALUE)

$(ROOT_VALUE): clean
	$(CXX) $(CXXFLAGS) e3dcset.cpp RscpProtocol.cpp AES.cpp SocketConnection.cpp -o $@

test:
	@echo "Tests not yet implemented"
	@exit 1

clean:
	-rm $(ROOT_VALUE) $(VECTOR)
