# Windows용 Linthesia

Windows 10 1903 이상 또는 Windows 11의 64비트 PC용입니다.

## 실행

배포 ZIP을 **모두 압축 해제**한 뒤 `Linthesia.exe`를 더블 클릭하세요.
DLL 파일은 `dll` 폴더에 모여 있습니다. `graphics`, `music`, `licenses` 폴더는 실행 파일 옆에 있습니다.
사용 설명서와 프로그램 라이선스는 `manual/README-ko.md`, `manual/COPYING`에 있습니다.
실행 파일과 폴더들을 함께 유지하세요.
실행할 PC에는 MSYS2나 개발 도구를 설치할 필요가 없습니다.

프로그램을 켜면 기본 곡 **Eve ／ 夜は仄か**를 자동으로 불러옵니다.
Enter → Enter를 누르면 트랙 선택을 거쳐 재생합니다.
다른 곡을 열려면 Esc로 곡 목록에 돌아가 선택하세요.
Space는 일시 정지/재개, Esc는 이전 화면입니다.
기본 MIDI 출력은 Windows의 MIDI Mapper입니다. USB MIDI 키보드를 연결한 뒤
Input에서 장치를 선택할 수 있습니다. 출력 장치가 없거나 소리가 나지 않으면
Output에서 Microsoft GS Wavetable Synth 또는 연결한 MIDI 음원을 선택하세요.

다른 곡은 `music` 폴더에 `.mid` 또는 `.midi` 파일로 넣을 수 있습니다.
명령 프롬프트에서도 직접 열 수 있습니다.

```bat
Linthesia.exe -w -f "C:\Music\곡.mid"
Linthesia.exe --lib-path "D:\MIDI"
Linthesia.exe --reset-lib-path
```

설정과 곡 데이터베이스는 `%APPDATA%\Linthesia`에 저장합니다.
프로그램 폴더 전체를 다른 위치로 옮겨도 그래픽과 기본 곡을 찾습니다.

## 소스에서 EXE 만들기

1. [MSYS2](https://www.msys2.org/)를 설치합니다. 기본 설치 경로는 `C:\msys64`입니다.
2. **MSYS2 UCRT64** 터미널을 열고 업데이트합니다. 터미널을 닫으라는 안내가
   나오면 닫았다가 다시 열고 같은 명령을 한 번 더 실행합니다.

   ```sh
   pacman -Syu
   ```

3. 필요한 64비트 UCRT 패키지를 설치합니다.

   ```sh
   pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-SDL2 mingw-w64-ucrt-x86_64-SDL2_image mingw-w64-ucrt-x86_64-SDL2_ttf mingw-w64-ucrt-x86_64-sqlite3
   ```

4. 탐색기에서 이 소스 폴더의 `build-windows.cmd`를 더블 클릭합니다.
   또는 PowerShell에서 다음 명령을 실행합니다.

   ```powershell
   powershell -NoProfile -ExecutionPolicy Bypass -File .\build-windows.ps1
   ```

   MSYS2를 다른 경로에 설치했다면 다음처럼 지정합니다.

   ```powershell
   .\build-windows.ps1 -MsysRoot 'D:\Tools\msys64'
   ```

완료되면 다음 결과가 생깁니다.

- `dist/windows-x64/Linthesia/Linthesia.exe`: 바로 실행할 파일
- `dist/windows-x64/Linthesia-windows-x64.zip`: 다른 Windows PC로 옮길 배포 파일

빌드 스크립트가 필요한 DLL을 재귀적으로 찾아 함께 넣고, 실행에 필요한
DLL은 `dll`, 설명서와 프로그램 라이선스는 `manual`에 복사합니다.
그래픽·폰트·곡·라이브러리 라이선스는 각각 `graphics`, `music`, `licenses`에 있습니다.
프로그램을 옮길 때는 실행 파일과 폴더들을 함께 옮기세요.
`build-windows`와 `dist/windows-x64/Linthesia`는 생성 결과이므로 빌드 때 갱신됩니다.

GitHub에 소스를 올린 경우 **Actions → Windows executable**에서도 빌드할 수 있습니다.
완료된 실행의 Artifacts에서 `Linthesia-windows-x64`를 내려받으세요.

## macOS/Linux에서 Windows용으로 빌드

`build-windows-cross.sh`로도 같은 배포 폴더와 ZIP을 만들 수 있습니다.
Zig 0.17.0과 SDL2 2.32.10, SDL2_ttf 2.24.0, SDL2_image 2.8.12의
MinGW 개발 패키지, SQLite 3.54.1 amalgamation이 필요합니다.
각 공식 다운로드 주소는 스크립트 첫 부분에 적혀 있습니다.
의존성 압축 파일들을 한 폴더 아래에 원래 폴더 이름대로 압축 해제한 뒤 실행합니다.

```sh
ZIG="/절대/경로/zig" LINTHESIA_WINDOWS_DEPS="/절대/경로/deps" bash build-windows-cross.sh
```

이 방식은 Windows EXE를 생성하며, 실행 및 MIDI 하드웨어 점검은 Windows에서 해야 합니다.

## 확인 범위

자동 빌드는 Windows에서 컴파일, 한글 경로의 MIDI 파일 읽기와 SQLite 저장,
PC 키보드 MIDI 입력 큐, DLL 포함 여부, `--help`·`--version` 실행과 잘못된
인자 처리를 검사합니다. 그래픽 표시와 실제 MIDI 장치의 입력·소리는
실제 Windows PC에서 확인해야 합니다. 이 프로그램은 OpenGL 드라이버가 필요합니다.

원본 및 수정 프로그램의 라이선스는 GPL-2.0입니다. 배포 ZIP의 `manual/COPYING`을 참고하세요.
함께 제공한 라이브러리의 라이선스는 `licenses` 폴더에 있습니다.

빌드 환경 참고: [MSYS2 UCRT64 환경](https://www.msys2.org/docs/environments/),
[MSYS2 패키지 관리](https://www.msys2.org/docs/package-management/).
