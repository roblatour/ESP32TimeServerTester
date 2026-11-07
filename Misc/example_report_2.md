<p align="center"><strong>ESP32TimeServerTester Report</strong></p>

## Settings

| Settings | Values |
| --- | --- |
| **Target time server:** | New ESP32\-P4\-WiFi\-ETH no case board |
| **Firmware mode:** | Combined |
| **IPv4 address:** | 192.168.1.114 |
| **IPv4 NTP Port:** | 123 |
| **IPv6 address:** | FDAC:1111:2222:1:82F1:B2FF:FED6:5814 |
| **IPv6 NTP Port:** | 123 |
| **PTP L2 master MAC:** | 80:f1:b2:d6:58:14 |
| **PTP domain:** | 0 |
| **PTP role/transport:** | L2 two-step E2E observer; legacy/ranked policy; immediate HMAC-SHA256-128; no servo/full BMCA/UDP |
| **PTP symmetric-key id:** | Present but redacted in this report |
| **PTP symmetric-key value:** | Present but redacted in this report |
| **NTPv4 symmetric-key id:** | Present but redacted in this report |
| **NTPv4 symmetric-key value:** | Present but redacted in this report |
| **Mini saturated stress tests:** | Enabled (1000 NTP requests each) |

## Test results

| No. | Test | Result | Reported date/time or PTP diagnostic |
| ---: | --- | --- | --- |
| 1 | IPv4 Ping target time server | <span style="color:green">Pass</span> | See detailed evidence |
| 2 | IPv4 NTPv3 Standard | <span style="color:green">Pass</span> | See detailed evidence |
| 3 | IPv4 NTPv4 Standard | <span style="color:green">Pass</span> | See detailed evidence |
| 4 | IPv4 NTPv4 Interleaved | <span style="color:green">Pass</span> | See detailed evidence |
| 5 | IPv4 NTPv4 Symmetric-key standard | <span style="color:green">Pass</span> | See detailed evidence |
| 6 | IPv4 NTPv4 Symmetric-key interleaved | <span style="color:green">Pass</span> | See detailed evidence |
| 7 | IPv4 NTPv4 Invalid symmetric-key | <span style="color:green">Pass</span> | See detailed evidence |
| 8 | IPv4 NTPv4 Missing symmetric-key | <span style="color:green">Pass</span> | See detailed evidence |
| 9 | IPv4 NTPv4 Truncated MAC | <span style="color:green">Pass</span> | See detailed evidence |
| 10 | IPv4 NTPv4 Standard mini saturated stress test | <span style="color:green">Pass</span> | See detailed evidence |
| 11 | IPv6 Ping target time server | <span style="color:green">Pass</span> | See detailed evidence |
| 12 | IPv6 NTPv3 Standard | <span style="color:green">Pass</span> | See detailed evidence |
| 13 | IPv6 NTPv4 Standard | <span style="color:green">Pass</span> | See detailed evidence |
| 14 | IPv6 NTPv4 Interleaved | <span style="color:green">Pass</span> | See detailed evidence |
| 15 | IPv6 NTPv4 Symmetric-key standard | <span style="color:green">Pass</span> | See detailed evidence |
| 16 | IPv6 NTPv4 Symmetric-key interleaved | <span style="color:green">Pass</span> | See detailed evidence |
| 17 | IPv6 NTPv4 Invalid symmetric-key | <span style="color:green">Pass</span> | See detailed evidence |
| 18 | IPv6 NTPv4 Missing symmetric-key | <span style="color:green">Pass</span> | See detailed evidence |
| 19 | IPv6 NTPv4 Truncated MAC | <span style="color:green">Pass</span> | See detailed evidence |
| 20 | IPv6 NTPv4 Standard mini saturated stress test | <span style="color:green">Pass</span> | See detailed evidence |
| 21 | Bounded simultaneous IPv4 / IPv6 NTP and PTP load | <span style="color:green">Pass</span> | See detailed evidence |
| 22 | PTP L2 Announce and master identity | <span style="color:green">Pass</span> | See detailed evidence |
| 23 | PTP L2 two-step Sync / Follow_Up | <span style="color:green">Pass</span> | See detailed evidence |
| 24 | PTP L2 E2E Delay_Req / Delay_Resp | <span style="color:green">Pass</span> | See detailed evidence |
| 25 | PTP L2 time properties and clock quality | <span style="color:green">Pass</span> | See detailed evidence |
| 26 | PTP L2 offset / path delay diagnostic | <span style="color:green">Pass</span> | See detailed evidence |
| 27 | PTP L2 sustained exchange integrity | <span style="color:green">Pass</span> | See detailed evidence |
| 28 | PTP L2 negative Delay_Req acceptance | <span style="color:green">Pass</span> | See detailed evidence |
| 29 | PTP L2 foreign-master policy and recovery | <span style="color:green">Pass</span> | See detailed evidence |
| 30 | PTP-only IP-independent operation | Not yet run | n/a |
| 31 | PTP ranking precedence | <span style="color:green">Pass</span> | See detailed evidence |
| 32 | PTP qualification and source isolation | <span style="color:green">Pass</span> | See detailed evidence |
| 33 | PTP duplicate, reorder and rollover policy | <span style="color:green">Pass</span> | See detailed evidence |
| 34 | PTP dataset changes and self-grandmaster reflection | <span style="color:green">Pass</span> | See detailed evidence |
| 35 | PTP multiple better masters | <span style="color:green">Pass</span> | See detailed evidence |
| 36 | PTP interval-sensitive expiry | <span style="color:green">Pass</span> | See detailed evidence |
| 37 | PTP signed service messages | <span style="color:green">Pass</span> | See detailed evidence |
| 38 | PTP authenticated E2E exchange | <span style="color:green">Pass</span> | See detailed evidence |
| 39 | PTP tampered and malformed authentication | <span style="color:green">Pass</span> | See detailed evidence |
| 40 | PTP mixed versus required authentication | <span style="color:green">Pass</span> | See detailed evidence |
| 41 | PTP authentication before selection | <span style="color:green">Pass</span> | See detailed evidence |
| 42 | PTP GNSS loss and recovery (assisted) | <span style="color:green">Pass</span> | See detailed evidence |
| 43 | PTP PPS loss and recovery (assisted) | <span style="color:green">Pass</span> | See detailed evidence |
| 44 | PTP server link loss and recovery (assisted) | <span style="color:green">Pass</span> | See detailed evidence |
| 45 | PTP restart / OTA recovery (assisted) | <span style="color:green">Pass</span> | See detailed evidence |
| 46 | PTP service recovery while standing by (assisted) | <span style="color:green">Pass</span> | See detailed evidence |

---

ESP32TimeServerTester v2.0.0

| Address Types | Addresses  |
| --- | --- |
| **MAC:** | 80:F1:B2:D1:D9:19 |
| **IPv4:** | 192.168.1.234 |
| **IPv6 LLA:** | FE80::82F1:B2FF:FED1:D919 |
| **IPv6 ULA:** | FDAC:1111:2222:1:82F1:B2FF:FED1:D919 |
| **IPv6 GUA:** | Not assigned |


---

https://github.com/roblatour/ESP32TimeServerTester

## PTP interpretation limits

Restricted Ethernet L2 two-step E2E observer with legacy/ranked foreign-master policy testing and immediate HMAC-SHA256-128 authentication; not full BMCA, default-profile/security-framework conformance, UDP PTP or a follower servo. PTP and NTP keys are independent. Use only an authorized isolated test link with no unrelated PTP master. GNSS/PPS remain the DUT's only time authority; PTP does not serve NTP holdover. The tester PHC is not disciplined; raw offset/path delay are diagnostics, not synchronization accuracy. Hardware Sync RX and Delay_Req TX timestamps are required for measurements; general-message RX timestamps are not required. Accuracy requires a hardware-timestamped linuxptp client with a running servo, packet capture and independent PPS/phase measurement. Source metadata is not runtime attestation. Incomplete or interrupted scenarios cannot pass.

## DUT policy metadata

DUT policy v1: selection=ranked two-fresh-Announce qualification; PTP authentication=enabled, mixed; capacity=8; domain=0; foreign timeout=10000 ms; local log Announce/Sync=1/0; GNSS/PPS qualification=3500 ms; SPP=1 outgoing key ID=1; NTP authentication=mixed; source identity=2d61b8da2ad789d9c393f2004473741a425ba3d4a09e43c030f316dabc430dd5; provenance=public-source-contract-v1-not-runtime-attestation. Source configuration only, not runtime attestation.

## Detailed test evidence

### 1. IPv4 Ping target time server

Ensure the DUT is reachable over an IPv4 connection.

Result: **Pass**

<pre>Operator firmware=Combined.
n/a</pre>

### 2. IPv4 NTPv3 Standard

Verify that the DUT provides standard NTP version 3 service over IPv4.

Result: **Pass**

<pre>Operator firmware=Combined.
2026-10-27 00:27:50.794248471 (UTC)</pre>

### 3. IPv4 NTPv4 Standard

Verify that the DUT provides standard NTP version 4 service over IPv4.

Result: **Pass**

<pre>Operator firmware=Combined.
2026-10-27 00:27:50.886474621 (UTC)</pre>

### 4. IPv4 NTPv4 Interleaved

Verify that the DUT supports interleaved NTP version 4 exchanges over IPv4.

Result: **Pass**

<pre>Operator firmware=Combined.
2026-10-27 00:27:50.985943213 (UTC)</pre>

### 5. IPv4 NTPv4 Symmetric-key standard

Verify that the DUT accepts authenticated standard NTP version 4 requests over IPv4.

Result: **Pass**

<pre>Operator firmware=Combined.
2026-10-27 00:27:51.086241768 (UTC)</pre>

### 6. IPv4 NTPv4 Symmetric-key interleaved

Verify that the DUT accepts authenticated interleaved NTP version 4 requests over IPv4.

Result: **Pass**

<pre>Operator firmware=Combined.
2026-10-27 00:27:51.185970484 (UTC)</pre>

### 7. IPv4 NTPv4 Invalid symmetric-key

Verify that the DUT rejects NTP version 4 requests with an invalid authentication key over IPv4.

Result: **Pass**

<pre>Operator firmware=Combined.
n/a</pre>

### 8. IPv4 NTPv4 Missing symmetric-key

Verify that the DUT handles unauthenticated NTP version 4 requests correctly over IPv4.

Result: **Pass**

<pre>Operator firmware=Combined.
n/a</pre>

### 9. IPv4 NTPv4 Truncated MAC

Verify that the DUT rejects NTP version 4 requests with incomplete authentication data over IPv4.

Result: **Pass**

<pre>Operator firmware=Combined.
n/a</pre>

### 10. IPv4 NTPv4 Standard mini saturated stress test

Verify that the DUT remains responsive during a short, saturated IPv4 NTP request load. (Runs only when mini stress tests are enabled for the target.)

Result: **Pass**

<pre>Operator firmware=Combined.
2026-10-27 00:28:01.212287176 (UTC)</pre>

### 11. IPv6 Ping target time server

Ensure the DUT is reachable over an IPv6 connection.

Result: **Pass**

<pre>Operator firmware=Combined.
n/a</pre>

### 12. IPv6 NTPv3 Standard

Verify that the DUT provides standard NTP version 3 service over IPv6.

Result: **Pass**

<pre>Operator firmware=Combined.
2026-10-27 00:28:01.306463336 (UTC)</pre>

### 13. IPv6 NTPv4 Standard

Verify that the DUT provides standard NTP version 4 service over IPv6.

Result: **Pass**

<pre>Operator firmware=Combined.
2026-10-27 00:28:01.406212436 (UTC)</pre>

### 14. IPv6 NTPv4 Interleaved

Verify that the DUT supports interleaved NTP version 4 exchanges over IPv6.

Result: **Pass**

<pre>Operator firmware=Combined.
2026-10-27 00:28:01.506218995 (UTC)</pre>

### 15. IPv6 NTPv4 Symmetric-key standard

Verify that the DUT accepts authenticated standard NTP version 4 requests over IPv6.

Result: **Pass**

<pre>Operator firmware=Combined.
2026-10-27 00:28:01.606440444 (UTC)</pre>

### 16. IPv6 NTPv4 Symmetric-key interleaved

Verify that the DUT accepts authenticated interleaved NTP version 4 requests over IPv6.

Result: **Pass**

<pre>Operator firmware=Combined.
2026-10-27 00:28:01.706341618 (UTC)</pre>

### 17. IPv6 NTPv4 Invalid symmetric-key

Verify that the DUT rejects NTP version 4 requests with an invalid authentication key over IPv6.

Result: **Pass**

<pre>Operator firmware=Combined.
n/a</pre>

### 18. IPv6 NTPv4 Missing symmetric-key

Verify that the DUT handles unauthenticated NTP version 4 requests correctly over IPv6.

Result: **Pass**

<pre>Operator firmware=Combined.
n/a</pre>

### 19. IPv6 NTPv4 Truncated MAC

Verify that the DUT rejects NTP version 4 requests with incomplete authentication data over IPv6.

Result: **Pass**

<pre>Operator firmware=Combined.
n/a</pre>

### 20. IPv6 NTPv4 Standard mini saturated stress test

Verify that the DUT remains responsive during a short, saturated IPv6 NTP request load. (Runs only when mini stress tests are enabled for the target.)

Result: **Pass**

<pre>Operator firmware=Combined.
2026-10-27 00:28:11.757608194 (UTC)</pre>

### 21. Bounded simultaneous IPv4 / IPv6 NTP and PTP load

Verify that the DUT remains responsive while serving bounded IPv4, IPv6, and PTP traffic together. (May only be run when Firmware mode is Combined.)

Result: **Pass**

<pre>Operator firmware=Combined.
DUT policy v1: selection=ranked two-fresh-Announce qualification; PTP authentication=enabled, mixed; capacity=8; domain=0; foreign timeout=10000 ms; local log Announce/Sync=1/0; GNSS/PPS qualification=3500 ms; SPP=1 outgoing key ID=1; NTP authentication=mixed; source identity=2d61b8da2ad789d9c393f2004473741a425ba3d4a09e43c030f316dabc430dd5; provenance=public-source-contract-v1-not-runtime-attestation. Source configuration only, not runtime attestation.
Target=80:f1:b2:d6:58:14; domain=0; operator firmware=Combined.
Observation=20000 ms; samples=8; response window=500 ms; request interval=250 ms; processing allowance=250 ms; DUT foreign quiet=10000 ms; DUT withdrawal=3500 ms; recovery=60000 ms; operator phase limit=120000 ms; sequence seed=enabled/65532.
Unloaded Sustained baseline (Pass):
MAC=80:f1:b2:d6:58:14 domain=0 mode=3; L2 untagged multicast two-step E2E; sequence-seed=controlled Delay_Req wrap; selection=ranked two-fresh-Announce qualification; authentication=enabled, mixed; policy v1; identity=2d61b8da2ad789d9c393f2004473741a425ba3d4a09e43c030f316dabc430dd5; provenance=public-source-contract-v1-not-runtime-attestation. window=20000 ms+500 ms drain; pairs=20; requests/responses=20/20; samples=20/8; duplicate=0 unmatched=0 expired=0 out-of-order=0 missing=0 sequence-gaps=0 response-loss=0; hardware Sync RX/Delay_Req TX required; timestamp-failures=0 unusable=0 RX-drops=0; path us min/mean/max/sd=27.232/27.336/27.473/0.062; raw local-minus-master arbitrary us min/mean/max=-1793051373468103.800/-1793051373467787.500/-1793051373467471.000. PHC not disciplined; no UTC conversion or accuracy claim. completed. 
NTP controls before load: IPv4=Pass; IPv6=Pass.
Loaded PTP (Pass):
MAC=80:f1:b2:d6:58:14 domain=0 mode=3; L2 untagged multicast two-step E2E; sequence-seed=controlled Delay_Req wrap; selection=ranked two-fresh-Announce qualification; authentication=enabled, mixed; policy v1; identity=2d61b8da2ad789d9c393f2004473741a425ba3d4a09e43c030f316dabc430dd5; provenance=public-source-contract-v1-not-runtime-attestation. window=20000 ms+500 ms drain; pairs=20; requests/responses=20/20; samples=20/8; duplicate=0 unmatched=0 expired=0 out-of-order=0 missing=0 sequence-gaps=0 response-loss=0; hardware Sync RX/Delay_Req TX required; timestamp-failures=0 unusable=0 RX-drops=0; path us min/mean/max/sd=27.192/27.311/27.392/0.051; raw local-minus-master arbitrary us min/mean/max=-1793051373468836.500/-1793051373468520.200/-1793051373468203.800. PHC not disciplined; no UTC conversion or accuracy claim. completed. 
NTP load: configured observation+drain duration=20500 ms (half-open request schedule; endpoint scheduling grace adds no requests); total rate=10 requests/s; max outstanding=1; full load window=yes; entire PTP collection covered=yes.
Monotonic windows (us): NTP=[11198320599,11218830598]; PTP collection=[11198325598,11218825612]; actual overlap=20500014 us; uncovered collection=0 us.
IPv4 load: attempts=103; successes=103; timeouts=0; invalid replies=0; local/socket errors=0.
IPv6 load: attempts=102; successes=102; timeouts=0; invalid replies=0; local/socket errors=0.
Client pacing misses=0; ENOBUFS/ENOMEM=0; UDP RX queue drop counters unavailable via this socket API (not assumed zero). PTP RX queue evidence is reported by the engine. Client overload is not server loss.
NTP controls after load: IPv4=Pass (error=0); IPv6=Pass (error=0).
Operator phase evidence:
No assisted phase requested.
</pre>

### 22. PTP L2 Announce and master identity

Verify that the DUT advertises valid PTP master and identity information.

Result: **Pass**

<pre>Operator firmware=Combined.
DUT policy v1: selection=ranked two-fresh-Announce qualification; PTP authentication=enabled, mixed; capacity=8; domain=0; foreign timeout=10000 ms; local log Announce/Sync=1/0; GNSS/PPS qualification=3500 ms; SPP=1 outgoing key ID=1; NTP authentication=mixed; source identity=2d61b8da2ad789d9c393f2004473741a425ba3d4a09e43c030f316dabc430dd5; provenance=public-source-contract-v1-not-runtime-attestation. Source configuration only, not runtime attestation.
Target=80:f1:b2:d6:58:14; domain=0; operator firmware=Combined.
Observation=20000 ms; samples=8; response window=500 ms; request interval=250 ms; processing allowance=250 ms; DUT foreign quiet=10000 ms; DUT withdrawal=3500 ms; recovery=60000 ms; operator phase limit=120000 ms; sequence seed=enabled/65532.
L2 observer; HMAC-SHA256-128 verified PTP v2.1; source=80f1b2fffed65814/1; GM=80f1b2fffed65814; priority=128/128; class=248; accuracy=0xfe; variance=65535; steps=0; timeSource=0x20; timescale=PTP/TAI; UTC offset=37 s (valid); leap61=0; leap59=0; timeTraceable=0; frequencyTraceable=0. Quality is advertised, not independently verified.
Operator phase evidence:
No assisted phase requested.
</pre>

### 23. PTP L2 two-step Sync / Follow_Up

Verify that the DUT sends valid two-step PTP synchronization exchanges.

Result: **Pass**

<pre>Operator firmware=Combined.
DUT policy v1: selection=ranked two-fresh-Announce qualification; PTP authentication=enabled, mixed; capacity=8; domain=0; foreign timeout=10000 ms; local log Announce/Sync=1/0; GNSS/PPS qualification=3500 ms; SPP=1 outgoing key ID=1; NTP authentication=mixed; source identity=2d61b8da2ad789d9c393f2004473741a425ba3d4a09e43c030f316dabc430dd5; provenance=public-source-contract-v1-not-runtime-attestation. Source configuration only, not runtime attestation.
Target=80:f1:b2:d6:58:14; domain=0; operator firmware=Combined.
Observation=20000 ms; samples=8; response window=500 ms; request interval=250 ms; processing allowance=250 ms; DUT foreign quiet=10000 ms; DUT withdrawal=3500 ms; recovery=60000 ms; operator phase limit=120000 ms; sequence seed=enabled/65532.
Matched HMAC-SHA256-128 verified L2 two-step Sync/Follow_Up seq=58, PTP v2.1; origin=1793062595.326712487; correction=0.000000 ns; RX=hardware. Functional pairing only; no precision or synchronization claim.
Operator phase evidence:
No assisted phase requested.
</pre>

### 24. PTP L2 E2E Delay_Req / Delay_Resp

Verify that the DUT completes end-to-end PTP delay measurements.

Result: **Pass**

<pre>Operator firmware=Combined.
DUT policy v1: selection=ranked two-fresh-Announce qualification; PTP authentication=enabled, mixed; capacity=8; domain=0; foreign timeout=10000 ms; local log Announce/Sync=1/0; GNSS/PPS qualification=3500 ms; SPP=1 outgoing key ID=1; NTP authentication=mixed; source identity=2d61b8da2ad789d9c393f2004473741a425ba3d4a09e43c030f316dabc430dd5; provenance=public-source-contract-v1-not-runtime-attestation. Source configuration only, not runtime attestation.
Target=80:f1:b2:d6:58:14; domain=0; operator firmware=Combined.
Observation=20000 ms; samples=8; response window=500 ms; request interval=250 ms; processing allowance=250 ms; DUT foreign quiet=10000 ms; DUT withdrawal=3500 ms; recovery=60000 ms; operator phase limit=120000 ms; sequence seed=enabled/65532.
Matched HMAC-SHA256-128 verified L2 Delay_Req/Delay_Resp seq=17 with exact source/requester identity; master ingress=1793062597.337329706; correction=0.000000 ns; Sync RX=hardware; Delay_Req TX=hardware. Functional E2E exchange only; no clock servo or accuracy claim.
Operator phase evidence:
No assisted phase requested.
</pre>

### 25. PTP L2 time properties and clock quality

Verify that the DUT advertises valid time properties and clock-quality data.

Result: **Pass**

<pre>Operator firmware=Combined.
DUT policy v1: selection=ranked two-fresh-Announce qualification; PTP authentication=enabled, mixed; capacity=8; domain=0; foreign timeout=10000 ms; local log Announce/Sync=1/0; GNSS/PPS qualification=3500 ms; SPP=1 outgoing key ID=1; NTP authentication=mixed; source identity=2d61b8da2ad789d9c393f2004473741a425ba3d4a09e43c030f316dabc430dd5; provenance=public-source-contract-v1-not-runtime-attestation. Source configuration only, not runtime attestation.
Target=80:f1:b2:d6:58:14; domain=0; operator firmware=Combined.
Observation=20000 ms; samples=8; response window=500 ms; request interval=250 ms; processing allowance=250 ms; DUT foreign quiet=10000 ms; DUT withdrawal=3500 ms; recovery=60000 ms; operator phase limit=120000 ms; sequence seed=enabled/65532.
L2 observer; HMAC-SHA256-128 verified PTP v2.1; source=80f1b2fffed65814/1; GM=80f1b2fffed65814; priority=128/128; class=248; accuracy=0xfe; variance=65535; steps=0; timeSource=0x20; timescale=PTP/TAI; UTC offset=37 s (valid); leap61=0; leap59=0; timeTraceable=0; frequencyTraceable=0. Quality is advertised, not independently verified. Two distinct Announce sequences were consistent.
Operator phase evidence:
No assisted phase requested.
</pre>

### 26. PTP L2 offset / path delay diagnostic

Collect PTP offset and path-delay measurements for diagnostic review.

Result: **Pass**

<pre>Operator firmware=Combined.
DUT policy v1: selection=ranked two-fresh-Announce qualification; PTP authentication=enabled, mixed; capacity=8; domain=0; foreign timeout=10000 ms; local log Announce/Sync=1/0; GNSS/PPS qualification=3500 ms; SPP=1 outgoing key ID=1; NTP authentication=mixed; source identity=2d61b8da2ad789d9c393f2004473741a425ba3d4a09e43c030f316dabc430dd5; provenance=public-source-contract-v1-not-runtime-attestation. Source configuration only, not runtime attestation.
Target=80:f1:b2:d6:58:14; domain=0; operator firmware=Combined.
Observation=20000 ms; samples=8; response window=500 ms; request interval=250 ms; processing allowance=250 ms; DUT foreign quiet=10000 ms; DUT withdrawal=3500 ms; recovery=60000 ms; operator phase limit=120000 ms; sequence seed=enabled/65532.
Single hardware-timestamp E2E sample, symmetric-path assumption: mean path delay=27.292 us; local PHC minus master's PTP/TAI timescale=-1793051373469269.500 us; corrections included at 1/65536 ns scale. PHC is not disciplined by this tester; offset is not UTC-converted. Not a nanosecond precision/accuracy claim; authentication=HMAC-SHA256-128 verified.
Operator phase evidence:
No assisted phase requested.
</pre>

### 27. PTP L2 sustained exchange integrity

Verify that PTP message exchanges remain valid over an extended observation period.

Result: **Pass**

<pre>Operator firmware=Combined.
DUT policy v1: selection=ranked two-fresh-Announce qualification; PTP authentication=enabled, mixed; capacity=8; domain=0; foreign timeout=10000 ms; local log Announce/Sync=1/0; GNSS/PPS qualification=3500 ms; SPP=1 outgoing key ID=1; NTP authentication=mixed; source identity=2d61b8da2ad789d9c393f2004473741a425ba3d4a09e43c030f316dabc430dd5; provenance=public-source-contract-v1-not-runtime-attestation. Source configuration only, not runtime attestation.
Target=80:f1:b2:d6:58:14; domain=0; operator firmware=Combined.
Observation=20000 ms; samples=8; response window=500 ms; request interval=250 ms; processing allowance=250 ms; DUT foreign quiet=10000 ms; DUT withdrawal=3500 ms; recovery=60000 ms; operator phase limit=120000 ms; sequence seed=enabled/65532.
MAC=80:f1:b2:d6:58:14 domain=0 mode=3; L2 untagged multicast two-step E2E; sequence-seed=controlled Delay_Req wrap; selection=ranked two-fresh-Announce qualification; authentication=enabled, mixed; policy v1; identity=2d61b8da2ad789d9c393f2004473741a425ba3d4a09e43c030f316dabc430dd5; provenance=public-source-contract-v1-not-runtime-attestation. window=20000 ms+500 ms drain; pairs=20; requests/responses=20/20; samples=20/8; duplicate=0 unmatched=0 expired=0 out-of-order=0 missing=0 sequence-gaps=0 response-loss=0; hardware Sync RX/Delay_Req TX required; timestamp-failures=0 unusable=0 RX-drops=0; path us min/mean/max/sd=27.152/27.308/27.472/0.078; raw local-minus-master arbitrary us min/mean/max=-1793051373469969.000/-1793051373469652.500/-1793051373469336.000. PHC not disciplined; no UTC conversion or accuracy claim. completed. 
Operator phase evidence:
No assisted phase requested.
</pre>

### 28. PTP L2 negative Delay_Req acceptance

Verify that the DUT handles invalid PTP Delay_Req fields safely. Note: signed-DUT coverage includes authenticated PTP-field mutations, but excludes version, TLV and Ethernet-framing cases. (Requires negative-test opt-in.)

Result: **Pass**

<pre>Operator firmware=Combined.
DUT policy v1: selection=ranked two-fresh-Announce qualification; PTP authentication=enabled, mixed; capacity=8; domain=0; foreign timeout=10000 ms; local log Announce/Sync=1/0; GNSS/PPS qualification=3500 ms; SPP=1 outgoing key ID=1; NTP authentication=mixed; source identity=2d61b8da2ad789d9c393f2004473741a425ba3d4a09e43c030f316dabc430dd5; provenance=public-source-contract-v1-not-runtime-attestation. Source configuration only, not runtime attestation.
Target=80:f1:b2:d6:58:14; domain=0; operator firmware=Combined.
Observation=20000 ms; samples=8; response window=500 ms; request interval=250 ms; processing allowance=250 ms; DUT foreign quiet=10000 ms; DUT withdrawal=3500 ms; recovery=60000 ms; operator phase limit=120000 ms; sequence seed=enabled/65532.
MAC=80:f1:b2:d6:58:14 domain=0 mode=3; L2 untagged multicast two-step E2E; sequence-seed=controlled Delay_Req wrap; selection=ranked two-fresh-Announce qualification; authentication=enabled, mixed; policy v1; identity=2d61b8da2ad789d9c393f2004473741a425ba3d4a09e43c030f316dabc430dd5; provenance=public-source-contract-v1-not-runtime-attestation. wrong-domain:pass(before=1 response=0 response-ICV-verified=0 after=1); version:not-covered-by-authenticated-variant; minor-version:not-covered-by-authenticated-variant; transportSpecific:pass(before=1 response=0 response-ICV-verified=0 after=1); reserved-domain:pass(before=1 response=0 response-ICV-verified=0 after=1); reserved-header:pass(before=1 response=0 response-ICV-verified=0 after=1); flags:pass(before=1 response=0 response-ICV-verified=0 after=1); control:pass(before=1 response=0 response-ICV-verified=0 after=1); short-PTP-header:not-covered-by-authenticated-variant; short-PTP-body:not-covered-by-authenticated-variant; messageLength:not-covered-by-authenticated-variant; oversized:not-covered-by-authenticated-variant; extra-TLV:not-covered-by-authenticated-variant; authentication-TLV-unsupported:not-covered-by-authenticated-variant; truncated-TLV:not-covered-by-authenticated-variant; odd-TLV:not-covered-by-authenticated-variant; overrunning-TLV:not-covered-by-authenticated-variant; zero-clock:pass(before=1 response=0 response-ICV-verified=0 after=1); ones-clock:pass(before=1 response=0 response-ICV-verified=0 after=1); zero-port:pass(before=1 response=0 response-ICV-verified=0 after=1); reserved-port:pass(before=1 response=0 response-ICV-verified=0 after=1); target-clock:pass(before=1 response=0 response-ICV-verified=0 after=1); nanoseconds:pass(before=1 response=0 response-ICV-verified=0 after=1); positive-correction:pass(before=1 response=0 response-ICV-verified=0 after=1); negative-correction:pass(before=1 response=0 response-ICV-verified=0 after=1); destination-filter:pass(before=1 response=0 response-ICV-verified=0 after=1); VLAN-filter:not-covered-by-authenticated-variant; legal-padding:not-covered-by-authenticated-variant; distinct-valid-port:pass(before=1 response=1 response-ICV-verified=1 after=1); Authenticated PTP-field probes were signed and self-verified before transmission; version, TLV/declared-length, VLAN and Ethernet-padding cases are not covered because they cannot retain a valid Immediate Authentication message. RX-drops=0. 
Operator phase evidence:
No assisted phase requested.
</pre>

### 29. PTP L2 foreign-master policy and recovery

Verify that the DUT applies its foreign-master policy and recovers when the candidate is withdrawn. (Requires foreign-master test opt-in.)

Result: **Pass**

<pre>Operator firmware=Combined.
DUT policy v1: selection=ranked two-fresh-Announce qualification; PTP authentication=enabled, mixed; capacity=8; domain=0; foreign timeout=10000 ms; local log Announce/Sync=1/0; GNSS/PPS qualification=3500 ms; SPP=1 outgoing key ID=1; NTP authentication=mixed; source identity=2d61b8da2ad789d9c393f2004473741a425ba3d4a09e43c030f316dabc430dd5; provenance=public-source-contract-v1-not-runtime-attestation. Source configuration only, not runtime attestation.
Target=80:f1:b2:d6:58:14; domain=0; operator firmware=Combined.
Observation=20000 ms; samples=8; response window=500 ms; request interval=250 ms; processing allowance=250 ms; DUT foreign quiet=10000 ms; DUT withdrawal=3500 ms; recovery=60000 ms; operator phase limit=120000 ms; sequence seed=enabled/65532.
MAC=80:f1:b2:d6:58:14 domain=0 mode=3; L2 untagged multicast two-step E2E; sequence-seed=controlled Delay_Req wrap; selection=ranked two-fresh-Announce qualification; authentication=enabled, mixed; policy v1; identity=2d61b8da2ad789d9c393f2004473741a425ba3d4a09e43c030f316dabc430dd5; provenance=public-source-contract-v1-not-runtime-attestation. 
Baseline: L2 observer; HMAC-SHA256-128 verified PTP v2.1; source=80f1b2fffed65814/1; GM=80f1b2fffed65814; priority=128/128; class=248; accuracy=0xfe; variance=65535; steps=0; timeSource=0x20; timescale=PTP/TAI; UTC offset=37 s (valid); leap61=0; leap59=0; timeTraceable=0; frequencyTraceable=0. Quality is advertised, not independently verified. 
wrong-domain: synthetic test candidate source=00f1b2fffed1d8ea/1 GM=00f1b2fffed1d8ea; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[11269386773,11269387042] us; TX sequence=65534 (fresh) successful=[11271395601,11271395960] us; TX sequence=65535 (fresh) successful=[11273405601,11273405865] us; activity=1 post-control=1; completed. 
malformed: synthetic test candidate source=00f1b2fffed1d8e7/1 GM=00f1b2fffed1d8e7; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[11274146081,11274146354] us; TX sequence=65534 (fresh) successful=[11276155601,11276155867] us; TX sequence=65535 (fresh) successful=[11278185601,11278185870] us; activity=1 post-control=1; completed. 
foreign policy: synthetic test candidate source=00f1b2fffed1db10/1 GM=00f1b2fffed1db10; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[11278906581,11278906846] us; TX sequence=65534 (fresh) successful=[11280915613,11280915893] us; TX sequence=65535 (fresh) successful=[11282925601,11282925866] us; TX sequence=0 (fresh) successful=[11284935605,11284935949] us; TX sequence=1 (fresh) successful=[11286965601,11286965866] us; TX sequence=2 (fresh) successful=[11288995601,11288995865] us; TX sequence=3 (fresh) successful=[11291005605,11291005945] us; TX sequence=4 (fresh) successful=[11293015601,11293015870] us; TX sequence=5 (fresh) successful=[11295035601,11295035867] us; TX sequence=6 (fresh) successful=[11297045601,11297045867] us; TX sequence=7 (fresh) successful=[11299065601,11299065866] us; TX sequence=8 (fresh) successful=[11301095601,11301095866] us; TX sequence=9 (fresh) successful=[11303115601,11303115866] us; 
Standby phase=4 start/deadline=11280915893/11303165893 us silence-after=11281165893 us sends=13 last-TX/activity=11303115866/11280785493 us held=1; completed. TX sequence=10 (fresh) successful=[11303175679,11303175944] us; Expiry reference=[11303175679,11303175944] us no-early-before=11313175679 us quiet=1. Recovered fresh Announce+two-step+matching Delay=1; first/last activity=11313191430/11317286193 us. 
inferior/non-winning identity tie-break: synthetic test candidate source=00f1b2fffed1db4e/1 GM=00f1b2fffed1db4e; priority=129/128 class=248 accuracy=0xfe variance=65535; rank=1 deciding=priority1; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[11317955867,11317956223] us; TX sequence=65534 (fresh) successful=[11319965605,11319965878] us; 
inferior/non-winning identity tie-break: synthetic test candidate source=00f1b2fffed1db7c/1 GM=80f1b2fffed65815; priority=128/128 class=248 accuracy=0xfe variance=65535; rank=1 deciding=grandmaster identity; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[11324726491,11324726757] us; TX sequence=65534 (fresh) successful=[11326735605,11326735959] us; 
winning identity tie-break then no-longer-better: synthetic test candidate source=00f1b2fffed1db6a/1 GM=80f1b2fffed65813; priority=128/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=grandmaster identity; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[11331496529,11331496794] us; TX sequence=65534 (fresh) successful=[11333505609,11333505878] us; 
Fresh same-source no-longer-better update: deciding=priority1; must recover before original expiry=11343505609 us. TX sequence=65535 (fresh) successful=[11334765675,11334765941] us; Fresh inferior update released standby before original expiry; RX-drops=0. Election does not discipline either clock or establish full BMCA/synchronization accuracy. 
Cleanup: injections stopped; waiting through monotonic 11345015941 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=11345324704/11349848736 us. cleanup healthy=1. 
Operator phase evidence:
No assisted phase requested.
</pre>

### 30. PTP-only IP-independent operation

Verify that PTP continues to operate without IP services. (May only be run when Firmware mode is PTP-Only.)

Result: **Not yet run**

<pre>n/a</pre>

### 31. PTP ranking precedence

Verify that the DUT selects the preferred PTP master according to its ranking policy. (Requires foreign-master test opt-in.)

Result: **Pass**

<pre>Operator firmware=Combined.
DUT policy v1: selection=ranked two-fresh-Announce qualification; PTP authentication=enabled, mixed; capacity=8; domain=0; foreign timeout=10000 ms; local log Announce/Sync=1/0; GNSS/PPS qualification=3500 ms; SPP=1 outgoing key ID=1; NTP authentication=mixed; source identity=2d61b8da2ad789d9c393f2004473741a425ba3d4a09e43c030f316dabc430dd5; provenance=public-source-contract-v1-not-runtime-attestation. Source configuration only, not runtime attestation.
Target=80:f1:b2:d6:58:14; domain=0; operator firmware=Combined.
Observation=20000 ms; samples=8; response window=500 ms; request interval=250 ms; processing allowance=250 ms; DUT foreign quiet=10000 ms; DUT withdrawal=3500 ms; recovery=60000 ms; operator phase limit=120000 ms; sequence seed=enabled/65532.
MAC=80:f1:b2:d6:58:14 domain=0 mode=3; L2 untagged multicast two-step E2E; sequence-seed=controlled Delay_Req wrap; selection=ranked two-fresh-Announce qualification; authentication=enabled, mixed; policy v1; identity=2d61b8da2ad789d9c393f2004473741a425ba3d4a09e43c030f316dabc430dd5; provenance=public-source-contract-v1-not-runtime-attestation. 
Ranking baseline: L2 observer; HMAC-SHA256-128 verified PTP v2.1; source=80f1b2fffed65814/1; GM=80f1b2fffed65814; priority=128/128; class=248; accuracy=0xfe; variance=65535; steps=0; timeSource=0x20; timescale=PTP/TAI; UTC offset=37 s (valid); leap61=0; leap59=0; timeTraceable=0; frequencyTraceable=0. Quality is advertised, not independently verified.
Order: priority1, class, accuracy, variance, priority2, grandmaster identity. Earlier fields tie; later fields oppose the deciding field wherever possible. Both directions are attempted; impossible directions are N/A, not tested passes. 
Ranking priority1 winning: synthetic test candidate source=00f1b2fffed1db80/1 GM=fffffffffffffffe; priority=127/255 class=254 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[11356126475,11356126741] us; TX sequence=65534 (fresh) successful=[11358135601,11358135867] us; Expected=standby until candidate expiry; observed=1; silence-after=11358385867 us; no-early-before=11368135601 us; last-DUT-activity=11357896648 us; completed. 
Cleanup: injections stopped; waiting through monotonic 11368385867 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=11369153402/11374207069 us. cleanup healthy=1. Ranking priority1 winning: PASS with fresh healthy after-control. 
Ranking priority1 losing: synthetic test candidate source=00f1b2fffed1dbd6/1 GM=0000000000000001; priority=129/0 class=1 accuracy=0x20 variance=0; rank=1 deciding=priority1; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[11384436448,11384436712] us; TX sequence=65534 (fresh) successful=[11386445601,11386445865] us; TX sequence=65535 (fresh) successful=[11388455601,11388455867] us; TX sequence=0 (fresh) successful=[11390475601,11390475866] us; Expected=continued healthy master service; observed=1; silence-after=11386695865 us; no-early-before=11400475601 us; last-DUT-activity=11390506204 us; completed. 
Cleanup: injections stopped; waiting through monotonic 11400725866 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=11401380048/11406436117 us. cleanup healthy=1. Ranking priority1 losing: PASS with fresh healthy after-control. 
Ranking class winning: synthetic test candidate source=00f1b2fffed1dbe3/1 GM=fffffffffffffffe; priority=128/255 class=247 accuracy=0xfe variance=65535; rank=-1 deciding=class; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[11416526498,11416526762] us; TX sequence=65534 (fresh) successful=[11418535602,11418535866] us; Expected=standby until candidate expiry; observed=1; silence-after=11418785866 us; no-early-before=11428535602 us; last-DUT-activity=11418525985 us; completed. 
Cleanup: injections stopped; waiting through monotonic 11428785866 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=11429553741/11434606410 us. cleanup healthy=1. Ranking class winning: PASS with fresh healthy after-control. 
Ranking class losing: synthetic test candidate source=00f1b2fffed1da29/1 GM=0000000000000001; priority=128/0 class=249 accuracy=0x20 variance=0; rank=1 deciding=class; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[11444836438,11444836703] us; TX sequence=65534 (fresh) successful=[11446845601,11446845866] us; TX sequence=65535 (fresh) successful=[11448865601,11448865866] us; TX sequence=0 (fresh) successful=[11450885601,11450885866] us; Expected=continued healthy master service; observed=1; silence-after=11447095866 us; no-early-before=11460885601 us; last-DUT-activity=11450916199 us; completed. 
Cleanup: injections stopped; waiting through monotonic 11461135866 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=11461774386/11466806456 us. cleanup healthy=1. Ranking class losing: PASS with fresh healthy after-control. 
Ranking accuracy winning: synthetic test candidate source=00f1b2fffed1da42/1 GM=fffffffffffffffe; priority=128/255 class=248 accuracy=0x31 variance=65535; rank=-1 deciding=accuracy; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[11476936458,11476936723] us; TX sequence=65534 (fresh) successful=[11478945601,11478945866] us; Expected=standby until candidate expiry; observed=1; silence-after=11479195866 us; no-early-before=11488945601 us; last-DUT-activity=11478886322 us; completed. 
Cleanup: injections stopped; waiting through monotonic 11489195866 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=11489966077/11495006749 us. cleanup healthy=1. Ranking accuracy winning: PASS with fresh healthy after-control. 
accuracy losing: N/A at advertised dataset's supported maximum boundary; this direction requires a different DUT dataset. 
Ranking variance winning: synthetic test candidate source=00f1b2fffed1da88/1 GM=fffffffffffffffe; priority=128/255 class=248 accuracy=0xfe variance=65534; rank=-1 deciding=variance; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[11505246462,11505246726] us; TX sequence=65534 (fresh) successful=[11507255601,11507255866] us; Expected=standby until candidate expiry; observed=1; silence-after=11507505866 us; no-early-before=11517255601 us; last-DUT-activity=11507076014 us; completed. 
Cleanup: injections stopped; waiting through monotonic 11517505866 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=11518265768/11523306435 us. cleanup healthy=1. Ranking variance winning: PASS with fresh healthy after-control. 
variance losing: N/A at advertised dataset's supported maximum boundary; this direction requires a different DUT dataset. 
Ranking priority2 winning: synthetic test candidate source=00f1b2fffed1dade/1 GM=fffffffffffffffe; priority=128/127 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority2; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[11533546463,11533546727] us; TX sequence=65534 (fresh) successful=[11535555601,11535555866] us; Expected=standby until candidate expiry; observed=1; silence-after=11535805866 us; no-early-before=11545555601 us; last-DUT-activity=11535375704 us; completed. 
Cleanup: injections stopped; waiting through monotonic 11545805866 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=11546573456/11551656193 us. cleanup healthy=1. Ranking priority2 winning: PASS with fresh healthy after-control. 
Ranking priority2 losing: synthetic test candidate source=00f1b2fffed1dae4/1 GM=0000000000000001; priority=128/129 class=248 accuracy=0xfe variance=65535; rank=1 deciding=priority2; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[11561856471,11561856735] us; TX sequence=65534 (fresh) successful=[11563865601,11563865866] us; TX sequence=65535 (fresh) successful=[11565885601,11565885866] us; TX sequence=0 (fresh) successful=[11567905601,11567905866] us; Expected=continued healthy master service; observed=1; silence-after=11564115866 us; no-early-before=11577905601 us; last-DUT-activity=11567906498 us; completed. 
Cleanup: injections stopped; waiting through monotonic 11578155866 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=11578761104/11583796171 us. cleanup healthy=1. Ranking priority2 losing: PASS with fresh healthy after-control. 
Ranking grandmaster identity winning: synthetic test candidate source=00f1b2fffed1dd31/1 GM=80f1b2fffed65813; priority=128/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=grandmaster identity; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[11593986476,11593986741] us; TX sequence=65534 (fresh) successful=[11595995601,11595995866] us; Expected=standby until candidate expiry; observed=1; silence-after=11596245866 us; no-early-before=11605995601 us; last-DUT-activity=11595876041 us; completed. 
Cleanup: injections stopped; waiting through monotonic 11606245866 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=11607012792/11612066460 us. cleanup healthy=1. Ranking grandmaster identity winning: PASS with fresh healthy after-control. 
Ranking grandmaster identity losing: synthetic test candidate source=00f1b2fffed1dd47/1 GM=80f1b2fffed65815; priority=128/128 class=248 accuracy=0xfe variance=65535; rank=1 deciding=grandmaster identity; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[11622306479,11622306743] us; TX sequence=65534 (fresh) successful=[11624315601,11624315865] us; TX sequence=65535 (fresh) successful=[11626335601,11626335866] us; TX sequence=0 (fresh) successful=[11628355601,11628355866] us; Expected=continued healthy master service; observed=1; silence-after=11624565865 us; no-early-before=11638355601 us; last-DUT-activity=11628386203 us; completed. 
Cleanup: injections stopped; waiting through monotonic 11638605866 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=11639244439/11644275508 us. cleanup healthy=1. Ranking grandmaster identity losing: PASS with fresh healthy after-control. 
Ranking coverage: 10/10 feasible directions passed; 2/12 directions N/A at supported dataset boundaries. PASS covers all feasible directions only, not a full bidirectional matrix when N/A is nonzero. No full BMCA or synchronization-accuracy claim. 
Operator phase evidence:
No assisted phase requested.
</pre>

### 32. PTP qualification and source isolation

Verify that the DUT qualifies trusted PTP sources without accepting unrelated candidates. (Requires foreign-master test opt-in.)

Result: **Pass**

<pre>Operator firmware=Combined.
DUT policy v1: selection=ranked two-fresh-Announce qualification; PTP authentication=enabled, mixed; capacity=8; domain=0; foreign timeout=10000 ms; local log Announce/Sync=1/0; GNSS/PPS qualification=3500 ms; SPP=1 outgoing key ID=1; NTP authentication=mixed; source identity=2d61b8da2ad789d9c393f2004473741a425ba3d4a09e43c030f316dabc430dd5; provenance=public-source-contract-v1-not-runtime-attestation. Source configuration only, not runtime attestation.
Target=80:f1:b2:d6:58:14; domain=0; operator firmware=Combined.
Observation=20000 ms; samples=8; response window=500 ms; request interval=250 ms; processing allowance=250 ms; DUT foreign quiet=10000 ms; DUT withdrawal=3500 ms; recovery=60000 ms; operator phase limit=120000 ms; sequence seed=enabled/65532.
MAC=80:f1:b2:d6:58:14 domain=0 mode=3; L2 untagged multicast two-step E2E; sequence-seed=controlled Delay_Req wrap; selection=ranked two-fresh-Announce qualification; authentication=enabled, mixed; policy v1; identity=2d61b8da2ad789d9c393f2004473741a425ba3d4a09e43c030f316dabc430dd5; provenance=public-source-contract-v1-not-runtime-attestation. 
source A: synthetic test candidate source=00f1b2fffed1dd90/1 GM=00f1b2fffed1dd90; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. 
source B: synthetic test candidate source=00f1b2fffed1dd93/1 GM=00f1b2fffed1dd93; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. 
Isolation phase: one fresh better Announce from each source-port identity must preserve healthy service; only source A's second fresh Announce may qualify source A. TX sequence=65533 (fresh) successful=[11654906580,11654906845] us; TX sequence=65533 (fresh) successful=[11654906877,11654907141] us; TX sequence=65534 (fresh) successful=[11655665601,11655665866] us; Source A second fresh Announce: source-A silence-after=11655915866 us; source B remains unqualified with one fresh Announce. Recovered fresh Announce+two-step+matching Delay=1; first/last activity=11665684148/11669726826 us. Source-port qualification remained isolated; source B's first Announce neither pooled with source A nor survived as a qualified candidate. 
Cleanup: injections stopped; waiting through monotonic 11665915866 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=11670726092/11675766422 us. cleanup healthy=1. 
Operator phase evidence:
No assisted phase requested.
</pre>

### 33. PTP duplicate, reorder and rollover policy

Verify foreign Announce duplicate, backward, half-range and sequence-rollover qualification and lifetime behavior under the DUT's selection policy. (Requires foreign-master test opt-in.)

Result: **Pass**

<pre>Operator firmware=Combined.
DUT policy v1: selection=ranked two-fresh-Announce qualification; PTP authentication=enabled, mixed; capacity=8; domain=0; foreign timeout=10000 ms; local log Announce/Sync=1/0; GNSS/PPS qualification=3500 ms; SPP=1 outgoing key ID=1; NTP authentication=mixed; source identity=2d61b8da2ad789d9c393f2004473741a425ba3d4a09e43c030f316dabc430dd5; provenance=public-source-contract-v1-not-runtime-attestation. Source configuration only, not runtime attestation.
Target=80:f1:b2:d6:58:14; domain=0; operator firmware=Combined.
Observation=20000 ms; samples=8; response window=500 ms; request interval=250 ms; processing allowance=250 ms; DUT foreign quiet=10000 ms; DUT withdrawal=3500 ms; recovery=60000 ms; operator phase limit=120000 ms; sequence seed=enabled/65532.
MAC=80:f1:b2:d6:58:14 domain=0 mode=3; L2 untagged multicast two-step E2E; sequence-seed=controlled Delay_Req wrap; selection=ranked two-fresh-Announce qualification; authentication=enabled, mixed; policy v1; identity=2d61b8da2ad789d9c393f2004473741a425ba3d4a09e43c030f316dabc430dd5; provenance=public-source-contract-v1-not-runtime-attestation. 
Sequence policy: exact synthetic Announce sequences (independent of Delay_Req seed); ranked rejects duplicate/backward/0x8000 advances without qualification or refresh; 65535-&gt;0-&gt;1 is fresh. Legacy accepts every supported Announce and refreshes its fixed timeout. 
duplicate: synthetic test candidate source=00f1b2fffed1ddb4/1 GM=00f1b2fffed1ddb4; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=100 (fresh) successful=[11682476457,11682476722] us; TX sequence=100 (duplicate/backward/half-range) successful=[11683736351,11683736613] us; TX sequence=101 (fresh) successful=[11684996348,11684996610] us; TX sequence=101 (duplicate/backward/half-range) successful=[11691665609,11691665874] us; Late update=101 expected=no refresh; original expiry=11694996348 us; expected expiry=11694996348 us. No-refresh recovery Announce+matching Delay=1 before 11701415601 us; completed. 
Cleanup: injections stopped; waiting through monotonic 11701915874 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=11702034747/11707076416 us. cleanup healthy=1. duplicate: PASS with healthy Announce/two-step/Delay after-control. 
backward across zero: synthetic test candidate source=00f1b2fffed1ddc7/1 GM=00f1b2fffed1ddc7; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=0 (fresh) successful=[11712556442,11712556707] us; TX sequence=65535 (duplicate/backward/half-range) successful=[11713816348,11713816610] us; TX sequence=1 (fresh) successful=[11715076344,11715076610] us; TX sequence=0 (duplicate/backward/half-range) successful=[11721745602,11721745867] us; Late update=0 expected=no refresh; original expiry=11725076344 us; expected expiry=11725076344 us. No-refresh recovery Announce+matching Delay=1 before 11731495601 us; completed. 
Cleanup: injections stopped; waiting through monotonic 11731995867 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=11732114422/11737166316 us. cleanup healthy=1. backward across zero: PASS with healthy Announce/two-step/Delay after-control. 
half-range: synthetic test candidate source=00f1b2fffed1dc16/1 GM=00f1b2fffed1dc16; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=100 (fresh) successful=[11742636451,11742636716] us; TX sequence=32868 (duplicate/backward/half-range) successful=[11743896349,11743896611] us; TX sequence=101 (fresh) successful=[11745156357,11745156618] us; TX sequence=32869 (duplicate/backward/half-range) successful=[11751825602,11751825867] us; Late update=32869 expected=no refresh; original expiry=11755156357 us; expected expiry=11755156357 us. No-refresh recovery Announce+matching Delay=1 before 11761575601 us; completed. 
Cleanup: injections stopped; waiting through monotonic 11762075867 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=11762206988/11767260754 us. cleanup healthy=1. half-range: PASS with healthy Announce/two-step/Delay after-control. 
forward rollover: synthetic test candidate source=00f1b2fffed1dc59/1 GM=00f1b2fffed1dc59; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=65535 (fresh) successful=[11772736460,11772736725] us; TX sequence=0 (fresh) successful=[11773996345,11773996607] us; TX sequence=1 (fresh) successful=[11780665602,11780665867] us; Late update=1 expected=refresh; original expiry=11783996345 us; expected expiry=11790665602 us. Recovered fresh Announce+two-step+matching Delay=1; first/last activity=11790675771/11794716449 us. 
Cleanup: injections stopped; waiting through monotonic 11790915867 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=11795715715/11800766195 us. cleanup healthy=1. forward rollover: PASS with healthy Announce/two-step/Delay after-control. 
All four sequence scenarios passed for ranked two-fresh-Announce qualification. No DUT-generated Sync rollover, persistent anti-replay or synchronization-accuracy claim. 
Operator phase evidence:
No assisted phase requested.
</pre>

### 34. PTP dataset changes and self-grandmaster reflection

Verify that a changed grandmaster must requalify and self-grandmaster reflection cannot establish or retain standby. (Requires ranked selection and foreign-master test opt-in.)

Result: **Pass**

<pre>Operator firmware=Combined.
DUT policy v1: selection=ranked two-fresh-Announce qualification; PTP authentication=enabled, mixed; capacity=8; domain=0; foreign timeout=10000 ms; local log Announce/Sync=1/0; GNSS/PPS qualification=3500 ms; SPP=1 outgoing key ID=1; NTP authentication=mixed; source identity=2d61b8da2ad789d9c393f2004473741a425ba3d4a09e43c030f316dabc430dd5; provenance=public-source-contract-v1-not-runtime-attestation. Source configuration only, not runtime attestation.
Target=80:f1:b2:d6:58:14; domain=0; operator firmware=Combined.
Observation=20000 ms; samples=8; response window=500 ms; request interval=250 ms; processing allowance=250 ms; DUT foreign quiet=10000 ms; DUT withdrawal=3500 ms; recovery=60000 ms; operator phase limit=120000 ms; sequence seed=enabled/65532.
MAC=80:f1:b2:d6:58:14 domain=0 mode=3; L2 untagged multicast two-step E2E; sequence-seed=controlled Delay_Req wrap; selection=ranked two-fresh-Announce qualification; authentication=enabled, mixed; policy v1; identity=2d61b8da2ad789d9c393f2004473741a425ba3d4a09e43c030f316dabc430dd5; provenance=public-source-contract-v1-not-runtime-attestation. 
grandmaster A -&gt; B: synthetic test candidate source=00f1b2fffed1dc9d/1 GM=00f1b2fffed1dc9d; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=3 actual refresh=8000000 us lifetime=24000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[11807476474,11807476739] us; TX sequence=65534 (fresh) successful=[11808735606,11808735872] us; Same source dataset GM=00f1b2fffed1dc9c; sequence continues. TX sequence=65535 (fresh) successful=[11810245662,11810245936] us; First changed-dataset Announce must release standby before 11816245660 us, strictly before original expiry=11832735606 us. Bounded recovery: fresh Announce+two-step+matching Delay before 11816245660 us; first/last activity=11810246286/11810746591 us. TX sequence=0 (fresh) successful=[11812005606,11812005871] us; Bounded recovery: fresh Announce+two-step+matching Delay before 11842255871 us; first/last activity=11836016218/11838016290 us. Changed grandmaster qualified only after its own second fresh Announce and held standby for its own lifetime. 
Cleanup: injections stopped; waiting through monotonic 11836255871 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=11838036248/11844076908 us. cleanup healthy=1. grandmaster change: PASS with healthy Announce/two-step/Delay after-control. 
self-grandmaster reflection: synthetic test candidate source=00f1b2fffed1dccf/1 GM=00f1b2fffed1dccf; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=3 actual refresh=8000000 us lifetime=24000000 us allowance=250 ms. Same source dataset GM=80f1b2fffed65814; sequence continues. TX sequence=65533 (fresh) successful=[11849536481,11849536745] us; TX sequence=65534 (fresh) successful=[11850795601,11850795866] us; Two strictly better self-GM Announces preserved idle healthy service. Same source dataset GM=00f1b2fffed1dccf; sequence continues. TX sequence=65535 (fresh) successful=[11852055660,11852055926] us; TX sequence=0 (fresh) successful=[11853315601,11853315866] us; Same source dataset GM=80f1b2fffed65814; sequence continues. TX sequence=1 (fresh) successful=[11854825647,11854825917] us; First changed-dataset Announce must release standby before 11860825647 us, strictly before original expiry=11877315601 us. Bounded recovery: fresh Announce+two-step+matching Delay before 11860825647 us; first/last activity=11854826265/11854826795 us. TX sequence=2 (fresh) successful=[11856085605,11856085871] us; Self-GM update released a qualified source; repeated reflection did not requalify it. 
Cleanup: injections stopped; waiting through monotonic 11880335871 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=11880976775/11885016459 us. cleanup healthy=1. self-grandmaster reflection: PASS with healthy Announce/two-step/Delay after-control. Both independent dataset/reflection scenarios passed; no full BMCA or clock-discipline claim. 
Operator phase evidence:
No assisted phase requested.
</pre>

### 35. PTP multiple better masters

Verify continued standby across overlapping candidate lifetimes in both best/worse expiry orders, then bounded healthy recovery. (Requires ranked selection, at least two candidate slots and foreign-master test opt-in.)

Result: **Pass**

<pre>Operator firmware=Combined.
DUT policy v1: selection=ranked two-fresh-Announce qualification; PTP authentication=enabled, mixed; capacity=8; domain=0; foreign timeout=10000 ms; local log Announce/Sync=1/0; GNSS/PPS qualification=3500 ms; SPP=1 outgoing key ID=1; NTP authentication=mixed; source identity=2d61b8da2ad789d9c393f2004473741a425ba3d4a09e43c030f316dabc430dd5; provenance=public-source-contract-v1-not-runtime-attestation. Source configuration only, not runtime attestation.
Target=80:f1:b2:d6:58:14; domain=0; operator firmware=Combined.
Observation=20000 ms; samples=8; response window=500 ms; request interval=250 ms; processing allowance=250 ms; DUT foreign quiet=10000 ms; DUT withdrawal=3500 ms; recovery=60000 ms; operator phase limit=120000 ms; sequence seed=enabled/65532.
MAC=80:f1:b2:d6:58:14 domain=0 mode=3; L2 untagged multicast two-step E2E; sequence-seed=controlled Delay_Req wrap; selection=ranked two-fresh-Announce qualification; authentication=enabled, mixed; policy v1; identity=2d61b8da2ad789d9c393f2004473741a425ba3d4a09e43c030f316dabc430dd5; provenance=public-source-contract-v1-not-runtime-attestation. 
best / early expiry: synthetic test candidate source=00f1b2fffed1df19/1 GM=00f1b2fffed1df18; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=3 actual refresh=8000000 us lifetime=24000000 us allowance=250 ms. 
worse / late expiry: synthetic test candidate source=00f1b2fffed1df18/1 GM=00f1b2fffed1df19; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=3 actual refresh=8000000 us lifetime=24000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[11891766554,11891766820] us; TX sequence=65534 (fresh) successful=[11893025601,11893025866] us; TX sequence=65533 (fresh) successful=[11894535601,11894535866] us; TX sequence=65534 (fresh) successful=[11895795601,11895795865] us; TX sequence=65535 (fresh) successful=[11895795902,11895796166] us; TX sequence=65535 (fresh) successful=[11907805609,11907805874] us; Independent expiry bounds: early candidate latest=11920046166 us; post-expiry checkpoint=11921296166 us; remaining candidate earliest=11931805609 us. Bounded recovery: fresh Announce+two-step+matching Delay before 11938055874 us; first/last activity=11931816216/11931816625 us. 
Cleanup: injections stopped; waiting through monotonic 11932055874 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=11932836208/11937876881 us. cleanup healthy=1. Best expired first: PASS; remaining candidate held standby through its independent lifetime, then healthy Announce/two-step/Delay recovered. 
worse / early expiry: synthetic test candidate source=00f1b2fffed1df7e/1 GM=00f1b2fffed1df7e; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=3 actual refresh=8000000 us lifetime=24000000 us allowance=250 ms. 
best / late expiry: synthetic test candidate source=00f1b2fffed1df71/1 GM=00f1b2fffed1df7d; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=3 actual refresh=8000000 us lifetime=24000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[11943356536,11943356801] us; TX sequence=65534 (fresh) successful=[11944615601,11944615866] us; TX sequence=65533 (fresh) successful=[11946125601,11946125865] us; TX sequence=65534 (fresh) successful=[11947385601,11947385865] us; TX sequence=65535 (fresh) successful=[11947385907,11947386171] us; TX sequence=65535 (fresh) successful=[11959395602,11959395867] us; Independent expiry bounds: early candidate latest=11971636171 us; post-expiry checkpoint=11972886171 us; remaining candidate earliest=11983395602 us. Bounded recovery: fresh Announce+two-step+matching Delay before 11989645867 us; first/last activity=11983406208/11983406623 us. 
Cleanup: injections stopped; waiting through monotonic 11983645867 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=11984412638/11989456311 us. cleanup healthy=1. Worse expired first: PASS; remaining candidate held standby through its independent lifetime, then healthy Announce/two-step/Delay recovered. Both overlapping expiry orders passed; on-wire availability only, not DUT table occupancy, selected-winner identity or PTP-derived holdover. 
Operator phase evidence:
No assisted phase requested.
</pre>

### 36. PTP interval-sensitive expiry

Verify negative and long foreign Announce lifetimes and unsupported-interval handling under the DUT's ranked or legacy policy. (Requires foreign-master test opt-in.)

Result: **Pass**

<pre>Operator firmware=Combined.
DUT policy v1: selection=ranked two-fresh-Announce qualification; PTP authentication=enabled, mixed; capacity=8; domain=0; foreign timeout=10000 ms; local log Announce/Sync=1/0; GNSS/PPS qualification=3500 ms; SPP=1 outgoing key ID=1; NTP authentication=mixed; source identity=2d61b8da2ad789d9c393f2004473741a425ba3d4a09e43c030f316dabc430dd5; provenance=public-source-contract-v1-not-runtime-attestation. Source configuration only, not runtime attestation.
Target=80:f1:b2:d6:58:14; domain=0; operator firmware=Combined.
Observation=20000 ms; samples=8; response window=500 ms; request interval=250 ms; processing allowance=250 ms; DUT foreign quiet=10000 ms; DUT withdrawal=3500 ms; recovery=60000 ms; operator phase limit=120000 ms; sequence seed=enabled/65532.
MAC=80:f1:b2:d6:58:14 domain=0 mode=3; L2 untagged multicast two-step E2E; sequence-seed=controlled Delay_Req wrap; selection=ranked two-fresh-Announce qualification; authentication=enabled, mixed; policy v1; identity=2d61b8da2ad789d9c393f2004473741a425ba3d4a09e43c030f316dabc430dd5; provenance=public-source-contract-v1-not-runtime-attestation. 
Interval policy: ranked lifetime=max(foreign timeout, 3*advertised interval); supported logInterval=-7..4, negative fractions rounded up to microseconds. Unsupported ranked intervals must neither qualify nor refresh a source. Legacy accepts all Announce interval bytes and uses the fixed timeout. Recovery budget=6000000 us; no synchronization-accuracy claim. 
minimum negative interval: synthetic test candidate source=00f1b2fffed1dfd7/1 GM=00f1b2fffed1dfd7; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=-7 actual refresh=7813 us lifetime=10000000 us allowance=250 ms. Probe logInterval=-7 expected=interval-derived lifetime. TX sequence=100 (fresh) successful=[11996216474,11996216740] us; TX sequence=101 (fresh) successful=[11997475605,11997475869] us; No service before 12007475605 us; fresh recovery required before 12013725869 us. Bounded recovery: fresh Announce+two-step+matching Delay before 12013725869 us; first/last activity=12007486201/12007486612 us. 
Cleanup: injections stopped; waiting through monotonic 12007725869 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=12008489369/12013547036 us. cleanup healthy=1. minimum negative interval: PASS with healthy Announce/two-step/Delay after-control. 
negative half-second interval: synthetic test candidate source=00f1b2fffed1dfe3/1 GM=00f1b2fffed1dfe3; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=-1 actual refresh=500000 us lifetime=10000000 us allowance=250 ms. Probe logInterval=-1 expected=interval-derived lifetime. TX sequence=100 (fresh) successful=[12019016468,12019016734] us; TX sequence=101 (fresh) successful=[12020275601,12020275866] us; No service before 12030275601 us; fresh recovery required before 12036525866 us. Bounded recovery: fresh Announce+two-step+matching Delay before 12036525866 us; first/last activity=12030286209/12030286613 us. 
Cleanup: injections stopped; waiting through monotonic 12030525866 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=12031288117/12036336785 us. cleanup healthy=1. negative half-second interval: PASS with healthy Announce/two-step/Delay after-control. 
long sixteen-second interval: synthetic test candidate source=00f1b2fffed1de3f/1 GM=00f1b2fffed1de3f; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=4 actual refresh=16000000 us lifetime=48000000 us allowance=250 ms. Probe logInterval=4 expected=interval-derived lifetime. TX sequence=100 (fresh) successful=[12041816469,12041816735] us; TX sequence=101 (fresh) successful=[12043075601,12043075866] us; No service before 12091075601 us; fresh recovery required before 12097325866 us. Bounded recovery: fresh Announce+two-step+matching Delay before 12097325866 us; first/last activity=12091076208/12091076624 us. 
Cleanup: injections stopped; waiting through monotonic 12091325866 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=12092076449/12097126317 us. cleanup healthy=1. long sixteen-second interval: PASS with healthy Announce/two-step/Delay after-control. 
below supported interval range: synthetic test candidate source=00f1b2fffed1de84/1 GM=00f1b2fffed1de84; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=4 actual refresh=16000000 us lifetime=48000000 us allowance=250 ms. Probe logInterval=-8 expected=rejected without qualification/refresh. TX sequence=100 (unsupported interval; must not qualify/refresh) successful=[12102606496,12102606762] us; TX sequence=101 (unsupported interval; must not qualify/refresh) successful=[12107365605,12107365870] us; TX sequence=102 (fresh) successful=[12112125601,12112125867] us; TX sequence=103 (fresh) successful=[12113385601,12113385866] us; TX sequence=104 (unsupported interval; must not qualify/refresh) successful=[12145395610,12145395875] us; Fresh unsupported update must not shorten/extend original expiry=12161385601 us; no-refresh recovery deadline=12167635866 us. No service before 12161385601 us; fresh recovery required before 12167635866 us. Bounded recovery: fresh Announce+two-step+matching Delay before 12167635866 us; first/last activity=12161396205/12161396623 us. 
Cleanup: injections stopped; waiting through monotonic 12193645875 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=12194526314/12199566984 us. cleanup healthy=1. below supported interval range: PASS with healthy Announce/two-step/Delay after-control. 
above supported interval range: synthetic test candidate source=00f1b2fffed1d13f/1 GM=00f1b2fffed1d13f; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=4 actual refresh=16000000 us lifetime=48000000 us allowance=250 ms. Probe logInterval=5 expected=rejected without qualification/refresh. TX sequence=100 (unsupported interval; must not qualify/refresh) successful=[12205046501,12205046767] us; TX sequence=101 (unsupported interval; must not qualify/refresh) successful=[12209805601,12209805866] us; TX sequence=102 (fresh) successful=[12214565601,12214565867] us; TX sequence=103 (fresh) successful=[12215825601,12215825866] us; TX sequence=104 (unsupported interval; must not qualify/refresh) successful=[12247835602,12247835867] us; Fresh unsupported update must not shorten/extend original expiry=12263825601 us; no-refresh recovery deadline=12270075866 us. No service before 12263825601 us; fresh recovery required before 12270075866 us. Bounded recovery: fresh Announce+two-step+matching Delay before 12270075866 us; first/last activity=12263835671/12264845659 us. 
Cleanup: injections stopped; waiting through monotonic 12296085867 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=12296963181/12302006454 us. cleanup healthy=1. above supported interval range: PASS with healthy Announce/two-step/Delay after-control. 
unspecified interval: synthetic test candidate source=00f1b2fffed1d1a8/1 GM=00f1b2fffed1d1a8; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=4 actual refresh=16000000 us lifetime=48000000 us allowance=250 ms. Probe logInterval=127 expected=rejected without qualification/refresh. TX sequence=100 (unsupported interval; must not qualify/refresh) successful=[12307466509,12307466775] us; TX sequence=101 (unsupported interval; must not qualify/refresh) successful=[12312225601,12312225866] us; TX sequence=102 (fresh) successful=[12316985601,12316985866] us; TX sequence=103 (fresh) successful=[12318245601,12318245866] us; TX sequence=104 (unsupported interval; must not qualify/refresh) successful=[12350255602,12350255868] us; Fresh unsupported update must not shorten/extend original expiry=12366245601 us; no-refresh recovery deadline=12372495866 us. No service before 12366245601 us; fresh recovery required before 12372495866 us. Bounded recovery: fresh Announce+two-step+matching Delay before 12372495866 us; first/last activity=12366246216/12366246639 us. 
Cleanup: injections stopped; waiting through monotonic 12398505868 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=12399419049/12404466718 us. cleanup healthy=1. unspecified interval: PASS with healthy Announce/two-step/Delay after-control. 
minimum signed-byte interval: synthetic test candidate source=00f1b2fffed1d023/1 GM=00f1b2fffed1d023; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=4 actual refresh=16000000 us lifetime=48000000 us allowance=250 ms. Probe logInterval=-128 expected=rejected without qualification/refresh. TX sequence=100 (unsupported interval; must not qualify/refresh) successful=[12409946527,12409946792] us; TX sequence=101 (unsupported interval; must not qualify/refresh) successful=[12414705601,12414705866] us; TX sequence=102 (fresh) successful=[12419465601,12419465867] us; TX sequence=103 (fresh) successful=[12420725601,12420725866] us; TX sequence=104 (unsupported interval; must not qualify/refresh) successful=[12452735602,12452735866] us; Fresh unsupported update must not shorten/extend original expiry=12468725601 us; no-refresh recovery deadline=12474975866 us. No service before 12468725601 us; fresh recovery required before 12474975866 us. Bounded recovery: fresh Announce+two-step+matching Delay before 12474975866 us; first/last activity=12468726219/12468726631 us. 
Cleanup: injections stopped; waiting through monotonic 12500985866 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=12501865920/12506908589 us. cleanup healthy=1. minimum signed-byte interval: PASS with healthy Announce/two-step/Delay after-control. 
All seven interval scenarios passed for ranked two-fresh-Announce qualification. 
Operator phase evidence:
No assisted phase requested.
</pre>

### 37. PTP signed service messages

Verify that the DUT sends PTP service messages with valid authentication.

Result: **Pass**

<pre>Operator firmware=Combined.
DUT policy v1: selection=ranked two-fresh-Announce qualification; PTP authentication=enabled, mixed; capacity=8; domain=0; foreign timeout=10000 ms; local log Announce/Sync=1/0; GNSS/PPS qualification=3500 ms; SPP=1 outgoing key ID=1; NTP authentication=mixed; source identity=2d61b8da2ad789d9c393f2004473741a425ba3d4a09e43c030f316dabc430dd5; provenance=public-source-contract-v1-not-runtime-attestation. Source configuration only, not runtime attestation.
Target=80:f1:b2:d6:58:14; domain=0; operator firmware=Combined.
Observation=20000 ms; samples=8; response window=500 ms; request interval=250 ms; processing allowance=250 ms; DUT foreign quiet=10000 ms; DUT withdrawal=3500 ms; recovery=60000 ms; operator phase limit=120000 ms; sequence seed=enabled/65532.
MAC=80:f1:b2:d6:58:14 domain=0 mode=3; L2 untagged multicast two-step E2E; sequence-seed=controlled Delay_Req wrap; selection=ranked two-fresh-Announce qualification; authentication=enabled, mixed; policy v1; identity=2d61b8da2ad789d9c393f2004473741a425ba3d4a09e43c030f316dabc430dd5; provenance=public-source-contract-v1-not-runtime-attestation. Independent HMAC-SHA256-128 verification: Announce, Sync and Follow_Up; paired sequence=839. Signed Delay_Req and matching verified Delay_Resp controls before/after=1/1. Signed periodic service exercised. Keys and ICVs redacted; functional authentication, not a servo/accuracy or anti-replay claim. 
Operator phase evidence:
No assisted phase requested.
</pre>

### 38. PTP authenticated E2E exchange

Verify that the DUT completes an authenticated end-to-end PTP delay exchange.

Result: **Pass**

<pre>Operator firmware=Combined.
DUT policy v1: selection=ranked two-fresh-Announce qualification; PTP authentication=enabled, mixed; capacity=8; domain=0; foreign timeout=10000 ms; local log Announce/Sync=1/0; GNSS/PPS qualification=3500 ms; SPP=1 outgoing key ID=1; NTP authentication=mixed; source identity=2d61b8da2ad789d9c393f2004473741a425ba3d4a09e43c030f316dabc430dd5; provenance=public-source-contract-v1-not-runtime-attestation. Source configuration only, not runtime attestation.
Target=80:f1:b2:d6:58:14; domain=0; operator firmware=Combined.
Observation=20000 ms; samples=8; response window=500 ms; request interval=250 ms; processing allowance=250 ms; DUT foreign quiet=10000 ms; DUT withdrawal=3500 ms; recovery=60000 ms; operator phase limit=120000 ms; sequence seed=enabled/65532.
MAC=80:f1:b2:d6:58:14 domain=0 mode=3; L2 untagged multicast two-step E2E; sequence-seed=controlled Delay_Req wrap; selection=ranked two-fresh-Announce qualification; authentication=enabled, mixed; policy v1; identity=2d61b8da2ad789d9c393f2004473741a425ba3d4a09e43c030f316dabc430dd5; provenance=public-source-contract-v1-not-runtime-attestation. Independent HMAC-SHA256-128 verification: Announce, Sync and Follow_Up; paired sequence=841. Signed Delay_Req and matching verified Delay_Resp controls before/after=1/1. Authorized signed E2E exchange exercised. Keys and ICVs redacted; functional authentication, not a servo/accuracy or anti-replay claim. 
Operator phase evidence:
No assisted phase requested.
</pre>

### 39. PTP tampered and malformed authentication

Verify that the DUT rejects PTP messages with tampered or malformed authentication.

Result: **Pass**

<pre>Operator firmware=Combined.
DUT policy v1: selection=ranked two-fresh-Announce qualification; PTP authentication=enabled, mixed; capacity=8; domain=0; foreign timeout=10000 ms; local log Announce/Sync=1/0; GNSS/PPS qualification=3500 ms; SPP=1 outgoing key ID=1; NTP authentication=mixed; source identity=2d61b8da2ad789d9c393f2004473741a425ba3d4a09e43c030f316dabc430dd5; provenance=public-source-contract-v1-not-runtime-attestation. Source configuration only, not runtime attestation.
Target=80:f1:b2:d6:58:14; domain=0; operator firmware=Combined.
Observation=20000 ms; samples=8; response window=500 ms; request interval=250 ms; processing allowance=250 ms; DUT foreign quiet=10000 ms; DUT withdrawal=3500 ms; recovery=60000 ms; operator phase limit=120000 ms; sequence seed=enabled/65532.
MAC=80:f1:b2:d6:58:14 domain=0 mode=3; L2 untagged multicast two-step E2E; sequence-seed=controlled Delay_Req wrap; selection=ranked two-fresh-Announce qualification; authentication=enabled, mixed; policy v1; identity=2d61b8da2ad789d9c393f2004473741a425ba3d4a09e43c030f316dabc430dd5; provenance=public-source-contract-v1-not-runtime-attestation. unknown-key-id: pass (signed controls=1/1; response=0; expected=0; response ICV verified=0). wrong-key-value: pass (signed controls=1/1; response=0; expected=0; response ICV verified=0). tampered-body: pass (signed controls=1/1; response=0; expected=0; response ICV verified=0). tampered-ICV: pass (signed controls=1/1; response=0; expected=0; response ICV verified=0). wrong-SPP: pass (signed controls=1/1; response=0; expected=0; response ICV verified=0). unsupported-indicators: pass (signed controls=1/1; response=0; expected=0; response ICV verified=0). wrong-TLV-type: pass (signed controls=1/1; response=0; expected=0; response ICV verified=0). wrong-TLV-length: pass (signed controls=1/1; response=0; expected=0; response ICV verified=0). truncated-ICV: pass (signed controls=1/1; response=0; expected=0; response ICV verified=0). duplicate-authentication-TLV: pass (signed controls=1/1; response=0; expected=0; response ICV verified=0). Unique requester identities/sequences; complete bounded response windows; no pass from silence without signed positive controls. 
Operator phase evidence:
No assisted phase requested.
</pre>

### 40. PTP mixed versus required authentication

Verify that the DUT enforces its configured PTP authentication policy.

Result: **Pass**

<pre>Operator firmware=Combined.
DUT policy v1: selection=ranked two-fresh-Announce qualification; PTP authentication=enabled, mixed; capacity=8; domain=0; foreign timeout=10000 ms; local log Announce/Sync=1/0; GNSS/PPS qualification=3500 ms; SPP=1 outgoing key ID=1; NTP authentication=mixed; source identity=2d61b8da2ad789d9c393f2004473741a425ba3d4a09e43c030f316dabc430dd5; provenance=public-source-contract-v1-not-runtime-attestation. Source configuration only, not runtime attestation.
Target=80:f1:b2:d6:58:14; domain=0; operator firmware=Combined.
Observation=20000 ms; samples=8; response window=500 ms; request interval=250 ms; processing allowance=250 ms; DUT foreign quiet=10000 ms; DUT withdrawal=3500 ms; recovery=60000 ms; operator phase limit=120000 ms; sequence seed=enabled/65532.
MAC=80:f1:b2:d6:58:14 domain=0 mode=3; L2 untagged multicast two-step E2E; sequence-seed=controlled Delay_Req wrap; selection=ranked two-fresh-Announce qualification; authentication=enabled, mixed; policy v1; identity=2d61b8da2ad789d9c393f2004473741a425ba3d4a09e43c030f316dabc430dd5; provenance=public-source-contract-v1-not-runtime-attestation. unsigned-v2.0-policy: pass (signed controls=1/1; response=1; expected=1; response ICV verified=1). unsigned-v2.1-policy: pass (signed controls=1/1; response=1; expected=1; response ICV verified=1). Unique requester identities/sequences; complete bounded response windows; no pass from silence without signed positive controls. 
Operator phase evidence:
No assisted phase requested.
</pre>

### 41. PTP authentication before selection

Verify that the DUT authenticates PTP candidates before considering them for master selection.

Result: **Pass**

<pre>Operator firmware=Combined.
DUT policy v1: selection=ranked two-fresh-Announce qualification; PTP authentication=enabled, mixed; capacity=8; domain=0; foreign timeout=10000 ms; local log Announce/Sync=1/0; GNSS/PPS qualification=3500 ms; SPP=1 outgoing key ID=1; NTP authentication=mixed; source identity=2d61b8da2ad789d9c393f2004473741a425ba3d4a09e43c030f316dabc430dd5; provenance=public-source-contract-v1-not-runtime-attestation. Source configuration only, not runtime attestation.
Target=80:f1:b2:d6:58:14; domain=0; operator firmware=Combined.
Observation=20000 ms; samples=8; response window=500 ms; request interval=250 ms; processing allowance=250 ms; DUT foreign quiet=10000 ms; DUT withdrawal=3500 ms; recovery=60000 ms; operator phase limit=120000 ms; sequence seed=enabled/65532.
MAC=80:f1:b2:d6:58:14 domain=0 mode=3; L2 untagged multicast two-step E2E; sequence-seed=controlled Delay_Req wrap; selection=ranked two-fresh-Announce qualification; authentication=enabled, mixed; policy v1; identity=2d61b8da2ad789d9c393f2004473741a425ba3d4a09e43c030f316dabc430dd5; provenance=public-source-contract-v1-not-runtime-attestation. Authenticated foreign-Announce acceptance/recovery positive control: 
Baseline: L2 observer; HMAC-SHA256-128 verified PTP v2.1; source=80f1b2fffed65814/1; GM=80f1b2fffed65814; priority=128/128; class=248; accuracy=0xfe; variance=65535; steps=0; timeSource=0x20; timescale=PTP/TAI; UTC offset=37 s (valid); leap61=0; leap59=0; timeTraceable=0; frequencyTraceable=0. Quality is advertised, not independently verified. 
wrong-domain: synthetic test candidate source=00f1b2fffed1d0f2/1 GM=00f1b2fffed1d0f2; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[12525706604,12525706868] us; TX sequence=65534 (fresh) successful=[12527735601,12527735866] us; TX sequence=65535 (fresh) successful=[12529745602,12529745867] us; activity=1 post-control=1; completed. 
malformed: synthetic test candidate source=00f1b2fffed1d0ef/1 GM=00f1b2fffed1d0ef; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[12530466476,12530466741] us; TX sequence=65534 (fresh) successful=[12532495601,12532495867] us; TX sequence=65535 (fresh) successful=[12534515601,12534515867] us; activity=1 post-control=1; completed. 
foreign policy: synthetic test candidate source=00f1b2fffed1d318/1 GM=00f1b2fffed1d318; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[12535226475,12535226740] us; TX sequence=65534 (fresh) successful=[12537235601,12537235866] us; TX sequence=65535 (fresh) successful=[12539245601,12539245869] us; TX sequence=0 (fresh) successful=[12541255601,12541255865] us; TX sequence=1 (fresh) successful=[12543265601,12543265866] us; TX sequence=2 (fresh) successful=[12545275601,12545275866] us; TX sequence=3 (fresh) successful=[12547285601,12547285866] us; TX sequence=4 (fresh) successful=[12549295601,12549295866] us; TX sequence=5 (fresh) successful=[12551305601,12551305866] us; TX sequence=6 (fresh) successful=[12553315601,12553315865] us; TX sequence=7 (fresh) successful=[12555325601,12555325865] us; TX sequence=8 (fresh) successful=[12557335601,12557335866] us; TX sequence=9 (fresh) successful=[12559345601,12559345866] us; 
Standby phase=4 start/deadline=12537235866/12559485866 us silence-after=12537485866 us sends=13 last-TX/activity=12559345866/12537115659 us held=1; completed. TX sequence=10 (fresh) successful=[12559495666,12559495932] us; Expiry reference=[12559495666,12559495932] us no-early-before=12569495666 us quiet=1. Recovered fresh Announce+two-step+matching Delay=1; first/last activity=12569514172/12573556851 us. 
inferior/non-winning identity tie-break: synthetic test candidate source=00f1b2fffed1d356/1 GM=00f1b2fffed1d356; priority=129/128 class=248 accuracy=0xfe variance=65535; rank=1 deciding=priority1; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[12574275798,12574276068] us; TX sequence=65534 (fresh) successful=[12576285601,12576285867] us; 
inferior/non-winning identity tie-break: synthetic test candidate source=00f1b2fffed1d344/1 GM=80f1b2fffed65815; priority=128/128 class=248 accuracy=0xfe variance=65535; rank=1 deciding=grandmaster identity; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[12581046455,12581046720] us; TX sequence=65534 (fresh) successful=[12583055601,12583055866] us; 
winning identity tie-break then no-longer-better: synthetic test candidate source=00f1b2fffed1d372/1 GM=80f1b2fffed65813; priority=128/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=grandmaster identity; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[12587816455,12587816721] us; TX sequence=65534 (fresh) successful=[12589825605,12589825870] us; 
Fresh same-source no-longer-better update: deciding=priority1; must recover before original expiry=12599825605 us. TX sequence=65535 (fresh) successful=[12591085660,12591085926] us; Fresh inferior update released standby before original expiry; RX-drops=0. Election does not discipline either clock or establish full BMCA/synchronization accuracy. 
Cleanup: injections stopped; waiting through monotonic 12601335926 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=12601645871/12606181133 us. cleanup healthy=1. unknown-key-id better Announce 1/2: continuous signed service=1; candidate rank=-1. unknown-key-id better Announce 2/2: continuous signed service=1; candidate rank=-1. tampered-ICV better Announce 1/2: continuous signed service=1; candidate rank=-1. tampered-ICV better Announce 2/2: continuous signed service=1; candidate rank=-1. Rejected authentication cannot qualify/suppress the master; both legacy first-Announce and ranked two-fresh-Announce triggers exercised. 
Cleanup: injections stopped; waiting through monotonic 12626715792 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=12627312544/12632367604 us. cleanup healthy=1. 
Operator phase evidence:
No assisted phase requested.
</pre>

### 42. PTP GNSS loss and recovery (assisted)

Verify that the DUT responds correctly when its GNSS reference is lost and restored. (Requires operator assistance.)

Result: **Pass**

<pre>Operator firmware=Combined.
DUT policy v1: selection=ranked two-fresh-Announce qualification; PTP authentication=enabled, mixed; capacity=8; domain=0; foreign timeout=10000 ms; local log Announce/Sync=1/0; GNSS/PPS qualification=3500 ms; SPP=1 outgoing key ID=1; NTP authentication=mixed; source identity=2d61b8da2ad789d9c393f2004473741a425ba3d4a09e43c030f316dabc430dd5; provenance=public-source-contract-v1-not-runtime-attestation. Source configuration only, not runtime attestation.
Target=80:f1:b2:d6:58:14; domain=0; operator firmware=Combined.
Observation=20000 ms; samples=8; response window=500 ms; request interval=250 ms; processing allowance=250 ms; DUT foreign quiet=10000 ms; DUT withdrawal=3500 ms; recovery=60000 ms; operator phase limit=600000 ms; sequence seed=enabled/65532.
MAC=80:f1:b2:d6:58:14 domain=0 mode=3; L2 untagged multicast two-step E2E; sequence-seed=controlled Delay_Req wrap; selection=ranked two-fresh-Announce qualification; authentication=enabled, mixed; policy v1; identity=2d61b8da2ad789d9c393f2004473741a425ba3d4a09e43c030f316dabc430dd5; provenance=public-source-contract-v1-not-runtime-attestation. window=20000 ms+500 ms drain; pairs=19; requests/responses=19/19; samples=19/8; duplicate=0 unmatched=0 expired=0 out-of-order=0 missing=0 sequence-gaps=0 response-loss=0; hardware Sync RX/Delay_Req TX required; timestamp-failures=0 unusable=0 RX-drops=0; path us min/mean/max/sd=27.311/27.395/27.492/0.054; raw local-minus-master arbitrary us min/mean/max=-1793051374692820.000/-1793051374692522.200/-1793051374692224.800. PHC not disciplined; no UTC conversion or accuracy claim. completed. fault=9 acknowledged; withdrawal-bound=3500 ms allowance=250 ms silent-window=12000 ms withdrawn=1; Ranked restart-listening allowance=4000 ms, separate from GNSS/PPS withdrawal=3500 ms. window=20000 ms+500 ms drain; pairs=19; requests/responses=19/19; samples=19/8; duplicate=0 unmatched=0 expired=0 out-of-order=0 missing=0 sequence-gaps=0 response-loss=0; hardware Sync RX/Delay_Req TX required; timestamp-failures=0 unusable=0 RX-drops=0; path us min/mean/max/sd=27.271/27.377/27.472/0.052; raw local-minus-master arbitrary us min/mean/max=-1793051374696324.500/-1793051374696026.200/-1793051374695728.200. PHC not disciplined; no UTC conversion or accuracy claim. completed. Restoration acknowledged qualified; fresh generation-local pair/request keys used. Time properties checked against supported arbitrary or GNSS-derived PTP/TAI policy; traceability claims rejected; real leap event NOT exercised (GNSS simulator required). DMA timestamp provenance/reset ordering requires independent capture and server instrumentation. 
Operator phase evidence:
Injection phase requested: Inject GNSS qualification loss on the DUT using the controlled fixture; keep the tester link and PPS as required by the case. Acknowledge only after the fault is active.
Injection phase: operator acknowledged; token=11; monotonic_ms=48674705.
Restoration phase requested: Restore the DUT GNSS/PPS, server Ethernet link and firmware as applicable. Verify fresh GNSS/PPS clock qualification on the DUT before acknowledging. Do not reset or disconnect the tester.
Restoration phase: operator acknowledged; token=12; monotonic_ms=48738511.

</pre>

### 43. PTP PPS loss and recovery (assisted)

Verify that the DUT responds correctly when its PPS reference is lost and restored. (Requires operator assistance.)

Result: **Pass**

<pre>Operator firmware=Combined.
DUT policy v1: selection=ranked two-fresh-Announce qualification; PTP authentication=enabled, mixed; capacity=8; domain=0; foreign timeout=10000 ms; local log Announce/Sync=1/0; GNSS/PPS qualification=3500 ms; SPP=1 outgoing key ID=1; NTP authentication=mixed; source identity=2d61b8da2ad789d9c393f2004473741a425ba3d4a09e43c030f316dabc430dd5; provenance=public-source-contract-v1-not-runtime-attestation. Source configuration only, not runtime attestation.
Target=80:f1:b2:d6:58:14; domain=0; operator firmware=Combined.
Observation=20000 ms; samples=8; response window=500 ms; request interval=250 ms; processing allowance=250 ms; DUT foreign quiet=10000 ms; DUT withdrawal=3500 ms; recovery=60000 ms; operator phase limit=600000 ms; sequence seed=enabled/65532.
MAC=80:f1:b2:d6:58:14 domain=0 mode=3; L2 untagged multicast two-step E2E; sequence-seed=controlled Delay_Req wrap; selection=ranked two-fresh-Announce qualification; authentication=enabled, mixed; policy v1; identity=2d61b8da2ad789d9c393f2004473741a425ba3d4a09e43c030f316dabc430dd5; provenance=public-source-contract-v1-not-runtime-attestation. window=20000 ms+500 ms drain; pairs=20; requests/responses=20/20; samples=20/8; duplicate=0 unmatched=0 expired=0 out-of-order=0 missing=0 sequence-gaps=0 response-loss=0; hardware Sync RX/Delay_Req TX required; timestamp-failures=0 unusable=0 RX-drops=0; path us min/mean/max/sd=27.233/27.336/27.433/0.057; raw local-minus-master arbitrary us min/mean/max=-1793051373319947.000/-1793051373319630.800/-1793051373319314.200. PHC not disciplined; no UTC conversion or accuracy claim. completed. fault=10 acknowledged; withdrawal-bound=3500 ms allowance=250 ms silent-window=12000 ms withdrawn=1; Ranked restart-listening allowance=4000 ms, separate from GNSS/PPS withdrawal=3500 ms. window=20000 ms+500 ms drain; pairs=19; requests/responses=19/19; samples=19/8; duplicate=0 unmatched=0 expired=0 out-of-order=0 missing=0 sequence-gaps=0 response-loss=0; hardware Sync RX/Delay_Req TX required; timestamp-failures=0 unusable=0 RX-drops=0; path us min/mean/max/sd=27.272/27.345/27.433/0.048; raw local-minus-master arbitrary us min/mean/max=-1793051373322530.200/-1793051373322230.000/-1793051373321930.000. PHC not disciplined; no UTC conversion or accuracy claim. completed. Restoration acknowledged qualified; fresh generation-local pair/request keys used. Time properties checked against supported arbitrary or GNSS-derived PTP/TAI policy; traceability claims rejected; real leap event NOT exercised (GNSS simulator required). DMA timestamp provenance/reset ordering requires independent capture and server instrumentation. 
Operator phase evidence:
Injection phase requested: Remove PPS from the DUT using the controlled fixture, keeping the tester link connected. Acknowledge only after PPS loss is active.
Injection phase: operator acknowledged; token=5; monotonic_ms=6742685.
Restoration phase requested: Restore the DUT GNSS/PPS, server Ethernet link and firmware as applicable. Verify fresh GNSS/PPS clock qualification on the DUT before acknowledging. Do not reset or disconnect the tester.
Restoration phase: operator acknowledged; token=6; monotonic_ms=6783340.

</pre>

### 44. PTP server link loss and recovery (assisted)

Verify that the DUT recovers correctly after its server link is interrupted and restored. (Requires operator assistance.)

Result: **Pass**

<pre>Operator firmware=Combined.
DUT policy v1: selection=ranked two-fresh-Announce qualification; PTP authentication=enabled, mixed; capacity=8; domain=0; foreign timeout=10000 ms; local log Announce/Sync=1/0; GNSS/PPS qualification=3500 ms; SPP=1 outgoing key ID=1; NTP authentication=mixed; source identity=2d61b8da2ad789d9c393f2004473741a425ba3d4a09e43c030f316dabc430dd5; provenance=public-source-contract-v1-not-runtime-attestation. Source configuration only, not runtime attestation.
Target=80:f1:b2:d6:58:14; domain=0; operator firmware=Combined.
Observation=20000 ms; samples=8; response window=500 ms; request interval=250 ms; processing allowance=250 ms; DUT foreign quiet=10000 ms; DUT withdrawal=3500 ms; recovery=60000 ms; operator phase limit=120000 ms; sequence seed=enabled/65532.
MAC=80:f1:b2:d6:58:14 domain=0 mode=3; L2 untagged multicast two-step E2E; sequence-seed=controlled Delay_Req wrap; selection=ranked two-fresh-Announce qualification; authentication=enabled, mixed; policy v1; identity=2d61b8da2ad789d9c393f2004473741a425ba3d4a09e43c030f316dabc430dd5; provenance=public-source-contract-v1-not-runtime-attestation. window=20000 ms+500 ms drain; pairs=20; requests/responses=20/20; samples=20/8; duplicate=0 unmatched=0 expired=0 out-of-order=0 missing=0 sequence-gaps=0 response-loss=0; hardware Sync RX/Delay_Req TX required; timestamp-failures=0 unusable=0 RX-drops=0; path us min/mean/max/sd=27.212/27.334/27.452/0.055; raw local-minus-master arbitrary us min/mean/max=-1793051374686130.200/-1793051374685816.500/-1793051374685502.500. PHC not disciplined; no UTC conversion or accuracy claim. completed. fault=11 acknowledged; withdrawal-bound=3500 ms allowance=250 ms silent-window=12000 ms withdrawn=1; Ranked restart-listening allowance=4000 ms, separate from GNSS/PPS withdrawal=3500 ms. window=20000 ms+500 ms drain; pairs=19; requests/responses=19/19; samples=19/8; duplicate=0 unmatched=0 expired=0 out-of-order=0 missing=0 sequence-gaps=0 response-loss=0; hardware Sync RX/Delay_Req TX required; timestamp-failures=0 unusable=0 RX-drops=0; path us min/mean/max/sd=12.233/26.591/27.505/3.477; raw local-minus-master arbitrary us min/mean/max=-1793051374688395.500/-1793051374688097.800/-1793051374687801.500. PHC not disciplined; no UTC conversion or accuracy claim. completed. Restoration acknowledged qualified; fresh generation-local pair/request keys used. Time properties checked against supported arbitrary or GNSS-derived PTP/TAI policy; traceability claims rejected; real leap event NOT exercised (GNSS simulator required). DMA timestamp provenance/reset ordering requires independent capture and server instrumentation. 
Operator phase evidence:
Injection phase requested: Disconnect the SERVER-side Ethernet link on an isolated switched test network, NOT the tester link. Acknowledge only after the DUT link is down.
Injection phase: operator acknowledged; token=9; monotonic_ms=48464172.
Restoration phase requested: Restore the DUT GNSS/PPS, server Ethernet link and firmware as applicable. Verify fresh GNSS/PPS clock qualification on the DUT before acknowledging. Do not reset or disconnect the tester.
Restoration phase: operator acknowledged; token=10; monotonic_ms=48497720.

</pre>

### 45. PTP restart / OTA recovery (assisted)

Verify that the DUT resumes correct PTP service after a restart or firmware update. (Requires operator assistance.)

Result: **Pass**

<pre>Operator firmware=Combined.
DUT policy v1: selection=ranked two-fresh-Announce qualification; PTP authentication=enabled, mixed; capacity=8; domain=0; foreign timeout=10000 ms; local log Announce/Sync=1/0; GNSS/PPS qualification=3500 ms; SPP=1 outgoing key ID=1; NTP authentication=mixed; source identity=2d61b8da2ad789d9c393f2004473741a425ba3d4a09e43c030f316dabc430dd5; provenance=public-source-contract-v1-not-runtime-attestation. Source configuration only, not runtime attestation.
Target=80:f1:b2:d6:58:14; domain=0; operator firmware=Combined.
Observation=20000 ms; samples=8; response window=500 ms; request interval=250 ms; processing allowance=250 ms; DUT foreign quiet=10000 ms; DUT withdrawal=3500 ms; recovery=60000 ms; operator phase limit=120000 ms; sequence seed=enabled/65532.
MAC=80:f1:b2:d6:58:14 domain=0 mode=3; L2 untagged multicast two-step E2E; sequence-seed=controlled Delay_Req wrap; selection=ranked two-fresh-Announce qualification; authentication=enabled, mixed; policy v1; identity=2d61b8da2ad789d9c393f2004473741a425ba3d4a09e43c030f316dabc430dd5; provenance=public-source-contract-v1-not-runtime-attestation. window=20000 ms+500 ms drain; pairs=19; requests/responses=19/19; samples=19/8; duplicate=0 unmatched=0 expired=0 out-of-order=0 missing=0 sequence-gaps=0 response-loss=0; hardware Sync RX/Delay_Req TX required; timestamp-failures=0 unusable=0 RX-drops=0; path us min/mean/max/sd=27.272/27.360/27.512/0.065; raw local-minus-master arbitrary us min/mean/max=-1793051374706424.000/-1793051374706125.500/-1793051374705827.500. PHC not disciplined; no UTC conversion or accuracy claim. completed. fault=12 acknowledged; withdrawal-bound=3500 ms allowance=250 ms silent-window=12000 ms withdrawn=1; Ranked restart-listening allowance=4000 ms, separate from GNSS/PPS withdrawal=3500 ms. window=20000 ms+500 ms drain; pairs=19; requests/responses=19/19; samples=19/8; duplicate=0 unmatched=0 expired=0 out-of-order=0 missing=0 sequence-gaps=0 response-loss=0; hardware Sync RX/Delay_Req TX required; timestamp-failures=0 unusable=0 RX-drops=0; path us min/mean/max/sd=27.232/27.339/27.432/0.050; raw local-minus-master arbitrary us min/mean/max=-1793051374709283.500/-1793051374708985.200/-1793051374708687.000. PHC not disciplined; no UTC conversion or accuracy claim. completed. Restoration acknowledged qualified; fresh generation-local pair/request keys used. Time properties checked against supported arbitrary or GNSS-derived PTP/TAI policy; traceability claims rejected; real leap event NOT exercised (GNSS simulator required). DMA timestamp provenance/reset ordering requires independent capture and server instrumentation. 
Operator phase evidence:
Injection phase requested: Start the controlled DUT restart or OTA phase; keep the tester link connected. Acknowledge only after the restart/OTA fault phase has begun.
Injection phase: operator acknowledged; token=15; monotonic_ms=49099612.
Restoration phase requested: Restore the DUT GNSS/PPS, server Ethernet link and firmware as applicable. Verify fresh GNSS/PPS clock qualification on the DUT before acknowledging. Do not reset or disconnect the tester.
Restoration phase: operator acknowledged; token=16; monotonic_ms=49132864.

</pre>

### 46. PTP service recovery while standing by (assisted)

Verify that GNSS loss/restoration does not release standby while a better foreign master remains active, then verify healthy service after candidate expiry. (Requires ranked selection, foreign-master and assisted-test opt-ins, and operator assistance.)

Result: **Pass**

<pre>Operator firmware=Combined.
DUT policy v1: selection=ranked two-fresh-Announce qualification; PTP authentication=enabled, mixed; capacity=8; domain=0; foreign timeout=10000 ms; local log Announce/Sync=1/0; GNSS/PPS qualification=3500 ms; SPP=1 outgoing key ID=1; NTP authentication=mixed; source identity=2d61b8da2ad789d9c393f2004473741a425ba3d4a09e43c030f316dabc430dd5; provenance=public-source-contract-v1-not-runtime-attestation. Source configuration only, not runtime attestation.
Target=80:f1:b2:d6:58:14; domain=0; operator firmware=Combined.
Observation=20000 ms; samples=8; response window=500 ms; request interval=250 ms; processing allowance=250 ms; DUT foreign quiet=10000 ms; DUT withdrawal=3500 ms; recovery=60000 ms; operator phase limit=600000 ms; sequence seed=enabled/65532.
MAC=80:f1:b2:d6:58:14 domain=0 mode=3; L2 untagged multicast two-step E2E; sequence-seed=controlled Delay_Req wrap; selection=ranked two-fresh-Announce qualification; authentication=enabled, mixed; policy v1; identity=2d61b8da2ad789d9c393f2004473741a425ba3d4a09e43c030f316dabc430dd5; provenance=public-source-contract-v1-not-runtime-attestation. 
GNSS standby recovery: synthetic test candidate source=00f1b2fffed1d25c/1 GM=00f1b2fffed1d25c; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=-1 actual refresh=500000 us lifetime=10000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[48825361618,48825361887] us; TX sequence=65534 (fresh) successful=[48826620613,48826620885] us; Scope: GNSS reception/data loss and restoration only; PPS and both Ethernet links stay connected. Operator verifies loss and fresh qualification; silence is not proof of GNSS loss. Fresh foreign Announces continue during both prompts; refresh=500000 us poll budget=100000 us. 
GNSS injection prompt completed: fresh Announce sends=30 last sequence=26 last TX=[48843110601,48843110872] us; standby violations=0 first=0 us protocol-fault=0; completed. 
GNSS fault observation completed: fresh Announce sends=60 last sequence=56 last TX=[48858830604,48858830937] us; standby violations=0 first=0 us protocol-fault=0; completed. 
GNSS restoration prompt completed: fresh Announce sends=92 last sequence=88 last TX=[48876550600,48876550872] us; standby violations=0 first=0 us protocol-fault=0; completed. 
Post-restoration standby completed: fresh Announce sends=130 last sequence=126 last TX=[48896420604,48896420948] us; standby violations=0 first=0 us protocol-fault=0; completed. TX sequence=127 (fresh) successful=[48896910686,48896910958] us; Foreign refresh stopped: no service before 48906910686 us; full-service recovery deadline=48913160958 us. Bounded recovery: fresh Announce+two-step+matching Delay before 48913160958 us; first/last activity=48906921327/48906921734 us. GNSS-only standby interaction complete: post-restoration hold=20000000 us; violations=0. No PPS/link/restart fault coverage, internal queue/generation-race, DMA or clock-discipline claim. 
Cleanup: injections stopped; waiting through monotonic 48907160958 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=48907941198/48913001472 us. cleanup healthy=1. 
Operator phase evidence:
Injection phase requested: Remove DUT GNSS reception/data using the controlled fixture, keeping PPS and both Ethernet links connected. Verify that DUT GNSS qualification is lost before acknowledging; wire silence alone is not proof. The tester continues refreshing the better foreign master. If skipping/stopping, restore GNSS manually.
Injection phase: operator acknowledged; token=13; monotonic_ms=48843520.
Restoration phase requested: Restore DUT GNSS reception/data while keeping PPS and both Ethernet links connected. Verify fresh GNSS/PPS qualification on the DUT before acknowledging. The tester is still refreshing the better foreign master: the DUT must remain in standby. If skipping/stopping, restore GNSS manually.
Restoration phase: operator acknowledged; token=14; monotonic_ms=48876880.

</pre>
