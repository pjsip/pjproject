# PJSIP on Zephyr RTOS

This tree is a Zephyr module: `zephyr/module.yml` points Zephyr at `zephyr/Kconfig` and `zephyr/CMakeLists.txt`, which build pjlib, pjlib-util, pjnath, pjmedia and pjsip with the per-library CMake files, configured from Kconfig (`CONFIG_PJPROJECT_*`). An application adds this tree to `ZEPHYR_EXTRA_MODULES` and links the `pjproject` target. The platform layer is `pjlib/include/pj/compat/os_zephyr.h` and the `PJ_CONFIG_ZEPHYR` preset in `pjlib/include/pj/config_site_sample.h`.

## The embedded_ua sample

`zephyr/samples/embedded_ua` builds the `embedded_ua` sample from `pjsip-apps/src/samples` as a Zephyr application. It is a SIP user agent on pjsip and pjmedia (no pjsua-lib): it registers to a PBX, answers calls with audio loopback, and is controlled with SIP MESSAGE requests. See the comment block at the top of `embedded_ua.c` for everything it does.

This is the bring-up and reference application of the Zephyr port. The full guide is on docs.pjsip.org; this file is the short version.

## Requirements

- A Zephyr workspace (west, Zephyr 4.4 or later) and the Zephyr SDK.
- A SIP server. The sample registers to the address in `CONFIG_EMBUA_SIP_SERVER_IP`, by default 192.0.2.2, the host side of the TAP interface below.
- For QEMU and native_sim, a TAP interface on the host, created once:

```sh
sudo ip tuntap add dev zeth mode tap user $USER
sudo ip addr add 192.0.2.2/24 dev zeth
sudo ip link set zeth up
```

The guest is 192.0.2.1 (`zephyr/samples/embedded_ua/prj.conf`). If the SIP server is elsewhere, give its address with `-DCONFIG_EMBUA_SIP_SERVER_IP=\"a.b.c.d\"` on the west command line; the host routes for it, and a server on the host itself is reached directly.

## Building and running

From the Zephyr workspace, with its Python environment active:

```sh
west build -b native_sim/native/64 /path/to/pjproject/zephyr/samples/embedded_ua
build/zephyr/zephyr.exe -seed-random
```

native_sim runs the whole thing, Zephyr's network stack and POSIX layer included, as a Linux process that can be debugged with gdb. `-seed-random` matters: without it native_sim's entropy source starts from a fixed seed, so every run has the same Call-ID and CSeq and a registrar rejects the second REGISTER as a replay.

For the Cortex-M33 target emulated by QEMU:

```sh
west build -b mps2/an521/cpu0 /path/to/pjproject/zephyr/samples/embedded_ua
/path/to/pjproject/zephyr/samples/embedded_ua/run-qemu.sh build
```

`run-qemu.sh` replaces `west build -t run`, which does not work on the MPS2 boards: their LAN9118 is part of the machine, and Zephyr insists on adding it as a pluggable device, which QEMU rejects. The script attaches the board's NIC to the TAP interface named in `CONFIG_EMBUA_HOST_TAP_IFACE`.

The MPS2 build limits SRAM to 640 KB (`zephyr/samples/embedded_ua/boards/mps2_an521_cpu0.overlay`), the size of the STM32H563 the sample is sized for, so an image that would not fit the real part fails to link.

## Configuration

Kconfig, in `prj.conf` or on the command line:

- `CONFIG_EMBUA_SIP_SERVER_IP`, `CONFIG_EMBUA_USERNAME`, `CONFIG_EMBUA_PASSWORD`, `CONFIG_EMBUA_USE_REGISTRATION`: the account. Without registration the UA is reached at `sip:192.0.2.1:55060`.
- `CONFIG_PJPROJECT_*`: what pjproject builds in. Threads (`CONFIG_PJPROJECT_THREADS`, off: single threaded), TLS, SRTP, the codecs, log level. See `zephyr/Kconfig`.
- `CONFIG_PJPROJECT_CONFIG_SITE`: a custom `config_site.h` instead of the generated one, which selects the `PJ_CONFIG_ZEPHYR` preset in `pj/config_site_sample.h`.

Networking settings worth knowing about: `CONFIG_NET_IPV4_FRAGMENT` is on because a SIP INVITE with SDP is usually larger than the Ethernet MTU; `CONFIG_ZVFS_OPEN_MAX` must be at least `CONFIG_PJPROJECT_IOQUEUE_MAX_HANDLES` (the build checks); `CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE` is the heap that pjlib's pools come from.

## Known limitations

- QEMU's MPS2 boards cannot receive full-size (1514-byte) Ethernet frames with Zephyr's smsc911x driver, which asks for a receive buffer two bytes larger than the stack allows. Fragmented IP packets lose their first fragment. Until that is fixed in Zephyr, lower the MTU on the host side: `sudo ip link set zeth mtu 1400`.
- IPv6 is off on Zephyr: its `sockaddr_in6` layout differs from pjlib's.
- The random generator on QEMU has no entropy device; `CONFIG_TIMER_RANDOM_GENERATOR` in `zephyr/samples/embedded_ua/boards/mps2_an521_cpu0.conf` makes it at least differ per boot. A real board uses its hardware RNG.
