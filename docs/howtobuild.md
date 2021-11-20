##### Pre-requisites
- Grpc (refer to https://github.com/grpc/grpc/blob/master/INSTALL.md to install it on your system)
- Glog
 
***

##### How to build (for optimized mode replace debug with optimized)
* git clone --recurse-submodules https://github.com/kalman5/oktoplus.git
* cd oktoplus
* mkdir build/debug
* cd build/debug
* ../buildme.sh (specify -c to compile with clang)
* make

##### Tested with:

Component | Versions |
--- |:---:|
GRPC   | 1.38.0 |
g++    | 10.3   |
clang  | 13.0   |

##### Run unit tests
You can find tests in build/debug/src/TestUnits
