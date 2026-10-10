<p align="center"><strong>ESP32TimeServerTester Report</strong></p>

## Settings

| Settings | Values |
| --- | --- |
| **Target time server:** | ESP32TimeServer Test \(ULA\) |
| **IPv4 address:** | 192.168.1.24 |
| **IPv4 NTP Port:** | 123 |
| **IPv6 address:** | FDAC:1111:2222:1:82F1:B2FF:FED2:CA9A |
| **IPv6 NTP Port:** | 123 |
| **NTPv4 symmetric-key id:** | Present but redacted in this report |
| **NTPv4 symmetric-key value:** | Present but redacted in this report |
| **Mini saturated stress tests:** | Enabled (1000 NTP requests each) |

## Test results

| No. | Test | Result | Reported date and time |
| ---: | --- | --- | --- |
| 1 | IPv4 Ping target time server | <span style="color:green">Pass</span> | n/a |
| 2 | IPv4 NTPv3 Standard | <span style="color:green">Pass</span> | 2026-10-09 21:11:12.745268999 (UTC) |
| 3 | IPv4 NTPv4 Standard | <span style="color:green">Pass</span> | 2026-10-09 21:11:12.844973999 (UTC) |
| 4 | IPv4 NTPv4 Interleaved | <span style="color:green">Pass</span> | 2026-10-09 21:11:12.944978999 (UTC) |
| 5 | IPv4 NTPv4 Symmetric-key standard | <span style="color:green">Pass</span> | 2026-10-09 21:11:13.045497999 (UTC) |
| 6 | IPv4 NTPv4 Symmetric-key interleaved | <span style="color:green">Pass</span> | 2026-10-09 21:11:13.145099999 (UTC) |
| 7 | IPv4 NTPv4 Invalid symmetric-key | <span style="color:green">Pass</span> | n/a |
| 8 | IPv4 NTPv4 Missing symmetric-key | <span style="color:green">Pass</span> | n/a |
| 9 | IPv4 NTPv4 Truncated MAC | <span style="color:green">Pass</span> | n/a |
| 10 | IPv4 NTPv4 Standard mini saturated stress test | <span style="color:green">Pass</span> | 2026-10-09 21:11:23.216595999 (UTC) |
| 11 | IPv6 Ping target time server | <span style="color:green">Pass</span> | n/a |
| 12 | IPv6 NTPv3 Standard | <span style="color:green">Pass</span> | 2026-10-09 21:11:24.165664999 (UTC) |
| 13 | IPv6 NTPv4 Standard | <span style="color:green">Pass</span> | 2026-10-09 21:11:24.265413999 (UTC) |
| 14 | IPv6 NTPv4 Interleaved | <span style="color:green">Pass</span> | 2026-10-09 21:11:24.365408999 (UTC) |
| 15 | IPv6 NTPv4 Symmetric-key standard | <span style="color:green">Pass</span> | 2026-10-09 21:11:24.465678999 (UTC) |
| 16 | IPv6 NTPv4 Symmetric-key interleaved | <span style="color:green">Pass</span> | 2026-10-09 21:11:24.565503999 (UTC) |
| 17 | IPv6 NTPv4 Invalid symmetric-key | <span style="color:green">Pass</span> | n/a |
| 18 | IPv6 NTPv4 Missing symmetric-key | <span style="color:green">Pass</span> | n/a |
| 19 | IPv6 NTPv4 Truncated MAC | <span style="color:green">Pass</span> | n/a |
| 20 | IPv6 NTPv4 Standard mini saturated stress test | <span style="color:green">Pass</span> | 2026-10-09 21:11:34.669088999 (UTC) |

---

ESP32TimeServerTester v1.0.0

| Address Types | Addresses  |
| --- | --- |
| **MAC:** | 80:F1:B2:D1:D9:19 |
| **IPv4:** | 192.168.1.234 |
| **IPv6 LLA:** | FE80::82F1:B2FF:FED1:D919 |
| **IPv6 ULA:** | FDAC:1111:2222:1:82F1:B2FF:FED1:D919 |
| **IPv6 GUA:** | Not assigned |


---

https://github.com/roblatour/ESP32TimeServerTester
