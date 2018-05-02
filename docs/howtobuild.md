##### Prerequisite
- Grpc (refer to https://github.com/grpc/grpc/blob/master/INSTALL.md to install it on your system)
- Glog
 
***

##### How to build (for optimized mode replace debug with optimized)
* git clone --recurse-submodules https://bbgithub.dev.bloomberg.com/gmendola/oktoplus.git
* cd oktoplus
* mkdir build/debug
* cd build/debug
* ../buildme.sh
* make

##### Tested with:
- GRPC 1.11.0
- gcc 7.2.0 / clang 5.0
- Ubuntu 17.10 

##### Run unit tests
You can find tests in build/debug/src/TestUnits
