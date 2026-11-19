CXX = C:\msys64\ucrt64\bin\g++.exe
CXXFLAGS = -O3 -shared -fPIC -D_WIN32 -DNAPI_VERSION=3 -I./headers/node-v26.7.0/include/node
LDFLAGS = lib/node.lib

NODE_VER = v26.7.0
HEADERS_URL = https://nodejs.org/download/release/$(NODE_VER)/node-$(NODE_VER)-headers.tar.gz
LIB_URL = https://nodejs.org/download/release/$(NODE_VER)/win-x64/node.lib

all: build/Release/vrutti_bridge.node

headers/node-$(NODE_VER)/include/node/node_api.h:
	mkdir -p headers
	curl -sL $(HEADERS_URL) -o headers.tar.gz
	tar -xzf headers.tar.gz -C headers
	rm headers.tar.gz

lib/node.lib:
	mkdir -p lib
	curl -sL $(LIB_URL) -o lib/node.lib

build/Release/vrutti_bridge.node: headers/node-$(NODE_VER)/include/node/node_api.h lib/node.lib bridge.cpp
	mkdir -p build/Release
	$(CXX) $(CXXFLAGS) bridge.cpp -o build/Release/vrutti_bridge.node $(LDFLAGS)

clean:
	rm -rf build headers lib
