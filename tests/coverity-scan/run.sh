#!/bin/bash

# Automatic exit on any error
set -e

if [ "$1" == "--help" ] || [ "$1" == "-h" ] ; then
  echo Options:
  echo
  echo ' -t, --test     Testng mode: run but do not submit'
  echo ' -h, --help     Display this help'
  exit 0
fi

if [ "$1" == "-t" ] || [ "$1" == "--test" ]; then
  TESTING=1
  echo Testing mode
else
  if [ "$COV_TOKEN" == "" ] ; then
    echo "Error: COV_TOKEN env var is not set"
    exit 1
  fi
fi

if [ `whoami` == "root" ] ; then
  SUDO=
else
  SUDO=sudo
fi

if ! [ -f tests/coverity-scan/packages.txt ] ; then
  echo You need to run this from pjproject root directory
  exit 1
fi

mkdir -p tmp

# Get PJ version and branch name
cat << EOF > getversion.mak
include version.mak

all:
	@echo \$(PJ_VERSION)
EOF
export MQUIET='--quiet'
export PJ_VERSION=`make -f getversion.mak`
export GIT_BRANCH=`git branch --show-current`
echo PJSIP version $PJ_VERSION on $GIT_BRANCH

echo
echo ===============================
echo Installing packages
echo ===============================
$SUDO apt update -y
cat tests/coverity-scan/packages.txt | xargs $SUDO apt-get -y -qq install

echo
echo ===============================
echo Download Coverity
echo ===============================
pushd tmp
if ! [ -d cov-analysis-linux64 ] ; then
  if ! [ -f cov-analysis-linux64.tar.gz ] ; then
    wget -q https://scan.coverity.com/download/cxx/linux64 --post-data "token=${COV_TOKEN}&project=PJSIP" -O cov-analysis-linux64.tar.gz
  fi
  mkdir cov-analysis-linux64
  tar xzf cov-analysis-linux64.tar.gz --strip 1 -C cov-analysis-linux64
fi
cd cov-analysis-linux64/bin
export PATH=$PATH:`pwd`
popd

echo
echo ===============================
echo Building SILK
echo ===============================
pushd tmp
if ! [ -f silk-src-1.0.9.zip ] ; then
  wget -q https://github.com/pjsip/third_party_libs/raw/main/silk-src-1.0.9.zip
fi
unzip -o -qq silk-src-1.0.9.zip
cd silk-1.0.9/sources/SILK_SDK_SRC_FLP_v1.0.9
make $MQUIET
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

/* Enable additional features (by tests/coverity-scan/run.sh) */
#define PJ_HAS_IPV6                  1
#define PJ_HAS_SSL_SOCK              1
#define PJ_TIMER_DEBUG               0
#define PJ_TODO(x)

#define PJMEDIA_HAS_VIDEO            1
#define PJMEDIA_RTCP_STAT_HAS_IPDV   1
#define PJMEDIA_HAS_RTCP_XR          1
#define PJMEDIA_STREAM_ENABLE_XR     1
#define PJMEDIA_SRTP_HAS_DTLS        1
#define PJMEDIA_SRTP_HAS_AES_CM_192  1
#define PJMEDIA_SRTP_HAS_AES_GCM_256 1
#define PJMEDIA_SRTP_HAS_AES_GCM_128 1
#define PJMEDIA_STREAM_ENABLE_KA     2

#define PJSIP_CHECK_VIA_SENT_BY      1

EOF
popd

echo
echo config_site.h:
echo ----------------------------------------
cat pjlib/include/pj/config_site.h
echo

echo
echo ===============================
echo Build PJPROJECT
echo ===============================
echo make dep clean..
make $MQUIET dep clean
rm -rf cov-int
cov-build --dir cov-int make $MQUIET


echo
echo ===============================
echo Submit scan
echo ===============================

rm -f tmp/cov-int.bz2
tar caf tmp/cov-int.bz2 cov-int

if [ "$TESTING" == "1" ] ; then
  echo Testing mode, showing curl command:
  CURL="echo curl"
  SAFE_COV_TOKEN=`echo $COV_TOKEN | sed 's/[a-zA-Z]/*/g'`
else
  CURL="curl"
  SAFE_COV_TOKEN=$COV_TOKEN
fi

$CURL --form token=${SAFE_COV_TOKEN} --form email=bennylp@pjsip.org --form file=@tmp/cov-int.bz2 \
      --form version=\"$PJ_VERSION@$GIT_BRANCH\" --form description=- \
      https://scan.coverity.com/builds?project=PJSIP

exit 0


echo swig bindings
cd pjsip-apps/src/swig && make $MQUIET

