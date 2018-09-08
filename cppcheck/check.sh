#!/bin/bash

CPPCHECK=/home/kalman/cppcheck/cppcheck-1.84/cppcheck
OPTIONS="--library=posix --library=std --library=gnu \
         --platform=unix64 --std=c++11 \
         --suppressions-list=cppcheck.suppression \
         --enable=all \
         --xml \
         --xml-version=2 \
         -I../src/Libraries \
         -I../src/TestUnit \
         --force \
         --suppress=syntaxError --suppress=noExplicitConstructor --suppress=unusedPrivateFunction \
         -j 8"

${CPPCHECK} ${OPTIONS} ../src 2>report.xml

