# SuNiffing 보안 분석 보고서 (Security Audit Report)

- **대상**: SuNiffing — airodump/aireplay의 Android(nexmon) 이식 구현체
- **분석일**: 2026-06-13
- **분석 대상 상태**: **working tree (미커밋 변경분 포함)**. HEAD 커밋 `85f291f`(refactor: POLL 구조 도입)가 아니라 그 위에 얹힌 수정 중인 파일을 분석함. HEAD 대비 7개 파일 +483/-118 줄 차이(`git diff --stat HEAD`). 따라서 **본 보고서의 줄 번호는 현재 working tree 기준**이며, `git checkout 85f291f`로는 재현되지 않는다.
- **분석 방법**: 전체 소스 정독 + **검증용 하니스 컴파일/실행을 통한 실측**(구조체 크기, 정수 산술 추적, 경계 위반 재현). 추론으로 끝낸 항목과 실측으로 확인한 항목을 명시 구분함.

> ⚠️ **합법성 경고**: 본 도구는 Deauth / Auth flood / CSA(Channel Switch Announcement) 프레임을 주입하여 무선 네트워크를 **능동적으로 방해**합니다. 본인 소유가 아니거나 명시적 서면 허가를 받지 않은 네트워크에 사용하는 것은 대부분의 국가에서 **불법**입니다. 본 보고서는 방어/교육/인가된 테스트 목적의 코드 안전성 검토입니다.

---

## 0. 시스템 구조 요약

```
┌──────────────────────────┐   spawn: su -c "LD_PRELOAD=... <daemon> <iface>"
│   Qt GUI App (사용자권한)  │ ───────────────────────────────────────────────┐
│   mainwindow.cpp          │                                                 ▼
│                           │   stdin  ◄── ST_IPC_CMD (33B)  ──────  ┌─────────────────────┐
│   onDaemonOutput()        │   stdout ──► ST_IPC_EVENT (114B) ────► │  Daemon (suseong)   │
│   (이벤트 파싱/표시)        │                                        │  ★ROOT 권한 실행★    │
└──────────────────────────┘                                        │  runner.cpp         │
                                                                     │  - RXloop: 캡처/파싱 │
   채널 홉핑: su -c "nexutil -k<ch>"  (0.5s 주기)                      │  - TXloop: 프레임주입 │
                                                                     │  libpcap + nexmon    │
                                                                     └─────────────────────┘
                                                                              ▲
                                                                   공중(over-the-air)
                                                                   = 공격자 제어 가능 입력
```

**핵심 신뢰 경계 문제**: 데몬은 **root로 실행**되며, `RXloop`가 파싱하는 `packet`은 **주변 누구나 송출 가능한 무선 프레임**이다. 따라서 파서의 메모리 안전성 결함은 곧 **원격(근접) root 권한 공격 표면**이 된다.

### 실측한 구조체 크기 (컴파일러 확인, `#pragma pack(1)`)

| 구조체 | 크기 | 비고 |
|---|---|---|
| `ST_RDT` | 8 | radiotap 고정 헤더 |
| `ST_WL` | 24 | 802.11 MAC 헤더 |
| `ST_BC_COMMON` | 12 | 비콘 고정부 |
| `ST_IPC_CMD` | 33 | 앱→데몬 명령 |
| `ST_IPC_EVENT` | 114 | 데몬→앱 이벤트 (`bssid`@1, `st_mac`@7, `essid`@13, `message`@50) |

---

## 심각도 요약

| ID | 심각도 | 항목 | 검증 |
|----|--------|------|------|
| C-1 | 🔴 Critical | `rdt->len` 미검증 → 광범위 OOB read (root) | 코드추적 |
| C-2 | 🔴 Critical | CSA 경로 고정배열 `newPacket[4096]` 스택 오버플로우 | **실측 재현** |
| C-3 | 🔴 Critical | CSA 길이 산술 언더플로우/uint16 래핑 → 거대 memcpy | **실측 재현** |
| C-4 | 🔴 Critical | radiotap `presentCount`/`getRdtInfo` 무경계 OOB read | **실측 재현** |
| H-1 | 🟠 High | 전체 데몬이 root + `/data/local/tmp` LD_PRELOAD 권한상승 | 코드추적 |
| H-2 | 🟠 High | `devType` 셸 문자열 보간 → command injection 표면 | 코드추적 |
| H-3 | 🟠 High | UI에서 raw MAC 바이트를 NUL종료 문자열로 사용 → over-read | **실측 재현** |
| H-4 | 🟠 High | `qstringToMac` `sscanf` 반환값 미검증 → 미초기화 MAC 주입 | 코드추적 |
| M-1 | 🟡 Medium | macOS 빌드 깨짐(`QCoreApplictation` 오타) + 데몬 이름 불일치 | 코드추적 |
| M-2 | 🟡 Medium | `pcapTX` null 미검증 → null deref | **코드확인** |
| M-3 | 🟡 Medium | `RXloop` early-return 시 TX 스레드 미정리 → `std::terminate` | **코드확인** |
| M-4 | 🟡 Medium | CSA 채널값 1바이트 절단 + IPC 입력 미검증 | 코드추적 |
| M-5 | 🟡 Medium | 체크인된 1MB ELF 데몬(비스트립, debug_info) | **파일확인** |
| L-1~L-4 | 🟢 Low | 권한비트/백업/잔재코드/문서 | 코드확인 |

> **정정 사항(1차 분석 대비)**: 1차에서 "Critical"로 분류했던 항목 중 **`getEssid`/`getCh`/`getInsertTagLoc` 자체의 태그 워크는 실측 결과 경계 검사가 올바르게 동작**하여 단독으로는 안전함을 확인했다(아래 §C-5 참조). 과대평가를 정정한다. 반면 CSA 경로(C-2/C-3)와 radiotap 파서(C-4)는 실측으로 오버플로우/OOB가 **재현**되었다.

---

## 🔴 C-1. `rdt->len` 미검증 → 광범위 OOB read (root 컨텍스트)

**위치**: `runner.cpp:174-177`
```cpp
const ST_RDT* rdt = reinterpret_cast<const ST_RDT*>(packet);
const ST_WL*  wl  = reinterpret_cast<const ST_WL*>(packet + rdt->len);   // rdt->len 신뢰
WLTYPE type = chkWlType(wl);                                             // wl->frameControl 역참조
```

**문제**:
1. `packet`이 최소 `sizeof(ST_RDT)`(8B) 이상이라는 검증이 없다. runt 프레임이면 `rdt->len` 읽기 자체가 OOB.
2. `rdt->len`은 **패킷이 스스로 주장하는 radiotap 길이**다. `caplen`보다 큰 값을 넣으면 `wl`이 캡처 버퍼 밖을 가리키고, 이후 `wl->frameControl`, `wl->bssid`(`runner.cpp:227`), `wl->da/sa`(`wireless.cpp:52-66`) 접근이 전부 OOB read.

**영향**: root 프로세스에서의 정보 노출/크래시. 이후 모든 파싱 경로의 선행 조건이므로 **최우선 수정**.

**검증**: `header->caplen`은 `pcap_open_live(..., BUFSIZ, ...)`로 snaplen이 크므로 실제 캡처 길이를 정확히 반영하고 버퍼는 `caplen` 바이트를 담는다. 즉 `rdt->len`이 거짓이면 `packet + rdt->len`은 `packet + caplen`을 넘어선다 — 경계 검사 부재가 곧 OOB. (코드 경로 추적으로 확인)

**권장 수정**:
```cpp
if (header->caplen < sizeof(ST_RDT)) continue;
if (rdt->len < sizeof(ST_RDT) || rdt->len > header->caplen) continue;
if ((size_t)rdt->len + sizeof(ST_WL) > header->caplen) continue;
```

---

## 🔴 C-2 / C-3. CSA 경로 스택 버퍼 오버플로우 (실측 재현)

**위치**: `runner.cpp:189-220` (CSA 분기, `currentCmd.action == Act::CSA && bssid==target_ap`일 때)
```cpp
int16_t  tagLen       = capLen - rdt->len - wirelessLen - sizeof(ST_BC_COMMON);  // int16_t!
uint16_t newPacketLen = capLen + 5;
if (isFcs) { tagLen -= 4; newPacketLen -= 4; }
uint16_t insertTagLoc = capLen - (isFcs?4:0) - tagLen + getInsertTagLoc(...);    // uint16_t
uint16_t remainLen    = capLen - insertTagLoc - (isFcs?4:0);                     // uint16_t

uint8_t newPacket[4096];                                          // ★고정 4096 스택 배열★
...
memcpy(newPacket, packet, insertTagLoc);                          // 상한 검사 없음
memcpy(newPacket + insertTagLoc + 5, packet + insertTagLoc, remainLen);  // 상한 검사 없음
```

**문제 1 (C-2, 오버사이즈 비콘)**: `newPacket`은 고정 4096B인데 복사 길이에 상한 검사가 전혀 없다. 802.11 비콘(다수 IE 포함)이나 모니터 모드 캡처가 4096B를 넘으면 `memcpy`가 스택 배열을 침범한다.

**문제 2 (C-3, 짧은 위조 프레임)**: `tagLen`이 `int16_t`라 `capLen < rdt->len + 36`이면 **음수**가 되고, `insertTagLoc`/`remainLen`은 `uint16_t`라 **거대한 양수로 래핑**된다. `chkBeacon`은 첫 옥텟이 `0x80`이면 통과하므로(아래 실측), 본문이 짧은 위조 "비콘"으로 트리거 가능.

**회귀(regression) 근거**: 원본 `bob-CsaAttack/main.cpp:115`는 `new u_char[newPacketLen]`로 **정확한 크기를 동적 할당**한다. 데몬 이식 과정(커밋 `66055a5`)에서 고정 4096 스택 배열로 바뀌며 결함이 도입됨 (git -S 확인).

**실측 결과** (`/tmp/csa_overflow.cpp`, 실제 산술 그대로 재현):

| 입력 | tagLen(int16) | remainLen(uint16) | memcpy#2 마지막 쓰기 바이트 | 결과 |
|---|---|---|---|---|
| 정상 비콘 200B | 146 | 146 | 205 | ok |
| **위조 40B 프레임** | **-14** | **65522** | **65581** | **★STACK OVERFLOW★** |
| **5000B 비콘** | 4946 | 4946 | **5005** | **★STACK OVERFLOW★** |
| **4100B 비콘** | 4046 | 4046 | **4105** | **★STACK OVERFLOW★** |
| **rdt->len>caplen (200/60)** | -176 | 65360 | **65601** | **★STACK OVERFLOW★** |

`getInsertTagLoc`는 길이가 0/음수면 0을 반환함을 별도 실측(`/tmp/insertloc.cpp`)으로 확인 → 오버플로우를 막아주지 못함(worst case).

**영향**: **root 프로세스 스택 스매싱 → 원격(근접) 코드 실행 가능성**. 가장 위험.

**트리거 전제**: CSA 공격 활성(`Act::CSA`) + 공격자가 `target_ap` BSSID를 위조한 비콘 송출. 즉 사용자가 CSA를 켠 상태에서, 공격자가 동일 BSSID로 위조 비콘을 쏘면 트리거. 또한 정상 운영 중에도 타겟 AP가 4096B 넘는 비콘/관리프레임을 보내면 우발적 발생.

**권장 수정**:
- 모든 길이 산술을 **부호 있는 충분한 폭(`int`/`ptrdiff_t`)** 으로 수행하고 음수/상한 검사.
- `if (newPacketLen > sizeof(newPacket)) continue;` 가드, 또는 원본처럼 `newPacketLen` 기반 동적 할당.
- 복사 전 `insertTagLoc`, `insertTagLoc + 5 + remainLen <= sizeof(newPacket)` 명시 확인.

```cpp
long tagLen = (long)capLen - rdt->len - sizeof(ST_WL) - sizeof(ST_BC_COMMON) - (isFcs?4:0);
if (tagLen < 0) continue;
long insertTagLoc = (long)capLen - (isFcs?4:0) - tagLen + getInsertTagLoc(...);
long remainLen    = (long)capLen - insertTagLoc - (isFcs?4:0);
if (insertTagLoc < 0 || remainLen < 0) continue;
size_t needed = insertTagLoc + 5 + remainLen;
if (needed > sizeof(newPacket)) continue;   // 또는 동적 할당
```

---

## 🔴 C-4. radiotap 파서 무경계 OOB read (실측 재현)

**위치**: `radiotap.cpp:37-108`
```cpp
int presentCount(const u_char* packet) {
    uint32_t* presentPtr = (uint32_t*)(packet+4);
    while(true) {
        count++;
        if (hasNextPresent(*presentPtr)) presentPtr++;   // caplen 무관, MSB=1이면 무한 전진
        else break;
    }
}
// getRdtInfo / hasFcs:
uint16_t freq = *(uint16_t*)(packet + offset);  // offset 상한 검사 없음
data.pwr = (int8_t)packet[offset];
```

**문제**:
- `presentCount`는 `caplen`을 모른 채 present-bitmap의 MSB(extension bit)가 1인 동안 4바이트씩 무한 전진한다. present 워드들의 MSB가 계속 1이면 **버퍼를 벗어나 계속 읽는다**.
- `getRdtInfo`/`hasFcs`의 `offset`도 `caplen`과 비교하지 않아, 채널/안테나 필드 위치가 패킷 끝을 넘으면 OOB read.

**실측 결과** (`/tmp/present_walk.cpp`): 64바이트 버퍼를 전부 `0xFF`로 채우면(=모든 present 워드 MSB=1) `presentCount`가 16워드를 걸으며 **버퍼 끝을 넘어 읽는 지점 도달(OOB=YES)**. 더 긴 MSB 연쇄면 임의로 멀리 읽는다. 정상 present 워드(MSB=0)에서는 1을 반환(정상).

**영향**: root 프로세스 정보 노출/크래시. `presentCount`는 AP/STATION 두 경로 모두에서 **무조건 호출**되므로(always-on) C-1과 더불어 가장 상시적인 표면.

**권장 수정**: 두 함수에 `caplen`(또는 버퍼 끝 포인터)을 인자로 전달하고, 매 전진/필드 접근 전에 `(const u_char*)presentPtr + 4 <= packet + caplen`, `offset + fieldSize <= caplen`을 확인. radiotap `len` 필드(`rdt->len`)를 상한으로도 활용.

---

## 🟢 C-5. (정정) `getEssid`/`getCh` 태그 워크는 단독으로는 안전

1차 분석에서 의심했던 ESSID/채널 파서의 안전성을 실측(`/tmp/getessid_verify.cpp`)으로 재검토한 결과:

| 케이스 | 결과 |
|---|---|
| `beaconLen` 음수(-10) | 루프 가드 `index+2 <= end`가 `end<index`로 거짓 → **복사 없음, 안전** |
| 태그 `tagLen=255` → `dest[33]` | `copyLen = (tagLen < destSize-1)? tagLen : destSize-1` = 32로 캡 → **오버플로우 없음** |
| 정상 경계 | `tagStart + beaconLen == packet + caplen` → **caplen이 정확하면 in-bounds** |

따라서 `getEssid`/`getCh`/`getInsertTagLoc`의 **내부 태그 순회는 `index+2+tagLen > end` 검사로 올바르게 보호**된다. 잔여 위험은 이 함수들 자체가 아니라 **진입 직전 `tagStart`/`tagLen` 계산이 `rdt->len`(C-1)에 의존**한다는 점이다. 즉 C-1을 고치면 이 경로는 안전해진다. **이 항목은 Critical이 아니며, C-1에 종속된 Low 위험으로 재분류.**

---

## 🟠 H-1. root 데몬 + 세계-쓰기가능 경로 LD_PRELOAD → 권한 상승

**위치**: `mainwindow.cpp:61-77`, `82-126`
```cpp
QString cmd = QString("LD_PRELOAD=/data/local/tmp/libnexmon.so %1 %2").arg(targetPath, devType);
daemonProcess->start("su", QStringList() << "-c" << cmd);
```

**문제**:
1. **전체 데몬이 root로 실행**된다. 따라서 C-1~C-4의 메모리 결함이 그대로 root 권한 표면이 된다. 캡처/주입에 실제로 필요한 권한은 `CAP_NET_RAW`/`CAP_NET_ADMIN` 수준이므로, 파서를 저권한 프로세스로 분리하거나 권한을 낮추는 설계가 바람직하다.
2. `/data/local/tmp/`는 **셸/타 앱이 쓰기 가능**한 경로다. 악의적 주체가 이 경로에 `libnexmon.so`를 심으면 `LD_PRELOAD`로 **root 코드가 그 라이브러리를 로드** → 권한 상승. 라이브러리는 앱 전용 경로(`/data/data/<pkg>/...`)에 두고 무결성(해시) 검증 권장.

---

## 🟠 H-2. `devType` 셸 문자열 보간 (command injection 표면)

**위치**: `mainwindow.cpp:100-105`, `145-149`, `68-69`
```cpp
QString cmd = QString("svc wifi disable; ... ifconfig %1 up; nexutil ...").arg(dev);  // dev=devType
daemonProcess->start("su", QStringList() << "-c" << cmd);
```

**문제**: `su -c`에 **조립된 한 줄 문자열**을 전달하므로, `devType`에 `;`, `$(...)`, 백틱이 포함되면 **root 셸에서 실행**된다. 현재 인터페이스명은 `pcap_findalldevs` 결과라 통상 안전하나, 인터페이스 이름은 신뢰 경계로 취급해야 한다(드라이버/USB 동글이 임의 이름 제공 가능).

**권장 수정**: `^[A-Za-z0-9_.-]+$` 화이트리스트 검증 후 사용. 가능하면 `su` 셸을 거치지 않고 `argv` 배열로 직접 실행하여 셸 메타문자 해석 자체를 제거.

---

## 🟠 H-3. UI에서 raw MAC 바이트를 NUL종료 문자열로 사용 (실측 재현)

**위치**: `mainwindow.cpp:242, 256`
```cpp
item->setData(..., QString::fromUtf8((char*)event.bssid.mac));   // 6바이트 바이너리, NUL 보장 없음
```

**문제**: `event.bssid.mac`은 6바이트 바이너리이며 NUL 종료가 없다. `QString::fromUtf8`은 NUL을 만날 때까지 스캔하므로, MAC에 `0x00`이 없으면 **인접 필드(`st_mac`@offset7 등)와 그 너머까지 읽는다**. 구조체 실측에서 `bssid`@1, `st_mac`@7로 **바로 붙어 있음**을 확인.

**실측 결과** (`/tmp/misc_verify.cpp`): NUL 없는 `bssid` 위로 `strlen` 수행 시 **45바이트**를 스캔(유효 6바이트뿐) → over-read 확인.

**권장 수정**: `LinkedBssid`에는 `prtMac()`로 포맷한 문자열만 저장. raw 바이트를 `QString`에 직접 넣지 말 것.

---

## 🟠 H-4. `qstringToMac` `sscanf` 반환값 미검증 → 미초기화 MAC 주입

**위치**: `mainwindow.cpp:521-529`
```cpp
static ST_MAC qstringToMac(const QString& macStr) {
    ST_MAC mac;
    uint32_t temp[6];                                   // 미초기화
    sscanf(macStr..., "%x:%x:%x:%x:%x:%x", &temp[0]...); // 반환값 무시
    for(int i=0;i<6;++i) mac.mac[i]=(uint8_t)temp[i];    // 파싱 실패 시 쓰레기값
    return mac;
}
```

**문제**: 파싱이 6개 미만 매칭되면 `temp`의 나머지는 **미초기화 스택 값**이며, 그 쓰레기 MAC이 deauth/CSA 타겟으로 주입된다. 원본 `bob-deauthattack/main.cpp:38`은 `if (count == 6)`를 검사하지만 앱 이식분에서 누락됨.

**권장 수정**: `uint32_t temp[6] = {0};` 초기화 + `if (sscanf(...) != 6) { /* 거부 */ }`.

---

## 🟡 M-1. macOS 빌드 깨짐 + 데몬 산출물 이름 불일치

**위치**: `mainwindow.cpp:74`, `CMakeLists.txt:13,77`
```cpp
QString targetPath = QCoreApplictation::applicationDirPath() + "/suseongdaemon";  // 오타
```
- `QCoreApplictation` → `QCoreApplication` 오타로 **macOS(`Q_OS_MAC`) 빌드가 컴파일 실패**.
- 여기서 찾는 데몬 이름은 `suseongdaemon`인데, CMake(`add_subdirectory(suseong)`, `llvm-strip ... ssdaemon`)는 산출물을 `ssdaemon`(또는 `suseong`)으로 만든다 → **경로 불일치**로 실행 실패. 산출물 이름을 한 곳으로 통일 필요.

---

## 🟡 M-2. `pcapTX` null 미검증 → null 역참조

**위치**: `runner.cpp:101-109`
```cpp
pcapRX = pcap_open_live(...);
pcapTX = pcap_open_live(...);
if(pcapRX == nullptr) { ...; return; }   // pcapTX는 검사 안 함
```
RX는 성공하고 TX만 실패하면 `pcapTX==nullptr`인 채로 진행되어, 이후 `pcap_sendpacket(pcapTX, ...)`(`runner.cpp:38,50,67,220`)에서 **null 역참조 크래시**. TX도 동일하게 null 체크 필요.

---

## 🟡 M-3. `RXloop` early-return 시 TX 스레드 미정리 → `std::terminate`

**위치**: `runner.cpp:115, 123-128`
```cpp
TXthread = std::thread(&Runner::TXloop, this);   // (115) 시작
...
int pcap_fd = pcap_get_selectable_fd(pcapRX);
if(pcap_fd < 0) { TRACE("..."); return; }        // (127) TXthread join 없이 반환
```
`pcap_get_selectable_fd` 실패 시 `TXthread`가 joinable인 채로 `RXloop`가 반환된다. 소멸자 `~Runner()`→`stop()`은 `isRunning=false`만 할 뿐 `join()`하지 않으므로, joinable한 `std::thread`가 파괴되며 **`std::terminate()`** 호출. 모든 early-exit 경로에서 `isRunning=false; if(TXthread.joinable()) TXthread.join();` 보장 필요.

---

## 🟡 M-4. CSA 채널값 1바이트 절단 + IPC 입력 미검증

**위치**: `runner.cpp:216`
```cpp
uint8_t csaTag[5] = {0x25, 0x03, 0x01, (uint8_t)currentCmd.channel, 0x03};
```
`channel`은 `int32_t`인데 `uint8_t`로 절단된다. UI는 1~14로 제한(`attackChSpin->setRange(1,14)`)하지만, 데몬은 IPC로 들어오는 `ST_IPC_CMD`를 **신뢰**하므로(범위/`interface` 내용/`action` 유효성 미검증) UI를 우회한 입력에 무방비다. 데몬 측에서 `channel ∈ [1,14]`, `action` 범위 등 입력 검증 추가 권장.

---

## 🟡 M-5. 체크인된 1MB ELF 데몬(비스트립, debug_info)

**위치**: `android/assets/suseong`
```
ELF 64-bit LSB pie executable, ARM aarch64, dynamically linked, with debug_info, not stripped, 1,044,696 bytes
```
APK에 번들되는 데몬 바이너리가 **소스로부터 재현 빌드되지 않고 리포지토리에 직접 커밋**되어 있다. (1) 공급망/재현성 문제 — 이 바이너리가 현재 소스와 일치하는지 보장되지 않음. (2) `debug_info` 포함·비스트립 상태로 배포되면 분석/역공학이 쉬워진다(릴리스 빌드는 `llvm-strip` 적용하도록 CMake에 이미 존재하나, 체크인 바이너리에는 미적용). 빌드 산출물을 VCS에서 제거하고 CI 빌드로 생성하길 권장.

---

## 🟢 Low / 정리 권장

- **L-1 권한 비트**: `dropPcapDaemon`이 데몬에 `0755`(others-exec) 부여(`mainwindow.cpp:512-514`). 앱 전용 디렉터리이므로 노출은 제한적이나 `0700`이면 충분.
- **L-2 백업**: `AndroidManifest.xml:20` `allowBackup="true"`. 보안 도구 특성상 `false` 권장(adb 백업으로 추출 데몬/설정 유출 가능).
- **L-3 잔재 코드**: `oldfiles/`(protobuf 잔재), 주석 처리된 ACK 경로(`runner.cpp:73-86`), 미사용 include 등 — 공격 표면/혼란 축소 위해 정리.
- **L-4 동시성**: stdout 보호는 `outMutex`로 RX 경로만 직렬화하며 TX는 stdout에 쓰지 않으므로 현 구조에선 충돌 없음(문제 아님, 확인차 기재).

---

# UI / UX 심층 분석

대상: `mainwindow.ui`, `display.ui`, `mainwindow.cpp`(UI 로직), `display.cpp`. 본 절은 **모바일(Android) 타깃**임을 전제로, (A) 레이아웃·반응형, (B) UI 로직 버그/메모리, (C) 상호작용·접근성, (D) 상태 표현·피드백 순으로 분석한다.

## UI 항목 요약

| ID | 심각도 | 항목 | 근거 |
|----|--------|------|------|
| U-1 | 🔴 Critical | `mainwindow.ui` 전체가 절대좌표(고정 px), 레이아웃 매니저 부재 → 모바일에서 레이아웃 붕괴 | .ui 직접확인 |
| U-2 | 🔴 Critical | `QListWidgetItem(parent)` + `addItem()` 이중 삽입 | 코드+Qt문서 |
| U-3 | 🟠 High | `viewToggle` 슬라이더를 토글로 사용 (y=-10 화면밖 침범) + 발견성 0 | .ui+코드 |
| U-4 | 🟠 High | 리스트 무한 증가(에이징/정렬/제거 없음) → 장시간 스캔 시 성능 저하·가독성 붕괴 | 코드추적 |
| U-5 | 🟠 High | `updateInfo` ESSID 갱신 조건식 오류(`!isEmpty() || !=`) → 빈 ESSID로 깜빡임 | **로직추적** |
| U-6 | 🟡 Medium | UI 블로킹: `onStartButton`이 `waitForFinished(3000)`로 메인스레드 정지(ANR 위험) | 코드추적 |
| U-7 | 🟡 Medium | 공격 진행 중 시각적 상태 표시 부재(어느 타깃을 언제까지 공격 중인지 불명확) | 코드추적 |
| U-8 | 🟡 Medium | 하드코딩된 인라인 스타일시트 산재 → 테마/다크모드 불가, 유지보수성 저하 | 코드추적 |
| U-9 | 🟡 Medium | 다국어/문자열 미분리(`tr()` 미사용), 한영 혼재 | 코드확인 |
| U-10 | 🟢 Low | 정렬·필터·검색 부재, 빈 상태(empty state) 안내 없음, 버튼 레이블 대소문자 불일치 | .ui+코드 |

---

## 🔴 U-1. `mainwindow.ui` 전체가 절대좌표 — 모바일 레이아웃 붕괴

**근거**: `mainwindow.ui` 모든 위젯이 `<geometry><rect>` 고정 픽셀로 배치되고, **단 하나의 레이아웃 매니저도 없다**. 최상위 `centralwidget`에도 layout이 없다.

```
MainWindow geometry: 373 x 660 (고정)
 startButton  : x=160 y=20  w=100 h=41   ← stopButton(x=260,w=100)과 x=260에서 경계 접함
 stopButton   : x=260 y=20  w=100 h=41   ← 화면폭 373에서 우측 끝(360)까지 꽉 참
 listWidget   : x=10  y=60  w=351 h=531  ← 높이 531 고정
 currentCh    : x=10  y=590 w=351 h=21
 viewToggle   : x=220 y=-10 w=61  h=25   ← ★y=-10: 화면 밖(음수)으로 배치★
 label(AP)    : x=190 y=0
 label_2(STA) : x=290 y=0
```

**문제 (모바일에서 치명적)**:
1. **고정 660px 높이**는 실제 안드로이드 기기(화면비/해상도 천차만별)에서 거의 항상 어긋난다. 작은 화면이면 `currentCh`(y=590)와 리스트 하단이 잘리고, 큰 화면이면 하단에 빈 공간이 남는다.
2. **회전(가로모드) 대응 불가**: AndroidManifest는 `screenOrientation="unspecified"`(회전 허용)인데, 절대좌표라 가로 전환 시 위젯이 화면을 벗어나거나 우측이 비어버린다.
3. **DPI 스케일링 취약**: 고정 px는 고DPI 기기에서 의도와 다른 물리적 크기로 표시된다.
4. `viewToggle`의 `y=-10`은 위젯 상단이 부모 영역 밖으로 잘려, 토글 손잡이의 일부만 보이거나 탭 영역이 좁아진다(터치 타깃 손상).

**영향**: 모바일 이식이 목적인데 **UI가 기기별로 깨지는 것이 거의 보장**된다. 보안 결함과 별개로, 실사용성을 막는 최우선 UI 결함.

**권장 수정**:
- 최상위에 `QVBoxLayout` 적용. 상단 바(devIn/start/stop/toggle)는 `QHBoxLayout`, 중앙 `listWidget`은 `stretch=1`로 가변, 하단 `currentCh`는 고정 높이.
- 모든 위젯의 `<geometry>` 절대좌표 제거하고 레이아웃에 위임.
- 터치 타깃은 최소 **48x48dp**(Material) / **44pt**(HIG) 확보 — 현재 start/stop 높이 41px는 경계선.
- 회전 시 재배치가 자동이 되도록 layout 기반으로 전환(추가로 가로모드 전용 배치가 필요하면 분기).

```xml
<!-- 개념 예시 -->
<layout class="QVBoxLayout">
  <item><layout class="QHBoxLayout">  <!-- devIn | start | stop | toggle -->
    <item><widget class="QComboBox" name="devIn"/></item>
    <item><widget class="QPushButton" name="startButton"/></item>
    <item><widget class="QPushButton" name="stopButton"/></item>
  </layout></item>
  <item><widget class="QListWidget" name="listWidget"/></item>  <!-- stretch -->
  <item><widget class="QLabel" name="currentCh"/></item>
</layout>
```

---

## 🔴 U-2. `QListWidgetItem(parent)` + `addItem()` 이중 삽입

**위치**: `mainwindow.cpp:246, 260-261`
```cpp
QListWidgetItem* newItem = new QListWidgetItem(ui->listWidget);  // (246) ← 생성과 동시에 리스트에 삽입됨
display* newWidget = new display(this);
...
ui->listWidget->addItem(newItem);                                // (260) ← 같은 아이템을 또 추가
ui->listWidget->setItemWidget(newItem, newWidget);               // (261)
```

**문제**: `QListWidgetItem(QListWidget *parent)` 생성자는 Qt 문서상 *"Constructs an empty list widget item ... and **inserts it into** the list widget"* 로, **생성 즉시 리스트에 추가**된다. 그 뒤 `addItem(newItem)`으로 **동일 포인터를 두 번 삽입**하면:
- 같은 항목이 모델에 중복 등록되어 **유령 행/렌더링 이상**이 발생할 수 있고,
- 모델이 항목 소유권을 갖는 구조에서 **이중 소유 → 소멸 시 double-free/크래시** 위험이 있다.

추가로 `setItemWidget`을 `addItem` 이후에 호출하는데, 항목 위젯은 항목이 리스트에 속한 뒤 설정하는 게 맞지만, 위처럼 이미 (246)에서 리스트에 속했으므로 (260)은 불필요·유해하다.

**근거**: Qt 공식 문서의 `QListWidgetItem` 부모 생성자 동작(자동 삽입). 본 환경에 Qt 빌드 도구가 없어 런타임 재현은 못 했으나(문서 기반 확인), 코드상 명백한 중복 경로다.

**권장 수정** — 둘 중 하나로 통일:
```cpp
// 방법 A: 부모를 넘기지 않고 addItem로만
QListWidgetItem* newItem = new QListWidgetItem();   // 부모 없음
...
ui->listWidget->addItem(newItem);
ui->listWidget->setItemWidget(newItem, newWidget);

// 방법 B: 생성자에서만 삽입, addItem 제거
QListWidgetItem* newItem = new QListWidgetItem(ui->listWidget);
...
// ui->listWidget->addItem(newItem);  ← 삭제
ui->listWidget->setItemWidget(newItem, newWidget);
```

---

## 🟠 U-3. `viewToggle`를 QSlider로 구현한 AP/STATION 전환 — 발견성·정확성 문제

**위치**: `mainwindow.ui:76-110`, `mainwindow.cpp:45-46, 423-450`
```cpp
ui->viewToggle  // QSlider, maximum=1, 화면 y=-10 위치
connect(ui->viewToggle, &QSlider::valueChanged, this, &MainWindow::onViewToggleChange);
```

**문제**:
1. **잘못된 위젯 선택**: 2-상태 전환(AP↔STATION)에 연속 값 위젯인 `QSlider`(max=1)를 썼다. 의미상 `QCheckBox`/세그먼트 컨트롤/`QTabBar`가 적합하다. 슬라이더는 "값 조절"로 오인되고, 드래그 도중 중간 상태 처리가 모호하다.
2. **발견성 0**: 라벨 "AP"/"STATION"이 `x=190~290, y=0`의 16px 텍스트로만 있고, 토글 자체가 `y=-10`로 화면 상단 밖을 침범해 **무엇을 누르면 뷰가 바뀌는지 사용자가 알기 어렵다**.
3. **상태 표시 약함**: 현재 어떤 모드인지 색상(groove 색)만으로 구분 — 색각 이상 사용자나 야외 시인성에서 취약.
4. **데이터 일관성**: 뷰 전환은 `setHidden`으로 숨길 뿐(`mainwindow.cpp:441-449`), 숨겨진 항목도 계속 누적된다(U-4와 연결).

**권장 수정**: `QTabBar`(탭: AP / STATION) 또는 두 개의 `QPushButton`(checkable, 세그먼트) 또는 `QComboBox`로 교체하고, 선택 모드를 명시 텍스트+강조로 표시. 화면 밖 좌표(y=-10) 제거.

---

## 🟠 U-4. 리스트 무한 증가 — 에이징/정렬/제거 부재

**위치**: `mainwindow.cpp:180-265` (`onDaemonOutput`), `displayItem` 맵
```cpp
QMap<QString, QListWidgetItem*> displayItem;   // 한 번 추가되면 영구 보존
```

**문제**:
1. **항목이 사라지지 않는다**: BSSID/STATION이 한 번 관찰되면 `displayItem`에 영구 등록. 채널 홉핑(13채널 순회)으로 장시간 스캔하면 주변의 모든 AP/단말이 누적되어 **수백~수천 행**까지 증가, 리스트 렌더링·탐색이 느려지고 가독성이 무너진다. airodump의 핵심 UX인 **"최근 본 것 위주 + 비활성 항목 정리"** 가 없다.
2. **정렬 없음**: 신규 항목이 도착 순서대로 append되어, 신호 강한/관심 대상 AP가 화면 위로 오지 않는다. PWR(신호세기)나 최근 관측 시각 기준 정렬이 일반적이다.
3. **마지막 관측 시각/활성 표시 없음**: 더 이상 보이지 않는(범위 밖) AP가 활성처럼 남는다. "last seen" 회색 처리/타임아웃 제거가 필요.

**권장 수정**:
- 항목에 `lastSeen` 타임스탬프 저장, 주기적으로 N초(예: 30s) 이상 미관측 항목을 흐리게/제거.
- `QListWidget` 대신 정렬 가능한 모델(`QSortFilterProxyModel` + `QListView`/`QTableView`)로 전환해 PWR/시각 기준 정렬 + 필터.
- 상한(예: 최대 표시 200개) + 가상 스크롤로 성능 보호.

---

## 🟠 U-5. `updateInfo` ESSID 갱신 조건식 논리 오류

**위치**: `display.cpp:33-46`
```cpp
void display::updateInfo(const QString& essid, const QString& pwr, const QString& ch) {
    if(!essid.isEmpty() || essid != ui->lEssid->text())   // ★ 항상 참에 가까움
    {
        ui->lEssid->setText(essid);
        ...
    }
```

**문제**: 조건이 `!essid.isEmpty() || essid != current` 다. **OR**이므로, `essid`가 비어 있어도(`isEmpty()`) 우변 `essid != current`(현재 텍스트가 비어있지 않으면 참)가 성립해 **빈 문자열로 덮어쓴다**. 즉 데몬이 일시적으로 빈 ESSID(파싱 실패 등)를 보내면 기존에 표시되던 정상 SSID가 **공란으로 깜빡**일 수 있다. 의도는 "비어있지 않고, 또한 기존과 다를 때만 갱신"이므로 **AND**여야 한다.

**로직추적 결과**(진리표):

| essid | isEmpty | essid != cur | 현재식(OR) | 의도(AND) |
|---|---|---|---|---|
| "" (현재 "ABC") | T | T | **갱신됨(버그)** | 갱신 안 함 |
| "ABC" (현재 "ABC") | F | F | 갱신 안 함 | 갱신 안 함 |
| "XYZ" (현재 "ABC") | F | T | 갱신 | 갱신 |

**권장 수정**:
```cpp
if(!essid.isEmpty() && essid != ui->lEssid->text()) { ... }
```
(추가로 `setInfo`/`updateInfo`가 스타일시트 문자열을 매 갱신마다 `setStyleSheet`로 재설정하는데, 잦은 호출 시 비용이 있다. 상태가 바뀔 때만 적용하거나 `setProperty`+QSS 셀렉터로 분리 권장.)

---

## 🟡 U-6. UI 스레드 블로킹 — ANR 위험

**위치**: `mainwindow.cpp:108-110, 142, 152-153`
```cpp
QProcess p;
p.start("su", QStringList() << "-c" << cmd);
p.waitForFinished(3000);    // ★ 메인(UI) 스레드 최대 3초 정지
...
daemonProcess->waitForFinished(2000);   // stop 시 2초
p.waitForFinished(5000);                // cleanup 시 5초
```

**문제**: `onStartButton`/`onStopButton`은 UI 스레드에서 실행되는데 `waitForFinished`로 **동기 대기**한다. `su`/`nexutil`/`svc wifi`는 수 초가 걸릴 수 있어, 그 동안 **UI가 얼어붙고** 안드로이드에서 **ANR(Application Not Responding)** 대화상자가 뜰 수 있다(메인스레드 5초 차단 기준 근접/초과).

**권장 수정**: 이 명령들을 **비동기**(`QProcess::finished` 시그널 + 람다)로 처리하고, 진행 중에는 버튼 비활성화 + 스피너/상태 텍스트를 표시. 또는 별도 워커 스레드로 이동.

---

## 🟡 U-7. 공격 진행 상태의 시각적 표현 부재

**위치**: `mainwindow.cpp:386-399`(stop), `452-483`(start), 상태표시는 `statusbar->showMessage(...3000ms)`뿐
**문제**: Deauth/Auth/CSA를 시작하면 statusbar에 3초짜리 토스트만 뜨고 사라진다. **현재 어떤 타깃을, 어떤 공격을, 언제부터 수행 중인지** 지속적으로 보여주는 UI가 없다. 채널 라벨에 `[is attacking]`을 잠깐 넣지만(`mainwindow.cpp:461`) 채널 홉핑 재개/정지와 엮여 일관성이 약하다. 공격 도구 특성상 **활성 공격의 명확한 지속 표시 + 원클릭 중단**은 안전성에도 직결된다.

**권장 수정**: 상단에 "공격 중: <type> → <BSSID> (경과 t초)" 배너 + 눈에 띄는 STOP 버튼 상시 노출. 공격 대상 리스트 행에 배지(예: 빨간 점) 표시.

---

## 🟡 U-8. 하드코딩 인라인 스타일시트 산재 → 테마/다크모드 불가

**위치**: `mainwindow.cpp:42, 428-438, 461`; `display.cpp:25,30,40,44`; `display.ui`/`mainwindow.ui` 내 QSS 문자열
**문제**: 색상·폰트 크기(`color:#87CEFA`, `font-size:16pt`, groove 색 등)가 코드/`.ui`에 산재한다. (1) **다크모드/시스템 테마 대응 불가** — 흰 배경·검은 글자 고정이라 다크 테마에서 부적절. (2) 같은 스타일을 여러 곳에서 중복 정의(`display.cpp`의 16pt 두 곳, `setInfo`/`updateInfo` 중복)해 **불일치·유지보수 비용**. 

**권장 수정**: 앱 전역 **QSS 테마 파일**로 분리하고 위젯에는 `objectName`/`property` 셀렉터만 부여. 색은 팔레트(`QPalette`)·테마 토큰으로 관리. 폰트는 pt 하드코딩 대신 상대 스케일/`QFont` 통일.

---

## 🟡 U-9. 국제화(i18n) 미적용 — 문자열 하드코딩·한영 혼재

**위치**: 전반 (예: `mainwindow.cpp:93` `"선택된 디바이스 없음"`, `"Attack Start"`, `"Stop Attack"`, `display.ui` `"WIFI_NAME"` 등)
**문제**: 사용자 표시 문자열이 `tr()` 없이 리터럴로 박혀 있고 한국어/영어가 섞여 있다. 번역·로캘 대응이 불가하고 톤이 일관되지 않는다.

**권장 수정**: 모든 사용자 노출 문자열을 `tr("...")`로 감싸고 `.ts`/`.qm` 번역 리소스로 분리. UI 언어를 한 가지로 통일하거나 로캘 기반 전환.

---

## 🟢 U-10. 기타 사용성 (Low)

- **빈 상태(empty state) 없음**: 스캔 시작 후 결과가 없을 때 리스트가 그냥 비어 있어, 사용자가 "동작 중인지" 알 수 없다. "스캔 중… 발견된 AP 없음" 안내 권장.
- **정렬/필터/검색 부재**: SSID 검색, PWR 정렬, 채널 필터가 없어 AP가 많을 때 타깃 찾기 어렵다(U-4와 연결).
- **버튼 레이블 불일치**: `mainwindow.ui`에서 `startButton` 텍스트는 "Start"(대문자), `stopButton`은 "stop"(소문자). 대소문자 통일 필요. 또한 코드(`onStopButton`)와 실제 정리 로직은 한글 주석이 많아 레이블 톤과 어긋남.
- **컨텍스트 메뉴 발견성**: 공격 메뉴가 길게-누르기(TapAndHold)에만 있어, 처음 사용자는 공격 기능 존재를 모를 수 있다. 행에 명시적 ⋮(더보기) 버튼 병행 권장.
- **STATION 행의 채널 표기**: `ch = "-"`로 고정(`mainwindow.cpp:221`)되는데 `display::updateInfo`는 `ch != "0"`일 때만 갱신하므로 "-"가 들어가 `"CH -"`로 표시될 수 있다(`display.cpp:51-53` 경로와 상호작용 확인 필요).

---

## 권장 수정 우선순위

### 보안 (Security)
1. **C-1 ~ C-4**: 파서 진입부 길이 가드 + CSA 동적할당/상한검사 + radiotap에 `caplen` 전달. → root RCE/OOB 표면 차단. **최우선**.
2. **H-1 / H-2**: LD_PRELOAD 경로를 앱 전용으로 + 인터페이스명 화이트리스트 + `su` 셸 미경유 exec. 가능하면 데몬 권한 최소화.
3. **H-3 / H-4**: raw MAC→`prtMac` 문자열화, `sscanf` 반환·초기화 검증. (저비용·즉시 적용)
4. **M-2 / M-3 / M-1**: null 체크, 스레드 정리, macOS 빌드 오타/이름 통일. (안정성)
5. **M-5 + 합법성 가드/문서화**: 체크인 바이너리 제거, 타겟 BSSID 화이트리스트·실행 경고 추가.

### UI / UX
1. **U-2**: `QListWidgetItem` 이중 삽입 제거. (크래시/중복 위험, 한 줄 수정) **최우선·저비용**.
2. **U-1**: `mainwindow.ui`를 레이아웃 매니저 기반으로 전환 + `viewToggle` 화면밖 좌표 제거. → 모바일 레이아웃 정상화. **이식 목적상 필수**.
3. **U-5**: `updateInfo` 조건 `||`→`&&` 수정. (ESSID 깜빡임 버그, 한 줄)
4. **U-6**: start/stop의 `waitForFinished` 동기대기를 비동기 시그널로 전환. (ANR 방지)
5. **U-3 / U-4 / U-7**: AP/STATION 전환을 적절한 위젯으로, 리스트 에이징·정렬 도입, 공격 상태 상시 표시.
6. **U-8 / U-9 / U-10**: QSS 테마 분리, `tr()` 국제화, 빈 상태/정렬/필터 등 사용성 보완.

> **권장 착수 순서**: 보안 C-1~C-4(크래시/RCE)와 UI U-2(크래시)를 먼저 묶어 처리한 뒤, U-1(레이아웃)으로 모바일 동작을 정상화하고, 이어서 안정성(U-5/U-6)·사용성 항목을 진행.

---

## 검증 방법론 부록

본 보고서의 "실측" 표기 항목은 다음 하니스를 컴파일/실행하여 확인했다 (g++ `-std=c++20 -Wall -Wextra`):

| 하니스 | 검증 대상 | 핵심 결과 |
|---|---|---|
| `sizecheck.cpp` | 구조체 크기/오프셋 | ST_WL=24, ST_IPC_EVENT bssid@1·st_mac@7 |
| `csa_overflow.cpp` | C-2/C-3 CSA 산술 | 40B/5000B/4100B 입력에서 memcpy가 4096 초과 재현 |
| `present_walk.cpp` | C-4 presentCount | 0xFF 연쇄에서 버퍼 끝 초과 읽기(OOB=YES) |
| `insertloc.cpp` | getInsertTagLoc 엣지 | len≤0이면 0 반환(오버플로우 미방지 확인) |
| `getessid_verify.cpp` | C-5 정정 | 음수/255 입력에서 getEssid 안전 → Critical 아님으로 정정 |
| `misc_verify.cpp` | TX dummy/chkBeacon/H-3 | dummy len=12 정상, chkBeacon은 첫옥텟0x80이면 통과, MAC over-read 45B |

> 본 분석은 정적 검토 + 산술/경계 시뮬레이션에 기반한다. 실제 무선 환경에서의 PoC 패킷 주입·크래시 재현은 수행하지 않았으며, 인가된 테스트 환경에서의 동적 검증(ASan 빌드 권장: `-fsanitize=address,undefined`)을 추가로 권장한다.
