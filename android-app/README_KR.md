# EVE burst error A — Android (OpenBGI) 포트

OpenBGI(EVE burst error A의 원작 엔진 "BGI" 재구현)를 **Android로 이식**한 빌드입니다.
게임패드 조작, **동영상 재생**, 소프트웨어 커서 자동 숨김, **게임 데이터 포함 자립형 APK**를 지원합니다.

## 특징

- **게임패드 조작** (최종 매핑)

  | 패드 | 동작 |
  |---|---|
  | A | Enter(결정) |
  | X | 마우스 좌클릭 |
  | B | 마우스 우클릭 |
  | Y | Ctrl(스킵) |
  | BACK / START | Escape(메뉴/취소) |
  | L / R 숄더 | 휠 스크롤 |
  | 왼쪽 스틱 | 마우스 커서 이동 |

  터치: 화면 탭 = 좌클릭, 스와이프 = 커서 이동.
  (일부 기기의 D-pad는 SDL 컨트롤러 이벤트로 전달되지 않아 커서는 스틱으로 조작합니다.)

- **동영상**: `pl_mpeg` 기반 플레이어(`src/gfx/movieplayer.c`)로 컷신·시작 로고·타이틀 어트랙트·오마케
  영상과 오디오를 재생합니다. 전체화면 동영상 텍스처는 직접 present 합니다.
- **커서**: Android에는 시스템 커서가 없어 백엔드가 자체 좌표(`gMouseX/gMouseY`)로 소프트웨어 화살표를
  그립니다. 무입력 시 자동으로 숨겨집니다(`src/os/android/window.c`).
- **한글**: 게임 데이터 자체가 한국어로 패치되어 있어 본문/메뉴 한글이 표시됩니다.

## 자립형 APK (게임 데이터 포함)

게임 데이터를 `assets/EVE/`에 넣고, 첫 실행 시 `getExternalFilesDir()/"EVE burst error A"`로 추출합니다
(`MainActivity.java`). 추출에는 수 분과 약 5GB의 여유 공간이 필요합니다.

```
# 1) assets 배치
android-app/app/src/main/assets/EVE/        # 게임 데이터(약 2.4GB, 163개 파일). git에는 올리지 않음

# 2) 릴리스 빌드 (debug keystore로 서명, 업데이트 설치 가능)
cd android-app
./gradlew.bat assembleRelease
# -> app/build/outputs/apk/release/app-release.apk (약 2.54GB)

# 3) 설치
adb install -r app/build/outputs/apk/release/app-release.apk
```

엔진만 확인하려면 `assets/EVE` 없이 빌드하고, 게임 폴더를 기기의
`/sdcard/Android/data/net.openbgi.eve/files/EVE burst error A/`에 직접 복사해도 됩니다.

## 요구 / 서드파티

- Android SDK: platform-tools, platforms;android-34, build-tools;34.0.0, **ndk;26.1.10909125**
- **SDL2 2.30.9** 소스: `android-app/app/jni/SDL/`에 배치 (git에서는 제외). `app/jni/CMakeLists.txt`가
  `add_subdirectory(SDL)`로 포함합니다.
- JDK 21, AGP 8.5.2 / Gradle 8.9

## 한계

- 엔진 `main` 패밀리(0x7F 미만) 미구현 opcode는 하드 에러이며, 그 외 패밀리(80~92)는 자동 스텁 처리됩니다.
  게임이 실제로 사용하는 미구현은 수학/벡터/배열 계열뿐이라 `array_fill`/`array_find`를 구현해 해결했습니다.
- 동영상은 `pl_mpeg`(MPEG-1) 기반이라 원작의 DirectShow 계열 포맷에 의존하지 않습니다.
