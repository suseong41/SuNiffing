**한국어** | [English](README.en.md)

# SuNiffing

Galaxy S10의 nexmon 모니터 모드로 주변 WiFi를 스캔하고,
인가된 환경에서 보안 점검(deauth·CSA 등)을 수행하는 Qt 앱.

> ⚠️ 본인 소유이거나 **명시적으로 허가받은** 네트워크에서만 사용하십시오.
> 무단 사용은 불법이며 그 책임은 전적으로 사용자에게 있습니다.

---

### 준비물

* Galaxy s10 series (s10 lite는 제외)
* root 권한
* SELinux 비활성화(permissive)

---

### 기능

* 스캔 — 주변 AP / STATION 실시간 탐지
  * 2.4GHz + 5GHz
  * 지원 채널 자동 탐색
  * ESSID · BSSID · 채널 · 신호세기(dBm) 표시
* 공격 (인가된 환경에서 점검)
  * Deauth — 대상 STATION 연결 해제
  * Auth — 인증 요청 플러딩
  * CSA — Channel Switch Announcement 주입

---

### 사용 방법

1. 준비물(root · SELinux permissive)을 갖춘 S10 준비
2. 앱 설치 후 실행 → 무선 인터페이스 선택
3. Start → 스캔 대역 선택(2.4 / 5 / Dual)
4. 목록에서 AP/STATION 확인
5. 대상을 길게 눌러 메뉴 → 공격 유형/파라미터 선택 후 실행
6. Stop → 종료 (WiFi 복구)

---

### 빌드

* Qt 6.10.2 · Android arm64(앱) + 데몬(`android/assets/suseong`)

---

### 참고

* [s10 램디스크 문제](https://m.blog.naver.com/gorhanhee/224025021274)
* [s10 nexmon issue](https://github.com/seemoo-lab/nexmon/issues/631)
* [SELinux 비활성화](https://github.com/evdenis/selinux_permissive)
