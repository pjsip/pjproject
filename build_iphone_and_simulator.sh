#!/bin/sh
# Oliver Epper <oliver.epper@gmail.com>

#
# ask user if she wants to overwrite files
#
read -p "This will overwrite site_config.h and user.mak. Continue?" -n 1 -r
if [[ ! $REPLY =~ ^[Yy]$ ]]
then
   [[ "$0" = "$BASH_SOURCE" ]] && exit 1 || return 1
fi

cat << EOF > pjlib/include/pj/config_site.h
#define PJ_CONFIG_IPHONE 1
#define PJ_HAS_SSL_SOCK 1
#define PJ_SSL_SOCK_IMP PJ_SSL_SOCK_IMP_APPLE
#include <pj/config_site_sample.h>
EOF

cat << EOF > user.mak
export CFLAGS += -Wno-unused-label -Werror
export LDFLAGS += -framework Network -framework Security
EOF



#
# build for simulator arm64 & create lib
#
IPHONESDK="/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator14.5.sdk" DEVPATH="/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform/Developer" ARCH="-arch arm64" MIN_IOS="-mios-simulator-version-min=14.5" ./configure-iphone
make dep && make clean
CFLAGS="-Wno-macro-redefined -Wno-unused-variable -Wno-unused-function -Wno-deprecated-declarations -Wno-unused-private-field" make

OUT_SIM_ARM64="out/sim_arm64"
mkdir -p $OUT_SIM_ARM64
# the Makefile is a little more selective about which .o files go into the lib
# so let's use libtool instead of ar
# ar -csr $OUT_SIM_ARM64/libpjproject.a `find . -not -path "./pjsip-apps/*" -name "*.o"`
libtool -static -o $OUT_SIM_ARM64/libpjproject.a `find . -not -path "./pjsip-apps/*" -not -path "./out/*" -name "*.a"`


#
# build for device arm64 & create lib
#
IPHONESDK="/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS.sdk" DEVPATH="/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer" ARCH="-arch arm64" MIN_IOS="-miphoneos-version-min=14.5" ./configure-iphone
make dep && make clean
CFLAGS="-Wno-macro-redefined -Wno-unused-variable -Wno-unused-function -Wno-deprecated-declarations -Wno-unused-private-field -fembed-bitcode" make

OUT_DEV_ARM64="out/dev_arm64"
mkdir -p $OUT_DEV_ARM64
libtool -static -o $OUT_DEV_ARM64/libpjproject.a `find . -not -path "./pjsip-apps/*" -not -path "./out/*" -name "*.a"`



#
# collect headers & create xcframework
#
LIBS="pjlib pjlib-util pjmedia pjnath pjsip" # third_party"
OUT_HEADERS="out/headers"
for path in $LIBS; do
	mkdir -p $OUT_HEADERS/$path
	cp -a $path/include/* $OUT_HEADERS/$path
done

XCFRAMEWORK="out/libpjproject.xcframework"
xcodebuild -create-xcframework -library $OUT_SIM_ARM64/libpjproject.a -library $OUT_DEV_ARM64/libpjproject.a -headers $OUT_HEADERS -output $XCFRAMEWORK