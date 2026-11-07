# ESP32 Time Server Tester (version 2)

**ESP32 Time Server Tester** is a Network Time Protocol (NTP) and Precision Time Protocol (PTP) Time Server testing application designed to run on an ESP32-P4 development board with an Ethernet connection (as these are inexpensive yet capable of hardware timestamping over Ethernet Layer 2, IPv4, and IPv6).  It is an ideal companion to [ESP32TimeServer](https://github.com/roblatour/ESP32TimeServer) or any other local NTP or PTP Time Server solution under development.


[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

---

## Overview

**ESP32 Time Server Tester** provides a dedicated browser-based interface for
testing NTP over IPv4 and IPv6, PTP Layer 2 Ethernet, and IEEE 1588 PTP Layer 3
multicast UDP over IPv4, IPv6, or both simultaneously.

![ESP32 Time Server Tester web interface](./misc/screenshot.jpg)

It allows you to save settings for multiple target servers, run individual tests or groups of
tests, and review live testing results directly in your browser. 

Additionally, the tool provides for a **Downloadable report** which details settings, test scenarios and testing results [click here for an example](./misc/example_report_1.md).
The screenshot and example reports may predate the PTP L3 UDP controls.

---

## Hardware


| Qty | Item                                                                                                                                                                                                         |
| --- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| 1   | any of the following (with or without their POE variants): [WaveShare ESP32-P4-ETH](https://www.waveshare.com/esp32-p4-eth.htm?sku=32086), WaveShare ESP32-P4-WIFI-ETH |
| 2   | (optional) WS2812 RGB LEDs ([AliExpress](https://www.aliexpress.com/item/1005009109749689.html)) used to reflect IPv4 and IPv6 connection and testing status                                                 |
| 1   | 470&Omega; resistor for the data connection to the first WS2812 (if the optional WS2812s are used)                                                                                                           |
| —   | Ethernet cable, hookup wires, and either a PoE-capable switch/injector (for a PoE board) or a USB-C power supply and cable                                                                                   |

> **WARNING — Do not power the ESP32-P4-POE-ETH from both USB-C and a PoE-powered
> Ethernet cable at the same time.** Use only one power source.

### Wiring

#### First WS2812 RGB LED → ESP32-P4-ETH / ESP32-P4-POE-ETH

| First WS2812 pin | Connection                    | ESP32-P4 pin |
| ---------------- | ----------------------------- | ------------ |
| GND              | Direct                        | GND          |
| DI               | Through a 470&Omega; resistor | GPIO 4       |
| 5V               | Direct                        | V5 / VBUS    |

**Important:** Place the 470&Omega; resistor in series between GPIO 4 and **DI**
on the first WS2812. Do not omit it.

#### First WS2812 RGB LED → second WS2812 RGB LED

| First WS2812 pin | Second WS2812 pin |
| ---------------- | ----------------- |
| DO               | DI                |
| GND              | GND               |
| 5V               | 5V                |


---

## Software

### Build and development environment

**To build and flash ESP32TimeServerTester**, install **Espressif ESP-IDF v6.1**
or later. See the [ESP-IDF installation instructions](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/index.html).
Visual Studio Code with the ESP-IDF extension is recommended for development,
but is not required to build and flash the project.

### Dependencies

The ESP-IDF Component Manager resolves `espressif/mdns` and
`espressif/led_strip` at build time, as declared in
[`main/idf_component.yml`](./main/idf_component.yml).

## Setup

### Setup Step 1 - Download

Clone this repository:

```cmd
git clone https://github.com/roblatour/ESP32TimeServerTester
```
```cmd
cd ESP32TimeServerTester
```

### Setup Step 2 - Configuration

Review and update [`main/ESP32TimeServerTesterSettings.h`](./main/ESP32TimeServerTesterSettings.h)
and [`main/ESP32TimeServerTesterSettingsSecret.h`](./main/ESP32TimeServerTesterSettingsSecret.h) for various application settings.

Of note: Over the Ethernet updates are enabled by

enabling `OTE_UPDATES_ENABLED` in `ESP32TimeServerTesterSettings.h` 

```cpp
#define OTE_UPDATES_ENABLED 1 // 0 = disabled, 1 = enabled
````
and optionally updating the default password in the `ESP32TimeServerTesterSettingsSecret.h` file
```cpp
static constexpr char OTEPassword[] = "ESP32TimeServerTesterpw"; 
```

but will require an initial firmware flash via USB before they become effective on the device.

Don't publish the `ESP32TimeServerTesterSettingsSecret.h` file or the resulting firmware binary if the password must remain private. Also, be aware, the password is transmitted to the device in plain text over the network as part of the OTE update process.

### Setup Step 3 - Build

Open a Command Prompt in the project root and load the ESP-IDF v6.1 (or later)
environment. If ESP-IDF is installed elsewhere, adjust the path below:

```cmd
call C:\esp\v6.1\esp-idf\export.bat
```

Before building for a different board or chip revision, remove any previously
generated `.\sdkconfig` so ESP-IDF can regenerate it from the common
`.\sdkconfig.defaults` and the selected revision profile in `.\config\`:

```cmd
if exist sdkconfig del sdkconfig
```

**Check the ESP32-P4 chip revision before building.** Connect the development
board over USB and run the following command, replacing `COMx` with its serial
port (for example, `COM6`):

```cmd
esptool.py -p COMx flash-id
```

The output includes the chip type and revision number. As in ESP32TimeServer,
this project combines the common `.\sdkconfig.defaults` with one of three
revision-specific defaults files in `.\config\`. Use the command below matching
your **chip's revision**, rather than relying on the development board's name:

- For ESP32-P4 chips at **revision v3.1 and above**, including newer WaveShare
  ESP32-P4-ETH / ESP32-P4-POE-ETH boards with these chip revisions:

  ```cmd
  idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;config/esp32p4_rev_v3_1.defaults" set-target esp32p4 build
  ```

- For ESP32-P4 chips at **revision v3.0**:

  ```cmd
  idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;config/esp32p4_rev_v3_0.defaults" set-target esp32p4 build
  ```

- For ESP32-P4 chips **earlier than revision v3.0**, including older WaveShare
  ESP32-P4-ETH boards with engineering-sample chips such as rev v1.3:

  ```cmd
  idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;config/esp32p4_rev_pre_v3.defaults" set-target esp32p4 build
  ```

The first build may download the `espressif/mdns` and `espressif/led_strip`
components through the ESP-IDF Component Manager into `.\managed_components\`.
This generated folder is not part of the repository. No manual chip-revision
selection in `menuconfig` is needed when using these profiles.

For subsequent builds with the same configuration, use `idf.py build`. If you
change chip revisions or regenerate `.\sdkconfig`, use the matching command above
again. **Do not force-flash firmware built for a different chip revision.**

### Setup Step 4 - Flash

Connect the board over USB and replace `COMx` with its serial port:

```cmd
idf.py -p COMx flash monitor
```

### Over The Ethernet (OTE) updates

ESP32TimeServer version 2 introduces Over the Ethernet updates as an alternative to flashing via USB.

For more information, please see....



1. Build with ESP-IDF 6.1+ using the device's chip-revision profile as detailed above.
   
2. Use the following command to perform an OTE update:

   ```cmd
   python tools\ote_upload.py -i ESP32TimeServerTester.local -p 3232 -f build\ESP32TimeServerTester.bin -a mypassword
   ```
   Note: the `-a` (or `--password`) takes precedence over `OTE_PASSWORD` or the variable
   selected by `--password-env` (see script code for more information). However, without either, the script prompts for the password. Command-line passwords can appear in shell history and process arguments; the environment variable or interactive prompt avoids putting the password directly on the command line.

4. A successful upload returns `OTE image validated; restarting in 5 seconds` (however will likely take longer as the device processes the update). 
   
5. Wait for the device to automatically restart (which should begin within ~30 seconds).

---

## Testing a Time Server

<span style="color: yellow;">
Important Notes:<br> 
- This program should only be used to test internal time servers which you are authorized to test.<br>
- Testing external time servers, or against public NTP pools, may cause your entire network to be banned.<br>
- Ideally testing should be done within an isolated network.<br> 
- For PTP testing, ensure there is only one Master Clock (the one you are testing) active within the testing network.<br><br>
</span> 

1. Connect the tester to your network via Ethernet and power it on. Open its
   web interface using the hostname or IPv4 address printed in the console
   log. 
   
   The default hostname is `ESP32TimeServerTester`. 
   
   So for example, using your default browser, browse to:
   
   `http://esp32timeservertester` 
   
   or 
   
   `http://x.x.x.x`
   (where `x.x.x.x` is the ESP32TimseServerTester's IPv4 address as shown in its console log).
2. Enter a friendly **target time server** name and select its firmware mode:
   unknown, NTP only, PTP L2 only, PTP L3 UDP only, NTP + PTP L2,
   NTP + PTP L3 UDP, PTP L2 + PTP L3 UDP, or NTP + PTP L2 + PTP L3 UDP.
   The mode is operator-declared, not detected automatically. Unknown permits
   ordinary positive tests; combined-load tests require an explicit mode
   containing NTP and the PTP transport under test.
3. Configure the relevant NTP addresses and ports and/or the **PTP L2** master
   MAC, domain, and optional key. Under those PTP settings, enter the **PTP L3
   UDP** master's IPv4 address, IPv6 address, or both, and its UDP domain.
   These are unicast source addresses used to identify the server's multicast
   packets; for the reference server, use its Ethernet IPv6 link-local
   address without a zone suffix. Leave a master address blank to disable that
   family; simultaneous tests require both addresses. PTP L3 UDP uses no
   symmetric key. The NTP and PTP L2 keys remain separate.
4. Save the settings and run individual tests or a group. **Run all PTP tests**
   combines the available PTP L2 and PTP L3 UDP groups. The displayed and
   execution order is:

   | Numbers | Group |
   | --- | --- |
   | 1-2 | IPv4 and IPv6 pings |
   | 3-20 | NTP |
   | 21-24 | Combined NTP + PTP L2 |
   | 25-27 | Combined NTP + PTP L3 UDP |
   | 28-52 | PTP L2, no operator assistance |
   | 53-65 | PTP L3 UDP, no operator assistance |
   | 66-76 | PTP L2, operator assistance |
   | 77-84 | PTP L3 UDP, operator assistance |

   The NTP + PTP L2 combined group includes an assisted holdover scenario.
5. Results appear live in the web interface. PTP tests can take longer than
   ping and NTP tests. You may close and reopen the browser while testing
   continues; assisted tests wait for explicit operator action. If configured,
   the WS2812 LEDs flash during a run.
   **Skip this test** cancels the current assisted test promptly without stopping
   the remaining run. Traffic is stopped and workers are joined safely. Restore
   any manually changed DUT fixture; unresolved candidate-expiry or restoration
   checks remain safety blockers for dependent PTP tests until verified by
   rerunning the relevant recovery test. Skip does not bypass those checks.
6. Click **Download report** to download a Markdown report containing the
   settings, results, and detailed evidence. Key values are redacted. Here is
   an [example report](./misc/ESP32TimeServerTester_ESP32TimeServer_Test_(ULA).md).

### PTP L3 UDP testing

- The tester joins `224.0.1.129` for IPv4 and `ff02::181` on the Ethernet
  interface for IPv6. Event and general traffic use UDP ports 319 and 320,
  with a multicast TTL/hop limit of 1. The network must pass IPv4 IGMP and/or
  IPv6 MLD multicast traffic. The reference server sends its Delay_Resp on
  general port 320; the tester accepts a matching response on either receive
  port.
- Tests cover Announce and master identity, two-step Sync/Follow_Up pairing,
  E2E Delay_Req/Delay_Resp matching, time properties, sustained traffic,
  malformed requests, and simultaneous dual-stack service. Combined-load
  variants also exercise NTP. The simultaneous test expects both transports
  to advertise the same grandmaster identity.
- Assisted GNSS, PPS, server-link and restart scenarios observe withdrawal
  after the configured **PTP L3 UDP withdrawal bound**, then request
  restoration and verify fresh service. Set the bound to the DUT's expected
  maximum withdrawal time. Operator confirmation, isolation and the relevant
  settings opt-ins are required.
- `PtpNegativeTestsEnabled`, `PtpAssistedTestsEnabled`, and
  `PtpCombinedLoadEnabled` in
  [`main/ESP32TimeServerTesterSettings.h`](./main/ESP32TimeServerTesterSettings.h)
  also gate the corresponding L3 tests. Combined load additionally requires
  the target's stress-test setting and NTP address/port for each selected
  family; required NTP authentication still needs its separate NTP key.
- These socket-based UDP tests do **not** obtain hardware timestamps or
  receive-drop counters. They do not measure clock offset, path delay, servo
  performance, or synchronization accuracy. Missing traffic or timeouts can
  also reflect the network or tester; a failed test alone does not establish
  the cause. A negative-test pass means no matching response was observed
  between healthy controls; it cannot rule out an unseen dropped response. The
  tests are not a full IEEE 1588 profile or BMCA conformance suite. Use packet
  capture and an independent hardware-timestamped client for further diagnosis.

---
## If you make code changes

From a Windows command prompt, you can run a regression suite with the following commands:

```cmd
set "VSROOT=C:\Program Files\Microsoft Visual Studio\18\Community"
```
```cmd
call "%VSROOT%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
```
```cmd
python tests\run_ptp_tests.py
```
The following targeted host regressions also exercise the production PTP
catalog, simulated UDP sockets and assisted recovery, profile migration,
generated web controls, and report settings:

```powershell
.\tests\run_ptp_assist_tests.ps1
.\tests\run_ptp_udp_tests.ps1
.\tests\run_ptp_profile_tests.ps1
.\tests\run_ntp_policy_worker_tests.ps1
```

Of note:
- The above requires the Visual Studio (Community Edition) **Desktop development with C++** workload be
  installed, with the MSVC x64/x86 build tools and a Windows SDK included. The
  PowerShell scripts accept `-VcVars` if Visual Studio is installed elsewhere.
- The profile/web regression also requires Node.js. These are host-side
  regressions, not a substitute for testing a real DUT on a multicast network.
---

## License

This project is released under the **MIT License** — see the [LICENSE](LICENSE)
file for details.

---

## Supporting This Project

To help support this project, or to just say thanks, you're welcome to 'buy me a
coffee'.

[<img alt="buy me a coffee" width="200px" src="https://cdn.buymeacoffee.com/buttons/v2/default-blue.png" />](https://www.buymeacoffee.com/roblatour)

---

Copyright © 2026 Rob Latour
