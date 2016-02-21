#!/bin/sh -eux

# Download newer version of cmake. We need 3.1 at least, travis provides 2.8.
CMAKE_URL="http://cmake.org/files/v3.3/cmake-3.3.1-Linux-x86_64.tar.gz"
mkdir cmake
curl -L ${CMAKE_URL} | tar --strip-components=1 -xz -C cmake
export PATH=${TRAVIS_BUILD_DIR}/cmake/bin:${PATH}

# GTest files
mkdir ${GTEST_DIR}
cp -r /usr/src/gtest/* ${GTEST_DIR}

# Create a build dir and change to that.
mkdir ${TRAVIS_BUILD_DIR}/buildtest
cd ${TRAVIS_BUILD_DIR}/buildtest

# Execute cmake and build and test.
cmake -DCMAKE_CXX_COMPILER=$CMAKE_CXX_COMPILER ${TRAVIS_BUILD_DIR}/test/
make 
make test
cd ${TRAVIS_BUILD_DIR}