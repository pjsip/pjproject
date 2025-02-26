# pjsua2maui - pjsip on .net maui 

## STEPS FOR BUILDING:

### Android

- SETUP:
    - Visual Studio Code & .NET MAUI - [Install steps](https://learn.microsoft.com/en-us/dotnet/maui/get-started/installation?view=net-maui-11.0&tabs=visual-studio-code)
    - Java 17 
    - Android NDK r21d
    - pjsip v2.14.1

-  Run the following commands:

```bash 
    export ANDROID_SDK_ROOT=/somewhere/there/is/installed/the/sdk/
    sdkmanager --install "ndk;28.0.12674087" --channel=3 --sdk_root=$ANDROID_SDK_ROOT
    sdkmanager --install "cmake;3.31.0" --sdk_root=$ANDROID_SDK_ROOT
    sdkmanager --install "build-tools;35.0.0" "platform-tools" "platforms;android-35" --sdk_root=$ANDROID_SDK_ROOT
    echo yes | sdkmanager --licenses --sdk_root=$ANDROID_SDK_ROOT

```

- Create the following variable: 

```bash

export ANDROID_NDK_ROOT=/path/to/ndk/root/installation

```

- Then build pjsip

```bash

TARGET_ABI=x86_64  ./configure-android
make dep && make clean && make

```


after run make dep and make, run the make on ``` pjsip-apps/src/swig/csharp ``` folder

```bash

make 

```

then start the android emulator, go to ``` pjsip-apps/src/swig/csharp/pjsua2maui/pjsua2maui ``` and run:

```bash

dotnet workload restore
dotnet restore
dotnet build -t:Run -c Debug -f net9.0-android  

```



### iOS

- SETUP:
    - Visual Studio Code & .NET MAUI - [Install steps](https://learn.microsoft.com/en-us/dotnet/maui/get-started/installation?view=net-maui-11.0&tabs=visual-studio-code)
    - Xcode 16.1 
    - pjsip v2.14.1

- PREPARING:

Create the following variable:

```bash

export DEVPATH="`xcrun -sdk iphonesimulator --show-sdk-platform-path`/Developer"

```


- Building pj stack:

Create the following ``` config_site.h ```  :

```c 

#define PJ_CONFIG_IPHONE 1

// disable background VoIP socket, use PushKit
#undef PJ_IPHONE_OS_HAS_MULTITASKING_SUPPORT
#define PJ_IPHONE_OS_HAS_MULTITASKING_SUPPORT 0

#ifndef __IPHONE_OS_VERSION_MIN_REQUIRED
#define __IPHONE_OS_VERSION_MIN_REQUIRED 122000
#endif

#include <pj/config_site_sample.h>

```



Run configure with the following flags to build the lib to run on simulator:

```bash

MIN_IOS="-miphoneos-version-min=12.2" ARCH="-arch x86_64" CFLAGS="-O2 -m32 -mios-simulator-version-min=12.2 -fembed-bitcode" LDFLAGS="-O2 -m32 -mios-simulator-version-min=12.2 -fembed-bitcode" ./configure-iphone

```

after run make dep and make, run the make on ``` pjsip-apps/src/swig/csharp ``` folder

```bash

make 

```

then go to ``` pjsip-apps/src/swig/csharp/pjsua2maui/pjsua2maui ``` and run:

```bash

dotnet workload restore
dotnet restore
dotnet build -t:Run -c Debug -f net9.0-ios -p:_DeviceName=:v2:udid=UDID_OF_YOUR_EMULATOR

```
