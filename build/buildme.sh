#!/bin/bash

echo "Last build command: " $0 $@ > last_build_command.txt

CURRENTDIR=${PWD##*/}
CCOMPILER=gcc
CXXCOMPILER=g++
NINJABUILD=0
USE_CLANG=0

OPTIONERROR=0
OPTIND=1 # Reset is necessary if getopts was used previously in the script.  It is a good idea to make this local in a function.
while getopts "h?cn" opt; do
  case "$opt" in
    h|\?)
      cat << EOF
Accepted options:
  -c: to compile with clang
  -h: show this help
  -n: to enable ninjabuild (executable must be in /usr/local/bin or /usr/bin)
EOF
      exit 0
      ;;
    c)  USE_CLANG=1
        echo "Selecting clang"        
      ;;
    n)  NINJABUILD=1
      ;;
  esac
done

shift $((OPTIND-1)) # Shift off the options and optional --.

if [ $OPTIONERROR -gt 0 ]; then
  exit -1
fi

if [ ${NINJABUILD} -eq 1 ];
then
  if [[ -x /usr/local/bin/ninja || -x /usr/bin/ninja ]]
  then
    echo "NINJABUILD ON"
    CMAKENINJAOPTION="-G Ninja"
  else
    echo "Ninja executable not found, re-run without '-n'"
    exit -1
  fi
else
  echo "NINJABUILD OFF"
fi

if [[ ${USE_CLANG} -eq 1 ]];
then
  CCOMPILER=clang
  CXXCOMPILER=clang++
fi

CMAKECOMMAND="cmake \
              -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
              ${CMAKENINJAOPTION} \
              -DCMAKE_CXX_COMPILER=${CXXCOMPILER} \
              -DCMAKE_C_COMPILER=${CCOMPILER} \
              -DCMAKE_BUILD_TYPE=${CURRENTDIR} \
              ../../"

echo "================================================================"
echo "===================== CMAKE COMMAND ============================"
echo "================================================================"
echo ""
echo ""
echo $CMAKECOMMAND
echo ""
echo ""
echo "================================================================"
echo "================================================================"
echo "================================================================"

${CMAKECOMMAND}

if [ "${CMAKENINJAOPTION}" != "" ];
then
  echo
  echo
  echo "Building with ninja"
  echo
  echo

  ninja
else
  cores=1
  if [ -r /proc/cpuinfo ] ; then
    cores=`cat /proc/cpuinfo | grep processor | wc -l`
  elif [ -x "/usr/sbin/sysctl" ] ; then
    cores=`sysctl -n hw.physicalcpu`
  fi
  CPUNUM=$cores

  echo
  echo
  echo Building using cmake with ${CXXCOMPILER} and ${CPUNUM} cores
  echo
  echo

  make -j ${CPUNUM} 
fi

