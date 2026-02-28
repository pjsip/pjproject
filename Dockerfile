FROM ubuntu:22.04

WORKDIR /pjsip

COPY . /pjsip

RUN apt-get update && apt-get install -y \
    swig \
    python3 \
    python3-pip \
    python3-venv \
    build-essential \
    gcc \
    g++ \
    make \
    autoconf \
    automake \
    libtool \
    git \
    wget \
    ffmpeg \
    iputils-ping \
    && rm -rf /var/lib/apt/lists/*

RUN export CFLAGS="$CFLAGS -fPIC"
RUN export CXXFLAGS="$CXXFLAGS -fPIC"

RUN ./configure --enable-shared CFLAGS="-fPIC" CXXFLAGS="-fPIC"

RUN make dep && make
RUN make install

WORKDIR /pjsip/pjsip-apps/src/swig/python

RUN make && make install

ENV LD_LIBRARY_PATH=/usr/local/lib:/pjsip/pjlib/lib:/pjsip/pjlib-util/lib:/pjsip/pjnath/lib:/pjsip/pjmedia/lib:/pjsip/pjsip/lib:/pjsip/third_party/lib

RUN python3 -c "import pjsua2; print('PJSIP Python Bindings Installed Successfully!')"

WORKDIR /app
