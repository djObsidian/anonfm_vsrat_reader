# anonfm_vsrat_reader

Всратый CLI-ридер кукареканий со стороны диджейки на [anon.fm](https://anon.fm).
Чистый ANSI C (C89), libcurl + wolfSSL, статически слинкованный, собирается
под WSL2 и кроссом в `.exe` под винду из той же папки.

Делает то же самое что [`kekejucica.py`](https://anon.fm/console/) с офсайта,
но:
- на сишке восемьдесят-девятого года (мы пользуемся СТАНДАРТИЗИРОВАННЫМ языком, как элита)
- с цветами стабильными per-nick (один и тот же ник → один цвет всегда)
- с конфигом цветов
- с watch-режимом и фильтром по диджею
- с поддержкой SOCKS5 (на случай если за роскомнадзором)
- одним самодостаточным бинарём (CA Mozilla вшит, рядом ничего не нужно)

Референс с офсайта (то к чему стремились):

![Reference](reference.png)

И что получилось у нас в PowerShell:

![UI](image_powershell.png)

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
| 6 | реальное ФИО диджея — игнорим, нечего палить        |

Документации нет, эндпоинт подсмотрен в питон-скрипте `kekejucica.py`. Формат
там устаревший (6 полей), реальный сейчас — 7. Питон-скрипт расщепляет ФИО
из 5-го поля регуляркой по `\n\nС уважением, ` — мы так не умеем, мы
культурные.

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
маршрутизатора с 4 МБ flash до сервера на S390x.

## Использование

```
anonfm_vsrat_reader [options]
  -w, --watch              poll-режим (default)
  -o, --once               распечатать последнюю пачку и выйти
  -i, --interval SECONDS   интервал поллинга (default 5)
  -n, --limit N            показать только N последних
  -d, --dj NAME            фильтр по нику диджея (substring, ci)
  -c, --colors PATH        путь до конфига цветов
      --no-color           без ANSI
      --socks5 HOST:PORT   ходить через SOCKS5 (DNS тоже через прокси)
      --from-file PATH     читать JSON из файла (для отладки)
      --from-url URL       свой URL (можно несколько раз → зеркала, первый
                           живой 2xx выигрывает)
  -h, --help               help
```

По дефолту запускается в poll-режиме с интервалом 5 секунд — чтобы
даблклик по `.exe` под виндой сразу давал живую ленту, как у людей.
`-o`/`--once` для одноразового вывода (например в пайп).

Без `--socks5` libcurl всё равно подхватывает `ALL_PROXY` /
`https_proxy` / `http_proxy` из окружения, так что `socks5h://...` через
переменную тоже работает. Привет тору.

Примеры:

```bash
# вотч-режим (по дефолту), раз в 5 секунд:
./anonfm_vsrat_reader

# только Кriesh, последние 10, один раз:
./anonfm_vsrat_reader -o -n 10 -d Kriesh

# одноразовый дамп для пайпа:
./anonfm_vsrat_reader -o --no-color | less

# через локальный Tor SOCKS:
./anonfm_vsrat_reader --socks5 127.0.0.1:9050

# через зеркало (или несколько зеркал — первый ответивший 2xx побеждает):
./anonfm_vsrat_reader --from-url https://mirror.example/answers.js \
                      --from-url https://anon.fm/answers.js
```

## Цвета

Каждому нику — стабильный цвет (одинаковый при каждом запуске и в watch'е).
Если у вас Obsidian всегда был зелёный — он и останется зелёным, не
сомневайтесь.

Без конфига — детерминированно: `FNV-1a(nick) % palette`. Палитры разделены
и НЕ пересекаются, чтобы DJ никогда визуально не маскировался под слушателя:

- **Слушательская** палитра — широкая, ~40 цветов (циан, голубой, синий,
  фиолетовый, розовый, малиновый, зелёный, оранжевый, жёлтый, пастель).
  Слушателей десятки, и мы хотим, чтоб каждый узнавался.
- **Диджейская** палитра — наоборот, тесная, всего три цвета
  (жёлтый/зелёный/оранжевый), потому что у нас полтора диджея и они и так
  узнаваемы по тому, кто сегодня на эфире. Меньше цветов — стабильнее
  ассоциация «жёлтый = Kriesh, зелёный = Obsidian».

Помимо ника, **тело сообщения** тоже красится по дефолту — ключи
`listener_text` и `dj_text` в конфиге, оба `15` (яркий белый) по
умолчанию. Мы же **VSRAT** ридер: вырать = читать вслух, и читать
вслух хочется именно тело. Цвет тела — ОДИН консистентный, не равный
цвету ника говорящего: бейдж говорит «кто», тело — «что», смешивать
их незачем. Раньше дефолт был 252 (мягкий серый), но на Win7 cmd с
WinAPI-фолбэком он иногда сливался с фоном — 15 — это прямой CGA-15
WHITE без неоднозначностей в downconvert'е. Если хочется варианта
«у каждого ника свой цвет тела» — поставь в конфиге `*_text * hash`.
Если хочется монохром — `off`.

С конфигом — явная мапа nick → 256-color индекс. Файл ищется в:
1. путь, переданный в `-c PATH`
2. `./anonfm_colors.conf`
3. `$XDG_CONFIG_HOME/anonfm/colors.conf`
4. `~/.config/anonfm/colors.conf`
5. `%APPDATA%\anonfm\colors.conf` (винда)

Формат — см. [`anonfm_colors.conf.sample`](anonfm_colors.conf.sample):

```
listener        araaerkb08ba    96
listener        *               hash    # дефолт для бейджей слушателей
dj              Kriesh          226
dj              Obsidian        118
dj              *               hash    # дефолт для бейджей диджеев

dj_text         *               hash    # включить раскраску ответов диджеев
listener_text   *               off     # тело сообщений слушателей не красить
```

Значения цвета:
- число от 0 до 255 — конкретный xterm-256 цвет
- `hash` — детерминированно из встроенной палитры по нику
- `off` — не красить (дефолт для `*_text`)

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

## Наши достижения

* No webtechnology used, NO CSS USED
* No webtechnology were harmed
* No browser required, no Electron, no V8, no node_modules на 400 МБ
* No Firefox required
* No Chrome required, no Chromium, no Edge, no Opera GX with RGB
* 0% JSON.parse, 0% XML, 0% YAML, 0% TOML — JSON парсится руками через `jsmn.h`
* Developed in chat
* VIM compatible (а как иначе)
* NO PHP USED
* NO C++ USED, no STL, no `std::shared_ptr`, no templates на 40 экранов
* NO C# USED, no .NET, no Mono
* NO RUST USED, no `unsafe { }`, no борроу-чекера, no twitter-thread про safety
* NO GO USED, no `if err != nil`, no goroutine leaks
* NO PYTHON USED, no `pip install`, no virtualenv, no PEP 8 holy wars
* No JavaScript, no TypeScript, no WebAssembly, no React, no Vue, no Svelte
* No async/await — мы блокирующие, как пенсионер на почте
* No 5G required (хватит и dial-up через PPP)
* No Docker required, no Kubernetes, no Helm chart, no service mesh
* No telemetry, no analytics, no Sentry, no DataDog, no "anonymized usage stats"
* No AI, no LLM-фичи, no GitHub Copilot suggestions accepted (ну почти)
* No Windows 10/11 required
* Friendly usage with Windows 7
* Debugged with `printf` и иногда `fprintf(stderr, ...)`
* anon.fm compatible, build with anon.fm technology
* Mozilla CA вшит прямо в `.text` секцию, как завещали отцы
* Один файл на 2.6 МБ статически — таскай как хочешь
* C89, как у дедушки

## Todo

* Поддержка Fidonet — пусть кукареки заодно расходятся по эхам
* Поддержка USENET / NNTP — `nntp://news.anon.fm/alt.anon.fm.kukareki`,
  читать через `tin`/`slrn`/`Pan`, как в 1998-м
* Шлюз в **телетекст** — раскидать по страницам 700–799, чтоб бабушка
  на даче через рамблер-телек тоже была в курсе кто такой Obsidian
* Шлюз в **Meshtastic** — реле кукареков по LoRa-меш сети, для тех кто в
  бункере / в лесу / в отъезде по геополитическим причинам. На прямой
  узел через UART, либо в публичный канал; payload урезать до 230 байт,
  emoji дропать, диджей-ник — двумя символами
* Поддержка ICQ (OSCAR) — слать новые кукареки в ICQ-аське через UIN
* Поддержка пейджера через RDS на FM-частоте
* Графический клиент на ncurses, чтоб совсем по-олдскульному
* Порт на DOS, чтоб запускать в DOSBox
* Порт на ZX Spectrum (через сетевой адаптер ZXNet)
* Озвучка кукареков SAPI4-голосом «Николай», как в Промт 98

## FAQ

**Q: А почему ANSI C, а не нормальный C23?**
A: Потому что мы элита и пользуемся **СТАНДАРТИЗИРОВАННЫМ** языком. C89 ратифицирован
ANSI X3.159-1989 и ISO/IEC 9899:1990. Что ратифицировал ваш `auto&&` и `consteval`?
Шансов собрать C89 кодом на любой железке — от ПДП до микроволновки — больше, чем
у любой более молодой ревизии. Стандарты не для модников, стандарты для тех,
кто хочет чтобы оно работало через 30 лет.

**Q: А почему `vsrat`?**
A: Потому что reader всратый, а не «elegant», «modern», «production-ready».
Никаких инверсий зависимостей, никаких фабрик абстрактных билдеров — мы
просто по таймеру дёргаем `answers.js` и пишем в stdout.

**Q: А почему watch по дефолту?**
A: Чтоб в винде даблкликнул по `.exe` — и сразу льётся лента, без
бубнов и батников.

**Q: А почему wolfSSL, а не OpenSSL?**
A: Потому что OpenSSL не собирается статически на 12 платформах сразу
без слёз, а wolfSSL — собирается. И весит меньше. И лицензия GPLv2
(или коммерческая, если вам в продакшен), но мы для себя — нам пофиг.

**Q: Будет ли запись истории / SQLite / экспорт в Markdown?**
A: Нет.

**Q: Будет ли GUI?**
A: Нет. Если очень надо — ваш терминал уже GUI.

**Q: А под BSD соберётся?**
A: Должно. Не пробовал. Если что — кидай PR.

## Лицензия

Делай шо хошь. Фолбэки и зависимости (wolfSSL, libcurl, jsmn, Mozilla
cacert) — под собственными лицензиями, разбирайся сам.

Лично мой код — public domain / WTFPL / делайте что хотите. Если ваш
корпоративный legal отдел такое не принимает — считайте, что MIT.
Если и MIT не принимает — у меня для вас плохие новости про вашу работу.
