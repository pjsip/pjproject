# build

## build project
```
CC=afl-clang CXX=afl-clang++ CFLAGS="-fsanitize=address" CXXFLAGS="-fsanitize=address" LDFLAGS="-fsanitize=address" ./configure
CC=afl-clang CXX=afl-clang++ CFLAGS="-fsanitize=address" CXXFLAGS="-fsanitize=address" LDFLAGS="-fsanitize=address" make dep
CC=afl-clang CXX=afl-clang++ CFLAGS="-fsanitize=address" CXXFLAGS="-fsanitize=address" LDFLAGS="-fsanitize=address" make lib
```

## build harness
Take `dns_harness.c` as an example.
```
afl-clang dns_harness.c -o dns_harness -Wall -DPJ_AUTOCONF=1 \
-I../../pjsip/include/ \
-I../../pjlib/include/ \
-I../../pjlib-util/include/ \
-I../../pjnath/include/ \
-I../../pjmedia/include/ \
-L../../pjsip/lib/ \
-L../../pjlib/lib/ \
-L../../pjlib-util/lib/ \
-L../../pjnath/lib/ \
-L../../pjmedia/lib/ \
-lpjsua-x86_64-unknown-linux-gnu \
-lpj-x86_64-unknown-linux-gnu \
-lpjsua2-x86_64-unknown-linux-gnu \
-lpjlib-util-x86_64-unknown-linux-gnu \
-lpjsua2-x86_64-unknown-linux-gnu \
-lpjsip-x86_64-unknown-linux-gnu \
-lpjnath-x86_64-unknown-linux-gnu \
-lpjmedia-x86_64-unknown-linux-gnu \
-lpjmedia-videodev-x86_64-unknown-linux-gnu \
-lpjmedia-codec-x86_64-unknown-linux-gnu \
-lpjmedia-audiodev-x86_64-unknown-linux-gnu \
-lilbccodec-x86_64-unknown-linux-gnu \
-lgsmcodec-x86_64-unknown-linux-gnu \
-lg7221codec-x86_64-unknown-linux-gnu \
-lspeex-x86_64-unknown-linux-gnu \
-lresample-x86_64-unknown-linux-gnu \
-lpthread -lm -fsanitize=address
```

Let's fuzz!