Здесь вся срань которая НЕ относится к использованию проекта, а к тому как он работает и как его собрать самому. Будем держать README чистой!

## Эндпоинт

`GET https://anon.fm/answers.js` — JSON-массив из ~50 последних кукареков.
Каждый — массив из 7 строк:

| # | поле                                                |
|---|-----------------------------------------------------|
| 0 | `"0"` пара слушатель→диджей, `"1"` — объявление DJ  |
| 1 | ник слушателя или `"!"`                             |
| 2 | HTML сообщения слушателя                            |
| 3 | `<span class="timestamp">HH:MM:SS.MMMM</span>`      |
| 4 | ник диджея                                          |
| 5 | HTML ответа диджея                                  |
| 6 | Сгенеренное ФИО диджея для тех кто любит старые справочники        |

Документации нет, эндпоинт подсмотрен в питон-скрипте `kekejucica.py`. Формат
там устаревший (6 полей), реальный сейчас — 7. Питон-скрипт расщепляет ФИО
из 5-го поля регуляркой по `\n\nС уважением, ` — мы так не умеем, мы
культурные.

## Структура

```
src/
├── main.c         # CLI, главный цикл
├── http.{c,h}     # libcurl GET с вшитым CA
├── parse.{c,h}    # jsmn → afm_entry_t[]
├── html.{c,h}     # decode HTML entities, strip tags
├── colors.{c,h}   # конфиг цветов + хэш-фолбэк
├── render.{c,h}   # ANSI-раскраска по схеме скриншота
├── seen.{c,h}     # FNV-1a hashset (открытая адресация) для watch-дедупа
└── platform.{c,h} # cross-platform: VT mode, UTF-8 console, term width, sleep
third_party/
└── jsmn.h         # MIT, single-header JSON tokenizer
vendor/
└── cacert.pem     # Mozilla CA bundle, вшивается в бинарь
cmake/
└── toolchain-mingw64.cmake
```

## Сборка

Все зависимости тянутся CMake'ом через `FetchContent`. Можно тянуть из гита
(если есть интернет в сборочной среде), либо положить распаковки рядом:

```
vendor/
  wolfssl/        # https://github.com/wolfSSL/wolfssl @ v5.7.6-stable
  curl/           # https://github.com/curl/curl       @ curl-8_11_0
  cacert.pem      # https://curl.se/ca/cacert.pem  ← уже в репо
```

`cacert.pem` лежит в репозитории — он маленький, и CMake встраивает его в
бинарь чтобы wolfSSL умел в HTTPS на винде, где нет дефолтного trust-store.
Доверяй Mozilla, не доверяй РКН.

### Linux (WSL2)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j20
./build/anonfm_vsrat_reader -n 10
```

### Кросс в Windows .exe из WSL2

Нужен `mingw-w64`:

```bash
sudo apt install mingw-w64
```

```bash
cmake -S . -B build-win -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw64.cmake
cmake --build build-win -j20
```

На выходе `build-win/anonfm_vsrat_reader.exe` (~2.6 МБ, никаких DLL рядом,
никаких VC++ Redistributable, никаких .NET Framework 4.7.2 SP1).

### Нативно под виндой

CMake умеет MSVC из коробки — теоретически собирается так же. Не пробовал.
Если что-то не так — мне поебать, кидай PR.

### Кросс под 32-битный Windows 7 (i686)

Для 32-битного Win7 нужен отдельный мингвовый тулчейн
`i686-w64-mingw32-gcc`. Под убунту:

```bash
sudo apt install mingw-w64
cmake -S . -B build-win32 -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw32.cmake
cmake --build build-win32 -j20
```

Это даёт PE32 (i386) бинарь с `SUBSYSTEM=6.01`, который запускается
на Win7 / Server 2008 R2 / 8 / 8.1 / 10 / 11 (и на 32-битном, и на
64-битном). UTF-8 рендерится через `MultiByteToWideChar` +
`WriteConsoleW` — codepage cmd.exe не важен. Цвета: на Win10+ через
ANSI escape (Virtual Terminal Processing), на Win7 автоматический
fallback на `SetConsoleTextAttribute` (xterm-256 → ближайший из 16
CGA-цветов).

**XP не поддерживается**: пробовали — wolfSSL хардкодит вызов
`InetPton`, который появился в `ws2_32.dll` только начиная с Vista.
Чтоб собрать под XP, надо патчить wolfSSL (заменить `InetPton` на
`inet_addr` с потерей IPv6) — это уже отдельный проект, не наш.

Емодзи на Win7 — лотерея: `WriteConsoleW` доставит правильные
кодепойнты до cmd.exe, но дефолтные «Consolas» / «Lucida Console» не
нарисуют астральные плоскости Юникода. Текст продолжит литься,
эмодзи отрисуются квадратиками — никаких падений. Чтоб настоящие
эмодзи под старой шиндой — Windows Terminal (Win10+).

### Кросс на ARM / MIPS / RISC-V / итд (musl-static)

Два пути.

**Путь №1 — через Zig** (используется в нашем GitHub Actions):

```bash
# 1) скачать zig (один тарбол, ~75 MB, покрывает все архи сразу)
curl -fsSL -o zig.tar.xz \
     https://ziglang.org/download/0.13.0/zig-linux-x86_64-0.13.0.tar.xz
tar xf zig.tar.xz && sudo mv zig-linux-x86_64-0.13.0 /opt/zig

# 2) маленький шим, чтобы CMake видел один cc/cxx/ar/ranlib
mkdir -p /tmp/zig-shim
cat > /tmp/zig-shim/cc <<'EOF'
#!/bin/sh
exec /opt/zig/zig cc -target aarch64-linux-musl "$@"
EOF
cat > /tmp/zig-shim/cxx     <<'EOF'
#!/bin/sh
exec /opt/zig/zig c++ -target aarch64-linux-musl "$@"
EOF
cat > /tmp/zig-shim/ar      <<'EOF'
#!/bin/sh
exec /opt/zig/zig ar "$@"
EOF
cat > /tmp/zig-shim/ranlib  <<'EOF'
#!/bin/sh
exec /opt/zig/zig ranlib "$@"
EOF
chmod +x /tmp/zig-shim/*

# 3) собрать
export CC=/tmp/zig-shim/cc CXX=/tmp/zig-shim/cxx
export AR=/tmp/zig-shim/ar RANLIB=/tmp/zig-shim/ranlib
export AFM_ZIG_TARGET=aarch64-linux-musl
cmake -S . -B build-aarch64 -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-zig-cc.cmake
cmake --build build-aarch64 -j20
```

Меняй `aarch64-linux-musl` на любой Zig-таргет: `x86-linux-musl`,
`arm-linux-musleabihf` (`-mcpu=cortex_a7`/`arm1176jzf_s`),
`mips-linux-musl`, `mipsel-linux-musl`, `riscv64-linux-musl`,
`powerpc64le-linux-musl`.

**Что у Zig не работает**:
- **ARMv5** (NSLU2-класс) — pre-ARMv6 не имеет нативных `LDREX`/`STREX`,
  и GCC builtins `__sync_*` ищут libgcc-стабы, которые Zig не шипит.
  Lookup на kuser_helper делает Linux-libgcc, но не Zig-compiler_rt.
- **s390x** (IBM мейнфрейм) — LLVM s390x backend не умеет лоуэрить
  `fp_to_fp16` SDAG-ноду, а Zig's compiler_rt тянет `__fixhfsi` для
  любого бинаря. Падает на любом `main(){}`. Ждём апстрим-фикс в Zig.

В обоих случаях если очень надо — собирай через musl-cross-make
(`toolchain-musl-cross.cmake`) с хоста, где musl.cc достижим:
там libgcc полная, с armv5 kuser_helper стабами и s390x compiler_rt.

**Путь №2 — через классический musl-cross-make** (если интернет
не блокирует musl.cc):

```bash
# скачать с https://musl.cc, распаковать в ~/cross
export PATH=~/cross/aarch64-linux-musl-cross/bin:$PATH
export AFM_MUSL_TARGET=aarch64-linux-musl
cmake -S . -B build-aarch64 -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-musl-cross.cmake
cmake --build build-aarch64 -j20
```

В обоих случаях получаются полностью статические ELF-ки без
зависимостей — суём на железку по `scp`, запускаем.

**Подводный камень**: libcurl по дефолту автодетектит на хосте libidn2,
zlib, libpsl, и тащит из `/usr/include` и `/usr/lib` в кросс-сборку,
где это всё, ясное дело, не той архитектуры. Мы это убили в
[`CMakeLists.txt`](CMakeLists.txt) (`CURL_USE_LIBIDN2=OFF`, `CURL_ZLIB=OFF`,
`USE_NGHTTP2=OFF`, ...) и в обоих тулчейн-файлах (через
`CMAKE_SYSROOT` + `CMAKE_FIND_ROOT_PATH`). HTTP/2 поэтому отключён —
`/answers.js` и без него отлично качается.

**Проверено локально (через zig cc)**:
- `aarch64-linux-musl` — ✓ ARM64 static ELF
- `mips-linux-musl` — ✓ MIPS32 BE static ELF

**Проверено через CI (см. [`.github/workflows/release.yml`](.github/workflows/release.yml))**:
все 11 таргетов из релиз-матрицы. На push тега `v*` собирается draft
GitHub Release со всеми бинарями — заходи в Releases и нажимай Publish.

### Сборка под всё сразу

```bash
./scripts/build_all.sh
```

Идёт по матрице (`win64`, `win32-xp`, `linux-x86_64`, `linux-aarch64`,
`linux-armv5`, `linux-mips`, `linux-riscv64`, ...), пропускает таргеты,
для которых нет тулчейна на PATH, и складывает готовые бинари в `dist/`.
Как у Tor / hysteria — один тарбол с зоопарком на любую железку, от
маршрутизатора с 4 МБ flash до AI кластера на RISC-V.