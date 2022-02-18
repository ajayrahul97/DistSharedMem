LDFLAGS = -L/usr/local/lib `pkg-config --libs protobuf grpc++`\
           -Wl,--no-as-needed -lgrpc++_reflection -Wl,--as-needed\
           -ldl

LDFLAGS = -L/usr/local/lib `pkg-config --libs protobuf grpc++`\
-pthread\
-lgrpc++_reflection\
-ldl

CXX = g++
CPPFLAGS += `pkg-config --cflags protobuf grpc`
CXXFLAGS += -std=c++11

GRPC_CPP_PLUGIN = grpc_cpp_plugin
GRPC_CPP_PLUGIN_PATH ?= `which $(GRPC_CPP_PLUGIN)`

all: kmeans dir

kmeans: psudsm.pb.o psudsm.grpc.pb.o  kmeans.o
	$(CXX) $^ $(LDFLAGS) -o $@

dir: psudsm.pb.o psudsm.grpc.pb.o dir.o
	$(CXX) $^ $(LDFLAGS) -o $@

%.grpc.pb.cc: %.proto
	protoc --grpc_out=. --plugin=protoc-gen-grpc=$(GRPC_CPP_PLUGIN_PATH) $<

%.pb.cc: %.proto
	protoc --cpp_out=. $<

clean:
	rm kmeans dir kmeans.o dir.o psudsm.pb.o psudsm.grpc.pb.o 
