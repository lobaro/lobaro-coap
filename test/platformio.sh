#!/bin/sh -eux

pip install --user platformio

for TESTNAME in ArduinoBuildTest
do
	echo $(pwd)
	cd ${TRAVIS_BUILD_DIR}/test/$TESTNAME/
	platformio ci $TESTNAME.ino -l . -l ../../ -b $BOARD
done
cd ${TRAVIS_BUILD_DIR}