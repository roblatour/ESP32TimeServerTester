<p align="center"><strong>ESP32TimeServerTester Report</strong></p>

## Settings

| Settings | Values |
| --- | --- |
| **Target time server:** | ESP32TimeServer New Board |
| **Firmware mode:** | NTP + PTP L2 + PTP L3 UDP |
| **IPv4 address:** | 192.168.1.24 |
| **IPv4 NTP Port:** | 123 |
| **IPv6 address:** | FE80::82F1:B2FF:FED2:CA9A |
| **IPv6 NTP Port:** | 123 |
| **PTP L2 master MAC:** | 80:F1:B2:D2:CA:9A |
| **PTP L2 domain:** | 0 |
| **PTP L2 symmetric-key id:** | Present but redacted in this report |
| **PTP L2 symmetric-key value:** | Present but redacted in this report |
| **PTP L3 UDP IPv4 master:** | 192.168.1.24 |
| **PTP L3 UDP IPv6 master:** | FE80::82F1:B2FF:FED2:CA9A |
| **PTP L3 UDP domain:** | 0 |
| **PTP L3 UDP withdrawal bound:** | 5000 ms |
| **PTP L3 UDP multicast:** | IPv4 224.0.1.129; IPv6 ff02::181; event/general UDP ports 319/320; no PTP authentication |
| **NTPv4 symmetric-key id:** | Present but redacted in this report |
| **NTPv4 symmetric-key value:** | Present but redacted in this report |
| **Mini saturated stress tests:** | Enabled (1000 NTP requests each) |

## Test results

| No. | Test | Result | Reported date/time or PTP diagnostic |
| ---: | --- | --- | --- |
| 1 | IPv4 Ping target time server | <span style="color:green">Pass</span> | See detailed evidence |
| 2 | IPv6 Ping target time server | <span style="color:green">Pass</span> | See detailed evidence |
| 3 | IPv4 NTPv3 Standard | <span style="color:green">Pass</span> | See detailed evidence |
| 4 | IPv4 NTPv4 Standard | <span style="color:green">Pass</span> | See detailed evidence |
| 5 | IPv4 NTPv4 Interleaved | <span style="color:green">Pass</span> | See detailed evidence |
| 6 | IPv4 NTPv4 Symmetric-key standard | <span style="color:green">Pass</span> | See detailed evidence |
| 7 | IPv4 NTPv4 Symmetric-key interleaved | <span style="color:green">Pass</span> | See detailed evidence |
| 8 | IPv4 NTPv4 Invalid symmetric-key | <span style="color:green">Pass</span> | See detailed evidence |
| 9 | IPv4 NTPv4 Missing symmetric-key | <span style="color:green">Pass</span> | See detailed evidence |
| 10 | IPv4 NTPv4 Truncated MAC | <span style="color:green">Pass</span> | See detailed evidence |
| 11 | IPv4 NTPv4 Standard mini saturated stress test | <span style="color:green">Pass</span> | See detailed evidence |
| 12 | IPv6 NTPv3 Standard | <span style="color:green">Pass</span> | See detailed evidence |
| 13 | IPv6 NTPv4 Standard | <span style="color:green">Pass</span> | See detailed evidence |
| 14 | IPv6 NTPv4 Interleaved | <span style="color:green">Pass</span> | See detailed evidence |
| 15 | IPv6 NTPv4 Symmetric-key standard | <span style="color:green">Pass</span> | See detailed evidence |
| 16 | IPv6 NTPv4 Symmetric-key interleaved | <span style="color:green">Pass</span> | See detailed evidence |
| 17 | IPv6 NTPv4 Invalid symmetric-key | <span style="color:green">Pass</span> | See detailed evidence |
| 18 | IPv6 NTPv4 Missing symmetric-key | <span style="color:green">Pass</span> | See detailed evidence |
| 19 | IPv6 NTPv4 Truncated MAC | <span style="color:green">Pass</span> | See detailed evidence |
| 20 | IPv6 NTPv4 Standard mini saturated stress test | <span style="color:green">Pass</span> | See detailed evidence |
| 21 | PTP L2 bounded simultaneous IPv4 / IPv6 NTP load | <span style="color:green">Pass</span> | See detailed evidence |
| 22 | PTP L2 standby with bounded IPv4 / IPv6 NTP load | <span style="color:green">Pass</span> | See detailed evidence |
| 23 | PTP L2 standby with NTP key-policy probes under load | <span style="color:green">Pass</span> | See detailed evidence |
| 24 | PTP L2 standby with NTP holdover under load (assisted) | Not yet run | n/a |
| 25 | PTP L3 UDP IPv4 with bounded NTP load | <span style="color:green">Pass</span> | See detailed evidence |
| 26 | PTP L3 UDP IPv6 with bounded NTP load | <span style="color:green">Pass</span> | See detailed evidence |
| 27 | PTP L3 UDP simultaneous IPv4 / IPv6 with bounded NTP load | <span style="color:green">Pass</span> | See detailed evidence |
| 28 | PTP L2 Announce and master identity | <span style="color:green">Pass</span> | See detailed evidence |
| 29 | PTP L2 two-step Sync / Follow_Up | <span style="color:green">Pass</span> | See detailed evidence |
| 30 | PTP L2 E2E Delay_Req / Delay_Resp | <span style="color:green">Pass</span> | See detailed evidence |
| 31 | PTP L2 time properties and clock quality | <span style="color:green">Pass</span> | See detailed evidence |
| 32 | PTP L2 offset / path delay diagnostic | <span style="color:green">Pass</span> | See detailed evidence |
| 33 | PTP L2 sustained exchange integrity | <span style="color:green">Pass</span> | See detailed evidence |
| 34 | PTP L2 negative Delay_Req acceptance | <span style="color:green">Pass</span> | See detailed evidence |
| 35 | PTP L2 foreign-master policy and recovery | <span style="color:green">Pass</span> | See detailed evidence |
| 36 | PTP L2-only IP-independent operation | Not yet run | n/a |
| 37 | PTP L2 ranking precedence | <span style="color:green">Pass</span> | See detailed evidence |
| 38 | PTP L2 qualification and source isolation | <span style="color:green">Pass</span> | See detailed evidence |
| 39 | PTP L2 duplicate, reorder and rollover policy | <span style="color:green">Pass</span> | See detailed evidence |
| 40 | PTP L2 dataset changes and self-grandmaster reflection | <span style="color:green">Pass</span> | See detailed evidence |
| 41 | PTP L2 multiple better masters | <span style="color:green">Pass</span> | See detailed evidence |
| 42 | PTP L2 interval-sensitive expiry | <span style="color:green">Pass</span> | See detailed evidence |
| 43 | PTP L2 signed service messages | <span style="color:green">Pass</span> | See detailed evidence |
| 44 | PTP L2 authenticated E2E exchange | <span style="color:green">Pass</span> | See detailed evidence |
| 45 | PTP L2 tampered and malformed authentication | <span style="color:green">Pass</span> | See detailed evidence |
| 46 | PTP L2 mixed versus required authentication | <span style="color:green">Pass</span> | See detailed evidence |
| 47 | PTP L2 authentication before selection | <span style="color:green">Pass</span> | See detailed evidence |
| 48 | PTP L2 optional unsigned service | Could not run | See detailed evidence |
| 49 | PTP L2 optional unsigned signed-input rejection | Could not run | See detailed evidence |
| 50 | PTP L2 unsigned Announce authentication policy | <span style="color:green">Pass</span> | See detailed evidence |
| 51 | PTP L2 invalid authentication cannot refresh standby | <span style="color:green">Pass</span> | See detailed evidence |
| 52 | PTP L2 bounded foreign-master capacity | <span style="color:green">Pass</span> | See detailed evidence |
| 53 | PTP L3 UDP IPv4 Announce and master identity | <span style="color:green">Pass</span> | See detailed evidence |
| 54 | PTP L3 UDP IPv6 Announce and master identity | <span style="color:green">Pass</span> | See detailed evidence |
| 55 | PTP L3 UDP IPv4 two-step Sync / Follow_Up | <span style="color:green">Pass</span> | See detailed evidence |
| 56 | PTP L3 UDP IPv6 two-step Sync / Follow_Up | <span style="color:green">Pass</span> | See detailed evidence |
| 57 | PTP L3 UDP IPv4 E2E Delay_Req / Delay_Resp | <span style="color:green">Pass</span> | See detailed evidence |
| 58 | PTP L3 UDP IPv6 E2E Delay_Req / Delay_Resp | <span style="color:green">Pass</span> | See detailed evidence |
| 59 | PTP L3 UDP IPv4 time properties and clock quality | <span style="color:green">Pass</span> | See detailed evidence |
| 60 | PTP L3 UDP IPv6 time properties and clock quality | <span style="color:green">Pass</span> | See detailed evidence |
| 61 | PTP L3 UDP IPv4 sustained exchange integrity | <span style="color:green">Pass</span> | See detailed evidence |
| 62 | PTP L3 UDP IPv6 sustained exchange integrity | <span style="color:green">Pass</span> | See detailed evidence |
| 63 | PTP L3 UDP IPv4 invalid Delay_Req rejection | <span style="color:green">Pass</span> | See detailed evidence |
| 64 | PTP L3 UDP IPv6 invalid Delay_Req rejection | <span style="color:green">Pass</span> | See detailed evidence |
| 65 | PTP L3 UDP simultaneous IPv4 / IPv6 service | <span style="color:green">Pass</span> | See detailed evidence |
| 66 | PTP L2 GNSS loss and recovery (assisted) | Not yet run | n/a |
| 67 | PTP L2 PPS loss and recovery (assisted) | Not yet run | n/a |
| 68 | PTP L2 server link loss and recovery (assisted) | Not yet run | n/a |
| 69 | PTP L2 restart / OTA recovery (assisted) | Not yet run | n/a |
| 70 | PTP L2 service recovery while standing by (assisted) | Not yet run | n/a |
| 71 | PTP L2 PPS recovery while standing by (assisted) | Not yet run | n/a |
| 72 | PTP L2 server link recovery while standing by (assisted) | Not yet run | n/a |
| 73 | PTP L2 restart recovery while standing by (assisted) | Not yet run | n/a |
| 74 | PTP L2 staged key rotation and revocation (assisted) | Not yet run | n/a |
| 75 | PTP L2 timestamp-referenced startup listening (assisted) | Not yet run | n/a |
| 76 | PTP L2 candidate expiry while timing is unqualified (assisted) | Not yet run | n/a |
| 77 | PTP L3 UDP IPv4 GNSS loss and recovery (assisted) | Not yet run | n/a |
| 78 | PTP L3 UDP IPv6 GNSS loss and recovery (assisted) | Not yet run | n/a |
| 79 | PTP L3 UDP IPv4 PPS loss and recovery (assisted) | Not yet run | n/a |
| 80 | PTP L3 UDP IPv6 PPS loss and recovery (assisted) | Not yet run | n/a |
| 81 | PTP L3 UDP IPv4 server link recovery (assisted) | Not yet run | n/a |
| 82 | PTP L3 UDP IPv6 server link recovery (assisted) | Not yet run | n/a |
| 83 | PTP L3 UDP IPv4 restart recovery (assisted) | Not yet run | n/a |
| 84 | PTP L3 UDP IPv6 restart recovery (assisted) | Not yet run | n/a |

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

PTP L2 is a restricted two-step E2E observer with legacy/ranked foreign-master policy testing and optional HMAC-SHA256-128 authentication. PTP L3 UDP tests unsigned IPv4 and link-local IPv6 multicast E2E service; the UDP socket API does not provide hardware timestamps, so those tests do not measure clock offset, path delay or synchronization accuracy. Neither transport is a follower servo or a full BMCA/profile conformance suite. PTP L2 and NTP keys are independent; PTP L3 UDP does not use a key. Use only an authorized isolated test link with no unrelated PTP master. The tester PHC is not disciplined. L2 measurements require hardware Sync RX and Delay_Req TX timestamps. Accuracy requires a hardware-timestamped client with a running servo, packet capture and independent PPS/phase measurement. Source metadata is not runtime attestation. Incomplete or interrupted scenarios cannot pass.

## DUT policy metadata

DUT policy v1: selection=ranked two-fresh-Announce qualification; PTP authentication=enabled, optional (signed with keys, unsigned without keys); capacity=8; domain=0; foreign timeout=10000 ms; local log Announce/Sync=1/0; GNSS/PPS qualification=3500 ms; SPP=1 outgoing key ID=1; NTP authentication=mixed; source identity=0d2f18d75f34779b071d5c8a17e573e1190b00d83ab6d2ba7cdd9aff570dc038; provenance=public-source-contract-v2-not-runtime-attestation. Source configuration only, not runtime attestation.

## Detailed test evidence

### 1. IPv4 Ping target time server

Ensure the DUT is reachable over an IPv4 connection.

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
n/a</pre>

### 2. IPv6 Ping target time server

Ensure the DUT is reachable over an IPv6 connection.

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
n/a</pre>

### 3. IPv4 NTPv3 Standard

Verify standard NTPv3 service over IPv4; under required NTP authentication, verify that unsigned NTPv3 is rejected using authenticated NTPv4 before/after controls.

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
2026-11-04 20:17:56.713421499 (UTC)</pre>

### 4. IPv4 NTPv4 Standard

Verify standard NTPv4 service over IPv4; under required NTP authentication, verify that unsigned NTPv4 is rejected using authenticated before/after controls.

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
2026-11-04 20:17:56.809891581 (UTC)</pre>

### 5. IPv4 NTPv4 Interleaved

Verify interleaved NTPv4 exchanges over IPv4, using authenticated requests and verified responses when NTP authentication is required.

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
2026-11-04 20:17:56.909442821 (UTC)</pre>

### 6. IPv4 NTPv4 Symmetric-key standard

Verify that the DUT accepts authenticated standard NTP version 4 requests over IPv4.

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
2026-11-04 20:17:57.009714628 (UTC)</pre>

### 7. IPv4 NTPv4 Symmetric-key interleaved

Verify that the DUT accepts authenticated interleaved NTP version 4 requests over IPv4.

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
2026-11-04 20:17:57.109479950 (UTC)</pre>

### 8. IPv4 NTPv4 Invalid symmetric-key

Verify that the DUT rejects NTPv4 requests with invalid authentication over IPv4, with healthy before/after controls.

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
Authenticated NTP controls before/after=1/1 (after error=0); link intact=1; negative probe=rejected; No usable time response to the probe.</pre>

### 9. IPv4 NTPv4 Missing symmetric-key

Verify that the DUT rejects an incomplete NTPv4 authentication trailer over IPv4, with healthy before/after controls.

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
Authenticated NTP controls before/after=1/1 (after error=0); link intact=1; negative probe=rejected; No usable time response to the probe.</pre>

### 10. IPv4 NTPv4 Truncated MAC

Verify that the DUT rejects NTPv4 requests with incomplete authentication data over IPv4, with healthy before/after controls.

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
Authenticated NTP controls before/after=1/1 (after error=0); link intact=1; negative probe=rejected; No usable time response to the probe.</pre>

### 11. IPv4 NTPv4 Standard mini saturated stress test

Verify responsiveness during a short, saturated IPv4 NTP load, with authenticated requests and verified responses when required. (Runs only when mini stress tests are enabled for the target.)

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
2026-11-04 20:18:07.148424155 (UTC)</pre>

### 12. IPv6 NTPv3 Standard

Verify standard NTPv3 service over IPv6; under required NTP authentication, verify that unsigned NTPv3 is rejected using authenticated NTPv4 before/after controls.

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
2026-11-04 20:18:07.239890769 (UTC)</pre>

### 13. IPv6 NTPv4 Standard

Verify standard NTPv4 service over IPv6; under required NTP authentication, verify that unsigned NTPv4 is rejected using authenticated before/after controls.

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
2026-11-04 20:18:07.339714092 (UTC)</pre>

### 14. IPv6 NTPv4 Interleaved

Verify interleaved NTPv4 exchanges over IPv6, using authenticated requests and verified responses when NTP authentication is required.

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
2026-11-04 20:18:07.439748656 (UTC)</pre>

### 15. IPv6 NTPv4 Symmetric-key standard

Verify that the DUT accepts authenticated standard NTP version 4 requests over IPv6.

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
2026-11-04 20:18:07.539836000 (UTC)</pre>

### 16. IPv6 NTPv4 Symmetric-key interleaved

Verify that the DUT accepts authenticated interleaved NTP version 4 requests over IPv6.

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
2026-11-04 20:18:07.639859305 (UTC)</pre>

### 17. IPv6 NTPv4 Invalid symmetric-key

Verify that the DUT rejects NTPv4 requests with invalid authentication over IPv6, with healthy before/after controls.

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
Authenticated NTP controls before/after=1/1 (after error=0); link intact=1; negative probe=rejected; No usable time response to the probe.</pre>

### 18. IPv6 NTPv4 Missing symmetric-key

Verify that the DUT rejects an incomplete NTPv4 authentication trailer over IPv6, with healthy before/after controls.

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
Authenticated NTP controls before/after=1/1 (after error=0); link intact=1; negative probe=rejected; No usable time response to the probe.</pre>

### 19. IPv6 NTPv4 Truncated MAC

Verify that the DUT rejects NTPv4 requests with incomplete authentication data over IPv6, with healthy before/after controls.

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
Authenticated NTP controls before/after=1/1 (after error=0); link intact=1; negative probe=rejected; No usable time response to the probe.</pre>

### 20. IPv6 NTPv4 Standard mini saturated stress test

Verify responsiveness during a short, saturated IPv6 NTP load, with authenticated requests and verified responses when required. (Runs only when mini stress tests are enabled for the target.)

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
2026-11-04 20:18:17.698082660 (UTC)</pre>

### 21. PTP L2 bounded simultaneous IPv4 / IPv6 NTP load

Verify that the DUT remains responsive while serving bounded IPv4, IPv6, and PTP L2 traffic together. (Requires NTP and PTP L2 firmware.)

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
DUT policy v1: selection=ranked two-fresh-Announce qualification; PTP authentication=enabled, optional (signed with keys, unsigned without keys); capacity=8; domain=0; foreign timeout=10000 ms; local log Announce/Sync=1/0; GNSS/PPS qualification=3500 ms; SPP=1 outgoing key ID=1; NTP authentication=mixed; source identity=0d2f18d75f34779b071d5c8a17e573e1190b00d83ab6d2ba7cdd9aff570dc038; provenance=public-source-contract-v2-not-runtime-attestation. Source configuration only, not runtime attestation.
Target=80:F1:B2:D2:CA:9A; domain=0; operator firmware=NtpPtpL2PtpL3.
Observation=20000 ms; samples=8; response window=500 ms; request interval=250 ms; processing allowance=250 ms; DUT foreign quiet=10000 ms; DUT withdrawal=3500 ms; recovery=60000 ms; operator phase limit=120000 ms; sequence seed=enabled/65532.
Unloaded Sustained baseline (Pass):
MAC=80:f1:b2:d2:ca:9a domain=0 mode=7; L2 untagged multicast two-step E2E; sequence-seed=controlled Delay_Req wrap; selection=ranked two-fresh-Announce qualification; authentication=enabled, optional (signed with keys, unsigned without keys); policy v1; identity=0d2f18d75f34779b071d5c8a17e573e1190b00d83ab6d2ba7cdd9aff570dc038; provenance=public-source-contract-v2-not-runtime-attestation. Observed outgoing=verified signed v2.1 (not inferred from configured enabled/required policy). window=20000 ms+500 ms drain; pairs=20; requests/responses=20/20; samples=20/8; duplicate=0 unmatched=0 expired=0 out-of-order=0 missing=0 sequence-gaps=0 response-loss=0; hardware Sync RX/Delay_Req TX required; timestamp-failures=0 unusable=0 RX-drops=0; path us min/mean/max/sd=27.272/27.364/27.453/0.052; raw local-minus-master arbitrary us min/mean/max=-1793823469970778.800/-1793823469970458.800/-1793823469970138.800. PHC not disciplined; no UTC conversion or accuracy claim. completed. 
NTP controls before load (unsigned): IPv4=Pass; IPv6=Pass.
Loaded PTP (Pass):
MAC=80:f1:b2:d2:ca:9a domain=0 mode=7; L2 untagged multicast two-step E2E; sequence-seed=controlled Delay_Req wrap; selection=ranked two-fresh-Announce qualification; authentication=enabled, optional (signed with keys, unsigned without keys); policy v1; identity=0d2f18d75f34779b071d5c8a17e573e1190b00d83ab6d2ba7cdd9aff570dc038; provenance=public-source-contract-v2-not-runtime-attestation. Observed outgoing=verified signed v2.1 (not inferred from configured enabled/required policy). window=20000 ms+500 ms drain; pairs=20; requests/responses=20/20; samples=20/8; duplicate=0 unmatched=0 expired=0 out-of-order=0 missing=0 sequence-gaps=0 response-loss=0; hardware Sync RX/Delay_Req TX required; timestamp-failures=0 unusable=0 RX-drops=0; path us min/mean/max/sd=27.312/27.379/27.452/0.035; raw local-minus-master arbitrary us min/mean/max=-1793823469971519.500/-1793823469971199.800/-1793823469970879.800. PHC not disciplined; no UTC conversion or accuracy claim. completed. 
NTP load: unsigned requests; configured observation+drain duration=20500 ms (half-open request schedule; endpoint scheduling grace adds no requests); scheduled load rate=10 requests/s; load worker max outstanding=1; full load window=yes; entire PTP collection covered=yes.
Monotonic windows (us): NTP=[172400592,192920655]; PTP collection=[172405595,192915609]; actual overlap=20510014 us; uncovered collection=0 us.
IPv4 load: attempts=103; successes=103; timeouts=0; invalid replies=0; local/socket errors=0.
IPv6 load: attempts=102; successes=102; timeouts=0; invalid replies=0; local/socket errors=0.
Client pacing misses=0; ENOBUFS/ENOMEM=0; UDP RX queue drop counters unavailable via this socket API (not assumed zero). PTP RX queue evidence is reported by the engine. Client overload is not server loss.
NTP controls after load: IPv4=Pass (error=0); IPv6=Pass (error=0).
Operator phase evidence:
No assisted phase requested.
</pre>

### 22. PTP L2 standby with bounded IPv4 / IPv6 NTP load

Verify NTP controls and bounded traffic while a qualified foreign master holds PTP in standby. (Requires NTP + PTP L2 firmware and load/foreign-master opt-ins.)

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
DUT policy v1: selection=ranked two-fresh-Announce qualification; PTP authentication=enabled, optional (signed with keys, unsigned without keys); capacity=8; domain=0; foreign timeout=10000 ms; local log Announce/Sync=1/0; GNSS/PPS qualification=3500 ms; SPP=1 outgoing key ID=1; NTP authentication=mixed; source identity=0d2f18d75f34779b071d5c8a17e573e1190b00d83ab6d2ba7cdd9aff570dc038; provenance=public-source-contract-v2-not-runtime-attestation. Source configuration only, not runtime attestation.
Target=80:F1:B2:D2:CA:9A; domain=0; operator firmware=NtpPtpL2PtpL3.
Observation=20000 ms; samples=8; response window=500 ms; request interval=250 ms; processing allowance=250 ms; DUT foreign quiet=10000 ms; DUT withdrawal=3500 ms; recovery=60000 ms; operator phase limit=120000 ms; sequence seed=enabled/65532.
Unloaded Sustained baseline (Pass):
MAC=80:f1:b2:d2:ca:9a domain=0 mode=7; L2 untagged multicast two-step E2E; sequence-seed=controlled Delay_Req wrap; selection=ranked two-fresh-Announce qualification; authentication=enabled, optional (signed with keys, unsigned without keys); policy v1; identity=0d2f18d75f34779b071d5c8a17e573e1190b00d83ab6d2ba7cdd9aff570dc038; provenance=public-source-contract-v2-not-runtime-attestation. Observed outgoing=verified signed v2.1 (not inferred from configured enabled/required policy). window=20000 ms+500 ms drain; pairs=20; requests/responses=20/20; samples=20/8; duplicate=0 unmatched=0 expired=0 out-of-order=0 missing=0 sequence-gaps=0 response-loss=0; hardware Sync RX/Delay_Req TX required; timestamp-failures=0 unusable=0 RX-drops=0; path us min/mean/max/sd=27.293/27.367/27.453/0.048; raw local-minus-master arbitrary us min/mean/max=-1793823469972260.500/-1793823469971940.800/-1793823469971620.800. PHC not disciplined; no UTC conversion or accuracy claim. completed. 
NTP controls before load (unsigned): IPv4=Pass; IPv6=Pass.
Loaded PTP (Pass):
MAC=80:f1:b2:d2:ca:9a domain=0 mode=7; L2 untagged multicast two-step E2E; sequence-seed=controlled Delay_Req wrap; selection=ranked two-fresh-Announce qualification; authentication=enabled, optional (signed with keys, unsigned without keys); policy v1; identity=0d2f18d75f34779b071d5c8a17e573e1190b00d83ab6d2ba7cdd9aff570dc038; provenance=public-source-contract-v2-not-runtime-attestation. Observed outgoing=verified signed v2.1 (not inferred from configured enabled/required policy). 
NTP-loaded standby: synthetic test candidate source=00f1b2fffed1d95e/1 GM=00f1b2fffed1d95e; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[221276671,221276944] us; TX sequence=65534 (fresh) successful=[223285614,223285990] us; TX sequence=65535 (fresh) successful=[225295606,225295886] us; TX sequence=0 (fresh) successful=[227325603,227325883] us; TX sequence=1 (fresh) successful=[229335609,229336021] us; TX sequence=2 (fresh) successful=[231365606,231365883] us; TX sequence=3 (fresh) successful=[233395602,233395895] us; TX sequence=4 (fresh) successful=[235425603,235425876] us; TX sequence=5 (fresh) successful=[237455602,237455886] us; TX sequence=6 (fresh) successful=[239475605,239475974] us; TX sequence=7 (fresh) successful=[241495602,241495871] us; TX sequence=8 (fresh) successful=[243511264,243511540] us; TX sequence=9 (fresh) successful=[245315618,245315907] us; Bounded recovery: fresh Announce+two-step+matching Delay before 261565907 us; first/last activity=255326352/255326791 us. A refreshed better candidate held PTP in standby for the complete bounded NTP load; PTP recovered after expiry. This does not claim NTP holdover or negative-key enforcement under load. 
Cleanup: injections stopped; waiting through monotonic 255565907 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=256326919/261406193 us. cleanup healthy=1. 
NTP load: unsigned requests; configured observation+drain duration=20500 ms (half-open request schedule; endpoint scheduling grace adds no requests); scheduled load rate=10 requests/s; load worker max outstanding=1; full load window=yes; entire PTP collection covered=yes.
Monotonic windows (us): NTP=[224800592,245320594]; PTP collection=[224805594,245315610]; actual overlap=20510016 us; uncovered collection=0 us.
IPv4 load: attempts=103; successes=103; timeouts=0; invalid replies=0; local/socket errors=0.
IPv6 load: attempts=102; successes=102; timeouts=0; invalid replies=0; local/socket errors=0.
Client pacing misses=0; ENOBUFS/ENOMEM=0; UDP RX queue drop counters unavailable via this socket API (not assumed zero). PTP RX queue evidence is reported by the engine. Client overload is not server loss.
NTP controls after load: IPv4=Pass (error=0); IPv6=Pass (error=0).
Operator phase evidence:
No assisted phase requested.
</pre>

### 23. PTP L2 standby with NTP key-policy probes under load

While a qualified foreign master holds PTP in standby, test invalid NTP authentication under bounded IPv4/IPv6 load, plus unsigned rejection when NTP authentication is required. Requires the separate target NTP key.

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
DUT policy v1: selection=ranked two-fresh-Announce qualification; PTP authentication=enabled, optional (signed with keys, unsigned without keys); capacity=8; domain=0; foreign timeout=10000 ms; local log Announce/Sync=1/0; GNSS/PPS qualification=3500 ms; SPP=1 outgoing key ID=1; NTP authentication=mixed; source identity=0d2f18d75f34779b071d5c8a17e573e1190b00d83ab6d2ba7cdd9aff570dc038; provenance=public-source-contract-v2-not-runtime-attestation. Source configuration only, not runtime attestation.
Target=80:F1:B2:D2:CA:9A; domain=0; operator firmware=NtpPtpL2PtpL3.
Observation=20000 ms; samples=8; response window=500 ms; request interval=250 ms; processing allowance=250 ms; DUT foreign quiet=10000 ms; DUT withdrawal=3500 ms; recovery=60000 ms; operator phase limit=120000 ms; sequence seed=enabled/65532.
Unloaded Sustained baseline (Pass):
MAC=80:f1:b2:d2:ca:9a domain=0 mode=7; L2 untagged multicast two-step E2E; sequence-seed=controlled Delay_Req wrap; selection=ranked two-fresh-Announce qualification; authentication=enabled, optional (signed with keys, unsigned without keys); policy v1; identity=0d2f18d75f34779b071d5c8a17e573e1190b00d83ab6d2ba7cdd9aff570dc038; provenance=public-source-contract-v2-not-runtime-attestation. Observed outgoing=verified signed v2.1 (not inferred from configured enabled/required policy). window=20000 ms+500 ms drain; pairs=20; requests/responses=20/20; samples=20/8; duplicate=0 unmatched=0 expired=0 out-of-order=0 missing=0 sequence-gaps=0 response-loss=0; hardware Sync RX/Delay_Req TX required; timestamp-failures=0 unusable=0 RX-drops=0; path us min/mean/max/sd=12.214/26.600/27.532/3.387; raw local-minus-master arbitrary us min/mean/max=-1793823469974574.500/-1793823469974254.000/-1793823469973935.000. PHC not disciplined; no UTC conversion or accuracy claim. completed. 
NTP controls before load (unsigned): IPv4=Pass; IPv6=Pass.
Loaded PTP (Pass):
MAC=80:f1:b2:d2:ca:9a domain=0 mode=7; L2 untagged multicast two-step E2E; sequence-seed=controlled Delay_Req wrap; selection=ranked two-fresh-Announce qualification; authentication=enabled, optional (signed with keys, unsigned without keys); policy v1; identity=0d2f18d75f34779b071d5c8a17e573e1190b00d83ab6d2ba7cdd9aff570dc038; provenance=public-source-contract-v2-not-runtime-attestation. Observed outgoing=verified signed v2.1 (not inferred from configured enabled/required policy). 
NTP-loaded standby: synthetic test candidate source=00f1b2fffed1d9ad/1 GM=00f1b2fffed1d9ad; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[290136638,290136910] us; TX sequence=65534 (fresh) successful=[292145613,292145972] us; TX sequence=65535 (fresh) successful=[294155606,294155899] us; TX sequence=0 (fresh) successful=[296171307,296171614] us; TX sequence=1 (fresh) successful=[298195609,298196116] us; TX sequence=2 (fresh) successful=[300225606,300225909] us; TX sequence=3 (fresh) successful=[302245606,302245892] us; TX sequence=4 (fresh) successful=[304271253,304271684] us; TX sequence=5 (fresh) successful=[306295606,306295877] us; TX sequence=6 (fresh) successful=[308325602,308325874] us; TX sequence=7 (fresh) successful=[310355606,310355879] us; TX sequence=8 (fresh) successful=[312385607,312385875] us; TX sequence=9 (fresh) successful=[314175633,314175910] us; Bounded recovery: fresh Announce+two-step+matching Delay before 330425910 us; first/last activity=324185720/324186298 us. A refreshed better candidate held PTP in standby for the complete bounded NTP load; PTP recovered after expiry. Concurrent negative NTP key-policy evidence is reported separately by the load worker; no holdover is claimed. 
Cleanup: injections stopped; waiting through monotonic 324425910 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=325186691/330226384 us. cleanup healthy=1. 
NTP load: unsigned requests; configured observation+drain duration=20500 ms (half-open request schedule; endpoint scheduling grace adds no requests); scheduled load rate=10 requests/s; load worker max outstanding=1; full load window=yes; entire PTP collection covered=yes.
Monotonic windows (us): NTP=[293660603,314180596]; PTP collection=[293665594,314175623]; actual overlap=20510029 us; uncovered collection=0 us.
IPv4 load: attempts=103; successes=103; timeouts=0; invalid replies=0; local/socket errors=0.
IPv6 load: attempts=102; successes=102; timeouts=0; invalid replies=0; local/socket errors=0.
Client pacing misses=0; ENOBUFS/ENOMEM=0; UDP RX queue drop counters unavailable via this socket API (not assumed zero). PTP RX queue evidence is reported by the engine. Client overload is not server loss.
Concurrent NTP key-policy probes: interval=[293780601,299781860] us; required policy=0; overall=pass. Additional probes/controls are concurrent with, and not counted in, the scheduled load rate; their complete interval must be inside the PTP standby/load window.
IPv4 signed controls before/after=1/1; invalid-MAC rejection=pass; unsigned required-policy rejection=not applicable (mixed policy).
IPv6 signed controls before/after=1/1; invalid-MAC rejection=pass; unsigned required-policy rejection=not applicable (mixed policy).
NTP controls after load: IPv4=Pass (error=0); IPv6=Pass (error=0).
Operator phase evidence:
No assisted phase requested.
</pre>

### 24. PTP L2 standby with NTP holdover under load (assisted)

With a qualified foreign master holding PTP in standby, independently confirm GNSS loss and legitimate NTP holdover, then verify bounded IPv4/IPv6 NTP load before restoring the DUT.

Result: **Not yet run**

<pre>n/a</pre>

### 25. PTP L3 UDP IPv4 with bounded NTP load

Verify IPv4 PTP multicast exchanges during bounded IPv4 NTP traffic.

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
PTP L3 UDP target IPv4=192.168.1.24; IPv6=(not selected); domain=0; operator firmware=NTP + PTP L2 + PTP L3 UDP; withdrawal bound=5000 ms. PTP L2 and NTP keys are not used by PTP L3 UDP.
Unloaded PTP L3 UDP baseline (Pass):
PTP L3 UDP unsigned E2E; domain=0; IPv4=192.168.1.24; IPv6=disabled; multicast=224.0.1.129 / ff02::181; event/general ports=319/320; hop limit=1. Sockets are bound to the multicast groups and the Ethernet interface. Hardware timestamps, packet-drop counters and clock accuracy are not available through these sockets.
IPv4: Pass; selected multicast packets=4; Announce=1; matched Sync/Follow_Up=0; Delay_Req=1; matched Delay_Resp=1; response timeouts=0; malformed=0; unsupported=0; duplicates=0; unmatched=0; sequence gaps=0; source changed=0.
IPv4 selected master: L3 UDP observer; unauthenticated PTP v2.0; source=80f1b2fffed2ca9a/2; GM=80f1b2fffed2ca9a; priority=128/128; class=248; accuracy=0xfe; variance=65535; steps=0; timeSource=0x20; timescale=PTP/TAI; UTC offset=37 s (valid); leap61=0; leap59=0; timeTraceable=0; frequencyTraceable=0. Quality is advertised, not independently verified.
IPv4: Pass; selected multicast packets=122; Announce=10; matched Sync/Follow_Up=20; Delay_Req=70; matched Delay_Resp=70; response timeouts=0; malformed=0; unsupported=0; duplicates=0; unmatched=0; sequence gaps=0; source changed=0.
IPv4 selected master: L3 UDP observer; unauthenticated PTP v2.0; source=80f1b2fffed2ca9a/2; GM=80f1b2fffed2ca9a; priority=128/128; class=248; accuracy=0xfe; variance=65535; steps=0; timeSource=0x20; timescale=PTP/TAI; UTC offset=37 s (valid); leap61=0; leap59=0; timeTraceable=0; frequencyTraceable=0. Quality is advertised, not independently verified.

IPv4 NTP control before load=Pass; error=0.
Loaded PTP L3 UDP (Pass):
PTP L3 UDP unsigned E2E; domain=0; IPv4=192.168.1.24; IPv6=disabled; multicast=224.0.1.129 / ff02::181; event/general ports=319/320; hop limit=1. Sockets are bound to the multicast groups and the Ethernet interface. Hardware timestamps, packet-drop counters and clock accuracy are not available through these sockets.
IPv4: Pass; selected multicast packets=4; Announce=1; matched Sync/Follow_Up=0; Delay_Req=1; matched Delay_Resp=1; response timeouts=0; malformed=0; unsupported=0; duplicates=0; unmatched=0; sequence gaps=0; source changed=0.
IPv4 selected master: L3 UDP observer; unauthenticated PTP v2.0; source=80f1b2fffed2ca9a/2; GM=80f1b2fffed2ca9a; priority=128/128; class=248; accuracy=0xfe; variance=65535; steps=0; timeSource=0x20; timescale=PTP/TAI; UTC offset=37 s (valid); leap61=0; leap59=0; timeTraceable=0; frequencyTraceable=0. Quality is advertised, not independently verified.
IPv4: Pass; selected multicast packets=121; Announce=10; matched Sync/Follow_Up=20; Delay_Req=69; matched Delay_Resp=69; response timeouts=0; malformed=0; unsupported=0; duplicates=0; unmatched=0; sequence gaps=0; source changed=0.
IPv4 selected master: L3 UDP observer; unauthenticated PTP v2.0; source=80f1b2fffed2ca9a/2; GM=80f1b2fffed2ca9a; priority=128/128; class=248; accuracy=0xfe; variance=65535; steps=0; timeSource=0x20; timescale=PTP/TAI; UTC offset=37 s (valid); leap61=0; leap59=0; timeTraceable=0; frequencyTraceable=0. Quality is advertised, not independently verified.

NTP load: authenticated requests and verified responses; 10 requests/s total; full window=yes; complete PTP collection covered=yes; client pacing misses=0; buffer errors=0.
IPv4 NTP: attempts=205; successes=205; timeouts=0; invalid=0; local/socket errors=0; after control=Pass (error=0).
PTP socket RX drop counters and hardware timestamps are unavailable; no synchronization-accuracy claim is made.
Operator phase evidence:
No assisted phase requested.
</pre>

### 26. PTP L3 UDP IPv6 with bounded NTP load

Verify IPv6 PTP multicast exchanges during bounded IPv6 NTP traffic.

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
PTP L3 UDP target IPv4=(not selected); IPv6=FE80::82F1:B2FF:FED2:CA9A; domain=0; operator firmware=NTP + PTP L2 + PTP L3 UDP; withdrawal bound=5000 ms. PTP L2 and NTP keys are not used by PTP L3 UDP.
Unloaded PTP L3 UDP baseline (Pass):
PTP L3 UDP unsigned E2E; domain=0; IPv4=disabled; IPv6=FE80::82F1:B2FF:FED2:CA9A; multicast=224.0.1.129 / ff02::181; event/general ports=319/320; hop limit=1. Sockets are bound to the multicast groups and the Ethernet interface. Hardware timestamps, packet-drop counters and clock accuracy are not available through these sockets.
IPv6: Pass; selected multicast packets=4; Announce=1; matched Sync/Follow_Up=0; Delay_Req=1; matched Delay_Resp=1; response timeouts=0; malformed=0; unsupported=0; duplicates=0; unmatched=0; sequence gaps=0; source changed=0.
IPv6 selected master: L3 UDP observer; unauthenticated PTP v2.0; source=80f1b2fffed2ca9a/3; GM=80f1b2fffed2ca9a; priority=128/128; class=248; accuracy=0xfe; variance=65535; steps=0; timeSource=0x20; timescale=PTP/TAI; UTC offset=37 s (valid); leap61=0; leap59=0; timeTraceable=0; frequencyTraceable=0. Quality is advertised, not independently verified.
IPv6: Pass; selected multicast packets=121; Announce=10; matched Sync/Follow_Up=20; Delay_Req=69; matched Delay_Resp=69; response timeouts=0; malformed=0; unsupported=0; duplicates=0; unmatched=0; sequence gaps=0; source changed=0.
IPv6 selected master: L3 UDP observer; unauthenticated PTP v2.0; source=80f1b2fffed2ca9a/3; GM=80f1b2fffed2ca9a; priority=128/128; class=248; accuracy=0xfe; variance=65535; steps=0; timeSource=0x20; timescale=PTP/TAI; UTC offset=37 s (valid); leap61=0; leap59=0; timeTraceable=0; frequencyTraceable=0. Quality is advertised, not independently verified.

IPv6 NTP control before load=Pass; error=0.
Loaded PTP L3 UDP (Pass):
PTP L3 UDP unsigned E2E; domain=0; IPv4=disabled; IPv6=FE80::82F1:B2FF:FED2:CA9A; multicast=224.0.1.129 / ff02::181; event/general ports=319/320; hop limit=1. Sockets are bound to the multicast groups and the Ethernet interface. Hardware timestamps, packet-drop counters and clock accuracy are not available through these sockets.
IPv6: Pass; selected multicast packets=4; Announce=1; matched Sync/Follow_Up=0; Delay_Req=1; matched Delay_Resp=1; response timeouts=0; malformed=0; unsupported=0; duplicates=0; unmatched=0; sequence gaps=0; source changed=0.
IPv6 selected master: L3 UDP observer; unauthenticated PTP v2.0; source=80f1b2fffed2ca9a/3; GM=80f1b2fffed2ca9a; priority=128/128; class=248; accuracy=0xfe; variance=65535; steps=0; timeSource=0x20; timescale=PTP/TAI; UTC offset=37 s (valid); leap61=0; leap59=0; timeTraceable=0; frequencyTraceable=0. Quality is advertised, not independently verified.
IPv6: Pass; selected multicast packets=121; Announce=10; matched Sync/Follow_Up=20; Delay_Req=69; matched Delay_Resp=69; response timeouts=0; malformed=0; unsupported=0; duplicates=0; unmatched=0; sequence gaps=0; source changed=0.
IPv6 selected master: L3 UDP observer; unauthenticated PTP v2.0; source=80f1b2fffed2ca9a/3; GM=80f1b2fffed2ca9a; priority=128/128; class=248; accuracy=0xfe; variance=65535; steps=0; timeSource=0x20; timescale=PTP/TAI; UTC offset=37 s (valid); leap61=0; leap59=0; timeTraceable=0; frequencyTraceable=0. Quality is advertised, not independently verified.

NTP load: authenticated requests and verified responses; 10 requests/s total; full window=yes; complete PTP collection covered=yes; client pacing misses=0; buffer errors=0.
IPv6 NTP: attempts=205; successes=205; timeouts=0; invalid=0; local/socket errors=0; after control=Pass (error=0).
PTP socket RX drop counters and hardware timestamps are unavailable; no synchronization-accuracy claim is made.
Operator phase evidence:
No assisted phase requested.
</pre>

### 27. PTP L3 UDP simultaneous IPv4 / IPv6 with bounded NTP load

Verify both PTP multicast transports concurrently during bounded IPv4 and IPv6 NTP traffic.

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
PTP L3 UDP target IPv4=192.168.1.24; IPv6=FE80::82F1:B2FF:FED2:CA9A; domain=0; operator firmware=NTP + PTP L2 + PTP L3 UDP; withdrawal bound=5000 ms. PTP L2 and NTP keys are not used by PTP L3 UDP.
Unloaded PTP L3 UDP baseline (Pass):
PTP L3 UDP unsigned E2E; domain=0; IPv4=192.168.1.24; IPv6=FE80::82F1:B2FF:FED2:CA9A; multicast=224.0.1.129 / ff02::181; event/general ports=319/320; hop limit=1. Sockets are bound to the multicast groups and the Ethernet interface. Hardware timestamps, packet-drop counters and clock accuracy are not available through these sockets.
IPv4: Pass; selected multicast packets=4; Announce=1; matched Sync/Follow_Up=0; Delay_Req=1; matched Delay_Resp=1; response timeouts=0; malformed=0; unsupported=0; duplicates=0; unmatched=0; sequence gaps=0; source changed=0.
IPv4 selected master: L3 UDP observer; unauthenticated PTP v2.0; source=80f1b2fffed2ca9a/2; GM=80f1b2fffed2ca9a; priority=128/128; class=248; accuracy=0xfe; variance=65535; steps=0; timeSource=0x20; timescale=PTP/TAI; UTC offset=37 s (valid); leap61=0; leap59=0; timeTraceable=0; frequencyTraceable=0. Quality is advertised, not independently verified.
IPv6: Pass; selected multicast packets=4; Announce=1; matched Sync/Follow_Up=0; Delay_Req=1; matched Delay_Resp=1; response timeouts=0; malformed=0; unsupported=0; duplicates=0; unmatched=0; sequence gaps=0; source changed=0.
IPv6 selected master: L3 UDP observer; unauthenticated PTP v2.0; source=80f1b2fffed2ca9a/3; GM=80f1b2fffed2ca9a; priority=128/128; class=248; accuracy=0xfe; variance=65535; steps=0; timeSource=0x20; timescale=PTP/TAI; UTC offset=37 s (valid); leap61=0; leap59=0; timeTraceable=0; frequencyTraceable=0. Quality is advertised, not independently verified.
IPv4: Pass; selected multicast packets=122; Announce=10; matched Sync/Follow_Up=20; Delay_Req=70; matched Delay_Resp=70; response timeouts=0; malformed=0; unsupported=0; duplicates=0; unmatched=0; sequence gaps=0; source changed=0.
IPv4 selected master: L3 UDP observer; unauthenticated PTP v2.0; source=80f1b2fffed2ca9a/2; GM=80f1b2fffed2ca9a; priority=128/128; class=248; accuracy=0xfe; variance=65535; steps=0; timeSource=0x20; timescale=PTP/TAI; UTC offset=37 s (valid); leap61=0; leap59=0; timeTraceable=0; frequencyTraceable=0. Quality is advertised, not independently verified.
IPv6: Pass; selected multicast packets=122; Announce=10; matched Sync/Follow_Up=20; Delay_Req=70; matched Delay_Resp=70; response timeouts=0; malformed=0; unsupported=0; duplicates=0; unmatched=0; sequence gaps=0; source changed=0.
IPv6 selected master: L3 UDP observer; unauthenticated PTP v2.0; source=80f1b2fffed2ca9a/3; GM=80f1b2fffed2ca9a; priority=128/128; class=248; accuracy=0xfe; variance=65535; steps=0; timeSource=0x20; timescale=PTP/TAI; UTC offset=37 s (valid); leap61=0; leap59=0; timeTraceable=0; frequencyTraceable=0. Quality is advertised, not independently verified.

IPv4 NTP control before load=Pass; error=0.
IPv6 NTP control before load=Pass; error=0.
Loaded PTP L3 UDP (Pass):
PTP L3 UDP unsigned E2E; domain=0; IPv4=192.168.1.24; IPv6=FE80::82F1:B2FF:FED2:CA9A; multicast=224.0.1.129 / ff02::181; event/general ports=319/320; hop limit=1. Sockets are bound to the multicast groups and the Ethernet interface. Hardware timestamps, packet-drop counters and clock accuracy are not available through these sockets.
IPv4: Pass; selected multicast packets=4; Announce=1; matched Sync/Follow_Up=0; Delay_Req=1; matched Delay_Resp=1; response timeouts=0; malformed=0; unsupported=0; duplicates=0; unmatched=0; sequence gaps=0; source changed=0.
IPv4 selected master: L3 UDP observer; unauthenticated PTP v2.0; source=80f1b2fffed2ca9a/2; GM=80f1b2fffed2ca9a; priority=128/128; class=248; accuracy=0xfe; variance=65535; steps=0; timeSource=0x20; timescale=PTP/TAI; UTC offset=37 s (valid); leap61=0; leap59=0; timeTraceable=0; frequencyTraceable=0. Quality is advertised, not independently verified.
IPv6: Pass; selected multicast packets=4; Announce=1; matched Sync/Follow_Up=0; Delay_Req=1; matched Delay_Resp=1; response timeouts=0; malformed=0; unsupported=0; duplicates=0; unmatched=0; sequence gaps=0; source changed=0.
IPv6 selected master: L3 UDP observer; unauthenticated PTP v2.0; source=80f1b2fffed2ca9a/3; GM=80f1b2fffed2ca9a; priority=128/128; class=248; accuracy=0xfe; variance=65535; steps=0; timeSource=0x20; timescale=PTP/TAI; UTC offset=37 s (valid); leap61=0; leap59=0; timeTraceable=0; frequencyTraceable=0. Quality is advertised, not independently verified.
IPv4: Pass; selected multicast packets=121; Announce=10; matched Sync/Follow_Up=20; Delay_Req=69; matched Delay_Resp=69; response timeouts=0; malformed=0; unsupported=0; duplicates=0; unmatched=0; sequence gaps=0; source changed=0.
IPv4 selected master: L3 UDP observer; unauthenticated PTP v2.0; source=80f1b2fffed2ca9a/2; GM=80f1b2fffed2ca9a; priority=128/128; class=248; accuracy=0xfe; variance=65535; steps=0; timeSource=0x20; timescale=PTP/TAI; UTC offset=37 s (valid); leap61=0; leap59=0; timeTraceable=0; frequencyTraceable=0. Quality is advertised, not independently verified.
IPv6: Pass; selected multicast packets=121; Announce=10; matched Sync/Follow_Up=20; Delay_Req=69; matched Delay_Resp=69; response timeouts=0; malformed=0; unsupported=0; duplicates=0; unmatched=0; sequence gaps=0; source changed=0.
IPv6 selected master: L3 UDP observer; unauthenticated PTP v2.0; source=80f1b2fffed2ca9a/3; GM=80f1b2fffed2ca9a; priority=128/128; class=248; accuracy=0xfe; variance=65535; steps=0; timeSource=0x20; timescale=PTP/TAI; UTC offset=37 s (valid); leap61=0; leap59=0; timeTraceable=0; frequencyTraceable=0. Quality is advertised, not independently verified.

NTP load: authenticated requests and verified responses; 10 requests/s total; full window=yes; complete PTP collection covered=yes; client pacing misses=0; buffer errors=0.
IPv4 NTP: attempts=103; successes=103; timeouts=0; invalid=0; local/socket errors=0; after control=Pass (error=0).
IPv6 NTP: attempts=102; successes=102; timeouts=0; invalid=0; local/socket errors=0; after control=Pass (error=0).
PTP socket RX drop counters and hardware timestamps are unavailable; no synchronization-accuracy claim is made.
Operator phase evidence:
No assisted phase requested.
</pre>

### 28. PTP L2 Announce and master identity

Verify that the DUT advertises valid PTP master and identity information.

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
DUT policy v1: selection=ranked two-fresh-Announce qualification; PTP authentication=enabled, optional (signed with keys, unsigned without keys); capacity=8; domain=0; foreign timeout=10000 ms; local log Announce/Sync=1/0; GNSS/PPS qualification=3500 ms; SPP=1 outgoing key ID=1; NTP authentication=mixed; source identity=0d2f18d75f34779b071d5c8a17e573e1190b00d83ab6d2ba7cdd9aff570dc038; provenance=public-source-contract-v2-not-runtime-attestation. Source configuration only, not runtime attestation.
Target=80:F1:B2:D2:CA:9A; domain=0; operator firmware=NtpPtpL2PtpL3.
Observation=20000 ms; samples=8; response window=500 ms; request interval=250 ms; processing allowance=250 ms; DUT foreign quiet=10000 ms; DUT withdrawal=3500 ms; recovery=60000 ms; operator phase limit=120000 ms; sequence seed=enabled/65532.
L2 observer; HMAC-SHA256-128 verified PTP v2.1; source=80f1b2fffed2ca9a/1; GM=80f1b2fffed2ca9a; priority=128/128; class=248; accuracy=0xfe; variance=65535; steps=0; timeSource=0x20; timescale=PTP/TAI; UTC offset=37 s (valid); leap61=0; leap59=0; timeTraceable=0; frequencyTraceable=0. Quality is advertised, not independently verified.
Operator phase evidence:
No assisted phase requested.
</pre>

### 29. PTP L2 two-step Sync / Follow_Up

Verify that the DUT sends valid two-step PTP synchronization exchanges.

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
DUT policy v1: selection=ranked two-fresh-Announce qualification; PTP authentication=enabled, optional (signed with keys, unsigned without keys); capacity=8; domain=0; foreign timeout=10000 ms; local log Announce/Sync=1/0; GNSS/PPS qualification=3500 ms; SPP=1 outgoing key ID=1; NTP authentication=mixed; source identity=0d2f18d75f34779b071d5c8a17e573e1190b00d83ab6d2ba7cdd9aff570dc038; provenance=public-source-contract-v2-not-runtime-attestation. Source configuration only, not runtime attestation.
Target=80:F1:B2:D2:CA:9A; domain=0; operator firmware=NtpPtpL2PtpL3.
Observation=20000 ms; samples=8; response window=500 ms; request interval=250 ms; processing allowance=250 ms; DUT foreign quiet=10000 ms; DUT withdrawal=3500 ms; recovery=60000 ms; operator phase limit=120000 ms; sequence seed=enabled/65532.
Matched HMAC-SHA256-128 verified L2 two-step Sync/Follow_Up seq=2548, PTP v2.1; origin=1793823803.602653636; correction=0.000000 ns; RX=hardware. Functional pairing only; no precision or synchronization claim.
Operator phase evidence:
No assisted phase requested.
</pre>

### 30. PTP L2 E2E Delay_Req / Delay_Resp

Verify that the DUT completes end-to-end PTP delay measurements.

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
DUT policy v1: selection=ranked two-fresh-Announce qualification; PTP authentication=enabled, optional (signed with keys, unsigned without keys); capacity=8; domain=0; foreign timeout=10000 ms; local log Announce/Sync=1/0; GNSS/PPS qualification=3500 ms; SPP=1 outgoing key ID=1; NTP authentication=mixed; source identity=0d2f18d75f34779b071d5c8a17e573e1190b00d83ab6d2ba7cdd9aff570dc038; provenance=public-source-contract-v2-not-runtime-attestation. Source configuration only, not runtime attestation.
Target=80:F1:B2:D2:CA:9A; domain=0; operator firmware=NtpPtpL2PtpL3.
Observation=20000 ms; samples=8; response window=500 ms; request interval=250 ms; processing allowance=250 ms; DUT foreign quiet=10000 ms; DUT withdrawal=3500 ms; recovery=60000 ms; operator phase limit=120000 ms; sequence seed=enabled/65532.
Matched HMAC-SHA256-128 verified L2 Delay_Req/Delay_Resp seq=85 with exact source/requester identity; master ingress=1793823805.614213284; correction=0.000000 ns; Sync RX=hardware; Delay_Req TX=hardware. Functional E2E exchange only; no clock servo or accuracy claim.
Operator phase evidence:
No assisted phase requested.
</pre>

### 31. PTP L2 time properties and clock quality

Verify that the DUT advertises valid time properties and clock-quality data.

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
DUT policy v1: selection=ranked two-fresh-Announce qualification; PTP authentication=enabled, optional (signed with keys, unsigned without keys); capacity=8; domain=0; foreign timeout=10000 ms; local log Announce/Sync=1/0; GNSS/PPS qualification=3500 ms; SPP=1 outgoing key ID=1; NTP authentication=mixed; source identity=0d2f18d75f34779b071d5c8a17e573e1190b00d83ab6d2ba7cdd9aff570dc038; provenance=public-source-contract-v2-not-runtime-attestation. Source configuration only, not runtime attestation.
Target=80:F1:B2:D2:CA:9A; domain=0; operator firmware=NtpPtpL2PtpL3.
Observation=20000 ms; samples=8; response window=500 ms; request interval=250 ms; processing allowance=250 ms; DUT foreign quiet=10000 ms; DUT withdrawal=3500 ms; recovery=60000 ms; operator phase limit=120000 ms; sequence seed=enabled/65532.
L2 observer; HMAC-SHA256-128 verified PTP v2.1; source=80f1b2fffed2ca9a/1; GM=80f1b2fffed2ca9a; priority=128/128; class=248; accuracy=0xfe; variance=65535; steps=0; timeSource=0x20; timescale=PTP/TAI; UTC offset=37 s (valid); leap61=0; leap59=0; timeTraceable=0; frequencyTraceable=0. Quality is advertised, not independently verified. Two distinct Announce sequences were consistent.
Operator phase evidence:
No assisted phase requested.
</pre>

### 32. PTP L2 offset / path delay diagnostic

Collect PTP offset and path-delay measurements for diagnostic review.

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
DUT policy v1: selection=ranked two-fresh-Announce qualification; PTP authentication=enabled, optional (signed with keys, unsigned without keys); capacity=8; domain=0; foreign timeout=10000 ms; local log Announce/Sync=1/0; GNSS/PPS qualification=3500 ms; SPP=1 outgoing key ID=1; NTP authentication=mixed; source identity=0d2f18d75f34779b071d5c8a17e573e1190b00d83ab6d2ba7cdd9aff570dc038; provenance=public-source-contract-v2-not-runtime-attestation. Source configuration only, not runtime attestation.
Target=80:F1:B2:D2:CA:9A; domain=0; operator firmware=NtpPtpL2PtpL3.
Observation=20000 ms; samples=8; response window=500 ms; request interval=250 ms; processing allowance=250 ms; DUT foreign quiet=10000 ms; DUT withdrawal=3500 ms; recovery=60000 ms; operator phase limit=120000 ms; sequence seed=enabled/65532.
Single hardware-timestamp E2E sample, symmetric-path assumption: mean path delay=27.472 us; local PHC minus master's PTP/TAI timescale=-1793823469976584.500 us; corrections included at 1/65536 ns scale. PHC is not disciplined by this tester; offset is not UTC-converted. Not a nanosecond precision/accuracy claim; authentication=HMAC-SHA256-128 verified.
Operator phase evidence:
No assisted phase requested.
</pre>

### 33. PTP L2 sustained exchange integrity

Verify that PTP message exchanges remain valid over an extended observation period.

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
DUT policy v1: selection=ranked two-fresh-Announce qualification; PTP authentication=enabled, optional (signed with keys, unsigned without keys); capacity=8; domain=0; foreign timeout=10000 ms; local log Announce/Sync=1/0; GNSS/PPS qualification=3500 ms; SPP=1 outgoing key ID=1; NTP authentication=mixed; source identity=0d2f18d75f34779b071d5c8a17e573e1190b00d83ab6d2ba7cdd9aff570dc038; provenance=public-source-contract-v2-not-runtime-attestation. Source configuration only, not runtime attestation.
Target=80:F1:B2:D2:CA:9A; domain=0; operator firmware=NtpPtpL2PtpL3.
Observation=20000 ms; samples=8; response window=500 ms; request interval=250 ms; processing allowance=250 ms; DUT foreign quiet=10000 ms; DUT withdrawal=3500 ms; recovery=60000 ms; operator phase limit=120000 ms; sequence seed=enabled/65532.
MAC=80:f1:b2:d2:ca:9a domain=0 mode=7; L2 untagged multicast two-step E2E; sequence-seed=controlled Delay_Req wrap; selection=ranked two-fresh-Announce qualification; authentication=enabled, optional (signed with keys, unsigned without keys); policy v1; identity=0d2f18d75f34779b071d5c8a17e573e1190b00d83ab6d2ba7cdd9aff570dc038; provenance=public-source-contract-v2-not-runtime-attestation. Observed outgoing=verified signed v2.1 (not inferred from configured enabled/required policy). window=20000 ms+500 ms drain; pairs=20; requests/responses=20/20; samples=20/8; duplicate=0 unmatched=0 expired=0 out-of-order=0 missing=0 sequence-gaps=0 response-loss=0; hardware Sync RX/Delay_Req TX required; timestamp-failures=0 unusable=0 RX-drops=0; path us min/mean/max/sd=27.232/27.361/27.452/0.056; raw local-minus-master arbitrary us min/mean/max=-1793823469977291.800/-1793823469976971.800/-1793823469976651.800. PHC not disciplined; no UTC conversion or accuracy claim. completed. 
Operator phase evidence:
No assisted phase requested.
</pre>

### 34. PTP L2 negative Delay_Req acceptance

Verify that the DUT handles invalid PTP Delay_Req fields safely. Note: signed-DUT coverage includes authenticated PTP-field mutations, but excludes version, TLV and Ethernet-framing cases. (Requires negative-test opt-in.)

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
DUT policy v1: selection=ranked two-fresh-Announce qualification; PTP authentication=enabled, optional (signed with keys, unsigned without keys); capacity=8; domain=0; foreign timeout=10000 ms; local log Announce/Sync=1/0; GNSS/PPS qualification=3500 ms; SPP=1 outgoing key ID=1; NTP authentication=mixed; source identity=0d2f18d75f34779b071d5c8a17e573e1190b00d83ab6d2ba7cdd9aff570dc038; provenance=public-source-contract-v2-not-runtime-attestation. Source configuration only, not runtime attestation.
Target=80:F1:B2:D2:CA:9A; domain=0; operator firmware=NtpPtpL2PtpL3.
Observation=20000 ms; samples=8; response window=500 ms; request interval=250 ms; processing allowance=250 ms; DUT foreign quiet=10000 ms; DUT withdrawal=3500 ms; recovery=60000 ms; operator phase limit=120000 ms; sequence seed=enabled/65532.
MAC=80:f1:b2:d2:ca:9a domain=0 mode=7; L2 untagged multicast two-step E2E; sequence-seed=controlled Delay_Req wrap; selection=ranked two-fresh-Announce qualification; authentication=enabled, optional (signed with keys, unsigned without keys); policy v1; identity=0d2f18d75f34779b071d5c8a17e573e1190b00d83ab6d2ba7cdd9aff570dc038; provenance=public-source-contract-v2-not-runtime-attestation. Observed outgoing=verified signed v2.1 (not inferred from configured enabled/required policy). wrong-domain:pass(minor=1 expected-response=0 before=1 response=0 response-ICV-verified=0 after=1); version:not-covered-by-authenticated-variant; minor-version:not-covered-by-authenticated-variant; unsupported-minor-version:not-covered-by-authenticated-variant; transportSpecific:pass(minor=1 expected-response=0 before=1 response=0 response-ICV-verified=0 after=1); reserved-domain:pass(minor=1 expected-response=0 before=1 response=0 response-ICV-verified=0 after=1); reserved-header:pass(minor=1 expected-response=0 before=1 response=0 response-ICV-verified=0 after=1); flags:pass(minor=1 expected-response=0 before=1 response=0 response-ICV-verified=0 after=1); control:pass(minor=1 expected-response=1 before=1 response=1 response-ICV-verified=1 after=1); short-PTP-header:not-covered-by-authenticated-variant; short-PTP-body:not-covered-by-authenticated-variant; messageLength:not-covered-by-authenticated-variant; oversized:not-covered-by-authenticated-variant; extra-TLV:not-covered-by-authenticated-variant; authentication-TLV-unsupported:not-covered-by-authenticated-variant; truncated-TLV:not-covered-by-authenticated-variant; odd-TLV:not-covered-by-authenticated-variant; overrunning-TLV:not-covered-by-authenticated-variant; zero-clock:pass(minor=1 expected-response=0 before=1 response=0 response-ICV-verified=0 after=1); ones-clock:pass(minor=1 expected-response=0 before=1 response=0 response-ICV-verified=0 after=1); zero-port:pass(minor=1 expected-response=0 before=1 response=0 response-ICV-verified=0 after=1); reserved-port:pass(minor=1 expected-response=0 before=1 response=0 response-ICV-verified=0 after=1); target-clock:pass(minor=1 expected-response=0 before=1 response=0 response-ICV-verified=0 after=1); nanoseconds:pass(minor=1 expected-response=0 before=1 response=0 response-ICV-verified=0 after=1); positive-correction:pass(minor=1 expected-response=0 before=1 response=0 response-ICV-verified=0 after=1); negative-correction:pass(minor=1 expected-response=0 before=1 response=0 response-ICV-verified=0 after=1); destination-filter:pass(minor=1 expected-response=0 before=1 response=0 response-ICV-verified=0 after=1); VLAN-filter:not-covered-by-authenticated-variant; legal-padding:not-covered-by-authenticated-variant; distinct-valid-port:pass(minor=1 expected-response=1 before=1 response=1 response-ICV-verified=1 after=1); Authenticated PTP-field probes were signed and self-verified before transmission; version, TLV/declared-length, VLAN and Ethernet-padding cases are not covered because they cannot retain a valid Immediate Authentication message. RX-drops=0. 
Operator phase evidence:
No assisted phase requested.
</pre>

### 35. PTP L2 foreign-master policy and recovery

Verify that the DUT applies its foreign-master policy and recovers when the candidate is withdrawn. (Requires foreign-master test opt-in.)

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
DUT policy v1: selection=ranked two-fresh-Announce qualification; PTP authentication=enabled, optional (signed with keys, unsigned without keys); capacity=8; domain=0; foreign timeout=10000 ms; local log Announce/Sync=1/0; GNSS/PPS qualification=3500 ms; SPP=1 outgoing key ID=1; NTP authentication=mixed; source identity=0d2f18d75f34779b071d5c8a17e573e1190b00d83ab6d2ba7cdd9aff570dc038; provenance=public-source-contract-v2-not-runtime-attestation. Source configuration only, not runtime attestation.
Target=80:F1:B2:D2:CA:9A; domain=0; operator firmware=NtpPtpL2PtpL3.
Observation=20000 ms; samples=8; response window=500 ms; request interval=250 ms; processing allowance=250 ms; DUT foreign quiet=10000 ms; DUT withdrawal=3500 ms; recovery=60000 ms; operator phase limit=120000 ms; sequence seed=enabled/65532.
MAC=80:f1:b2:d2:ca:9a domain=0 mode=7; L2 untagged multicast two-step E2E; sequence-seed=controlled Delay_Req wrap; selection=ranked two-fresh-Announce qualification; authentication=enabled, optional (signed with keys, unsigned without keys); policy v1; identity=0d2f18d75f34779b071d5c8a17e573e1190b00d83ab6d2ba7cdd9aff570dc038; provenance=public-source-contract-v2-not-runtime-attestation. Observed outgoing=verified signed v2.1 (not inferred from configured enabled/required policy). 
Baseline: L2 observer; HMAC-SHA256-128 verified PTP v2.1; source=80f1b2fffed2ca9a/1; GM=80f1b2fffed2ca9a; priority=128/128; class=248; accuracy=0xfe; variance=65535; steps=0; timeSource=0x20; timescale=PTP/TAI; UTC offset=37 s (valid); leap61=0; leap59=0; timeTraceable=0; frequencyTraceable=0. Quality is advertised, not independently verified. 
wrong-domain: synthetic test candidate source=00f1b2fffed1d856/1 GM=00f1b2fffed1d856; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[381086921,381087193] us; TX sequence=65534 (fresh) successful=[383115606,383115882] us; TX sequence=65535 (fresh) successful=[385125606,385125984] us; activity=1 post-control=1; completed. 
malformed: synthetic test candidate source=00f1b2fffed1d843/1 GM=00f1b2fffed1d843; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[385846561,385846827] us; TX sequence=65534 (fresh) successful=[387855606,387855877] us; TX sequence=65535 (fresh) successful=[389875602,389875887] us; activity=1 post-control=1; completed. 
foreign policy: synthetic test candidate source=00f1b2fffed1d87c/1 GM=00f1b2fffed1d87c; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[390606591,390606860] us; TX sequence=65534 (fresh) successful=[392615606,392615897] us; TX sequence=65535 (fresh) successful=[394635602,394635875] us; TX sequence=0 (fresh) successful=[396655650,396655919] us; TX sequence=1 (fresh) successful=[398685599,398685946] us; TX sequence=2 (fresh) successful=[400695602,400695875] us; TX sequence=3 (fresh) successful=[402725599,402725868] us; TX sequence=4 (fresh) successful=[404755599,404755932] us; TX sequence=5 (fresh) successful=[406785599,406785868] us; TX sequence=6 (fresh) successful=[408795599,408795871] us; TX sequence=7 (fresh) successful=[410805599,410805908] us; TX sequence=8 (fresh) successful=[412835599,412835868] us; TX sequence=9 (fresh) successful=[414865602,414865872] us; 
Standby phase=4 start/deadline=392615897/414865897 us silence-after=392865897 us sends=13 last-TX/activity=414865872/392565556 us held=1; completed. TX sequence=10 (fresh) successful=[414866028,414866298] us; Expiry reference=[414866028,414866298] us no-early-before=424866028 us quiet=1. Recovered fresh Announce+two-step+matching Delay=1; first/last activity=425875327/430976241 us. 
inferior/non-winning identity tie-break: synthetic test candidate source=00f1b2fffed1d8aa/1 GM=00f1b2fffed1d8aa; priority=129/128 class=248 accuracy=0xfe variance=65535; rank=1 deciding=priority1; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[431635938,431636335] us; TX sequence=65534 (fresh) successful=[433645602,433645987] us; 
inferior/non-winning identity tie-break: synthetic test candidate source=00f1b2fffed1d8d8/1 GM=80f1b2fffed2ca9b; priority=128/128 class=248 accuracy=0xfe variance=65535; rank=1 deciding=grandmaster identity; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[438406566,438406831] us; TX sequence=65534 (fresh) successful=[440415602,440415869] us; 
winning identity tie-break then no-longer-better: synthetic test candidate source=00f1b2fffed1d8d6/1 GM=80f1b2fffed2ca99; priority=128/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=grandmaster identity; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[445176662,445176932] us; TX sequence=65534 (fresh) successful=[447185606,447185901] us; 
Fresh same-source no-longer-better update: deciding=priority1; must recover before original expiry=457185606 us. TX sequence=65535 (fresh) successful=[448445686,448445958] us; Fresh inferior update released standby before original expiry; RX-drops=0. Election does not discipline either clock or establish full BMCA/synchronization accuracy. 
Cleanup: injections stopped; waiting through monotonic 458695958 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=458976260/463526311 us. cleanup healthy=1. 
Operator phase evidence:
No assisted phase requested.
</pre>

### 36. PTP L2-only IP-independent operation

Verify that PTP L2 continues to operate without IP services. (Requires PTP L2 only firmware.)

Result: **Not yet run**

<pre>n/a</pre>

### 37. PTP L2 ranking precedence

Verify that the DUT selects the preferred PTP master according to its ranking policy. (Requires foreign-master test opt-in.)

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
DUT policy v1: selection=ranked two-fresh-Announce qualification; PTP authentication=enabled, optional (signed with keys, unsigned without keys); capacity=8; domain=0; foreign timeout=10000 ms; local log Announce/Sync=1/0; GNSS/PPS qualification=3500 ms; SPP=1 outgoing key ID=1; NTP authentication=mixed; source identity=0d2f18d75f34779b071d5c8a17e573e1190b00d83ab6d2ba7cdd9aff570dc038; provenance=public-source-contract-v2-not-runtime-attestation. Source configuration only, not runtime attestation.
Target=80:F1:B2:D2:CA:9A; domain=0; operator firmware=NtpPtpL2PtpL3.
Observation=20000 ms; samples=8; response window=500 ms; request interval=250 ms; processing allowance=250 ms; DUT foreign quiet=10000 ms; DUT withdrawal=3500 ms; recovery=60000 ms; operator phase limit=120000 ms; sequence seed=enabled/65532.
MAC=80:f1:b2:d2:ca:9a domain=0 mode=7; L2 untagged multicast two-step E2E; sequence-seed=controlled Delay_Req wrap; selection=ranked two-fresh-Announce qualification; authentication=enabled, optional (signed with keys, unsigned without keys); policy v1; identity=0d2f18d75f34779b071d5c8a17e573e1190b00d83ab6d2ba7cdd9aff570dc038; provenance=public-source-contract-v2-not-runtime-attestation. Observed outgoing=verified signed v2.1 (not inferred from configured enabled/required policy). 
Ranking baseline: L2 observer; HMAC-SHA256-128 verified PTP v2.1; source=80f1b2fffed2ca9a/1; GM=80f1b2fffed2ca9a; priority=128/128; class=248; accuracy=0xfe; variance=65535; steps=0; timeSource=0x20; timescale=PTP/TAI; UTC offset=37 s (valid); leap61=0; leap59=0; timeTraceable=0; frequencyTraceable=0. Quality is advertised, not independently verified.
Order: priority1, class, accuracy, variance, priority2, grandmaster identity. Earlier fields tie; later fields oppose the deciding field wherever possible. Both directions are attempted; impossible directions are N/A, not tested passes. 
Ranking priority1 winning: synthetic test candidate source=00f1b2fffed1d8ec/1 GM=fffffffffffffffe; priority=127/255 class=254 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[469746557,469746827] us; TX sequence=65534 (fresh) successful=[471755602,471755874] us; Expected=standby until candidate expiry; observed=1; silence-after=472005874 us; no-early-before=481755602 us; last-DUT-activity=471576288 us; completed. 
Cleanup: injections stopped; waiting through monotonic 482005874 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=482776143/487836196 us. cleanup healthy=1. Ranking priority1 winning: PASS with fresh healthy after-control. 
Ranking priority1 losing: synthetic test candidate source=00f1b2fffed1db32/1 GM=0000000000000001; priority=129/0 class=1 accuracy=0x20 variance=0; rank=1 deciding=priority1; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[498056533,498056803] us; TX sequence=65534 (fresh) successful=[500065606,500065877] us; TX sequence=65535 (fresh) successful=[502095602,502095967] us; TX sequence=0 (fresh) successful=[504115606,504115885] us; Expected=continued healthy master service; observed=1; silence-after=500315877 us; no-early-before=514115606 us; last-DUT-activity=504176203 us; completed. 
Cleanup: injections stopped; waiting through monotonic 514365885 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=514964023/519996122 us. cleanup healthy=1. Ranking priority1 losing: PASS with fresh healthy after-control. 
Ranking class winning: synthetic test candidate source=00f1b2fffed1db4f/1 GM=fffffffffffffffe; priority=128/255 class=247 accuracy=0xfe variance=65535; rank=-1 deciding=class; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[530146530,530146799] us; TX sequence=65534 (fresh) successful=[532155606,532155892] us; Expected=standby until candidate expiry; observed=1; silence-after=532405892 us; no-early-before=542155606 us; last-DUT-activity=532096117 us; completed. 
Cleanup: injections stopped; waiting through monotonic 542405892 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=543175935/548266190 us. cleanup healthy=1. Ranking class winning: PASS with fresh healthy after-control. 
Ranking class losing: synthetic test candidate source=00f1b2fffed1db95/1 GM=0000000000000001; priority=128/0 class=249 accuracy=0x20 variance=0; rank=1 deciding=class; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[558456528,558456798] us; TX sequence=65534 (fresh) successful=[560465606,560465873] us; TX sequence=65535 (fresh) successful=[562485602,562485885] us; TX sequence=0 (fresh) successful=[564505606,564505877] us; Expected=continued healthy master service; observed=1; silence-after=560715873 us; no-early-before=574505606 us; last-DUT-activity=564596192 us; completed. 
Cleanup: injections stopped; waiting through monotonic 574755877 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=575345000/580395921 us. cleanup healthy=1. Ranking class losing: PASS with fresh healthy after-control. 
Ranking accuracy winning: synthetic test candidate source=00f1b2fffed1dbae/1 GM=fffffffffffffffe; priority=128/255 class=248 accuracy=0x31 variance=65535; rank=-1 deciding=accuracy; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[590526600,590526871] us; TX sequence=65534 (fresh) successful=[592535610,592535903] us; Expected=standby until candidate expiry; observed=1; silence-after=592785903 us; no-early-before=602535610 us; last-DUT-activity=592475914 us; completed. 
Cleanup: injections stopped; waiting through monotonic 602785903 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=603555724/608646213 us. cleanup healthy=1. Ranking accuracy winning: PASS with fresh healthy after-control. 
accuracy losing: N/A at advertised dataset's supported maximum boundary; this direction requires a different DUT dataset. 
Ranking variance winning: synthetic test candidate source=00f1b2fffed1dbf4/1 GM=fffffffffffffffe; priority=128/255 class=248 accuracy=0xfe variance=65534; rank=-1 deciding=variance; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[618836560,618836830] us; TX sequence=65534 (fresh) successful=[620845606,620845892] us; Expected=standby until candidate expiry; observed=1; silence-after=621095892 us; no-early-before=630845606 us; last-DUT-activity=620675773 us; completed. 
Cleanup: injections stopped; waiting through monotonic 631095892 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=631855691/636946202 us. cleanup healthy=1. Ranking variance winning: PASS with fresh healthy after-control. 
variance losing: N/A at advertised dataset's supported maximum boundary; this direction requires a different DUT dataset. 
Ranking priority2 winning: synthetic test candidate source=00f1b2fffed1da3a/1 GM=fffffffffffffffe; priority=128/127 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority2; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[647136572,647136843] us; TX sequence=65534 (fresh) successful=[649145610,649145940] us; Expected=standby until candidate expiry; observed=1; silence-after=649395940 us; no-early-before=659145610 us; last-DUT-activity=648965692 us; completed. 
Cleanup: injections stopped; waiting through monotonic 659395940 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=660155541/665246190 us. cleanup healthy=1. Ranking priority2 winning: PASS with fresh healthy after-control. 
Ranking priority2 losing: synthetic test candidate source=00f1b2fffed1da40/1 GM=0000000000000001; priority=128/129 class=248 accuracy=0xfe variance=65535; rank=1 deciding=priority2; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[675436571,675436841] us; TX sequence=65534 (fresh) successful=[677445606,677445897] us; TX sequence=65535 (fresh) successful=[679465602,679465884] us; TX sequence=0 (fresh) successful=[681485602,681485870] us; Expected=continued healthy master service; observed=1; silence-after=677695897 us; no-early-before=691485602 us; last-DUT-activity=681576197 us; completed. 
Cleanup: injections stopped; waiting through monotonic 691735870 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=692291472/697326220 us. cleanup healthy=1. Ranking priority2 losing: PASS with fresh healthy after-control. 
Ranking grandmaster identity winning: synthetic test candidate source=00f1b2fffed1da9d/1 GM=80f1b2fffed2ca99; priority=128/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=grandmaster identity; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[707536652,707536921] us; TX sequence=65534 (fresh) successful=[709545606,709545893] us; Expected=standby until candidate expiry; observed=1; silence-after=709795893 us; no-early-before=719545606 us; last-DUT-activity=709416468 us; completed. 
Cleanup: injections stopped; waiting through monotonic 719795893 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=720565322/725646237 us. cleanup healthy=1. Ranking grandmaster identity winning: PASS with fresh healthy after-control. 
Ranking grandmaster identity losing: synthetic test candidate source=00f1b2fffed1daa3/1 GM=80f1b2fffed2ca9b; priority=128/128 class=248 accuracy=0xfe variance=65535; rank=1 deciding=grandmaster identity; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[735846596,735846866] us; TX sequence=65534 (fresh) successful=[737855606,737855888] us; TX sequence=65535 (fresh) successful=[739875602,739875869] us; TX sequence=0 (fresh) successful=[741895602,741895869] us; Expected=continued healthy master service; observed=1; silence-after=738105888 us; no-early-before=751895602 us; last-DUT-activity=741926198 us; completed. 
Cleanup: injections stopped; waiting through monotonic 752145869 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=752698332/757736305 us. cleanup healthy=1. Ranking grandmaster identity losing: PASS with fresh healthy after-control. 
Ranking coverage: 10/10 feasible directions passed; 2/12 directions N/A at supported dataset boundaries. PASS covers all feasible directions only, not a full bidirectional matrix when N/A is nonzero. No full BMCA or synchronization-accuracy claim. 
Operator phase evidence:
No assisted phase requested.
</pre>

### 38. PTP L2 qualification and source isolation

Verify that the DUT qualifies trusted PTP sources without accepting unrelated candidates. (Requires foreign-master test opt-in.)

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
DUT policy v1: selection=ranked two-fresh-Announce qualification; PTP authentication=enabled, optional (signed with keys, unsigned without keys); capacity=8; domain=0; foreign timeout=10000 ms; local log Announce/Sync=1/0; GNSS/PPS qualification=3500 ms; SPP=1 outgoing key ID=1; NTP authentication=mixed; source identity=0d2f18d75f34779b071d5c8a17e573e1190b00d83ab6d2ba7cdd9aff570dc038; provenance=public-source-contract-v2-not-runtime-attestation. Source configuration only, not runtime attestation.
Target=80:F1:B2:D2:CA:9A; domain=0; operator firmware=NtpPtpL2PtpL3.
Observation=20000 ms; samples=8; response window=500 ms; request interval=250 ms; processing allowance=250 ms; DUT foreign quiet=10000 ms; DUT withdrawal=3500 ms; recovery=60000 ms; operator phase limit=120000 ms; sequence seed=enabled/65532.
MAC=80:f1:b2:d2:ca:9a domain=0 mode=7; L2 untagged multicast two-step E2E; sequence-seed=controlled Delay_Req wrap; selection=ranked two-fresh-Announce qualification; authentication=enabled, optional (signed with keys, unsigned without keys); policy v1; identity=0d2f18d75f34779b071d5c8a17e573e1190b00d83ab6d2ba7cdd9aff570dc038; provenance=public-source-contract-v2-not-runtime-attestation. Observed outgoing=verified signed v2.1 (not inferred from configured enabled/required policy). 
source A: synthetic test candidate source=00f1b2fffed1dafc/1 GM=00f1b2fffed1dafc; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. 
source B: synthetic test candidate source=00f1b2fffed1daff/1 GM=00f1b2fffed1daff; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. 
Isolation phase: one fresh better Announce from each source-port identity must preserve healthy service; only source A's second fresh Announce may qualify source A. TX sequence=65533 (fresh) successful=[768396682,768396957] us; TX sequence=65533 (fresh) successful=[768397024,768397290] us; TX sequence=65534 (fresh) successful=[769155602,769155981] us; Source A second fresh Announce: source-A silence-after=769405981 us; source B remains unqualified with one fresh Announce. Recovered fresh Announce+two-step+matching Delay=1; first/last activity=779158133/783236199 us. Source-port qualification remained isolated; source B's first Announce neither pooled with source A nor survived as a qualified candidate. 
Cleanup: injections stopped; waiting through monotonic 779405981 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=784176115/789276193 us. cleanup healthy=1. 
Operator phase evidence:
No assisted phase requested.
</pre>

### 39. PTP L2 duplicate, reorder and rollover policy

Verify foreign Announce duplicate, backward, half-range and sequence-rollover qualification and lifetime behavior under the DUT's selection policy. (Requires foreign-master test opt-in.)

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
DUT policy v1: selection=ranked two-fresh-Announce qualification; PTP authentication=enabled, optional (signed with keys, unsigned without keys); capacity=8; domain=0; foreign timeout=10000 ms; local log Announce/Sync=1/0; GNSS/PPS qualification=3500 ms; SPP=1 outgoing key ID=1; NTP authentication=mixed; source identity=0d2f18d75f34779b071d5c8a17e573e1190b00d83ab6d2ba7cdd9aff570dc038; provenance=public-source-contract-v2-not-runtime-attestation. Source configuration only, not runtime attestation.
Target=80:F1:B2:D2:CA:9A; domain=0; operator firmware=NtpPtpL2PtpL3.
Observation=20000 ms; samples=8; response window=500 ms; request interval=250 ms; processing allowance=250 ms; DUT foreign quiet=10000 ms; DUT withdrawal=3500 ms; recovery=60000 ms; operator phase limit=120000 ms; sequence seed=enabled/65532.
MAC=80:f1:b2:d2:ca:9a domain=0 mode=7; L2 untagged multicast two-step E2E; sequence-seed=controlled Delay_Req wrap; selection=ranked two-fresh-Announce qualification; authentication=enabled, optional (signed with keys, unsigned without keys); policy v1; identity=0d2f18d75f34779b071d5c8a17e573e1190b00d83ab6d2ba7cdd9aff570dc038; provenance=public-source-contract-v2-not-runtime-attestation. Observed outgoing=verified signed v2.1 (not inferred from configured enabled/required policy). 
Sequence policy: exact synthetic Announce sequences (independent of Delay_Req seed); ranked rejects duplicate/backward/0x8000 advances without qualification or refresh; 65535-&gt;0-&gt;1 is fresh. Legacy accepts every supported Announce and refreshes its fixed timeout. 
duplicate: synthetic test candidate source=00f1b2fffed1dd10/1 GM=00f1b2fffed1dd10; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=100 (fresh) successful=[795956577,795956848] us; TX sequence=100 (duplicate/backward/half-range) successful=[797216403,797216672] us; TX sequence=101 (fresh) successful=[798476357,798476622] us; TX sequence=101 (duplicate/backward/half-range) successful=[805145610,805145885] us; Late update=101 expected=no refresh; original expiry=808476357 us; expected expiry=808476357 us. No-refresh recovery Announce+matching Delay=1 before 814895597 us; completed. 
Cleanup: injections stopped; waiting through monotonic 815395885 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=815486995/820576194 us. cleanup healthy=1. duplicate: PASS with healthy Announce/two-step/Delay after-control. 
backward across zero: synthetic test candidate source=00f1b2fffed1dd23/1 GM=00f1b2fffed1dd23; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=0 (fresh) successful=[826016971,826017245] us; TX sequence=65535 (duplicate/backward/half-range) successful=[827276354,827276617] us; TX sequence=1 (fresh) successful=[828536395,828536660] us; TX sequence=0 (duplicate/backward/half-range) successful=[835205603,835205881] us; Late update=0 expected=no refresh; original expiry=838536395 us; expected expiry=838536395 us. No-refresh recovery Announce+matching Delay=1 before 844955594 us; completed. 
Cleanup: injections stopped; waiting through monotonic 845455881 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=845550902/850646202 us. cleanup healthy=1. backward across zero: PASS with healthy Announce/two-step/Delay after-control. 
half-range: synthetic test candidate source=00f1b2fffed1dd72/1 GM=00f1b2fffed1dd72; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=100 (fresh) successful=[856076677,856076948] us; TX sequence=32868 (duplicate/backward/half-range) successful=[857336412,857336676] us; TX sequence=101 (fresh) successful=[858596352,858596615] us; TX sequence=32869 (duplicate/backward/half-range) successful=[865265603,865265874] us; Late update=32869 expected=no refresh; original expiry=868596352 us; expected expiry=868596352 us. No-refresh recovery Announce+matching Delay=1 before 875015594 us; completed. 
Cleanup: injections stopped; waiting through monotonic 875515874 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=875634871/880726200 us. cleanup healthy=1. half-range: PASS with healthy Announce/two-step/Delay after-control. 
forward rollover: synthetic test candidate source=00f1b2fffed1dd85/1 GM=00f1b2fffed1dd85; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=65535 (fresh) successful=[886156704,886156974] us; TX sequence=0 (fresh) successful=[887416385,887416649] us; TX sequence=1 (fresh) successful=[894085603,894085871] us; Late update=1 expected=refresh; original expiry=897416385 us; expected expiry=904085603 us. Recovered fresh Announce+two-step+matching Delay=1; first/last activity=904095705/908186190 us. 
Cleanup: injections stopped; waiting through monotonic 904335871 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=909125670/914206195 us. cleanup healthy=1. forward rollover: PASS with healthy Announce/two-step/Delay after-control. 
All four sequence scenarios passed for ranked two-fresh-Announce qualification. No DUT-generated Sync rollover, persistent anti-replay or synchronization-accuracy claim. 
Operator phase evidence:
No assisted phase requested.
</pre>

### 40. PTP L2 dataset changes and self-grandmaster reflection

Verify that a changed grandmaster must requalify and self-grandmaster reflection cannot establish or retain standby. (Requires ranked selection and foreign-master test opt-in.)

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
DUT policy v1: selection=ranked two-fresh-Announce qualification; PTP authentication=enabled, optional (signed with keys, unsigned without keys); capacity=8; domain=0; foreign timeout=10000 ms; local log Announce/Sync=1/0; GNSS/PPS qualification=3500 ms; SPP=1 outgoing key ID=1; NTP authentication=mixed; source identity=0d2f18d75f34779b071d5c8a17e573e1190b00d83ab6d2ba7cdd9aff570dc038; provenance=public-source-contract-v2-not-runtime-attestation. Source configuration only, not runtime attestation.
Target=80:F1:B2:D2:CA:9A; domain=0; operator firmware=NtpPtpL2PtpL3.
Observation=20000 ms; samples=8; response window=500 ms; request interval=250 ms; processing allowance=250 ms; DUT foreign quiet=10000 ms; DUT withdrawal=3500 ms; recovery=60000 ms; operator phase limit=120000 ms; sequence seed=enabled/65532.
MAC=80:f1:b2:d2:ca:9a domain=0 mode=7; L2 untagged multicast two-step E2E; sequence-seed=controlled Delay_Req wrap; selection=ranked two-fresh-Announce qualification; authentication=enabled, optional (signed with keys, unsigned without keys); policy v1; identity=0d2f18d75f34779b071d5c8a17e573e1190b00d83ab6d2ba7cdd9aff570dc038; provenance=public-source-contract-v2-not-runtime-attestation. Observed outgoing=verified signed v2.1 (not inferred from configured enabled/required policy). 
grandmaster A -&gt; B: synthetic test candidate source=00f1b2fffed1ddc6/1 GM=00f1b2fffed1ddc6; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=3 actual refresh=8000000 us lifetime=24000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[920886670,920886940] us; TX sequence=65534 (fresh) successful=[922137783,922138166] us; Same source dataset GM=00f1b2fffed1ddc5; sequence continues. TX sequence=65535 (fresh) successful=[923645677,923646063] us; First changed-dataset Announce must release standby before 929645668 us, strictly before original expiry=946137783 us. Bounded recovery: fresh Announce+two-step+matching Delay before 929645668 us; first/last activity=923646437/924146656 us. TX sequence=0 (fresh) successful=[925405611,925405995] us; Bounded recovery: fresh Announce+two-step+matching Delay before 955655995 us; first/last activity=949415681/950425645 us. Changed grandmaster qualified only after its own second fresh Announce and held standby for its own lifetime. 
Cleanup: injections stopped; waiting through monotonic 949655995 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=951425521/955506206 us. cleanup healthy=1. grandmaster change: PASS with healthy Announce/two-step/Delay after-control. 
self-grandmaster reflection: synthetic test candidate source=00f1b2fffed1dc29/1 GM=00f1b2fffed1dc29; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=3 actual refresh=8000000 us lifetime=24000000 us allowance=250 ms. Same source dataset GM=80f1b2fffed2ca9a; sequence continues. TX sequence=65533 (fresh) successful=[960946639,960946910] us; TX sequence=65534 (fresh) successful=[962205610,962205889] us; Two strictly better self-GM Announces preserved idle healthy service. Same source dataset GM=00f1b2fffed1dc29; sequence continues. TX sequence=65535 (fresh) successful=[963465703,963465986] us; TX sequence=0 (fresh) successful=[964725602,964725884] us; Same source dataset GM=80f1b2fffed2ca9a; sequence continues. TX sequence=1 (fresh) successful=[966235687,966235959] us; First changed-dataset Announce must release standby before 972235678 us, strictly before original expiry=988725602 us. Bounded recovery: fresh Announce+two-step+matching Delay before 972235678 us; first/last activity=966236326/966236867 us. TX sequence=2 (fresh) successful=[967495611,967495893] us; Self-GM update released a qualified source; repeated reflection did not requalify it. 
Cleanup: injections stopped; waiting through monotonic 991745893 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=992302443/996386973 us. cleanup healthy=1. self-grandmaster reflection: PASS with healthy Announce/two-step/Delay after-control. Both independent dataset/reflection scenarios passed; no full BMCA or clock-discipline claim. 
Operator phase evidence:
No assisted phase requested.
</pre>

### 41. PTP L2 multiple better masters

Verify continued standby across overlapping candidate lifetimes in both best/worse expiry orders, then bounded healthy recovery. (Requires ranked selection, at least two candidate slots and foreign-master test opt-in.)

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
DUT policy v1: selection=ranked two-fresh-Announce qualification; PTP authentication=enabled, optional (signed with keys, unsigned without keys); capacity=8; domain=0; foreign timeout=10000 ms; local log Announce/Sync=1/0; GNSS/PPS qualification=3500 ms; SPP=1 outgoing key ID=1; NTP authentication=mixed; source identity=0d2f18d75f34779b071d5c8a17e573e1190b00d83ab6d2ba7cdd9aff570dc038; provenance=public-source-contract-v2-not-runtime-attestation. Source configuration only, not runtime attestation.
Target=80:F1:B2:D2:CA:9A; domain=0; operator firmware=NtpPtpL2PtpL3.
Observation=20000 ms; samples=8; response window=500 ms; request interval=250 ms; processing allowance=250 ms; DUT foreign quiet=10000 ms; DUT withdrawal=3500 ms; recovery=60000 ms; operator phase limit=120000 ms; sequence seed=enabled/65532.
MAC=80:f1:b2:d2:ca:9a domain=0 mode=7; L2 untagged multicast two-step E2E; sequence-seed=controlled Delay_Req wrap; selection=ranked two-fresh-Announce qualification; authentication=enabled, optional (signed with keys, unsigned without keys); policy v1; identity=0d2f18d75f34779b071d5c8a17e573e1190b00d83ab6d2ba7cdd9aff570dc038; provenance=public-source-contract-v2-not-runtime-attestation. Observed outgoing=verified signed v2.1 (not inferred from configured enabled/required policy). 
best / early expiry: synthetic test candidate source=00f1b2fffed1dc43/1 GM=00f1b2fffed1dc42; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=3 actual refresh=8000000 us lifetime=24000000 us allowance=250 ms. 
worse / late expiry: synthetic test candidate source=00f1b2fffed1dc42/1 GM=00f1b2fffed1dc43; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=3 actual refresh=8000000 us lifetime=24000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[1003066815,1003067086] us; TX sequence=65534 (fresh) successful=[1004325602,1004325869] us; TX sequence=65533 (fresh) successful=[1005835606,1005835878] us; TX sequence=65534 (fresh) successful=[1007095602,1007095874] us; TX sequence=65535 (fresh) successful=[1007095928,1007096194] us; TX sequence=65535 (fresh) successful=[1019105617,1019105889] us; Independent expiry bounds: early candidate latest=1031346194 us; post-expiry checkpoint=1032596194 us; remaining candidate earliest=1043105617 us. Bounded recovery: fresh Announce+two-step+matching Delay before 1049355889 us; first/last activity=1043106377/1044120401 us. 
Cleanup: injections stopped; waiting through monotonic 1043355889 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=1045122288/1049166904 us. cleanup healthy=1. Best expired first: PASS; remaining candidate held standby through its independent lifetime, then healthy Announce/two-step/Delay recovered. 
worse / early expiry: synthetic test candidate source=00f1b2fffed1dcdb/1 GM=00f1b2fffed1dcdb; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=3 actual refresh=8000000 us lifetime=24000000 us allowance=250 ms. 
best / late expiry: synthetic test candidate source=00f1b2fffed1dcda/1 GM=00f1b2fffed1dcda; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=3 actual refresh=8000000 us lifetime=24000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[1054646766,1054647037] us; TX sequence=65534 (fresh) successful=[1055905606,1055905971] us; TX sequence=65533 (fresh) successful=[1057415606,1057415971] us; TX sequence=65534 (fresh) successful=[1058675602,1058675956] us; TX sequence=65535 (fresh) successful=[1058676019,1058676285] us; TX sequence=65535 (fresh) successful=[1070685610,1070685959] us; Independent expiry bounds: early candidate latest=1082926285 us; post-expiry checkpoint=1084176285 us; remaining candidate earliest=1094685610 us. Bounded recovery: fresh Announce+two-step+matching Delay before 1100935959 us; first/last activity=1094696092/1094696671 us. 
Cleanup: injections stopped; waiting through monotonic 1094935959 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=1095696028/1100739869 us. cleanup healthy=1. Worse expired first: PASS; remaining candidate held standby through its independent lifetime, then healthy Announce/two-step/Delay recovered. Both overlapping expiry orders passed; on-wire availability only, not DUT table occupancy, selected-winner identity or PTP-derived holdover. 
Operator phase evidence:
No assisted phase requested.
</pre>

### 42. PTP L2 interval-sensitive expiry

Verify negative and long foreign Announce lifetimes and unsupported-interval handling under the DUT's ranked or legacy policy. (Requires foreign-master test opt-in.)

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
DUT policy v1: selection=ranked two-fresh-Announce qualification; PTP authentication=enabled, optional (signed with keys, unsigned without keys); capacity=8; domain=0; foreign timeout=10000 ms; local log Announce/Sync=1/0; GNSS/PPS qualification=3500 ms; SPP=1 outgoing key ID=1; NTP authentication=mixed; source identity=0d2f18d75f34779b071d5c8a17e573e1190b00d83ab6d2ba7cdd9aff570dc038; provenance=public-source-contract-v2-not-runtime-attestation. Source configuration only, not runtime attestation.
Target=80:F1:B2:D2:CA:9A; domain=0; operator firmware=NtpPtpL2PtpL3.
Observation=20000 ms; samples=8; response window=500 ms; request interval=250 ms; processing allowance=250 ms; DUT foreign quiet=10000 ms; DUT withdrawal=3500 ms; recovery=60000 ms; operator phase limit=120000 ms; sequence seed=enabled/65532.
MAC=80:f1:b2:d2:ca:9a domain=0 mode=7; L2 untagged multicast two-step E2E; sequence-seed=controlled Delay_Req wrap; selection=ranked two-fresh-Announce qualification; authentication=enabled, optional (signed with keys, unsigned without keys); policy v1; identity=0d2f18d75f34779b071d5c8a17e573e1190b00d83ab6d2ba7cdd9aff570dc038; provenance=public-source-contract-v2-not-runtime-attestation. Observed outgoing=verified signed v2.1 (not inferred from configured enabled/required policy). 
Interval policy: ranked lifetime=max(foreign timeout, 3*advertised interval); supported logInterval=-7..4, negative fractions rounded up to microseconds. Unsupported ranked intervals must neither qualify nor refresh a source. Legacy accepts all Announce interval bytes and uses the fixed timeout. Recovery budget=6000000 us; no synchronization-accuracy claim. 
minimum negative interval: synthetic test candidate source=00f1b2fffed1df30/1 GM=00f1b2fffed1df30; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=-7 actual refresh=7813 us lifetime=10000000 us allowance=250 ms. Probe logInterval=-7 expected=interval-derived lifetime. TX sequence=100 (fresh) successful=[1107486579,1107486850] us; TX sequence=101 (fresh) successful=[1108745606,1108745959] us; No service before 1118745606 us; fresh recovery required before 1124995959 us. Bounded recovery: fresh Announce+two-step+matching Delay before 1124995959 us; first/last activity=1118749094/1119756063 us. 
Cleanup: injections stopped; waiting through monotonic 1118995959 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=1120755942/1124806685 us. cleanup healthy=1. minimum negative interval: PASS with healthy Announce/two-step/Delay after-control. 
negative half-second interval: synthetic test candidate source=00f1b2fffed1df4e/1 GM=00f1b2fffed1df4e; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=-1 actual refresh=500000 us lifetime=10000000 us allowance=250 ms. Probe logInterval=-1 expected=interval-derived lifetime. TX sequence=100 (fresh) successful=[1130276662,1130276936] us; TX sequence=101 (fresh) successful=[1131535606,1131535873] us; No service before 1141535606 us; fresh recovery required before 1147785873 us. Bounded recovery: fresh Announce+two-step+matching Delay before 1147785873 us; first/last activity=1141546277/1141546681 us. 
Cleanup: injections stopped; waiting through monotonic 1141785873 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=1142565868/1147676195 us. cleanup healthy=1. negative half-second interval: PASS with healthy Announce/two-step/Delay after-control. 
long sixteen-second interval: synthetic test candidate source=00f1b2fffed1df9a/1 GM=00f1b2fffed1df9a; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=4 actual refresh=16000000 us lifetime=48000000 us allowance=250 ms. Probe logInterval=4 expected=interval-derived lifetime. TX sequence=100 (fresh) successful=[1153086747,1153087018] us; TX sequence=101 (fresh) successful=[1154345598,1154345869] us; No service before 1202345598 us; fresh recovery required before 1208595869 us. Bounded recovery: fresh Announce+two-step+matching Delay before 1208595869 us; first/last activity=1202356314/1202356736 us. 
Cleanup: injections stopped; waiting through monotonic 1202595869 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=1203356664/1208446190 us. cleanup healthy=1. long sixteen-second interval: PASS with healthy Announce/two-step/Delay after-control. 
below supported interval range: synthetic test candidate source=00f1b2fffed1dfe3/1 GM=00f1b2fffed1dfe3; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=4 actual refresh=16000000 us lifetime=48000000 us allowance=250 ms. Probe logInterval=-8 expected=rejected without qualification/refresh. TX sequence=100 (unsupported interval; must not qualify/refresh) successful=[1213876728,1213876999] us; TX sequence=101 (unsupported interval; must not qualify/refresh) successful=[1218635610,1218635876] us; TX sequence=102 (fresh) successful=[1223395614,1223395892] us; TX sequence=103 (fresh) successful=[1224655602,1224655869] us; TX sequence=104 (unsupported interval; must not qualify/refresh) successful=[1256665629,1256665900] us; Fresh unsupported update must not shorten/extend original expiry=1272655602 us; no-refresh recovery deadline=1278905869 us. No service before 1272655602 us; fresh recovery required before 1278905869 us. Bounded recovery: fresh Announce+two-step+matching Delay before 1278905869 us; first/last activity=1272666293/1272666707 us. 
Cleanup: injections stopped; waiting through monotonic 1304915900 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=1305788296/1310827003 us. cleanup healthy=1. below supported interval range: PASS with healthy Announce/two-step/Delay after-control. 
above supported interval range: synthetic test candidate source=00f1b2fffed1de9a/1 GM=00f1b2fffed1de9a; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=4 actual refresh=16000000 us lifetime=48000000 us allowance=250 ms. Probe logInterval=5 expected=rejected without qualification/refresh. TX sequence=100 (unsupported interval; must not qualify/refresh) successful=[1316286619,1316286888] us; TX sequence=101 (unsupported interval; must not qualify/refresh) successful=[1321045595,1321045860] us; TX sequence=102 (fresh) successful=[1325805595,1325805861] us; TX sequence=103 (fresh) successful=[1327065594,1327065860] us; TX sequence=104 (unsupported interval; must not qualify/refresh) successful=[1359075629,1359075899] us; Fresh unsupported update must not shorten/extend original expiry=1375065594 us; no-refresh recovery deadline=1381315860 us. No service before 1375065594 us; fresh recovery required before 1381315860 us. Bounded recovery: fresh Announce+two-step+matching Delay before 1381315860 us; first/last activity=1375066294/1375066720 us. 
Cleanup: injections stopped; waiting through monotonic 1407325899 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=1408132962/1413176652 us. cleanup healthy=1. above supported interval range: PASS with healthy Announce/two-step/Delay after-control. 
unspecified interval: synthetic test candidate source=00f1b2fffed1d115/1 GM=00f1b2fffed1d115; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=4 actual refresh=16000000 us lifetime=48000000 us allowance=250 ms. Probe logInterval=127 expected=rejected without qualification/refresh. TX sequence=100 (unsupported interval; must not qualify/refresh) successful=[1418656636,1418656907] us; TX sequence=101 (unsupported interval; must not qualify/refresh) successful=[1423415594,1423415861] us; TX sequence=102 (fresh) successful=[1428175595,1428175862] us; TX sequence=103 (fresh) successful=[1429435594,1429435861] us; TX sequence=104 (unsupported interval; must not qualify/refresh) successful=[1461445629,1461445901] us; Fresh unsupported update must not shorten/extend original expiry=1477435594 us; no-refresh recovery deadline=1483685861 us. No service before 1477435594 us; fresh recovery required before 1483685861 us. Bounded recovery: fresh Announce+two-step+matching Delay before 1483685861 us; first/last activity=1477436312/1477436738 us. 
Cleanup: injections stopped; waiting through monotonic 1509695901 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=1510496588/1515536307 us. cleanup healthy=1. unspecified interval: PASS with healthy Announce/two-step/Delay after-control. 
minimum signed-byte interval: synthetic test candidate source=00f1b2fffed1d18c/1 GM=00f1b2fffed1d18c; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=4 actual refresh=16000000 us lifetime=48000000 us allowance=250 ms. Probe logInterval=-128 expected=rejected without qualification/refresh. TX sequence=100 (unsupported interval; must not qualify/refresh) successful=[1521016664,1521016934] us; TX sequence=101 (unsupported interval; must not qualify/refresh) successful=[1525775594,1525775860] us; TX sequence=102 (fresh) successful=[1530535598,1530535865] us; TX sequence=103 (fresh) successful=[1531795594,1531795861] us; TX sequence=104 (unsupported interval; must not qualify/refresh) successful=[1563805603,1563805870] us; Fresh unsupported update must not shorten/extend original expiry=1579795594 us; no-refresh recovery deadline=1586045861 us. No service before 1579795594 us; fresh recovery required before 1586045861 us. Bounded recovery: fresh Announce+two-step+matching Delay before 1586045861 us; first/last activity=1579805691/1580816477 us. 
Cleanup: injections stopped; waiting through monotonic 1612055870 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=1612899251/1617946951 us. cleanup healthy=1. minimum signed-byte interval: PASS with healthy Announce/two-step/Delay after-control. 
All seven interval scenarios passed for ranked two-fresh-Announce qualification. 
Operator phase evidence:
No assisted phase requested.
</pre>

### 43. PTP L2 signed service messages

Verify that the DUT sends PTP service messages with valid authentication.

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
DUT policy v1: selection=ranked two-fresh-Announce qualification; PTP authentication=enabled, optional (signed with keys, unsigned without keys); capacity=8; domain=0; foreign timeout=10000 ms; local log Announce/Sync=1/0; GNSS/PPS qualification=3500 ms; SPP=1 outgoing key ID=1; NTP authentication=mixed; source identity=0d2f18d75f34779b071d5c8a17e573e1190b00d83ab6d2ba7cdd9aff570dc038; provenance=public-source-contract-v2-not-runtime-attestation. Source configuration only, not runtime attestation.
Target=80:F1:B2:D2:CA:9A; domain=0; operator firmware=NtpPtpL2PtpL3.
Observation=20000 ms; samples=8; response window=500 ms; request interval=250 ms; processing allowance=250 ms; DUT foreign quiet=10000 ms; DUT withdrawal=3500 ms; recovery=60000 ms; operator phase limit=120000 ms; sequence seed=enabled/65532.
MAC=80:f1:b2:d2:ca:9a domain=0 mode=7; L2 untagged multicast two-step E2E; sequence-seed=controlled Delay_Req wrap; selection=ranked two-fresh-Announce qualification; authentication=enabled, optional (signed with keys, unsigned without keys); policy v1; identity=0d2f18d75f34779b071d5c8a17e573e1190b00d83ab6d2ba7cdd9aff570dc038; provenance=public-source-contract-v2-not-runtime-attestation. Observed outgoing=verified signed v2.1 (not inferred from configured enabled/required policy). Independent HMAC-SHA256-128 verification: Announce, Sync and Follow_Up; paired sequence=3329. Signed Delay_Req and matching verified Delay_Resp controls before/after=1/1. Signed periodic service exercised. Keys and ICVs redacted; functional authentication, not a servo/accuracy or anti-replay claim. 
Operator phase evidence:
No assisted phase requested.
</pre>

### 44. PTP L2 authenticated E2E exchange

Verify that the DUT completes an authenticated end-to-end PTP delay exchange.

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
DUT policy v1: selection=ranked two-fresh-Announce qualification; PTP authentication=enabled, optional (signed with keys, unsigned without keys); capacity=8; domain=0; foreign timeout=10000 ms; local log Announce/Sync=1/0; GNSS/PPS qualification=3500 ms; SPP=1 outgoing key ID=1; NTP authentication=mixed; source identity=0d2f18d75f34779b071d5c8a17e573e1190b00d83ab6d2ba7cdd9aff570dc038; provenance=public-source-contract-v2-not-runtime-attestation. Source configuration only, not runtime attestation.
Target=80:F1:B2:D2:CA:9A; domain=0; operator firmware=NtpPtpL2PtpL3.
Observation=20000 ms; samples=8; response window=500 ms; request interval=250 ms; processing allowance=250 ms; DUT foreign quiet=10000 ms; DUT withdrawal=3500 ms; recovery=60000 ms; operator phase limit=120000 ms; sequence seed=enabled/65532.
MAC=80:f1:b2:d2:ca:9a domain=0 mode=7; L2 untagged multicast two-step E2E; sequence-seed=controlled Delay_Req wrap; selection=ranked two-fresh-Announce qualification; authentication=enabled, optional (signed with keys, unsigned without keys); policy v1; identity=0d2f18d75f34779b071d5c8a17e573e1190b00d83ab6d2ba7cdd9aff570dc038; provenance=public-source-contract-v2-not-runtime-attestation. Observed outgoing=verified signed v2.1 (not inferred from configured enabled/required policy). Independent HMAC-SHA256-128 verification: Announce, Sync and Follow_Up; paired sequence=3331. Signed Delay_Req and matching verified Delay_Resp controls before/after=1/1. Authorized signed E2E exchange exercised. Keys and ICVs redacted; functional authentication, not a servo/accuracy or anti-replay claim. 
Operator phase evidence:
No assisted phase requested.
</pre>

### 45. PTP L2 tampered and malformed authentication

Verify that the DUT rejects PTP messages with tampered or malformed authentication.

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
DUT policy v1: selection=ranked two-fresh-Announce qualification; PTP authentication=enabled, optional (signed with keys, unsigned without keys); capacity=8; domain=0; foreign timeout=10000 ms; local log Announce/Sync=1/0; GNSS/PPS qualification=3500 ms; SPP=1 outgoing key ID=1; NTP authentication=mixed; source identity=0d2f18d75f34779b071d5c8a17e573e1190b00d83ab6d2ba7cdd9aff570dc038; provenance=public-source-contract-v2-not-runtime-attestation. Source configuration only, not runtime attestation.
Target=80:F1:B2:D2:CA:9A; domain=0; operator firmware=NtpPtpL2PtpL3.
Observation=20000 ms; samples=8; response window=500 ms; request interval=250 ms; processing allowance=250 ms; DUT foreign quiet=10000 ms; DUT withdrawal=3500 ms; recovery=60000 ms; operator phase limit=120000 ms; sequence seed=enabled/65532.
MAC=80:f1:b2:d2:ca:9a domain=0 mode=7; L2 untagged multicast two-step E2E; sequence-seed=controlled Delay_Req wrap; selection=ranked two-fresh-Announce qualification; authentication=enabled, optional (signed with keys, unsigned without keys); policy v1; identity=0d2f18d75f34779b071d5c8a17e573e1190b00d83ab6d2ba7cdd9aff570dc038; provenance=public-source-contract-v2-not-runtime-attestation. Observed outgoing=verified signed v2.1 (not inferred from configured enabled/required policy). unknown-key-id: pass (signed controls=1/1; response=0; expected=0; response ICV verified=0). wrong-key-value: pass (signed controls=1/1; response=0; expected=0; response ICV verified=0). tampered-body: pass (signed controls=1/1; response=0; expected=0; response ICV verified=0). tampered-ICV: pass (signed controls=1/1; response=0; expected=0; response ICV verified=0). wrong-SPP: pass (signed controls=1/1; response=0; expected=0; response ICV verified=0). unsupported-indicators: pass (signed controls=1/1; response=0; expected=0; response ICV verified=0). wrong-TLV-type: pass (signed controls=1/1; response=0; expected=0; response ICV verified=0). wrong-TLV-length: pass (signed controls=1/1; response=0; expected=0; response ICV verified=0). truncated-ICV: pass (signed controls=1/1; response=0; expected=0; response ICV verified=0). duplicate-authentication-TLV: pass (signed controls=1/1; response=0; expected=0; response ICV verified=0). Unique requester identities/sequences; complete bounded response windows; no pass from silence without signed positive controls. 
Operator phase evidence:
No assisted phase requested.
</pre>

### 46. PTP L2 mixed versus required authentication

Verify that the DUT enforces its configured PTP authentication policy.

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
DUT policy v1: selection=ranked two-fresh-Announce qualification; PTP authentication=enabled, optional (signed with keys, unsigned without keys); capacity=8; domain=0; foreign timeout=10000 ms; local log Announce/Sync=1/0; GNSS/PPS qualification=3500 ms; SPP=1 outgoing key ID=1; NTP authentication=mixed; source identity=0d2f18d75f34779b071d5c8a17e573e1190b00d83ab6d2ba7cdd9aff570dc038; provenance=public-source-contract-v2-not-runtime-attestation. Source configuration only, not runtime attestation.
Target=80:F1:B2:D2:CA:9A; domain=0; operator firmware=NtpPtpL2PtpL3.
Observation=20000 ms; samples=8; response window=500 ms; request interval=250 ms; processing allowance=250 ms; DUT foreign quiet=10000 ms; DUT withdrawal=3500 ms; recovery=60000 ms; operator phase limit=120000 ms; sequence seed=enabled/65532.
MAC=80:f1:b2:d2:ca:9a domain=0 mode=7; L2 untagged multicast two-step E2E; sequence-seed=controlled Delay_Req wrap; selection=ranked two-fresh-Announce qualification; authentication=enabled, optional (signed with keys, unsigned without keys); policy v1; identity=0d2f18d75f34779b071d5c8a17e573e1190b00d83ab6d2ba7cdd9aff570dc038; provenance=public-source-contract-v2-not-runtime-attestation. Observed outgoing=verified signed v2.1 (not inferred from configured enabled/required policy). unsigned-v2.0-policy: pass (signed controls=1/1; response=1; expected=1; response ICV verified=1). unsigned-v2.1-policy: pass (signed controls=1/1; response=1; expected=1; response ICV verified=1). Unique requester identities/sequences; complete bounded response windows; no pass from silence without signed positive controls. 
Operator phase evidence:
No assisted phase requested.
</pre>

### 47. PTP L2 authentication before selection

Verify that the DUT authenticates PTP candidates before considering them for master selection.

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
DUT policy v1: selection=ranked two-fresh-Announce qualification; PTP authentication=enabled, optional (signed with keys, unsigned without keys); capacity=8; domain=0; foreign timeout=10000 ms; local log Announce/Sync=1/0; GNSS/PPS qualification=3500 ms; SPP=1 outgoing key ID=1; NTP authentication=mixed; source identity=0d2f18d75f34779b071d5c8a17e573e1190b00d83ab6d2ba7cdd9aff570dc038; provenance=public-source-contract-v2-not-runtime-attestation. Source configuration only, not runtime attestation.
Target=80:F1:B2:D2:CA:9A; domain=0; operator firmware=NtpPtpL2PtpL3.
Observation=20000 ms; samples=8; response window=500 ms; request interval=250 ms; processing allowance=250 ms; DUT foreign quiet=10000 ms; DUT withdrawal=3500 ms; recovery=60000 ms; operator phase limit=120000 ms; sequence seed=enabled/65532.
MAC=80:f1:b2:d2:ca:9a domain=0 mode=7; L2 untagged multicast two-step E2E; sequence-seed=controlled Delay_Req wrap; selection=ranked two-fresh-Announce qualification; authentication=enabled, optional (signed with keys, unsigned without keys); policy v1; identity=0d2f18d75f34779b071d5c8a17e573e1190b00d83ab6d2ba7cdd9aff570dc038; provenance=public-source-contract-v2-not-runtime-attestation. Observed outgoing=verified signed v2.1 (not inferred from configured enabled/required policy). Authenticated foreign-Announce acceptance/recovery positive control: 
Baseline: L2 observer; HMAC-SHA256-128 verified PTP v2.1; source=80f1b2fffed2ca9a/1; GM=80f1b2fffed2ca9a; priority=128/128; class=248; accuracy=0xfe; variance=65535; steps=0; timeSource=0x20; timescale=PTP/TAI; UTC offset=37 s (valid); leap61=0; leap59=0; timeTraceable=0; frequencyTraceable=0. Quality is advertised, not independently verified. 
wrong-domain: synthetic test candidate source=00f1b2fffed1d051/1 GM=00f1b2fffed1d051; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[1636727001,1636727275] us; TX sequence=65534 (fresh) successful=[1638735599,1638735864] us; TX sequence=65535 (fresh) successful=[1640755595,1640755861] us; activity=1 post-control=1; completed. 
malformed: synthetic test candidate source=00f1b2fffed1d04a/1 GM=00f1b2fffed1d04a; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[1641486484,1641486749] us; TX sequence=65534 (fresh) successful=[1643505595,1643505861] us; TX sequence=65535 (fresh) successful=[1645525595,1645525861] us; activity=1 post-control=1; completed. 
foreign policy: synthetic test candidate source=00f1b2fffed1d047/1 GM=00f1b2fffed1d047; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[1646246489,1646246755] us; TX sequence=65534 (fresh) successful=[1648255594,1648255861] us; TX sequence=65535 (fresh) successful=[1650265595,1650265861] us; TX sequence=0 (fresh) successful=[1652275595,1652275861] us; TX sequence=1 (fresh) successful=[1654285595,1654285861] us; TX sequence=2 (fresh) successful=[1656295595,1656295862] us; TX sequence=3 (fresh) successful=[1658305595,1658305861] us; TX sequence=4 (fresh) successful=[1660315595,1660315861] us; TX sequence=5 (fresh) successful=[1662325595,1662325862] us; TX sequence=6 (fresh) successful=[1664335595,1664335861] us; TX sequence=7 (fresh) successful=[1666345595,1666345861] us; TX sequence=8 (fresh) successful=[1668355595,1668355861] us; TX sequence=9 (fresh) successful=[1670365595,1670365862] us; 
Standby phase=4 start/deadline=1648255861/1670505861 us silence-after=1648505861 us sends=13 last-TX/activity=1670365862/1648137244 us held=1; completed. TX sequence=10 (fresh) successful=[1670515664,1670515931] us; Expiry reference=[1670515664,1670515931] us no-early-before=1680515664 us quiet=1. Recovered fresh Announce+two-step+matching Delay=1; first/last activity=1680526050/1684566719 us. 
inferior/non-winning identity tie-break: synthetic test candidate source=00f1b2fffed1d0b5/1 GM=00f1b2fffed1d0b5; priority=129/128 class=248 accuracy=0xfe variance=65535; rank=1 deciding=priority1; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[1685285800,1685286072] us; TX sequence=65534 (fresh) successful=[1687295595,1687295861] us; 
inferior/non-winning identity tie-break: synthetic test candidate source=00f1b2fffed1d0a3/1 GM=80f1b2fffed2ca9b; priority=128/128 class=248 accuracy=0xfe variance=65535; rank=1 deciding=grandmaster identity; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[1692056457,1692056723] us; TX sequence=65534 (fresh) successful=[1694065595,1694065862] us; 
winning identity tie-break then no-longer-better: synthetic test candidate source=00f1b2fffed1d0d1/1 GM=80f1b2fffed2ca99; priority=128/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=grandmaster identity; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[1698826572,1698826839] us; TX sequence=65534 (fresh) successful=[1700835594,1700835860] us; 
Fresh same-source no-longer-better update: deciding=priority1; must recover before original expiry=1710835594 us. TX sequence=65535 (fresh) successful=[1702095652,1702095919] us; Fresh inferior update released standby before original expiry; RX-drops=0. Election does not discipline either clock or establish full BMCA/synchronization accuracy. 
Cleanup: injections stopped; waiting through monotonic 1712345919 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=1712636033/1717156015 us. cleanup healthy=1. unknown-key-id better Announce 1/2: continuous signed service=1; candidate rank=-1. unknown-key-id better Announce 2/2: continuous signed service=1; candidate rank=-1. tampered-ICV better Announce 1/2: continuous signed service=1; candidate rank=-1. tampered-ICV better Announce 2/2: continuous signed service=1; candidate rank=-1. Rejected authentication cannot qualify/suppress the master; both legacy first-Announce and ranked two-fresh-Announce triggers exercised. 
Cleanup: injections stopped; waiting through monotonic 1737685790 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=1738233808/1743255912 us. cleanup healthy=1. 
Operator phase evidence:
No assisted phase requested.
</pre>

### 48. PTP L2 optional unsigned service

Verify enabled, optional keyless PTP uses unsigned v2.0 Announce, Sync, Follow_Up and Delay_Resp with base lengths, legacy controls, valid UTC metadata and hardware-timestamped exchanges. (Not applicable to signed service.)

Result: **Could not run**
Reason: optional unsigned scenarios are not applicable to a verified signed DUT; intentionally keyless optional service must be configured on the DUT before testing

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
DUT policy v1: selection=ranked two-fresh-Announce qualification; PTP authentication=enabled, optional (signed with keys, unsigned without keys); capacity=8; domain=0; foreign timeout=10000 ms; local log Announce/Sync=1/0; GNSS/PPS qualification=3500 ms; SPP=1 outgoing key ID=1; NTP authentication=mixed; source identity=0d2f18d75f34779b071d5c8a17e573e1190b00d83ab6d2ba7cdd9aff570dc038; provenance=public-source-contract-v2-not-runtime-attestation. Source configuration only, not runtime attestation.
Target=80:F1:B2:D2:CA:9A; domain=0; operator firmware=NtpPtpL2PtpL3.
Observation=20000 ms; samples=8; response window=500 ms; request interval=250 ms; processing allowance=250 ms; DUT foreign quiet=10000 ms; DUT withdrawal=3500 ms; recovery=60000 ms; operator phase limit=120000 ms; sequence seed=enabled/65532.
MAC=80:f1:b2:d2:ca:9a domain=0 mode=7; L2 untagged multicast two-step E2E; sequence-seed=controlled Delay_Req wrap; selection=ranked two-fresh-Announce qualification; authentication=enabled, optional (signed with keys, unsigned without keys); policy v1; identity=0d2f18d75f34779b071d5c8a17e573e1190b00d83ab6d2ba7cdd9aff570dc038; provenance=public-source-contract-v2-not-runtime-attestation. Observed outgoing=verified signed v2.1 (not inferred from configured enabled/required policy). CouldNotRun: optional unsigned scenarios are not applicable to a verified signed DUT; intentionally keyless optional service must be configured on the DUT before testing
Operator phase evidence:
No assisted phase requested.
</pre>

### 49. PTP L2 optional unsigned signed-input rejection

Verify that signed-looking and tampered Delay_Req and better Announce packets cannot elicit responses or suppress healthy unsigned service. (Requires optional unsigned DUT, isolated link, negative-test and foreign-master opt-ins; no operator action.)

Result: **Could not run**
Reason: optional unsigned scenarios are not applicable to a verified signed DUT; intentionally keyless optional service must be configured on the DUT before testing

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
DUT policy v1: selection=ranked two-fresh-Announce qualification; PTP authentication=enabled, optional (signed with keys, unsigned without keys); capacity=8; domain=0; foreign timeout=10000 ms; local log Announce/Sync=1/0; GNSS/PPS qualification=3500 ms; SPP=1 outgoing key ID=1; NTP authentication=mixed; source identity=0d2f18d75f34779b071d5c8a17e573e1190b00d83ab6d2ba7cdd9aff570dc038; provenance=public-source-contract-v2-not-runtime-attestation. Source configuration only, not runtime attestation.
Target=80:F1:B2:D2:CA:9A; domain=0; operator firmware=NtpPtpL2PtpL3.
Observation=20000 ms; samples=8; response window=500 ms; request interval=250 ms; processing allowance=250 ms; DUT foreign quiet=10000 ms; DUT withdrawal=3500 ms; recovery=60000 ms; operator phase limit=120000 ms; sequence seed=enabled/65532.
MAC=80:f1:b2:d2:ca:9a domain=0 mode=7; L2 untagged multicast two-step E2E; sequence-seed=controlled Delay_Req wrap; selection=ranked two-fresh-Announce qualification; authentication=enabled, optional (signed with keys, unsigned without keys); policy v1; identity=0d2f18d75f34779b071d5c8a17e573e1190b00d83ab6d2ba7cdd9aff570dc038; provenance=public-source-contract-v2-not-runtime-attestation. Observed outgoing=verified signed v2.1 (not inferred from configured enabled/required policy). CouldNotRun: optional unsigned scenarios are not applicable to a verified signed DUT; intentionally keyless optional service must be configured on the DUT before testing
Operator phase evidence:
No assisted phase requested.
</pre>

### 50. PTP L2 unsigned Announce authentication policy

Verify that keyed mixed or required PTP authentication accepts or rejects unsigned better Announce messages with signed controls. (Requires isolated link and foreign-master opt-in.)

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
DUT policy v1: selection=ranked two-fresh-Announce qualification; PTP authentication=enabled, optional (signed with keys, unsigned without keys); capacity=8; domain=0; foreign timeout=10000 ms; local log Announce/Sync=1/0; GNSS/PPS qualification=3500 ms; SPP=1 outgoing key ID=1; NTP authentication=mixed; source identity=0d2f18d75f34779b071d5c8a17e573e1190b00d83ab6d2ba7cdd9aff570dc038; provenance=public-source-contract-v2-not-runtime-attestation. Source configuration only, not runtime attestation.
Target=80:F1:B2:D2:CA:9A; domain=0; operator firmware=NtpPtpL2PtpL3.
Observation=20000 ms; samples=8; response window=500 ms; request interval=250 ms; processing allowance=250 ms; DUT foreign quiet=10000 ms; DUT withdrawal=3500 ms; recovery=60000 ms; operator phase limit=120000 ms; sequence seed=enabled/65532.
MAC=80:f1:b2:d2:ca:9a domain=0 mode=7; L2 untagged multicast two-step E2E; sequence-seed=controlled Delay_Req wrap; selection=ranked two-fresh-Announce qualification; authentication=enabled, optional (signed with keys, unsigned without keys); policy v1; identity=0d2f18d75f34779b071d5c8a17e573e1190b00d83ab6d2ba7cdd9aff570dc038; provenance=public-source-contract-v2-not-runtime-attestation. Observed outgoing=verified signed v2.1 (not inferred from configured enabled/required policy). 
unsigned authentication policy: synthetic test candidate source=00f1b2fffed1d30a/1 GM=00f1b2fffed1d30a; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[1753526467,1753526490] us; TX sequence=65534 (fresh) successful=[1754785607,1754785626] us; Bounded recovery: fresh Announce+two-step+matching Delay before 1771035626 us; first/last activity=1764796254/1766795752 us. 
Cleanup: injections stopped; waiting through monotonic 1765035626 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=1766816705/1772856421 us. cleanup healthy=1. Unsigned v2.0 better Announce qualified/suppressed and expired under keyed mixed policy; signed service and Delay controls verified. 
unsigned authentication policy: synthetic test candidate source=00f1b2fffed1d35a/1 GM=00f1b2fffed1d35a; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[1778316531,1778316554] us; TX sequence=65534 (fresh) successful=[1779575599,1779575618] us; Bounded recovery: fresh Announce+two-step+matching Delay before 1795825618 us; first/last activity=1789585652/1789586232 us. 
Cleanup: injections stopped; waiting through monotonic 1789825618 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=1790586640/1795626362 us. cleanup healthy=1. Unsigned v2.1 better Announce qualified/suppressed and expired under keyed mixed policy; signed service and Delay controls verified. 
Operator phase evidence:
No assisted phase requested.
</pre>

### 51. PTP L2 invalid authentication cannot refresh standby

Verify that invalid signed updates cannot extend a qualified foreign candidate's original lifetime. (Requires isolated link, negative-test and foreign-master opt-ins.)

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
DUT policy v1: selection=ranked two-fresh-Announce qualification; PTP authentication=enabled, optional (signed with keys, unsigned without keys); capacity=8; domain=0; foreign timeout=10000 ms; local log Announce/Sync=1/0; GNSS/PPS qualification=3500 ms; SPP=1 outgoing key ID=1; NTP authentication=mixed; source identity=0d2f18d75f34779b071d5c8a17e573e1190b00d83ab6d2ba7cdd9aff570dc038; provenance=public-source-contract-v2-not-runtime-attestation. Source configuration only, not runtime attestation.
Target=80:F1:B2:D2:CA:9A; domain=0; operator firmware=NtpPtpL2PtpL3.
Observation=20000 ms; samples=8; response window=500 ms; request interval=250 ms; processing allowance=250 ms; DUT foreign quiet=10000 ms; DUT withdrawal=3500 ms; recovery=60000 ms; operator phase limit=120000 ms; sequence seed=enabled/65532.
MAC=80:f1:b2:d2:ca:9a domain=0 mode=7; L2 untagged multicast two-step E2E; sequence-seed=controlled Delay_Req wrap; selection=ranked two-fresh-Announce qualification; authentication=enabled, optional (signed with keys, unsigned without keys); policy v1; identity=0d2f18d75f34779b071d5c8a17e573e1190b00d83ab6d2ba7cdd9aff570dc038; provenance=public-source-contract-v2-not-runtime-attestation. Observed outgoing=verified signed v2.1 (not inferred from configured enabled/required policy). 
signed live candidate: synthetic test candidate source=00f1b2fffed1d369/1 GM=00f1b2fffed1d369; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=1 actual refresh=2000000 us lifetime=10000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[1802366473,1802366743] us; TX sequence=65534 (fresh) successful=[1803625599,1803625870] us; tampered-ICV same-source fresh-sequence=65535 TX=[1811385767,1811385786] us; unknown-key-id same-source fresh-sequence=0 TX=[1811385961,1811385980] us; Original expiry reference=[1803625599,1803625870] us; recovery deadline=1819875870 us; earliest possible invalid-update expiry=1821385767 us. Bounded recovery: fresh Announce+two-step+matching Delay before 1819875870 us; first/last activity=1813636257/1813636694 us. Original candidate expired despite invalid same-source updates; signed post-control passed. 
Cleanup: injections stopped; waiting through monotonic 1821635980 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=1821663682/1825706321 us. cleanup healthy=1. 
Operator phase evidence:
No assisted phase requested.
</pre>

### 52. PTP L2 bounded foreign-master capacity

Exercise configured capacity, qualified-candidate protection and stronger unqualified replacement. Internal table occupancy requires server-side instrumentation. (Requires ranked selection and foreign-master opt-in.)

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
DUT policy v1: selection=ranked two-fresh-Announce qualification; PTP authentication=enabled, optional (signed with keys, unsigned without keys); capacity=8; domain=0; foreign timeout=10000 ms; local log Announce/Sync=1/0; GNSS/PPS qualification=3500 ms; SPP=1 outgoing key ID=1; NTP authentication=mixed; source identity=0d2f18d75f34779b071d5c8a17e573e1190b00d83ab6d2ba7cdd9aff570dc038; provenance=public-source-contract-v2-not-runtime-attestation. Source configuration only, not runtime attestation.
Target=80:F1:B2:D2:CA:9A; domain=0; operator firmware=NtpPtpL2PtpL3.
Observation=20000 ms; samples=8; response window=500 ms; request interval=250 ms; processing allowance=250 ms; DUT foreign quiet=10000 ms; DUT withdrawal=3500 ms; recovery=60000 ms; operator phase limit=120000 ms; sequence seed=enabled/65532.
MAC=80:f1:b2:d2:ca:9a domain=0 mode=7; L2 untagged multicast two-step E2E; sequence-seed=controlled Delay_Req wrap; selection=ranked two-fresh-Announce qualification; authentication=enabled, optional (signed with keys, unsigned without keys); policy v1; identity=0d2f18d75f34779b071d5c8a17e573e1190b00d83ab6d2ba7cdd9aff570dc038; provenance=public-source-contract-v2-not-runtime-attestation. Observed outgoing=verified signed v2.1 (not inferred from configured enabled/required policy). 
capacity qualified candidate: synthetic test candidate source=00f1b2fffed1d387/1 GM=00f1b2fffed1d387; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=4 actual refresh=16000000 us lifetime=48000000 us allowance=250 ms. 
capacity qualified candidate: synthetic test candidate source=00f1b2fffed1d386/1 GM=00f1b2fffed1d386; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=4 actual refresh=16000000 us lifetime=48000000 us allowance=250 ms. 
capacity qualified candidate: synthetic test candidate source=00f1b2fffed1d3b9/1 GM=00f1b2fffed1d3b9; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=4 actual refresh=16000000 us lifetime=48000000 us allowance=250 ms. 
capacity qualified candidate: synthetic test candidate source=00f1b2fffed1d3b8/1 GM=00f1b2fffed1d3b8; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=4 actual refresh=16000000 us lifetime=48000000 us allowance=250 ms. 
capacity qualified candidate: synthetic test candidate source=00f1b2fffed1d3bb/1 GM=00f1b2fffed1d3bb; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=4 actual refresh=16000000 us lifetime=48000000 us allowance=250 ms. 
capacity qualified candidate: synthetic test candidate source=00f1b2fffed1d3ba/1 GM=00f1b2fffed1d3ba; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=4 actual refresh=16000000 us lifetime=48000000 us allowance=250 ms. 
capacity qualified candidate: synthetic test candidate source=00f1b2fffed1d3bd/1 GM=00f1b2fffed1d3bd; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=4 actual refresh=16000000 us lifetime=48000000 us allowance=250 ms. 
capacity qualified candidate: synthetic test candidate source=00f1b2fffed1d3bc/1 GM=00f1b2fffed1d3bc; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=4 actual refresh=16000000 us lifetime=48000000 us allowance=250 ms. 
single stronger newcomer: synthetic test candidate source=00f1b2fffed1d3b2/1 GM=00f1b2fffed1d3b2; priority=126/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=4 actual refresh=16000000 us lifetime=48000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[1835195700,1835195976] us; Bounded recovery: fresh Announce+two-step+matching Delay before 1887937475 us; first/last activity=1881695672/1882706438 us. All 8 candidate sources were qualified; one stronger newcomer was sent once; the weakest qualified source held standby after the others were released. 
Cleanup: injections stopped; waiting through monotonic 1883447913 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=1883706332/1887747022 us. cleanup healthy=1. 
capacity unqualified candidate: synthetic test candidate source=00f1b2fffed1d206/1 GM=00f1b2fffed1d206; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=4 actual refresh=16000000 us lifetime=48000000 us allowance=250 ms. 
capacity unqualified candidate: synthetic test candidate source=00f1b2fffed1d239/1 GM=00f1b2fffed1d239; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=4 actual refresh=16000000 us lifetime=48000000 us allowance=250 ms. 
capacity unqualified candidate: synthetic test candidate source=00f1b2fffed1d238/1 GM=00f1b2fffed1d238; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=4 actual refresh=16000000 us lifetime=48000000 us allowance=250 ms. 
capacity unqualified candidate: synthetic test candidate source=00f1b2fffed1d23b/1 GM=00f1b2fffed1d23b; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=4 actual refresh=16000000 us lifetime=48000000 us allowance=250 ms. 
capacity unqualified candidate: synthetic test candidate source=00f1b2fffed1d23a/1 GM=00f1b2fffed1d23a; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=4 actual refresh=16000000 us lifetime=48000000 us allowance=250 ms. 
capacity unqualified candidate: synthetic test candidate source=00f1b2fffed1d23d/1 GM=00f1b2fffed1d23d; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=4 actual refresh=16000000 us lifetime=48000000 us allowance=250 ms. 
capacity unqualified candidate: synthetic test candidate source=00f1b2fffed1d23c/1 GM=00f1b2fffed1d23c; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=4 actual refresh=16000000 us lifetime=48000000 us allowance=250 ms. 
capacity unqualified candidate: synthetic test candidate source=00f1b2fffed1d23f/1 GM=00f1b2fffed1d23f; priority=127/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=4 actual refresh=16000000 us lifetime=48000000 us allowance=250 ms. 
stronger unqualified replacement: synthetic test candidate source=00f1b2fffed1d230/1 GM=00f1b2fffed1d230; priority=126/128 class=248 accuracy=0xfe variance=65535; rank=-1 deciding=priority1; logInterval=4 actual refresh=16000000 us lifetime=48000000 us allowance=250 ms. TX sequence=65533 (fresh) successful=[1894485723,1894485995] us; TX sequence=65534 (fresh) successful=[1895736347,1895736615] us; Bounded recovery: fresh Announce+two-step+matching Delay before 1949986615 us; first/last activity=1943746186/1943746720 us. A stronger newcomer qualified after the full unqualified fill and expired normally. These are bounded on-wire consequences, not direct occupancy or eviction instrumentation. 
Cleanup: injections stopped; waiting through monotonic 1943986615 us (cancellation does not skip cleanup). Recovered fresh Announce+two-step+matching Delay=1; first/last activity=1944746129/1949786811 us. cleanup healthy=1. 
Operator phase evidence:
No assisted phase requested.
</pre>

### 53. PTP L3 UDP IPv4 Announce and master identity

Verify the selected IPv4 multicast master advertises a valid PTP Announce and clock identity.

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
PTP L3 UDP target IPv4=192.168.1.24; IPv6=(not selected); domain=0; operator firmware=NTP + PTP L2 + PTP L3 UDP; withdrawal bound=5000 ms. PTP L2 and NTP keys are not used by PTP L3 UDP.
PTP L3 UDP unsigned E2E; domain=0; IPv4=192.168.1.24; IPv6=disabled; multicast=224.0.1.129 / ff02::181; event/general ports=319/320; hop limit=1. Sockets are bound to the multicast groups and the Ethernet interface. Hardware timestamps, packet-drop counters and clock accuracy are not available through these sockets.
IPv4: Pass; selected multicast packets=3; Announce=1; matched Sync/Follow_Up=0; Delay_Req=0; matched Delay_Resp=0; response timeouts=0; malformed=0; unsupported=0; duplicates=0; unmatched=0; sequence gaps=0; source changed=0.
IPv4 selected master: L3 UDP observer; unauthenticated PTP v2.0; source=80f1b2fffed2ca9a/2; GM=80f1b2fffed2ca9a; priority=128/128; class=248; accuracy=0xfe; variance=65535; steps=0; timeSource=0x20; timescale=PTP/TAI; UTC offset=37 s (valid); leap61=0; leap59=0; timeTraceable=0; frequencyTraceable=0. Quality is advertised, not independently verified.

Operator phase evidence:
No assisted phase requested.
</pre>

### 54. PTP L3 UDP IPv6 Announce and master identity

Verify the selected IPv6 multicast master advertises a valid PTP Announce and clock identity.

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
PTP L3 UDP target IPv4=(not selected); IPv6=FE80::82F1:B2FF:FED2:CA9A; domain=0; operator firmware=NTP + PTP L2 + PTP L3 UDP; withdrawal bound=5000 ms. PTP L2 and NTP keys are not used by PTP L3 UDP.
PTP L3 UDP unsigned E2E; domain=0; IPv4=disabled; IPv6=FE80::82F1:B2FF:FED2:CA9A; multicast=224.0.1.129 / ff02::181; event/general ports=319/320; hop limit=1. Sockets are bound to the multicast groups and the Ethernet interface. Hardware timestamps, packet-drop counters and clock accuracy are not available through these sockets.
IPv6: Pass; selected multicast packets=5; Announce=1; matched Sync/Follow_Up=0; Delay_Req=0; matched Delay_Resp=0; response timeouts=0; malformed=0; unsupported=0; duplicates=0; unmatched=0; sequence gaps=0; source changed=0.
IPv6 selected master: L3 UDP observer; unauthenticated PTP v2.0; source=80f1b2fffed2ca9a/3; GM=80f1b2fffed2ca9a; priority=128/128; class=248; accuracy=0xfe; variance=65535; steps=0; timeSource=0x20; timescale=PTP/TAI; UTC offset=37 s (valid); leap61=0; leap59=0; timeTraceable=0; frequencyTraceable=0. Quality is advertised, not independently verified.

Operator phase evidence:
No assisted phase requested.
</pre>

### 55. PTP L3 UDP IPv4 two-step Sync / Follow_Up

Match an IPv4 multicast two-step Sync and Follow_Up from the selected master.

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
PTP L3 UDP target IPv4=192.168.1.24; IPv6=(not selected); domain=0; operator firmware=NTP + PTP L2 + PTP L3 UDP; withdrawal bound=5000 ms. PTP L2 and NTP keys are not used by PTP L3 UDP.
PTP L3 UDP unsigned E2E; domain=0; IPv4=192.168.1.24; IPv6=disabled; multicast=224.0.1.129 / ff02::181; event/general ports=319/320; hop limit=1. Sockets are bound to the multicast groups and the Ethernet interface. Hardware timestamps, packet-drop counters and clock accuracy are not available through these sockets.
IPv4: Pass; selected multicast packets=7; Announce=1; matched Sync/Follow_Up=1; Delay_Req=0; matched Delay_Resp=0; response timeouts=0; malformed=0; unsupported=0; duplicates=0; unmatched=0; sequence gaps=0; source changed=0.
IPv4 selected master: L3 UDP observer; unauthenticated PTP v2.0; source=80f1b2fffed2ca9a/2; GM=80f1b2fffed2ca9a; priority=128/128; class=248; accuracy=0xfe; variance=65535; steps=0; timeSource=0x20; timescale=PTP/TAI; UTC offset=37 s (valid); leap61=0; leap59=0; timeTraceable=0; frequencyTraceable=0. Quality is advertised, not independently verified.

Operator phase evidence:
No assisted phase requested.
</pre>

### 56. PTP L3 UDP IPv6 two-step Sync / Follow_Up

Match an IPv6 multicast two-step Sync and Follow_Up from the selected master.

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
PTP L3 UDP target IPv4=(not selected); IPv6=FE80::82F1:B2FF:FED2:CA9A; domain=0; operator firmware=NTP + PTP L2 + PTP L3 UDP; withdrawal bound=5000 ms. PTP L2 and NTP keys are not used by PTP L3 UDP.
PTP L3 UDP unsigned E2E; domain=0; IPv4=disabled; IPv6=FE80::82F1:B2FF:FED2:CA9A; multicast=224.0.1.129 / ff02::181; event/general ports=319/320; hop limit=1. Sockets are bound to the multicast groups and the Ethernet interface. Hardware timestamps, packet-drop counters and clock accuracy are not available through these sockets.
IPv6: Pass; selected multicast packets=5; Announce=1; matched Sync/Follow_Up=1; Delay_Req=0; matched Delay_Resp=0; response timeouts=0; malformed=0; unsupported=0; duplicates=0; unmatched=0; sequence gaps=0; source changed=0.
IPv6 selected master: L3 UDP observer; unauthenticated PTP v2.0; source=80f1b2fffed2ca9a/3; GM=80f1b2fffed2ca9a; priority=128/128; class=248; accuracy=0xfe; variance=65535; steps=0; timeSource=0x20; timescale=PTP/TAI; UTC offset=37 s (valid); leap61=0; leap59=0; timeTraceable=0; frequencyTraceable=0. Quality is advertised, not independently verified.

Operator phase evidence:
No assisted phase requested.
</pre>

### 57. PTP L3 UDP IPv4 E2E Delay_Req / Delay_Resp

Send an IPv4 multicast Delay_Req and match the selected master's response to its requester and sequence.

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
PTP L3 UDP target IPv4=192.168.1.24; IPv6=(not selected); domain=0; operator firmware=NTP + PTP L2 + PTP L3 UDP; withdrawal bound=5000 ms. PTP L2 and NTP keys are not used by PTP L3 UDP.
PTP L3 UDP unsigned E2E; domain=0; IPv4=192.168.1.24; IPv6=disabled; multicast=224.0.1.129 / ff02::181; event/general ports=319/320; hop limit=1. Sockets are bound to the multicast groups and the Ethernet interface. Hardware timestamps, packet-drop counters and clock accuracy are not available through these sockets.
IPv4: Pass; selected multicast packets=4; Announce=1; matched Sync/Follow_Up=0; Delay_Req=1; matched Delay_Resp=1; response timeouts=0; malformed=0; unsupported=0; duplicates=0; unmatched=0; sequence gaps=0; source changed=0.
IPv4 selected master: L3 UDP observer; unauthenticated PTP v2.0; source=80f1b2fffed2ca9a/2; GM=80f1b2fffed2ca9a; priority=128/128; class=248; accuracy=0xfe; variance=65535; steps=0; timeSource=0x20; timescale=PTP/TAI; UTC offset=37 s (valid); leap61=0; leap59=0; timeTraceable=0; frequencyTraceable=0. Quality is advertised, not independently verified.

Operator phase evidence:
No assisted phase requested.
</pre>

### 58. PTP L3 UDP IPv6 E2E Delay_Req / Delay_Resp

Send an IPv6 multicast Delay_Req and match the selected master's response to its requester and sequence.

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
PTP L3 UDP target IPv4=(not selected); IPv6=FE80::82F1:B2FF:FED2:CA9A; domain=0; operator firmware=NTP + PTP L2 + PTP L3 UDP; withdrawal bound=5000 ms. PTP L2 and NTP keys are not used by PTP L3 UDP.
PTP L3 UDP unsigned E2E; domain=0; IPv4=disabled; IPv6=FE80::82F1:B2FF:FED2:CA9A; multicast=224.0.1.129 / ff02::181; event/general ports=319/320; hop limit=1. Sockets are bound to the multicast groups and the Ethernet interface. Hardware timestamps, packet-drop counters and clock accuracy are not available through these sockets.
IPv6: Pass; selected multicast packets=6; Announce=1; matched Sync/Follow_Up=0; Delay_Req=1; matched Delay_Resp=1; response timeouts=0; malformed=0; unsupported=0; duplicates=0; unmatched=0; sequence gaps=0; source changed=0.
IPv6 selected master: L3 UDP observer; unauthenticated PTP v2.0; source=80f1b2fffed2ca9a/3; GM=80f1b2fffed2ca9a; priority=128/128; class=248; accuracy=0xfe; variance=65535; steps=0; timeSource=0x20; timescale=PTP/TAI; UTC offset=37 s (valid); leap61=0; leap59=0; timeTraceable=0; frequencyTraceable=0. Quality is advertised, not independently verified.

Operator phase evidence:
No assisted phase requested.
</pre>

### 59. PTP L3 UDP IPv4 time properties and clock quality

Validate and report the selected IPv4 master's advertised time properties and clock quality.

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
PTP L3 UDP target IPv4=192.168.1.24; IPv6=(not selected); domain=0; operator firmware=NTP + PTP L2 + PTP L3 UDP; withdrawal bound=5000 ms. PTP L2 and NTP keys are not used by PTP L3 UDP.
PTP L3 UDP unsigned E2E; domain=0; IPv4=192.168.1.24; IPv6=disabled; multicast=224.0.1.129 / ff02::181; event/general ports=319/320; hop limit=1. Sockets are bound to the multicast groups and the Ethernet interface. Hardware timestamps, packet-drop counters and clock accuracy are not available through these sockets.
IPv4: Pass; selected multicast packets=5; Announce=1; matched Sync/Follow_Up=0; Delay_Req=0; matched Delay_Resp=0; response timeouts=0; malformed=0; unsupported=0; duplicates=0; unmatched=0; sequence gaps=0; source changed=0.
IPv4 selected master: L3 UDP observer; unauthenticated PTP v2.0; source=80f1b2fffed2ca9a/2; GM=80f1b2fffed2ca9a; priority=128/128; class=248; accuracy=0xfe; variance=65535; steps=0; timeSource=0x20; timescale=PTP/TAI; UTC offset=37 s (valid); leap61=0; leap59=0; timeTraceable=0; frequencyTraceable=0. Quality is advertised, not independently verified.

Operator phase evidence:
No assisted phase requested.
</pre>

### 60. PTP L3 UDP IPv6 time properties and clock quality

Validate and report the selected IPv6 master's advertised time properties and clock quality.

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
PTP L3 UDP target IPv4=(not selected); IPv6=FE80::82F1:B2FF:FED2:CA9A; domain=0; operator firmware=NTP + PTP L2 + PTP L3 UDP; withdrawal bound=5000 ms. PTP L2 and NTP keys are not used by PTP L3 UDP.
PTP L3 UDP unsigned E2E; domain=0; IPv4=disabled; IPv6=FE80::82F1:B2FF:FED2:CA9A; multicast=224.0.1.129 / ff02::181; event/general ports=319/320; hop limit=1. Sockets are bound to the multicast groups and the Ethernet interface. Hardware timestamps, packet-drop counters and clock accuracy are not available through these sockets.
IPv6: Pass; selected multicast packets=5; Announce=1; matched Sync/Follow_Up=0; Delay_Req=0; matched Delay_Resp=0; response timeouts=0; malformed=0; unsupported=0; duplicates=0; unmatched=0; sequence gaps=0; source changed=0.
IPv6 selected master: L3 UDP observer; unauthenticated PTP v2.0; source=80f1b2fffed2ca9a/3; GM=80f1b2fffed2ca9a; priority=128/128; class=248; accuracy=0xfe; variance=65535; steps=0; timeSource=0x20; timescale=PTP/TAI; UTC offset=37 s (valid); leap61=0; leap59=0; timeTraceable=0; frequencyTraceable=0. Quality is advertised, not independently verified.

Operator phase evidence:
No assisted phase requested.
</pre>

### 61. PTP L3 UDP IPv4 sustained exchange integrity

Observe a bounded IPv4 multicast stream and repeated matched E2E exchanges without claiming clock accuracy.

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
PTP L3 UDP target IPv4=192.168.1.24; IPv6=(not selected); domain=0; operator firmware=NTP + PTP L2 + PTP L3 UDP; withdrawal bound=5000 ms. PTP L2 and NTP keys are not used by PTP L3 UDP.
PTP L3 UDP unsigned E2E; domain=0; IPv4=192.168.1.24; IPv6=disabled; multicast=224.0.1.129 / ff02::181; event/general ports=319/320; hop limit=1. Sockets are bound to the multicast groups and the Ethernet interface. Hardware timestamps, packet-drop counters and clock accuracy are not available through these sockets.
IPv4: Pass; selected multicast packets=6; Announce=1; matched Sync/Follow_Up=0; Delay_Req=1; matched Delay_Resp=1; response timeouts=0; malformed=0; unsupported=0; duplicates=0; unmatched=0; sequence gaps=0; source changed=0.
IPv4 selected master: L3 UDP observer; unauthenticated PTP v2.0; source=80f1b2fffed2ca9a/2; GM=80f1b2fffed2ca9a; priority=128/128; class=248; accuracy=0xfe; variance=65535; steps=0; timeSource=0x20; timescale=PTP/TAI; UTC offset=37 s (valid); leap61=0; leap59=0; timeTraceable=0; frequencyTraceable=0. Quality is advertised, not independently verified.
IPv4: Pass; selected multicast packets=121; Announce=10; matched Sync/Follow_Up=20; Delay_Req=69; matched Delay_Resp=69; response timeouts=0; malformed=0; unsupported=0; duplicates=0; unmatched=0; sequence gaps=0; source changed=0.
IPv4 selected master: L3 UDP observer; unauthenticated PTP v2.0; source=80f1b2fffed2ca9a/2; GM=80f1b2fffed2ca9a; priority=128/128; class=248; accuracy=0xfe; variance=65535; steps=0; timeSource=0x20; timescale=PTP/TAI; UTC offset=37 s (valid); leap61=0; leap59=0; timeTraceable=0; frequencyTraceable=0. Quality is advertised, not independently verified.

Operator phase evidence:
No assisted phase requested.
</pre>

### 62. PTP L3 UDP IPv6 sustained exchange integrity

Observe a bounded IPv6 multicast stream and repeated matched E2E exchanges without claiming clock accuracy.

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
PTP L3 UDP target IPv4=(not selected); IPv6=FE80::82F1:B2FF:FED2:CA9A; domain=0; operator firmware=NTP + PTP L2 + PTP L3 UDP; withdrawal bound=5000 ms. PTP L2 and NTP keys are not used by PTP L3 UDP.
PTP L3 UDP unsigned E2E; domain=0; IPv4=disabled; IPv6=FE80::82F1:B2FF:FED2:CA9A; multicast=224.0.1.129 / ff02::181; event/general ports=319/320; hop limit=1. Sockets are bound to the multicast groups and the Ethernet interface. Hardware timestamps, packet-drop counters and clock accuracy are not available through these sockets.
IPv6: Pass; selected multicast packets=4; Announce=1; matched Sync/Follow_Up=0; Delay_Req=1; matched Delay_Resp=1; response timeouts=0; malformed=0; unsupported=0; duplicates=0; unmatched=0; sequence gaps=0; source changed=0.
IPv6 selected master: L3 UDP observer; unauthenticated PTP v2.0; source=80f1b2fffed2ca9a/3; GM=80f1b2fffed2ca9a; priority=128/128; class=248; accuracy=0xfe; variance=65535; steps=0; timeSource=0x20; timescale=PTP/TAI; UTC offset=37 s (valid); leap61=0; leap59=0; timeTraceable=0; frequencyTraceable=0. Quality is advertised, not independently verified.
IPv6: Pass; selected multicast packets=120; Announce=10; matched Sync/Follow_Up=20; Delay_Req=68; matched Delay_Resp=68; response timeouts=0; malformed=0; unsupported=0; duplicates=0; unmatched=0; sequence gaps=0; source changed=0.
IPv6 selected master: L3 UDP observer; unauthenticated PTP v2.0; source=80f1b2fffed2ca9a/3; GM=80f1b2fffed2ca9a; priority=128/128; class=248; accuracy=0xfe; variance=65535; steps=0; timeSource=0x20; timescale=PTP/TAI; UTC offset=37 s (valid); leap61=0; leap59=0; timeTraceable=0; frequencyTraceable=0. Quality is advertised, not independently verified.

Operator phase evidence:
No assisted phase requested.
</pre>

### 63. PTP L3 UDP IPv4 invalid Delay_Req rejection

Test malformed or wrong-domain IPv4 requests with healthy before/after controls. Requires negative-test opt-in.

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
PTP L3 UDP target IPv4=192.168.1.24; IPv6=(not selected); domain=0; operator firmware=NTP + PTP L2 + PTP L3 UDP; withdrawal bound=5000 ms. PTP L2 and NTP keys are not used by PTP L3 UDP.
PTP L3 UDP unsigned E2E; domain=0; IPv4=192.168.1.24; IPv6=disabled; multicast=224.0.1.129 / ff02::181; event/general ports=319/320; hop limit=1. Sockets are bound to the multicast groups and the Ethernet interface. Hardware timestamps, packet-drop counters and clock accuracy are not available through these sockets.
IPv4: Pass; selected multicast packets=4; Announce=1; matched Sync/Follow_Up=0; Delay_Req=1; matched Delay_Resp=1; response timeouts=0; malformed=0; unsupported=0; duplicates=0; unmatched=0; sequence gaps=0; source changed=0.
IPv4 selected master: L3 UDP observer; unauthenticated PTP v2.0; source=80f1b2fffed2ca9a/2; GM=80f1b2fffed2ca9a; priority=128/128; class=248; accuracy=0xfe; variance=65535; steps=0; timeSource=0x20; timescale=PTP/TAI; UTC offset=37 s (valid); leap61=0; leap59=0; timeTraceable=0; frequencyTraceable=0. Quality is advertised, not independently verified.
wrong-domain: before/after controls passed; matching invalid-request response=not observed.
wrong-version: before/after controls passed; matching invalid-request response=not observed.
inconsistent-length: before/after controls passed; matching invalid-request response=not observed.
zero-clock-identity: before/after controls passed; matching invalid-request response=not observed.
truncated-header: before/after controls passed; matching invalid-request response=not observed.
No observed response is evidence of rejection only within this observation; socket or network packet loss cannot be excluded.

Operator phase evidence:
No assisted phase requested.
</pre>

### 64. PTP L3 UDP IPv6 invalid Delay_Req rejection

Test malformed or wrong-domain IPv6 requests with healthy before/after controls. Requires negative-test opt-in.

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
PTP L3 UDP target IPv4=(not selected); IPv6=FE80::82F1:B2FF:FED2:CA9A; domain=0; operator firmware=NTP + PTP L2 + PTP L3 UDP; withdrawal bound=5000 ms. PTP L2 and NTP keys are not used by PTP L3 UDP.
PTP L3 UDP unsigned E2E; domain=0; IPv4=disabled; IPv6=FE80::82F1:B2FF:FED2:CA9A; multicast=224.0.1.129 / ff02::181; event/general ports=319/320; hop limit=1. Sockets are bound to the multicast groups and the Ethernet interface. Hardware timestamps, packet-drop counters and clock accuracy are not available through these sockets.
IPv6: Pass; selected multicast packets=4; Announce=1; matched Sync/Follow_Up=0; Delay_Req=1; matched Delay_Resp=1; response timeouts=0; malformed=0; unsupported=0; duplicates=0; unmatched=0; sequence gaps=0; source changed=0.
IPv6 selected master: L3 UDP observer; unauthenticated PTP v2.0; source=80f1b2fffed2ca9a/3; GM=80f1b2fffed2ca9a; priority=128/128; class=248; accuracy=0xfe; variance=65535; steps=0; timeSource=0x20; timescale=PTP/TAI; UTC offset=37 s (valid); leap61=0; leap59=0; timeTraceable=0; frequencyTraceable=0. Quality is advertised, not independently verified.
wrong-domain: before/after controls passed; matching invalid-request response=not observed.
wrong-version: before/after controls passed; matching invalid-request response=not observed.
inconsistent-length: before/after controls passed; matching invalid-request response=not observed.
zero-clock-identity: before/after controls passed; matching invalid-request response=not observed.
truncated-header: before/after controls passed; matching invalid-request response=not observed.
No observed response is evidence of rejection only within this observation; socket or network packet loss cannot be excluded.

Operator phase evidence:
No assisted phase requested.
</pre>

### 65. PTP L3 UDP simultaneous IPv4 / IPv6 service

Observe both multicast transports concurrently and verify independent complete exchanges and a common grandmaster.

Result: **Pass**

<pre>Operator firmware=NTP + PTP L2 + PTP L3 UDP.
PTP L3 UDP target IPv4=192.168.1.24; IPv6=FE80::82F1:B2FF:FED2:CA9A; domain=0; operator firmware=NTP + PTP L2 + PTP L3 UDP; withdrawal bound=5000 ms. PTP L2 and NTP keys are not used by PTP L3 UDP.
PTP L3 UDP unsigned E2E; domain=0; IPv4=192.168.1.24; IPv6=FE80::82F1:B2FF:FED2:CA9A; multicast=224.0.1.129 / ff02::181; event/general ports=319/320; hop limit=1. Sockets are bound to the multicast groups and the Ethernet interface. Hardware timestamps, packet-drop counters and clock accuracy are not available through these sockets.
IPv4: Pass; selected multicast packets=4; Announce=1; matched Sync/Follow_Up=0; Delay_Req=1; matched Delay_Resp=1; response timeouts=0; malformed=0; unsupported=0; duplicates=0; unmatched=0; sequence gaps=0; source changed=0.
IPv4 selected master: L3 UDP observer; unauthenticated PTP v2.0; source=80f1b2fffed2ca9a/2; GM=80f1b2fffed2ca9a; priority=128/128; class=248; accuracy=0xfe; variance=65535; steps=0; timeSource=0x20; timescale=PTP/TAI; UTC offset=37 s (valid); leap61=0; leap59=0; timeTraceable=0; frequencyTraceable=0. Quality is advertised, not independently verified.
IPv6: Pass; selected multicast packets=4; Announce=1; matched Sync/Follow_Up=0; Delay_Req=1; matched Delay_Resp=1; response timeouts=0; malformed=0; unsupported=0; duplicates=0; unmatched=0; sequence gaps=0; source changed=0.
IPv6 selected master: L3 UDP observer; unauthenticated PTP v2.0; source=80f1b2fffed2ca9a/3; GM=80f1b2fffed2ca9a; priority=128/128; class=248; accuracy=0xfe; variance=65535; steps=0; timeSource=0x20; timescale=PTP/TAI; UTC offset=37 s (valid); leap61=0; leap59=0; timeTraceable=0; frequencyTraceable=0. Quality is advertised, not independently verified.
IPv4: Pass; selected multicast packets=122; Announce=10; matched Sync/Follow_Up=20; Delay_Req=70; matched Delay_Resp=70; response timeouts=0; malformed=0; unsupported=0; duplicates=0; unmatched=0; sequence gaps=0; source changed=0.
IPv4 selected master: L3 UDP observer; unauthenticated PTP v2.0; source=80f1b2fffed2ca9a/2; GM=80f1b2fffed2ca9a; priority=128/128; class=248; accuracy=0xfe; variance=65535; steps=0; timeSource=0x20; timescale=PTP/TAI; UTC offset=37 s (valid); leap61=0; leap59=0; timeTraceable=0; frequencyTraceable=0. Quality is advertised, not independently verified.
IPv6: Pass; selected multicast packets=122; Announce=10; matched Sync/Follow_Up=20; Delay_Req=70; matched Delay_Resp=70; response timeouts=0; malformed=0; unsupported=0; duplicates=0; unmatched=0; sequence gaps=0; source changed=0.
IPv6 selected master: L3 UDP observer; unauthenticated PTP v2.0; source=80f1b2fffed2ca9a/3; GM=80f1b2fffed2ca9a; priority=128/128; class=248; accuracy=0xfe; variance=65535; steps=0; timeSource=0x20; timescale=PTP/TAI; UTC offset=37 s (valid); leap61=0; leap59=0; timeTraceable=0; frequencyTraceable=0. Quality is advertised, not independently verified.

Operator phase evidence:
No assisted phase requested.
</pre>

### 66. PTP L2 GNSS loss and recovery (assisted)

Verify that the DUT responds correctly when its GNSS reference is lost and restored. (Requires operator assistance.)

Result: **Not yet run**

<pre>n/a</pre>

### 67. PTP L2 PPS loss and recovery (assisted)

Verify that the DUT responds correctly when its PPS reference is lost and restored. (Requires operator assistance.)

Result: **Not yet run**

<pre>n/a</pre>

### 68. PTP L2 server link loss and recovery (assisted)

Verify that the DUT recovers correctly after its server link is interrupted and restored. (Requires operator assistance.)

Result: **Not yet run**

<pre>n/a</pre>

### 69. PTP L2 restart / OTA recovery (assisted)

Verify that the DUT resumes correct PTP service after a restart or firmware update. (Requires operator assistance.)

Result: **Not yet run**

<pre>n/a</pre>

### 70. PTP L2 service recovery while standing by (assisted)

Verify that GNSS loss/restoration does not release standby while a better foreign master remains active, then verify healthy service after candidate expiry. (Requires ranked selection, foreign-master and assisted-test opt-ins, and operator assistance.)

Result: **Not yet run**

<pre>n/a</pre>

### 71. PTP L2 PPS recovery while standing by (assisted)

Verify a refreshed better candidate preserves standby across controlled PPS loss and restoration.

Result: **Not yet run**

<pre>n/a</pre>

### 72. PTP L2 server link recovery while standing by (assisted)

Verify standby across controlled server-link recovery while fresh candidates are transmitted during ranked startup listening.

Result: **Not yet run**

<pre>n/a</pre>

### 73. PTP L2 restart recovery while standing by (assisted)

Verify standby across a controlled DUT restart while fresh candidates are transmitted during ranked startup listening.

Result: **Not yet run**

<pre>n/a</pre>

### 74. PTP L2 staged key rotation and revocation (assisted)

Verify an isolated, explicitly rebuilt multi-key fixture across inbound-key acceptance, outgoing-key rotation and old-key revocation. Never use public fixture keys in production.

Result: **Not yet run**

<pre>Test disabled: PtpKeyRotationTestEnabled is set to false in the current firmware build. Set PtpKeyRotationTestEnabled to true in ESP32TimeServerTesterSettings.h and rebuild/flash the tester firmware to allow this test to run. Other test prerequisites still apply.
n/a</pre>

### 75. PTP L2 timestamp-referenced startup listening (assisted)

Bound the worker-start time using an independently instrumented qualified DUT, continuously observe startup listening, and classify ambiguous timing as inconclusive.

Result: **Not yet run**

<pre>n/a</pre>

### 76. PTP L2 candidate expiry while timing is unqualified (assisted)

While a qualified foreign master holds standby, independently confirm GNSS qualification loss without a generation reset, let the candidate expire, then verify immediate bounded recovery after requalification.

Result: **Not yet run**

<pre>n/a</pre>

### 77. PTP L3 UDP IPv4 GNSS loss and recovery (assisted)

Verify IPv4 service withdrawal and recovery during a controlled GNSS interruption.

Result: **Not yet run**

<pre>n/a</pre>

### 78. PTP L3 UDP IPv6 GNSS loss and recovery (assisted)

Verify IPv6 service withdrawal and recovery during a controlled GNSS interruption.

Result: **Not yet run**

<pre>n/a</pre>

### 79. PTP L3 UDP IPv4 PPS loss and recovery (assisted)

Verify IPv4 service withdrawal and recovery during a controlled PPS interruption.

Result: **Not yet run**

<pre>n/a</pre>

### 80. PTP L3 UDP IPv6 PPS loss and recovery (assisted)

Verify IPv6 service withdrawal and recovery during a controlled PPS interruption.

Result: **Not yet run**

<pre>n/a</pre>

### 81. PTP L3 UDP IPv4 server link recovery (assisted)

Verify IPv4 service withdrawal and recovery when the server-side Ethernet link is interrupted.

Result: **Not yet run**

<pre>n/a</pre>

### 82. PTP L3 UDP IPv6 server link recovery (assisted)

Verify IPv6 service withdrawal and recovery when the server-side Ethernet link is interrupted.

Result: **Not yet run**

<pre>n/a</pre>

### 83. PTP L3 UDP IPv4 restart recovery (assisted)

Verify IPv4 service withdrawal and recovery across a controlled DUT restart.

Result: **Not yet run**

<pre>n/a</pre>

### 84. PTP L3 UDP IPv6 restart recovery (assisted)

Verify IPv6 service withdrawal and recovery across a controlled DUT restart.

Result: **Not yet run**

<pre>n/a</pre>
