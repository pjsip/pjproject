# pjsua2maui - pjsip on .net maui 

## STEPS FOR BUILDING:

### Android

- SETUP:
    - Visual Studio Code & .NET MAUI - [Install steps](https://learn.microsoft.com/en-us/dotnet/maui/get-started/installation?view=net-maui-11.0&tabs=visual-studio-code)
    - Java 17 
    - Android NDK r21d or later
    - pjsip v2.14.1 or later

-  Run the following commands:

```bash 
    export ANDROID_SDK_ROOT=/somewhere/there/is/installed/the/sdk/
    sdkmanager --install "ndk;28.0.12674087" --channel=3 --sdk_root=$ANDROID_SDK_ROOT
    sdkmanager --install "cmake;3.31.0" --sdk_root=$ANDROID_SDK_ROOT
    sdkmanager --install "build-tools;35.0.0" "platform-tools" "platforms;android-35" --sdk_root=$ANDROID_SDK_ROOT
    echo yes | sdkmanager --licenses --sdk_root=$ANDROID_SDK_ROOT
```
note:
If you are sharing the Android SDK location with Android Studio, the recommended way to install/update the SDK and accept licenses is by using Android Studio.

- Create the following variable: 

```bash

export ANDROID_NDK_ROOT=/path/to/ndk/root/installation

```

- Then build pjsip

For device: 
```bash
    ./configure-android
```

For simulator:
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
or, for run in device:

Open Android Studio and connect an Android device, such as by using "Pair Devices Using Wifi"
(Optional) Start Logcat in VS Code, for obtaining Android log.
Install Logcat: View-Extensions. Install Logcat extension.
Start Logcat: View-Command Pallette.

### iOS

- SETUP:
    - Visual Studio Code & .NET MAUI - [Install steps](https://learn.microsoft.com/en-us/dotnet/maui/get-started/installation?view=net-maui-11.0&tabs=visual-studio-code)
    - Xcode 16.1 or later
    - pjsip v2.14.1 or later

- PREPARING:
 
- Building pj stack:

Create the following ``` config_site.h ```  :

```c 

#define PJ_CONFIG_IPHONE 1
 
#include <pj/config_site_sample.h>

```
Create the following variable:

```bash

export DEVPATH="`xcrun -sdk iphonesimulator --show-sdk-platform-path`/Developer"

```
Run configure with the following flags to build the lib to run on simulator:

```bash

MIN_IOS="-miphoneos-version-min=12.2" ARCH="-arch x86_64" CFLAGS="-O2 -m32 -mios-simulator-version-min=12.2 -fembed-bitcode" LDFLAGS="-O2 -m32 -mios-simulator-version-min=12.2 -fembed-bitcode" ./configure-iphone

```
For run on device: 

```bash
./configure-iphone
```

after run make dep and make, run the make on ``` pjsip-apps/src/swig/csharp ``` folder

```bash

make 

```

then go to ``` pjsip-apps/src/swig/csharp/pjsua2maui/pjsua2maui ``` and run:

For simulator:

```bash
dotnet workload restore
dotnet restore
dotnet build -t:Run -c Debug -f net9.0-ios -p:_DeviceName=:v2:udid=UDID_OF_YOUR_EMULATOR
```

For device:

```bash
dotnet workload restore
dotnet restore
dotnet build -t:Run -c Debug -f net9.0-ios -p:_DeviceName=:v2:udid=UDID_OF_YOUR_DEVICE
```

For the codesign, here are the steps:

```bash
dotnet publish -c Debug -f net9.0-ios -p:RuntimeIdentifier=ios-arm64
```
Open the resulting .ipa file in Xcode: ``` pjsip-apps/src/swig/csharp/pjsua2maui/pjsua2maui/bin/Debug/net9.0-ios/ios-arm64/publish/pjsua2maui.ipa ```

Drag it to your connected device under "Devices and Simulators."
Xcode will handle the signing and deployment.
