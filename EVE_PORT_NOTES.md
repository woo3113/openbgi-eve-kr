# EVE burst error A — OpenBGI 포팅 작업 노트

작업 위치: `C:\Users\woo31\openbgi` (git clone of Cytlan/openbgi)
대상 게임: `C:\Users\woo31\Downloads\EVE burst error A` (한국어 패치본)

## 대상 분석 결과

| 항목 | 값 |
|---|---|
| 엔진 | BGI / Ethornell (Buriko General Interpreter) |
| 버전 | 1.627.1 (compatibility 1.72) — exe 문자열로 확인 |
| 아카이브 | BURIKO ARC20 |
| 시스템 프로그램 | `system.arc`(ipl._bp), `sysprg.arc`(69개 `._bp`) |
| 시나리오 | a000..b035 등 (`BurikoCompiledScriptVer1.00`) |
| 한국어 패치 | 시나리오 재컴파일 + `sysgrp.arc`/`sysprg.arc`/`data02130.arc` 교체 + `dinput8.dll`(GDI32 텍스트 훅) + `EVE_BLACK.ttf` + `sjis_ext.bin` |

## 검증 진행 (Win64 빌드, MSYS2 mingw gcc 16.2)

* `make win CROSS=x86_64-w64-mingw32-` → `bin/bgi.exe` (게임은 x86이지만 엔진은 데이터만 읽으므로 x64 무방)
* 실행: `bin\bgi.exe "<게임폴더>" "Do not use mutex."`
  (Win32 백엔드는 exe가 있는 폴더를 게임폴더로 보므로 반드시 게임폴더를 인자로 준다)
* `bin\games.json`에 게임 등록:
  `{ "match": "EVEBurstErrorA", "profile": "1.640", "product": "EVEBurstErrorA" }`
* 프로필 1.616은 `81 D0` 미정의 → **1.640 사용** (게임=1.627, 1.72 호환)

### 성공한 것
- VM이 `ipl._bp`를 실행, 프로필 감지, 디스크 마커 처리, 환경 초기화, `sysprg.arc`의
  `environ._bp`/`bitmap._bp`/`title._bp`/`scrdrv._bp` 로드까지 진행
- 창이 뜸: 제목 **"EVE burst error A - OpenBGI"**

## 남은 블로커

### 1) 미구현 opcode 17개 (게임의 `._bp` 69개를 디스어셈블해 전수 확인, `--list-missing`와 일치)
```
B0(ext0): 06
C0(ext1): 06
90(gfx0): CE
91(gfx1): 31 DB
92(gfx2): 9B F5 F6
A0(snd) : 1C
80(sys) : CF
81(sys81): D0 D1 D2 D3 D4 D5
```
- 첫 실패: `scrdrv._bp`의 `$81D0`. 호출부로 보아 (핸들생성, 해제, 사용...) 계열 핸들 API.
- 의미는 원본 `EVE_A.exe`(또는 아무 1.640+ BGI exe) RE 필요 (Ghidra 12.1.4 설치됨).
- 구현: `src/vm/ops_*.c` 핸들러 + `src/asm/opnames.c` 이름 + (필요시) `Vm_Optable*Init`.

### 2) `BGI.gdb` = "BURIKO GDB 3.00" (1.616+ 새 포맷)
- 디코드 결과 앞 16바이트가 `BURIKO GDB 3.00`, 그 뒤 u32 = 디코드 크기(2313389).
- OpenBGI `src/sysobj/database.c`는 옛 고정 레이아웃만 구현 → 오프셋 오독 → 크래시.
- 임시 조치: 3.00 매직이면 "DB 없음"으로 처리(크래시 방지, 새 게임 시작 상태). → 제대로 된 3.00 파서 구현 권장.

### 3) 한국어 표시
- 패치는 `dinput8.dll`로 GDI 텍스트를 훅해 `EVE_BLACK.ttf`로 렌더 + `sjis_ext.bin` 매핑.
- OpenBGI는 FreeType + 자체 텍스트엔진 → 훅 없음. 동일 매핑+폰트를 엔진에 이식 필요.

### 4) 오디오
- Vorbis(BW codec 3) 미포함 → 무음. `src/snd/stb_vorbis.c` 추가로 해결.

## 로컬 수정 (upstream 대비)
- `src/sysobj/database.c`: GDB 3.00 매직 가드 + 레코드 범위 검증(크래시 방지), 디버그 출력
- `src/vm/ops_sys.c`: `Opcode_Sys_GdbLoad_472`에 디버그 출력
- `src/os/win32/dialogs.c`: `OS_MessageBox`가 `BGI_MSGBOX_STDERR=1`이면 팝업 대신 stderr 출력
- `bin/games.json`: EVE 등록
- `scripts/`: build_win.sh, rebuild_win.sh, build_tools_win.sh, screenshot.ps1, sdc_check.py, gdb_dump.py

## 다음 단계 후보
1. 미구현 opcode 17개 구현(RE) → 데스크톱에서 게임 완주 가능 여부 확인
2. Android 백엔드(`src/os/android/`, SDL2 + OpenGL ES + AAudio/OpenSL + FreeType) + 게임패드 매핑
3. GDB 3.00 파서 구현
4. 한국어 매핑/폰트 이식
5. Vorbis 추가

## 한글 인코딩 조사 (2026-10)
* `sjis_ext.bin` = **UTF-16LE 한글 문자 목록 1343자** = 커스텀 폰트(`EVE_BLACK.ttf`)의 글리프 순서.
  예: 첫 30자 = "아마기코지로나한테무슨원이라도있는거야후우몸의사소돌왔다벌써"
* 시나리오/스크립트는 **확장 SJIS 코드(2바이트, lead 0x81..0xFB)** 로 저장되고, 이 코드가
  `sjis_ext` 인덱스로 매핑됨. 단순 "user-area 0xF040 + i" 가정은 부분적으로만 맞음(일부만 한글로 복원됨)
  → 정확한 매핑식은 `dinput8.dll`(GDI 훅) 디스어셈블로 확정 필요.
* OpenBGI 이식 시: 엔진 텍스트 렌더러에서 확장 SJIS 코드 → 한글 코드포인트 → `EVE_BLACK.ttf` 글리프로
  렌더하도록 이식. (관련 스크립트: scripts/korean_scan.py, korean_map.py, korean_findings.txt)

## RE 작업 (Ghidra)
* 원본 `EVE_A.exe` = 스텁 PE + 내부 엔진 PE@0x1F000 (내부 PE == `EVE_A_실행기.exe`, 0x11AC00, imagebase 0x400000)
* Ghidra 프로젝트: `C:\Users\woo31\openbgi\ghidraproj` (program `EVE_engine.exe` = 추출 엔진)
* 스크립트: `ghidra_scripts/find_dispatch.java` (문자열 `$81%02X`/`$80%02X` 참조 → 디스패처 함수 → 디컴파일)

## 미구현 opcode 진행 (스텁 브링업)
* 정적 RE는 `$81%02X`가 간접 참조라 xref 0, 핸들러 점프테이블 없음 → **스텁 방식**으로 전환.
* `src/vm/ops_base.c` `Opcode_NotImplemented`에 `BGI_STUB` 경로:
  - `BGI_STUB=1`이면 알려진 17개를 스텁(기본: pop/push 는 디컴파일러 추정치).
  - `BGI_STUB_<FAM>_<OP>` = "pops,pushes" 로 런타임 오버라이드.
* 진행: `81 D0`(스텁) 통과 → `scrdrv._bp` 0x92의 `90 65`(filter_set)에서
  **우선순위 0xfd00**을 게임이 정상 전달 → `CheckPriority`(0..4095)가 과도하게 거부.
  → `src/vm/checks.c` `CheckPriority` 범위를 0..65535로 완화(EVE가 0xfd00 사용).
* 다음: 계속 실행해 다음 오류/미구현 지점 확인 → 스텁 효과 조정 반복.

## 로컬 수정 추가
- `src/vm/ops_base.c`: BGI_STUB 스텁 경로
- `src/vm/checks.c`: CheckPriority 0..65535

## Android 백엔드 (진행 중)
* 툴체인: cmdline-tools 설치 → **디스크 부족(C: 1GB)** 으로 SDK를 `E:\Android\Sdk`로 이동, 거기에 platform-tools/platforms;android-34/build-tools;34.0.0/ndk;26.1.10909125 설치.
* SDL2 2.30.9 소스: `C:\Users\woo31\Android\SDL2-2.30.9`.
* 프로젝트: `C:\Users\woo31\openbgi\android-app` (SDL2 `android-project` 템플릿 복사, `app/jni/SDL` → SDL2 소스 junction).
* 백엔드: `src/os/android/`
  - posix에서 복사: `files.c paths.c time.c thread.c registry.c machine.c process.c` (POSIX 재사용)
  - 새로 작성: `sjis.c` (+생성한 `cp932_table.h`, CP932↔Unicode, iconv 불필요)
  - 작성 예정: SDL2 창/입력/프레젠트, `font.c`(stb_truetype), `audio.c`(SDL2 audio), 다이얼로그/메시지박스, SDL_main 엔트리
  - 게임 데이터는 용량(수 GB) 때문에 APK 번들 불가 → 외부 저장소 경로에서 읽도록
* 게임패드: SDL2 `SDL_GameController` → 엔진 가상키/마우스 매핑.

### Android 구현 완료(1차)
* 툴체인 최종: **C:에 113GB 확보 후 `C:\Users\woo31\Android\Sdk`에 설치 완료**
  (platform-tools / platforms;android-34 / build-tools;34.0.0 / **ndk;26.1.10909125**)
* SDL2 2.30.9: `C:\Users\woo31\Android\SDL2-2.30.9` (junction: `android-app/app/jni/SDL`)
* Gradle: AGP 8.5.2 + Gradle 8.9 (JDK 21 호환), `local.properties`(sdk.dir), `GRADLE_USER_HOME=C:\Users\woo31\Android\gradle`
* 빌드 스크립트: `C:\Users\woo31\Android\build_apk.ps1`
* `src/os/android/`: files/paths/time/thread/registry/machine/process(POSIX 재사용), sjis(+cp932_table), window(SDL2 창/입력/**게임패드**), font(stb_truetype), audio(SDL2), child/edit(최소), stubs(MCI/무비/쉘/다이얼로그), entry(SDL_main), android_internal.h
* 게임 데이터 위치: `BGI_GAMEDIR` 또는 앱 외부저장소(`SDL_AndroidGetExternalStoragePath`), 설정은 내부저장소
* APK: net.openbgi.eve, arm64-v8a, minSdk 21

### APK 빌드 성공 (2026-10)
* `gradlew assembleDebug` → **BUILD SUCCESSFUL**
* 산출물: `C:\Users\woo31\openbgi\android-app\app\build\outputs\apk\debug\app-debug.apk`
  (arm64-v8a, `lib/arm64-v8a/libmain.so` + `libSDL2.so`)
* 수정한 컴파일 오류: `OS_PumpMessages` 반환형(int), `OsFont_t` typedef 중복(os.h 정의 재사용)
* 실기 테스트는 기기 필요(개발 PC에 기기/에뮬레이터 없음). 설치: `adb install -r app-debug.apk`,
  게임 데이터는 `/sdcard/Android/data/net.openbgi.eve/files/` 에 복사(또는 `BGI_GAMEDIR`).

### 실기 구동 성공 (2026-10-10, MANGMI Pocket Max)
* `adb install` 후 실행 → **타이틀 화면이 한글로 정상 표시**(수사시작/수사재개/환경설정/수사종료)
* 기기: MANGMI Pocket Max (arm64), 게임 데이터 2.5GB/173파일 푸시(68초)
* 실기에서 해결한 문제:
  1. x86 전용 CPU 검사(CPUID/CMOV/MMX) → `src/sys/engine.c` 가드를 `__i386__/__x86_64__`로 (ARM 스킵)
  2. 프로필 미인식(`$8052` 미정의) → 게임 폴더에 `games.json` 배치(1.640 인식, 게임 디렉터리 우선 탐색)
  3. 미구현 opcode(`$81D0`) → `src/os/android/entry.c`에서 `BGI_STUB=1` 기본 활성화
  4. 외부저장소 하위 폴더 자동 인식(`OS_Init`에서 `system.arc` 있는 하위폴더 선택)
* 실행/배포 스크립트: `C:\Users\woo31\Android\deploy.ps1`, `run_app.ps1`
* 남은 것: 패드 실조작 검증, 화면비(레터박스), 인게임 한글 대사, opcode 시맨틱, 오디오, GDB 3.00

### 화면/전체화면 수정
* 게임 프레임 = **1280x720 (16:9)** (ipl._bp의 `set_window_size` 계산에서 확인). 화면 1920x1080(16:9)와 동일 → 꽉 채움.
* 초기 버그: 표시 버퍼가 800x600(4:3)에 고정 → 크롭/왜곡. → **`EnsureSurface()`로 실제 프레임 크기(1280x720)를 따라가도록** 수정.
* 몰입형 전체화면: `OS_WindowCreate`에서 `SDL_SetWindowFullscreen(FULLSCREEN)`, `OS_WindowSetWindowed`는 무시.
* 표시: `Present()`가 원본 비율 유지로 화면에 fit(16:9↔16:9면 여백/크롭 없음).
* 로그 태그 `BGI`(`__android_log_print`)로 `picture size 1280x720` 확인.

### 입력 좌표/브링업 수정 (기기)
* 터치/마우스 좌표 어긋남 수정: 창 좌표 → **그림 좌표(1280x720)** 변환(`WinToPic*`/`PicToWin*`, 레터박스 보정). 마우스 이동/클릭, 커서 조회/설정, 스틱 이동에 적용. 터치 핸들러 제거(SDL이 터치→마우스 합성).
* 엔진 stderr을 `files/bgi_log.txt`로 저장(`entry.c` freopen; `run-as net.openbgi.eve cat files/bgi_log.txt`). `OS_MessageBox`가 항상 메시지를 로그에 남김.
* 스텁에 push 값(`value`) 추가. `81 D4/D5`는 **길이 출력 파라미터에 0 기록**(직렬화 루프의 잘못된 카운트 방지).
* `src/vm/ops_sys.c` `Opcode_Sys_Gfree`(80 21): **`p==0`이면 통과**(원본은 null free 허용; `userdata._bp`가 호출) — "illegal address $0 for freeing global memory" 해결.

### 한글 텍스트 매핑 (dinput8.dll 훅 RE) — 해결
* 패치 방식: 시나리오 텍스트를 **trail 바이트가 0x01~0x3F(비표준 SJIS)** 인 2바이트 코드로 저장 →
  `dinput8.dll`이 `MultiByteToWideChar`/`TextOutA`/`CreateFontA`/`GetGlyphOutlineA`를 후킹해 처리.
  `EVE_BLACK.ttf`(표준 유니코드 위치에 한글 11172자)와 `sjis_ext.bin`(UTF-16LE 한글 1343자)을 로드.
* 매핑(훅 코드 0x10001834에서 복원, `sjis_ext` 인덱스):
  `i1 = (L<0xA0)?64:0; edx = adj(T); edi=(i1+L)*0x3A + edx; idx = edi + ~last - 0x2BBA`
  (`adj`: T-1+(T<0x2D), 다시 -1+(<0x21), 그리고 >=0x0E/0x0B/0x0A 보정; `last=(>=0x0A)`)
  → `sjis_ext[idx]`가 한글 코드포인트.
* 검증: `a000`의 `81 07 81 08 81 0B …` = **"나한테"**, 전체가 "코지로 나한테 무슨…"(sjis_ext 첫 문장과 일치).
* 이식: `src/os/android/sjis.c`에서 커스텀 코드(trail 0x01~0x3F)면 `sjis_ext` 조회, 아니면 CP932.
  폰트는 시스템 CJK(한글 포함) 사용 → 엔진 자체 텍스트 렌더러가 한글 표시.

### 실기에서 한글 대사 정상 확인 (2026-10-10)
* 안드로이드에서 **"뭐야, 잠들어 버렸던 모양이군……"** / **"나쁘지 않은 날씨, 평화로운 한때……"**, 이름 **코지로** — PC판과 동일.
* 터치 탭으로 다음 대사 진행도 확인.
* 핵심 수정: 매핑 상수 `-0x2B9A` → **`-0x2BBA`(+32)** (12개 실제 대사 줄로 검증).

### BGM (Vorbis)
* `src/snd/stb_vorbis.c` 추가, Android CMake에 `-DBGI_USE_STB_VORBIS` → BW codec 3(Ogg Vorbis) 재생. 실기에서 **BGM 재생 확인**.

### 오프닝 동영상 (.mpg) — 미구현(엔진 한계)
* OpenBGI는 동영상 재생을 의도적으로 지원하지 않음: `92 F0 movie_play_arc`는 길이 0,
  `92 F2 video_texture_attach`는 실패(1), `92 F4 video_texture_control`은 1 반환.
* `moviemngr._bp`가 `gfx2.0xf5/bmp_get_pixel`로 영상 비트맵을 폴링하는 구조 → 92 F5/F6 의미 구현 필요.
* 재생하려면 MPEG-1 디코더(예: pl_mpeg) + 엔진 영상 텍스처/비트맵/오디오 통합 = 큰 작업.

### 동영상 구현 (pl_mpeg)
* `src/gfx/pl_mpeg.h` 추가, `src/gfx/movieplayer.c`에 플레이어(비디오 텍스처 → 비트맵 슬롯).
* `92 F2 video_texture_attach` → `Movie_Attach`(아카이브에서 .mpg 로드, 슬롯 생성),
  `92 F4 video_texture_control` → `Movie_Control`(0=종료여부, 1=재생/재개, 2=일시정지, 3=정지),
  `92 F5` → `Movie_Status`(현재 재생 ms), `92 F0 movie_play_arc` → `Movie_PlayArc`.
* 프레임 펌프: `Vm_FrameServices`에서 `Movie_Pump()`; 종료 시 `Movie_Shutdown()`.
* 오디오는 미연결(비디오만). 필요 시 `Sound_MovieMix` 형태로 믹서 연결 예정.

### 오프닝 동영상 경로 확정 (로그)
* 게임은 `92 F0 movie_play_arc("data02xxx.arc", "<name>.mpg", 0,0,1280,720)`로 재생.
  `수사시작` 후 로그에 `[MOVIE] PlayArc data02xxx.arc/1203a.mpg` 확인.
* 원본은 DirectShow 자식 윈도우; 이식판은 **백버퍼에 디코드 → `Gfx_UpdateScaled` → `Window_Repaint`** 로 직접 표시.
  (`display_enable(0)` 동안 일반 present가 꺼져 있으므로 직접 출력해야 함)
* **비동기**: `movie_play_arc`는 시작만 하고 반환, 스크립트가 `90 F2 movie_playing`으로 대기,
  `90 F1 movie_stop`으로 중단(`Movie_Stop`/`Movie_IsPlaying`에 `Movie_Arc*` 연결).
* `92 F2` attach(비디오 텍스처)도 구현돼 있으나, 오프닝은 `92 F0` 경로. attach는 그룹/배경 스크립트용.

### 동영상 디코드 교훈 (pl_mpeg)
* `plm_t->video_buffer`는 **디먹싱 스트림 버퍼**다(RGB 아님). RGB는 `plm_set_video_decode_callback` + `plm_frame_to_rgb(frame,dest,stride)`로 받아야 한다.
  (이걸 잘못 써서 `w*h*3`로 읽어 네이티브 힙 오버런 → SIGSEGV였다.)
* 오디오: `plm_set_audio_enabled(plm,1)` + `plm_set_audio_decode_callback`. 콜백은 float 인터리브 `plm_samples_t`.
* 링버퍼(≈1.5s)에 int16 스테레오로 넣고, `src/snd/wm/voice.c`의 `Mixer_Fill`에서 `Movie_MixAudio(acc,n)`로 32비트 acc에 더함.
  재생 레이트가 44100과 다르면 16.16 step으로 리샘플(`voice.c`와 동일 방식). 믹스 레이트 `WM_MIX_RATE=44100`.

### 동영상 경로 전체 (모두 처리)
* 컷신: `92 F0 movie_play_arc` — `scrsnd.sub_1da9`(토큰 447)가 유일 호출부. → `Movie_PlayArc`. **동작 확인.**
* 오마케(보너스) 클립: `90 F0 movie_play(path)` (omakecg/omakescene). 안드로이드는 `OS_Movie*` 스텁이라
  `Movie_Play`의 실패 분기에서 `Movie_PlayFile`(루스 파일/`data01xxx`·`data02xxx` 멤버)로 대체 재생.
* 비트맵 시퀀스(`movPlayBm`, `.bm`): `gfx2.seq_load`/`gfx0.seq_decode` → OpenBGI `BmSeq`(BF_Movie type 0) 기존 구현.
* 비디오 텍스처(`92 F2 video_texture_attach`): 그룹/버스트샷 애니용. `Movie_Attach`로 구현(이름 있는 호출 시 동작).
* 확인 방법: 실행 로그 `bgi_log.txt`의 `[MOVIE] arc ...` / `[MOVIE] file ...` 라인. 안 나오면 그 opcode를 추적.

### 시작 로고/부가 화면 (PC와 동일)
* `title._bp`의 `sub_062c` → `data01xxx.arc`의 **`makerlogo`**(컴파일 시나리오, 실제 존재) 를 `userdata._bp`(4,…)+`scrdrv._bp`로 실행.
  이 씬이 **`eldia.mpg`(EL DIA 로고 영상)** 를 비디오 텍스처로 재생. 이어 `주의` 화면 → `op_demo.mpg` → 타이틀.
* **버그였던 것**: `92 F2 video_texture_attach` 인자 순서. 실제 푸시 순서는 `(bmp, arc, name, a, b)`인데
  문서(`name, arc, a, b, bmp`)를 믿어 `bmp`↔`name`이 뒤바뀌어 이름이 빈 값 → attach 실패 → `bg_blend(128)` "비트맵 128 없음" 에러.
  핸들러 팝 순서를 `b, a, name, arc, bmp`로 고침(`src/vm/ops_gfx2.c`).
* **가상 키보드 자동 표시**: 창 생성 시 `SDL_StartTextInput()`을 무조건 호출한 것이 원인 → 제거(편집 시 `OS_EditShow`가 켬).

### 추가 구현 (인게임)
* **`array_fill`(0x64)/`array_fill`(0x65)** — OpenBGI에 없어 `scrslct2._bp`("대화하기" 선택지)에서
  "Opcode $64 ... not implemented" 에러가 났음 → `src/vm/ops_base.c`에 구현·등록.
  (`array_fill(dst,elemSize,count,value)`, `array_find(src,elemSize,count,val)→index`)
* 참고: **main 패밀리(0x7F 미만)만 하드 에러**이고, 80~92 등은 `BGI_STUB`로 자동 처리됨.
  main 미구현은 수학/벡터(`fixmul/vadd/vec_len` 등)와 배열뿐 → 배열만 게임에서 사용.
* 게임패드 매핑 변경: **A=Enter, X=좌클릭**, B=우클릭, Y=Ctrl(스킵), BACK/START=ESC, L/R=휠, 스틱=커서.

* 이 게임은 **시스템(OS) 화살표 커서**를 씀 → 안드로이드 SDL은 시스템 커서가 안 보임.
  → `src/os/android/window.c`의 `Present()`에서 **소프트웨어 화살표 커서**를 그림(`DrawCursor`), 3초 무입력 시 자동 숨김.
* **`SDL_WarpMouseInWindow`가 안드로이드에서 동작하지 않음** → 백엔드가 **자체 커서 좌표**(`gMouseX/gMouseY`, 그림 좌표)를 들고
  `OS_CursorGetPos/SetPos`로 노출. 스틱 이동이 이 좌표를 갱신 → 커서가 움직이고 게임의 마우스 호버도 반영됨.
* 게임패드(`AIR X Controller`, SDL 컨트롤러로 인식): 버튼은 SDL 표준 인덱스 —
  `0=A`(좌클릭), `1=B`(우클릭), `2=X`(ESC), `3=Y`(Ctrl 스킵), `4=BACK`(ESC), `6=START`(Enter), `9/10=L/R`(휠).
  왼쪽 스틱=커서(`PadAxis`, 속도 6px/프레임). 타이틀 등 마우스 메뉴는 **스틱으로 커서 이동 + A/좌클릭**.
* **이 기기의 D-pad는 SDL로 안 옴**(버튼 11~14/HAT/축 어느 것으로도 이벤트 없음) → 커서는 스틱으로 조작.
  (혹시 HAT으로 오는 기기를 위해 `SDL_JoystickGetHat` 폴링도 넣어둠 → 방향키+커서.)
* 게임 키(엔진 기본): 마우스(좌/우/휠), Enter=결정, Space=취소, 방향키=상하좌우, Tab, Ctrl=스킵.

### 상태
* 오프닝(1203a.mpg, 12.96s) 영상+소리 재생, 크래시 없음. 한글 대사/백그라운드/UI 정상.
* 시작 로고(eldia.mpg/op_demo.mpg) 로드·재생, 타이틀 20초 대기 어트랙트(`TitleDemo`→`eve_r_op.mpg`) **정상 애니메이션**.
* **동영상 텍스처(92 F2)는 게임이 present를 안 하므로 직접 present**해야 애니메이션됨
  (`PresentSlotFrame`, 전체화면 텍스처만; 작은 버스트샷은 게임이 합성). 원작은 DirectShow가 직접 렌더.
* `91 F1 video_texture_state` 인자 순서 수정(`&out, bmp`), `91 F2/F6` release → `Movie_Release`로 슬롯 회수.
* 동영상 텍스처 오디오도 켬(공용 링버퍼). **검증 완료(소리 나옴)**.

## 타이틀 "수사 재개" 겹침 조사

* 증상: 타이틀에서 **수사 재개**로 로드하면 게임 UI/대사 위에 **타이틀 키비주얼(캐스트 일러스트)** 잔상이 남음.
  단, **새 게임 → (인게임) 로드**는 정상. (기기에 진단 빌드 재설치 후 재현 시엔 정상 로드됨 — 간헐적.)
* 원인 분석:
  * `syswnd._bp`의 `main()`이 로드 UI 배경으로 **현재 화면(타이틀)을 `gfx0.snapshot(3778)`** 한 뒤
    **전체화면 스프라이트(`0x21c88`)** 로 만들어 표시. 하위 창(`usdtwnd`/`cnfgwnd*`/`keycnfgwnd`)이 페이드아웃 후
    `sprite_delete` + `bmp_free(3778)`로 정리하고, `scrdrv._bp`(1163)도 잔여 시 삭제하도록 되어 있음.
  * 엔진 `Mgr_DeleteFromTable`(`src/gfx/mgr/helpers.c`)이 **`IsVisible(o)`일 때만** 화면을 다시 그림.
    `obj_fade_ex`로 페이드아웃된 창/스프라이트는 invisible → 삭제해도 그 영역을 다시 그리지 않아 **잔상**이 남음
    (윈도우 삭제는 `invalidateAll=0`).
  * 컴포지터는 배경 객체(`cls=0`)가 `Bmp_Clear`로 화면을 지우므로 별도 클리어 불필요(밴드 클리어 시도는 되돌림).
* 수정: `Mgr_DeleteFromTable`에서 **invisible이어도 자신의 footprint를 invalidate** → 삭제된 영역을 다시 그림(배경이 클리어).
* 진단용 로그(`[CMP]`/`[WIN]`)는 제거함.
* 참고: `assets/EVE_off`는 **이름만 바뀐 동일한 게임 데이터(2.54GB, 163파일)**였음 → 디버그/릴리스 모두 자립형과 동일.


















