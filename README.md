# anonfm_vsrat_reader

Всратый CLI-ридер кукареканий со стороны диджейки на [anon.fm](https://anon.fm).
Чистый ANSI C (C89), libcurl + wolfSSL, статически слинкованный, собирается
под WSL2 и кроссом в `.exe` под винду из той же папки.

Делает то же самое что [`kekejucica.py`](https://anon.fm/console/) с офсайта,
но:
- на сишке восемьдесят-девятого года (для хардкора)
- с цветами стабильными per-nick (один и тот же ник → один цвет всегда)
- с конфигом цветов
- с watch-режимом и фильтром по диджею
- с поддержкой SOCKS5 (на случай если за роскомнадзором)
- одним самодостаточным бинарём (CA Mozilla вшит, рядом ничего не нужно)

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
```

## Цвета

Каждому нику — стабильный цвет (одинаковый при каждом запуске и в watch'е).
Если у вас Obsidian всегда был зелёный — он и останется зелёным, не
сомневайтесь.

Без конфига — детерминированно: `FNV-1a(nick) % palette`. Палитры разделены
для слушателей (холодные тона: синие/розовые/фиолетовые) и диджеев
(широкая разноцветная палитра — красные, оранжевые, жёлтые, зелёные,
голубые, синие, фиолетовые, малиновые), чтобы два диджея в эфире
визуально не сливались в одну жёлтую кашу.

Помимо ника, можно красить и **тело сообщения**: ключи `listener_text`
и `dj_text` в конфиге. По дефолту — `off`, тело не красится (а то глаза
вытекут). Включить можно только через конфиг — отдельного аргумента
запуска нет, мы тут не тостер собираем.

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
* Developed in console
* No mouse used during development
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
* Поддержка USENET (`alt.anon.fm.kukareki`)
* Поддержка ICQ (OSCAR) — слать новые кукареки в ICQ-аське через UIN
* Поддержка пейджера через RDS на FM-частоте
* Графический клиент на ncurses, чтоб совсем по-олдскульному
* Порт на DOS, чтоб запускать в DOSBox
* Порт на ZX Spectrum (через сетевой адаптер ZXNet)
* Озвучка кукареков SAPI4-голосом «Николай», как в Промт 98

## FAQ

**Q: А почему ANSI C, а не нормальный C23?**
A: Для хардкора.

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
