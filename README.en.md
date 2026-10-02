[한국어](README.md) | **English**

# SuNiffing

A Qt app that scans nearby WiFi using nexmon monitor mode on the Galaxy S10,
and performs security testing (deauth, CSA, etc.) in authorized environments.

<p align="center">
  <img src="docs/screenshot.png" alt="SuNiffing screenshot" width="300">
</p>

> ⚠️ Use only on networks you **own or have explicit authorization** to test.
> Unauthorized use is illegal, and all responsibility lies with the user.

---

### Requirements

* Galaxy S10 series (S10 Lite excluded) — BCM4375b1
* Root access (Magisk)
* SELinux permissive
* **nexmon-patched Wi-Fi firmware** — a patched `bcmdhd_sta.bin_b1` installed at `/vendor/firmware/` (via an S10 nexmon Magisk module). The app's monitor mode depends on it.
  * ⚠️ The patched firmware is tied to a specific base version (18.41.x). It must match your device's Wi-Fi firmware version, or monitor mode will not initialize.

> `nexutil` and `libnexmon.so` are **bundled in the app** and auto-deployed to `/data/local/tmp` at runtime — no manual copying needed. The only separate prerequisite a user must set up is the **patched firmware (Magisk module)** above.

---

### Features

* Scan — real-time discovery of nearby APs / STATIONs
  * 2.4GHz + 5GHz
  * Automatic detection of supported channels
  * ESSID · BSSID · channel · signal strength (dBm)
* Attack (authorized testing only)
  * Deauth — disconnect a target STATION
  * Auth — authentication request flooding
  * CSA — Channel Switch Announcement injection

---

### Usage

1. Prepare an S10 that meets the requirements (root · SELinux permissive)
2. Install and launch the app → select the wireless interface
3. Start → choose the scan band (2.4 / 5 / Dual)
4. Review the discovered APs/STATIONs in the list
5. Long-press a target to open the menu → choose an attack type/parameters → run
6. Stop → end the session (WiFi restored)

---

### Build

* Qt 6.10.2 · Android arm64 (app) + daemon (`android/assets/suseong`)

---

### References

* [S10 ramdisk issue](https://m.blog.naver.com/gorhanhee/224025021274)
* [S10 nexmon issue](https://github.com/seemoo-lab/nexmon/issues/631)
* [Disabling SELinux](https://github.com/evdenis/selinux_permissive)
