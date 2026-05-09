# anonfm_vsrat_reader

Всратый CLI-ридер кукареканий со стороны диджейки на [anon.fm](https://anon.fm).
Чистый ANSI C (C89), libcurl + wolfSSL, статически слинкованный, собирается
под WSL2 и кроссом в `.exe` под винду из той же папки.

Делает то же самое что [`kekejucica.py`](https://anon.fm/console/) с офсайта,
но:
- на сишке восемьдесят-девятого года
- с цветами стабильными per-nick (один и тот же ник → один цвет всегда)
- с конфигом цветов
- с watch-режимом и фильтром по диджею
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
| 6 | реальное ФИО диджея — игнорим                       |

Документации нет, эндпоинт подсмотрен в питон-скрипте `kekejucica.py`. Формат
там устаревший (6 полей), реальный сейчас — 7.

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

На выходе `build-win/anonfm_vsrat_reader.exe` (~2.6 МБ, никаких DLL рядом).

### Нативно под виндой

CMake умеет MSVC из коробки — теоретически собирается так же. Не пробовал.
Если что-то не так — мне поебать, кидай PR.

## Использование

```
anonfm_vsrat_reader [options]
  -w, --watch              poll-режим
  -i, --interval SECONDS   интервал поллинга (default 5)
  -n, --limit N            показать только N последних
  -d, --dj NAME            фильтр по нику диджея (substring, ci)
  -c, --colors PATH        путь до конфига цветов
      --no-color           без ANSI
      --from-file PATH     читать JSON из файла (для отладки)
  -h, --help               help
```

Примеры:

```bash
# последние 50, в цвете
./anonfm_vsrat_reader

# только Кriesh, последние 10
./anonfm_vsrat_reader -n 10 -d Kriesh

# вотч-режим: раз в 5 секунд догоняет ленту, печатает только новое
./anonfm_vsrat_reader -w -i 5

# для пайпов:
./anonfm_vsrat_reader --no-color | less
```

## Цвета

Каждому нику — стабильный цвет (одинаковый при каждом запуске и в watch'е).

Без конфига — детерминированно: `FNV-1a(nick) % palette`. Палитры разделены
для слушателей (синий/розовый/фиолетовый) и диджеев (жёлтый/зелёный).

С конфигом — явная мапа nick → 256-color индекс. Файл ищется в:
1. путь, переданный в `-c PATH`
2. `./anonfm_colors.conf`
3. `$XDG_CONFIG_HOME/anonfm/colors.conf`
4. `~/.config/anonfm/colors.conf`
5. `%APPDATA%\anonfm\colors.conf` (винда)

Формат — см. [`anonfm_colors.conf.sample`](anonfm_colors.conf.sample):

```
listener  araaerkb08ba    96
listener  *               hash      # дефолт для остальных слушателей
dj        Kriesh          226
dj        Obsidian        118
dj        *               hash
```

`hash` = «брать из встроенной палитры по FNV-1a-индексу».
Число от 0 до 255 = конкретный xterm-256 цвет.

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

## Лицензия

Делай шо хошь. Фолбэки и зависимости (wolfSSL, libcurl, jsmn, Mozilla
cacert) — под собственными лицензиями.
