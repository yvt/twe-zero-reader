default: all
all: twe-zero-reader

twe-zero-reader: twe-zero-reader.cpp
	$(CXX) -o twe-zero-reader twe-zero-reader.cpp -std=c++11 -g
