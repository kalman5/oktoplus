##### Pre-requisites
- Grpc (refer to https://github.com/grpc/grpc/blob/master/INSTALL.md to install it on your system)
- Glog
 
***

##### How to build (for optimized mode replace debug with optimized)
* git clone --recurse-submodules https://github.com/kalman5/oktoplus.git
* cd oktoplus
* mkdir build/debug
* cd build/debug
* ../buildme.sh
* make

##### Tested with:

Component | Versions |  |  |  |
--- |:---:|:---:|:---: |:---:|
GRPC | 1.11.0 |  | |
g++ | 7.2.0 | 7.3.0 | 7.4 |
clang | 5.0 | 6.0 | 7.0 | 9.0 |
Ubuntu | 16.04 | 17.10 |  |

##### Run unit tests
You can find tests in build/debug/src/TestUnits
