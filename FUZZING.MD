### Export Flags

```
export CC=clang
export CXX=clang++
export LIB_FUZZING_ENGINE=-fsanitize=fuzzer
export CFLAGS="-O1 -fno-omit-frame-pointer -gline-tables-only -DFUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION -fsanitize=address -fsanitize-address-use-after-scope -fsanitize=fuzzer-no-link"
export CXXFLAGS="$CFLAGS"
export LDFLAGS="$CFLAGS"
```

### Compile Guide

Note: While compiling fuzzing suite, Don't use bloat libraries.

```
./configure --disable-ffmpeg --disable-ssl --disable-speex-aec --disable-speex-codec --disable-g7221-codec --disable-gsm-codec --disable-ilbc-codec --disable-resample --disable-libsrtp --disable-libwebrtc --disable-libyuv

make dep
make -j$(nproc)
make fuzz
```


### Run

```
cd tests/fuzz
mkdir cov-json
unzip seed/fuzz-json_seed_corpus.zip
./fuzz-json cov-json/ fuzz-json_seed_corpus/
```

