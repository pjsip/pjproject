---
name: Bug report
about: Create a report to help us improve
title: ''
labels: 'type: bug'
assignees: ''

---

**Describe the bug**
A clear and concise description of what the bug is.

**To Reproduce**
Steps to reproduce the behavior:

1. run pjsua app with parameter: `--video --registrar sip:some.sip.registrar ...`
1. make two video calls to: `sip:abc` and `sip:xyz`1
1. setup video conference: `vid conf cc ...`
1. assertion occurs!

**Expected behavior**
A clear and concise description of what you expected to happen.

**Logs/Screenshots**
If applicable, add screenshots PJSIP logs with verbosity level 5, packet capture (e.g: using Wireshark), audio/video recording, screenshot, etc.

**Desktop/Smartphone (please complete the following information):**

- OS, Distribution & Version: [e.g. MacOS Mojave, Ubuntu 16.04, iOS 13]
- PJSIP
  - version: ...
  - applied patch(es): [e.g: patches from issue/PR xyz]
  - `configure` script params: ...
  - `config_site.h` contents: ...
  - related third party libraries & versions: [e.g: OpenSSL 1.1.1b]

**Smartphone (please also complete the following information):**

- Device: [e.g. iPhone 6]

**Additional context**
Add any other context about the problem here.
