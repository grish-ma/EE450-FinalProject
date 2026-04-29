CC = gcc
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic

COMMON_CPP = net_utils.cpp text_proto.cpp file_utils.cpp crypto_utils.cpp
COMMON_OBJ = net_utils.o text_proto.o file_utils.o crypto_utils.o sha256.o

all: hospital_server authentication_server appointment_server prescription_server client

sha256.o: sha256.c sha256.h
	$(CC) -c sha256.c -o sha256.o

net_utils.o: net_utils.cpp net_utils.h
	$(CXX) $(CXXFLAGS) -c net_utils.cpp -o net_utils.o

text_proto.o: text_proto.cpp text_proto.h
	$(CXX) $(CXXFLAGS) -c text_proto.cpp -o text_proto.o

file_utils.o: file_utils.cpp file_utils.h
	$(CXX) $(CXXFLAGS) -c file_utils.cpp -o file_utils.o

crypto_utils.o: crypto_utils.cpp crypto_utils.h sha256.h text_proto.h
	$(CXX) $(CXXFLAGS) -c crypto_utils.cpp -o crypto_utils.o

hospital_server: hospital_server.cpp constants.h $(COMMON_OBJ)
	$(CXX) $(CXXFLAGS) hospital_server.cpp $(COMMON_OBJ) -o hospital_server

authentication_server: authentication_server.cpp constants.h $(COMMON_OBJ)
	$(CXX) $(CXXFLAGS) authentication_server.cpp $(COMMON_OBJ) -o authentication_server

appointment_server: appointment_server.cpp constants.h $(COMMON_OBJ)
	$(CXX) $(CXXFLAGS) appointment_server.cpp $(COMMON_OBJ) -o appointment_server

prescription_server: prescription_server.cpp constants.h $(COMMON_OBJ)
	$(CXX) $(CXXFLAGS) prescription_server.cpp $(COMMON_OBJ) -o prescription_server

client: client.cpp constants.h $(COMMON_OBJ)
	$(CXX) $(CXXFLAGS) client.cpp $(COMMON_OBJ) -o client

clean:
	rm -f *.o hospital_server authentication_server appointment_server prescription_server client

.PHONY: all clean
