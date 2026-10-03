# WiFi 보안(암호화/인증) 파싱 리서치

> 목적: **취약 배지** 표시를 위해 각 AP의 보안 방식(Open/WEP/WPA/WPA2/WPA3 + cipher + PMF)을
> 비콘에서 파싱한다. 실기기(Galaxy S10, nexmon)에서 파싱 가능 여부를 검증하고, 그 결과를
> 바탕으로 데몬/앱에 반영한다.

---

## 1. 정보가 있는 곳 — 전부 비콘 프레임 안

별도 조회 API가 아니라, 이미 `getEssid`/`getCh`가 읽는 **비콘 태그 영역**에 들어있다.

| 소스 | 위치 | 의미 |
|---|---|---|
| Capability Info의 **Privacy 비트** | 고정 파라미터(태그 시작 바로 앞 2바이트) `& 0x0010` | 암호화 사용 여부(Open 판별) |
| **RSN IE** — 태그 48 (0x30) | 태그 영역 | WPA2/WPA3 (cipher·AKM·PMF) |
| **WPA vendor IE** — 태그 221 (0xDD), OUI `00-50-F2` 타입 01 | 태그 영역 | 구형 WPA1 |

코드 기준 오프셋(현 `runner.cpp`/`wireless.cpp`와 동일 관례):
- 802.11 mgmt 헤더 24 + 고정 파라미터 12(timestamp8 + interval2 + **capability2**) → 태그 시작
- `tagStart = packet + rdt->len + 24 + 12`
- capability = `tagStart - 2` 의 2바이트(LE), Privacy = `& 0x0010`

---

## 2. RSN IE(태그 48) 구조

```
48 | len | version(2,LE) | Group Cipher(4) |
   PairwiseCount(2,LE) | Pairwise[](4×n) |
   AKMCount(2,LE)      | AKM[](4×n)      |
   RSN Capabilities(2,LE) | [PMKID ...] | [Group Mgmt Cipher(4)]
```
각 cipher/AKM = **OUI(3바이트) + 타입(1바이트)**. 표준 OUI `00-0F-AC`.
WPA vendor IE(221)도 구조 유사하나 OUI `00-50-F2`, `version → multicast → unicast[] → akm[]` 순.

---

## 3. Cipher / AKM 선택자 (OUI 00-0F-AC 뒤 타입)

| Cipher | 의미 | | AKM | 인증 |
|---|---|---|---|---|
| 2 | TKIP (구형·약함) | | 1 | 802.1X (Enterprise) |
| 4 | CCMP-128 (AES, WPA2 표준) | | 2 | PSK (WPA2-Personal) |
| 8 | GCMP-128 | | 6 | PSK-SHA256 |
| 9 | GCMP-256 (WPA3-192) | | 8 | SAE (WPA3-Personal) |
| 10 | CCMP-256 | | 18 | OWE (Enhanced Open) |

---

## 4. 판별 로직

- Privacy 꺼짐 + RSN·WPA IE 없음 → **Open**
- RSN AKM=OWE(18) → **Enhanced Open**
- Privacy 켜짐 + RSN·WPA IE 모두 없음 → **WEP**
- WPA IE(221)만 있음 → **WPA1** (보통 TKIP)
- RSN(48) 있음:
  - AKM SAE(8) → **WPA3** (PSK+SAE 동시 → WPA2/3 전환)
  - AKM PSK(2) → **WPA2**
  - AKM 802.1X(1) → **WPA2/3-Enterprise**
- cipher = pairwise 선택자 (CCMP/TKIP/GCMP…)

## 5. PMF (802.11w) — deauth 방어 여부

RSN Capabilities(2바이트)의 비트:
- 비트 7 (`0x0080`) = **MFPC** (관리 프레임 보호 가능)
- 비트 6 (`0x0040`) = **MFPR** (필수)

→ 켜져 있으면 **deauth 공격이 방어됨**. 이 도구에 직접 유용(공격 가능 여부 사전 표시).

---

## 6. 실기기 검증 (2026-09-05, Galaxy S10)

### 방법
1. 모니터 모드 진입 (`svc wifi disable; ifconfig wlan0 up; nexutil -d; nexutil -k1; nexutil -s0x613 -i -v2`)
2. 채널을 돌며 **tcpdump로 비콘만 raw 캡처** (radiotap):
   `LD_PRELOAD=/data/local/tmp/libnexmon.so tcpdump -i wlan0 -w b_$ch.pcap type mgt subtype beacon`
   - link-type = **IEEE802_11_RADIO(127)** 확인됨
   - airodump-ng은 내장 wireless-tools 체크에서 실패 → tcpdump 사용
3. pcap을 파이썬 파서로 파싱(Privacy 비트 + 태그 48/221 디코드) — **데몬에 포팅할 로직과 동일**

### 결과 (6개 AP, MAC 마스킹)

| ESSID | 보안 | PMF |
|---|---|---|
| *(hidden)* 12:07:89 | WPA2-PSK-CCMP/TKIP | — |
| *(hidden)* 46:4d:67 | **WPA1-PSK-TKIP** ⚠️ | — |
| KT_GiGA_5G_Wave2_6DF4 | WPA2-PSK-CCMP/TKIP | — |
| LGWiFi_624E | WPA2-PSK-CCMP | **PMF-cap** 🔒 |
| PARK2.4 | WPA2-PSK-CCMP | — |
| PARK5 | WPA2-PSK-CCMP | — |

### 발견 (전부 성공)
- WPA2/WPA1, cipher CCMP·TKIP·혼합, AKM PSK 정확 디코드
- **취약 대상 실포착**: `46:4d:67` = WPA1-PSK-TKIP(폐기 권고). WPA2인데 TKIP 허용(혼합)도 약점 후보
- **PMF 탐지 작동**: LGWiFi만 PMF-cap → deauth 방어. 나머지는 deauth 취약
- hidden SSID여도 보안 파싱 정상
- (이 환경엔 WEP/Open/WPA3 없음 — 로직엔 포함)

---

## 7. 앱 통합 계획

- **파싱**: `getSecurity()` 신설 (`security.cpp/.h` 또는 `wireless.cpp`에 추가). `getEssid`처럼 태그 워킹.
- **IPC**: `ST_IPC_EVENT`([ipc_proto.h])에 `char security[24]`(또는 코드화된 enum) + `uint8_t pmf` 필드 추가.
- **데몬**: [runner.cpp]의 비콘 emit 경로에서 `getSecurity()` 호출해 채움.
- **표시**: [devicelist.cpp] 델리게이트에서 채널/신호처럼 **취약 배지** 렌더.
- **취약 판정 제안**:
  - 🔴 취약: WEP / WPA1 / TKIP 포함
  - 🟡 deauth 가능: PMF 없음
  - 🟢 견고: WPA3(SAE) 또는 PMF-req

### 수정 파일 요약
- (새 파일 시) `security.cpp/.h` → `suseong/CMakeLists.txt` + 루트 `CMakeLists.txt` 양쪽 등록
- `ipc_proto.h` (필드 추가 → 구조체 크기 변경, 앱/데몬 동시 재빌드 필요)
- `wireless.cpp`(파싱), `runner.cpp`(호출), `devicelist.cpp`(표시)
- **데몬 재빌드 필수** (radiotap/runner처럼 데몬 바이너리에 들어감)

---

## 부록: 파서 핵심(포팅 참고)

```
beacon = radiotap(rt_len) 이후
fc0 = frame[0]; if (fc0 != 0x80) skip           # 비콘만
cap = LE16(frame[34:36]); privacy = cap & 0x0010
tags = frame[36:]
walk tags: id,len,data
  0   -> SSID
  48  -> RSN  : ver(2) group(4) [count(2)+suite(4)*n] x2(pairwise,akm) caps(2)
  221 && data[0:4]==00 50 F2 01 -> WPA1
suite 이름 = OUI(3)+타입(1) 매핑 (3장 표)
PMF = rsn.caps & 0x0080(cap) / 0x0040(req)
```
