#!/bin/bash

# Automatic exit on any error
set -e

SUDO=
#SUDO=$SUDO

if ! [ -f tests/coverity-scan/packages.txt ] ; then
  echo You need to run this from pjproject root directory
  exit 1
fi

mkdir -p tmp

# Get PJ version
cat << EOF > getversion.mak
include version.mak

all:
	@echo \$(PJ_VERSION)
EOF
export PJ_VERSION=`make -f getversion.mak`
echo PJSIP version $PJ_VERSION

echo
echo ===============================
echo Installing packages
echo ===============================
$SUDO apt update -y
cat tests/coverity-scan/packages.txt | xargs $SUDO apt-get -y install

echo
echo ===============================
echo Building SILK
echo ===============================
pushd tmp
if ! [ -f silk-src-1.0.9.zip ] ; then
  wget https://github.com/pjsip/third_party_libs/raw/main/silk-src-1.0.9.zip
fi
unzip -o -qq silk-src-1.0.9.zip
cd silk-1.0.9/sources/SILK_SDK_SRC_FLP_v1.0.9
make -s
export SILK_DIR=`pwd`
popd

echo
echo ===============================
echo Configure
echo ===============================
./configure --with-silk=$SILK_DIR | tee configure.out
echo configure output is in configure.out

echo
echo Configuring config_site.h
pushd pjlib/include/pj
cp -f config_site_test.h config_site.h
cat << EOF >> config_site.h

#define PJMEDIA_HAS_VIDEO 1
#define PJ_TODO(x)
EOF
popd

echo
echo config_site.h:
echo ----------------------------------------
cat pjlib/include/pj/config_site.h
echo

echo
echo ===============================
echo Download Coverity
echo ===============================
pushd tmp
if ! [ -f cov-analysis-linux64.tar.gz ] ; then
  wget -q https://scan.coverity.com/download/cxx/linux64 --post-data "token=${COV_TOKEN}&project=PJSIP" -O cov-analysis-linux64.tar.gz
fi
if ! [ -d cov-analysis-linux64 ] ; then
  mkdir cov-analysis-linux64
  tar xzf cov-analysis-linux64.tar.gz --strip 1 -C cov-analysis-linux64
fi
cd cov-analysis-linux64/bin
export PATH=$PATH:`pwd`
popd

echo
echo ===============================
echo Build PJPROJECT
echo ===============================
if ! [ -d cov-int ] ; then
  make dep
  cov-build --dir cov-int make
fi


echo
echo ===============================
echo Submit scan
echo ===============================

if ! [ -f tmp/cov-int.bz2 ] ; then
  tar caf tmp/cov-int.bz2 cov-int
fi
CURL="echo curl"

$CURL --form token=${COV_TOKEN} --form email=bennylp@pjsip.org --form file=@tmp/cov-int.bz2 --form version="$PJ_VERSION" --form description="-" "https://scan.coverity.com/builds?project=PJSIP"


exit 0


echo swig bindings
cd pjsip-apps/src/swig && make

