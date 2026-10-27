# Сборка компоненты под Android (мобильная платформа 1С)

Контейнерная сборка `SimpleKafka1C` для мобильной платформы «1С:Предприятие»
(ОС Android) со статической линковкой всех сторонних зависимостей.

## Что получается

- `out/android/SimpleKafka1C-android.zip` — архив мобильной внешней компоненты
  для загрузки в конфигурацию 1С;
- `out/android/libSimpleKafka1C_ARM64.so` — arm64-v8a;
- `out/android/libSimpleKafka1C_ARM.so` — armeabi-v7a;
- `out/android/MANIFEST.XML` — описание содержимого архива;
- `out/android/ANDROID_MANIFEST_EXTENSIONS.XML` — разрешение `INTERNET` для
  приложения, в которое встроена компонента;
- `out/android/build-info.txt` — версии тулчейна, коммит vcpkg, sha256, список
  экспортируемых символов и список требуемых библиотек.

Мобильная платформа «1С:Предприятие» загружает внешнюю компоненту не отдельной
библиотекой, а архивом: в архив обязательно входит `MANIFEST.XML`, который
указывает платформе, какой файл и для какой архитектуры процессора загружать.

Статически внутрь `.so` линкуются: librdkafka, zlib, snappy, zstd, OpenSSL,
AVRO, Protobuf, curl, fmt, Boost.JSON и `libc++` (`ANDROID_STL=c++_static`).
Внешние зависимости результата — только системные библиотеки Android:
`libc`, `libm`, `libdl` и ещё `liblog.so` — её подтягивает Protobuf.

## Что входит в сборку

Сборка одна и полная — набор возможностей такой же, как на Windows и Linux:

- **Kafka** — продюсер, консьюмер, очереди (share groups), админ API, ACL,
  транзакции, push-режим;
- **AVRO и Protobuf** — методы работы со схемами: `СохранитьСхемуAVRO`,
  `ПреобразоватьВФорматAVRO`, `ОтправитьСообщениеAVRO`, `СохранитьФайлAVRO`,
  `ДекодироватьСообщениеAVRO`, `ПолучитьСхемуAVRO`, `СохранитьСхемуProtobuf`,
  `ПреобразоватьВФорматProtobuf`, `СохранитьФайлProtobuf`,
  `ДекодироватьСообщениеProtobuf`, `ОтправитьСообщениеProtobuf`,
  `ОтправитьСообщениеProtobufСОжиданиемРезультата`;
- **Schema Registry** — `ЗарегистрироватьСхему`, `ПолучитьСхемуПоИД`,
  `ПолучитьПоследнююСхему`, `ПолучитьВерсииСхемы`, `УдалитьСхему`;
- **TLS** — OpenSSL статически линкуется в библиотеку, поэтому
  `security.protocol=ssl` работает, а Schema Registry доступен по `https://`;
- **SASL** — отдельных библиотек не требуется: SCRAM librdkafka реализует
  сам, без cyrus-sasl, который под NDK не собирается в принципе;
  OAUTHBEARER приходит вместе с TLS.

Единственное, чего в Android нет, — **cyrus-sasl** (GSSAPI): под NDK он не
собирается. Kerberos и LDAP не поддерживаются, SCRAM и OAUTHBEARER работают.

## Точка входа JNI_OnLoad — обязательное требование платформы

Мобильная платформа загружает библиотеку компоненты через `dlopen` и сразу после
этого вызывает точку входа `JNI_OnLoad`, найденную в библиотеке через `dlsym`.
Результат поиска платформа **не проверяет на `NULL`** и вызывает найденный адрес
как есть. Библиотека без этого экспорта приводит к вызову по нулевому адресу:
приложение падает с `SIGSEGV` прямо на строке `ПодключитьВнешнююКомпоненту`,
без сообщения об ошибке.

Поэтому в Android-сборку обязательно включается `src/jnienv.cpp`, который
экспортирует `JNI_OnLoad` и `JNI_OnUnload`. Символы перечислены в списке
экспортируемых в `src/version.script` — этот файл общий для всех платформ,
и лишних экспортов он не добавляет: на desktop `src/jnienv.cpp` не собирается,
поэтому символа в библиотеке просто нет, а линкер запись без совпадения
игнорирует. Точка входа сохраняет переданный `JavaVM*` и ничего больше не
делает — компонента написана на чистом C++ и JNI ей не нужна, но сам символ
платформе необходим.

Подключение `src/jnienv.cpp` к сборке — часть build-time патча
`docker/android/patches/0001-android-build.patch` (см. раздел
«Исходники проекта не меняются» ниже).

В журнале Android этот сбой узнаётся так:

```
F libc    : Fatal signal 11 (SIGSEGV), code 1, fault addr 0x0 in tid ... (com.e1c.mobile)
F DEBUG   :     #00 pc 0000000000000000  <unknown>
F DEBUG   :     #01 pc ...  lib1cem.so (LoadLibraryW+...)
```

`pc 0000000000000000` — это и есть вызов по `NULL`, а `LoadLibraryW` — функция
движка мобильной платформы, которая загружает внешнюю компоненту.

Проверить экспорт можно по списку символов в `out/android/build-info.txt`:

```sh
grep JNI_OnLoad out/android/build-info.txt
```

## Исходники проекта не меняются

Android-сборка не трогает исходники проекта. Все изменения, которые нужны под
Android, оформлены одним build-time патчем
`docker/android/patches/0001-android-build.patch` и применяются к копии
исходников внутри контейнера сборки:

```dockerfile
COPY docker/android/patches /patches
RUN for p in /patches/*.patch; do patch -p1 -d /src -i "$p"; done
```

Так же сделано и для Linux-сборки (`docker/rhel9/patches`) — это принятый в
проекте способ доставки платформенных правок.

Патч добавляет:

- исключение блока cyrus-sasl в `CMakeLists.txt` — под Android этого пакета
  нет, SASL работает на встроенном в librdkafka SCRAM;
- печать ABI сборки в лог контейнера.

Исправления переносимости, нужные всем платформам, живут в общих исходниках:
`currentDateTime()` берёт время с системных часов — на libc++ (тулчейн
Android NDK и macOS) часы высокого разрешения не привязаны к эпохе Unix, и
POSIX-ветка функции не компилировалась; `getpid()` подключён явно, через
`<unistd.h>`.

Подключение `src/jnienv.cpp` к сборке лежит в главном `CMakeLists.txt`: файл
подключается только когда целевая система — Android, потому что `jni.h` есть
только в Android NDK. Патч этот кусок не трогает.

Если исходники проекта меняются, патч нужно пересоздать. Базой патча является
**закоммиченное** состояние репозитория, поэтому порядок такой:

```sh
# 1. применить текущий патч и поправить Android-специфичные изменения
git apply docker/android/patches/0001-android-build.patch
#    ... правки ...
# 2. снять разницу с закоммиченным состоянием — это и есть новый патч
git diff -- CMakeLists.txt \
    > docker/android/patches/0001-android-build.patch
# 3. вернуть дерево к закоммиченному состоянию
git checkout -- CMakeLists.txt
```

Проверить, что патч применится к текущему состоянию исходников:

```sh
git apply --check docker/android/patches/0001-android-build.patch
```

## Запуск

Требуется Docker с BuildKit (по умолчанию в Docker 23+).

Windows:

```powershell
pwsh docker/android/build.ps1
```

Linux/macOS:

```sh
sh docker/android/build.sh
```

Напрямую:

```sh
docker build -f docker/android/Dockerfile --target export --output out/android .
```

Первая сборка занимает значительное время: vcpkg собирает все зависимости из
исходников под NDK. Повторные сборки используют кеш слоёв Docker и кеш vcpkg и
пересобирают только саму компоненту.

## Параметры

| Параметр | По умолчанию | Назначение |
|---|---|---|
| `BASE_IMAGE` | `ubuntu:22.04` | Базовый образ сборки |
| `VCPKG_REF` | `master` | Коммит или тег vcpkg — фиксирует версии зависимостей |
| `BUILD_TYPE` | `Release` | Тип сборки CMake |
| `API_LEVEL` | `21` | Уровень API Android (21 = Android 5.0) |

| `DIAGNOSTICS` | `OFF` | Диагностическая сборка с логированием в logcat (тег `SK1C`) |

Пример фиксации версий зависимостей:

```sh
VCPKG_REF=2025.06.13 sh docker/android/build.sh
```

### Пакеты vcpkg

Набор зависимостей для мобильной сборки:

| Пакет | Зачем |
|---|---|
| `librdkafka[ssl,curl,zlib,zstd]` | Kafka-клиент с шифрованным транспортом |
| `openssl` | TLS: в Android нет системного OpenSSL, библиотека приходит из vcpkg |
| `boost-json` | JSON-форматирование отправляемых и принимаемых сообщений |
| `boost-algorithm` | `Boost.Algorithm`: `split`, `replace`, `trim`, `classification` |
| `boost-property-tree` | `Boost.PropertyTree`: чтение JSON |
| `boost-container` | `Boost.Container` — нужен схемным исходникам |
| `zlib`, `snappy`, `zstd` | кодек сжатия для librdkafka и для сообщений |
| `avro-cpp[core,snappy]` | сериализация AVRO, включая кодек snappy |
| `protobuf` | разбор Protobuf в runtime, без `protoc` |
| `abseil`, `utf8-range` | зависимости Protobuf, на них ссылается его CMake-конфигурация |
| `curl[core,ssl]` | Schema Registry по HTTP и https |
| `fmt` | форматирование ошибок в avro-cpp |

vcpkg разбил Boost на отдельные порты по одной библиотеке на порт, поэтому
header-only библиотеки (`boost-algorithm`, `boost-property-tree`) нужно
запрашивать явно — в составе `boost-json` их нет. Полный мета-порт `boost`
тоже подойдёт, но он заметно тяжелее.

Для сборки вручную:

```sh
vcpkg install --triplet arm64-android21 \
    --overlay-ports=vcpkg-overlay \
    --overlay-triplets=vcpkg-overlay/triplets \
    "librdkafka[ssl,curl,zlib,zstd]" openssl boost-json boost-algorithm \
    boost-property-tree boost-container zlib snappy zstd \
    avro-cpp[core,snappy] protobuf abseil utf8-range curl[core,ssl] fmt
```

## Архитектуры и уровень API

Мобильная платформа «1С:Предприятие» поставляется для четырёх архитектур:
ARM 32 бита, ARM64, x86 и x86_64. В этой сборке сделаны ARM (`armeabi-v7a`) и
ARM64 (`arm64-v8a`) — это все современные реальные устройства и ТСД. Сборка под
`x86` и `x86_64` нужна только для эмулятора.

Триплеты vcpkg лежат в `vcpkg-overlay/triplets/`. Они отличаются от штатных
`arm64-android` и `arm-neon-android` только уровнем API: **21 (Android 5.0)** —
это самая старая версия Android, поддерживаемая мобильной платформой
«1С:Предприятие». Штатные триплеты vcpkg нацелены на API 28 (Android 9) и
оставили бы устройства на Android 5–8 без компоненты.

Чтобы поднять минимальную версию, измените `VCPKG_CMAKE_SYSTEM_VERSION` в
триплетах и передайте `API_LEVEL` в сборку.

## Подключение в 1С

1. Соберите архив `SimpleKafka1C-android.zip`.
2. Загрузите его в конфигурацию 1С как макет с типом **«Внешняя компонента»**
   (например, имя макета — `Компонента`).
3. Подключите компоненту на клиенте — так же, как на Windows и Linux:

   ```1c
   Подключено = Ждать ПодключитьВнешнююКомпонентуАсинх(
       "ОбщийМакет.Компонента", "Integration",, ТипПодключенияВнешнейКомпоненты.Изолированно);

   Если Не Подключено Тогда
       Ждать УстановитьВнешнююКомпонентуАсинх("ОбщийМакет.Компонента");
       Подключено = Ждать ПодключитьВнешнююКомпонентуАсинх(
           "ОбщийМакет.Компонента", "Integration");
   КонецЕсли;

   Компонента = Новый("AddIn.Integration.simpleKafka1C");
   ```

   Мобильная платформа загружает компоненту из архива, описанного в
   `MANIFEST.XML`, поэтому отдельная установка библиотек на устройство не
   нужна. Изолированный режим подключения — возможность клиента под
   управлением ОС Windows, на мобильной платформе компонента выполняется в
   процессе приложения.

4. Имя объекта `AddIn.Integration.simpleKafka1C` — то же, что на Windows и
   Linux: его возвращает `GetClassNames()`, поэтому код конфигурации работает
   без изменений.

### Имя компоненты

Имя внешней компоненты на мобильной платформе должно быть уникальным: только
латинские буквы, цифры и знак подчёркивания, первый символ — буква или `_`,
регистрозависимо. Если компонента публикуется от имени организации с
собственным сайтом, по правилам 1С имя должно начинаться с обратного
доменного имени — например `com_github_NuclearAPK_...`. В этом сборке
используется `name="simpleKafka1C"` в `MANIFEST.XML`; при публикации имя
стоит привести к требуемому виду и синхронно поправить `GetClassNames()` в
`src/exports.cpp`.

### Состав архива

Мобильная платформа ожидает в архиве компоненты для всех поддерживаемых ОС:
Windows Runtime, Android, iOS и Aurora. Эта сборка содержит только Android —
на устройствах с Android компонента доступна, на других ОС платформа просто
не найдёт подходящей записи в `MANIFEST.XML`. Чтобы собрать единый архив для
всех ОС, нужно добавить соответствующие библиотеки и записи `<component>` в
`MANIFEST.XML`.

## Проверка результата

```sh
unzip -l SimpleKafka1C-android.zip                    # состав архива
```

Требования к имени архива и структуре `MANIFEST.XML` описаны в статье
«Технология создания внешних компонент» на
[its.1c.ru](https://its.1c.ru/db/content/metod8dev/src/developers/platform/i8103221.htm).

Экспорт ограничен файлом `src/version.script`: наружу выходят только
`GetClassObject`, `DestroyObject`, `GetClassNames`, `SetPlatformCapabilities`
и `GetAttachType`, а в Android-сборке ещё и `JNI_OnLoad` с `JNI_OnUnload` —
их требует мобильная платформа (см. раздел про `JNI_OnLoad` выше). Остальные
символы скрыты, как и требует мобильная платформа.

В списке экспортов остаются ещё два служебных символа
Protobuf — `__start_pb_defaults` и `__stop_pb_defaults`. Это границы секции
Protobuf в готовой библиотеке: их создаёт линкер, и версионным скриптом они
не скрываются. На работу компоненты они не влияют.

## Проверка на устройстве

В репозитории есть харнесс подключения для Android — `tests/android-harness`.
Он повторяет последовательность вызовов мобильной платформы (`Init`,
`setMemManager`, `RegisterExtensionAs`, свойства, методы) и дополнительно
проверяет схемные методы: вызывает AVRO, Protobuf и Schema Registry так же,
как это делает 1С, и показывает результат.

Сборка харнесса (нужен Android NDK):

```sh
cmake -S tests/android-harness -B out/build/android-harness -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-21 \
    -DANDROID_NATIVE_API_LEVEL=21 -DANDROID_STL=c++_static
cmake --build out/build/android-harness
```

Запуск на устройстве:

```sh
adb push out/build/android-harness/connect_harness /data/local/tmp/
adb push out/android/libSimpleKafka1C_ARM64.so /data/local/tmp/
adb shell chmod 755 /data/local/tmp/connect_harness
adb shell /data/local/tmp/connect_harness /data/local/tmp/libSimpleKafka1C_ARM64.so
```

Ожидаемый вывод — `connect sequence completed` и по одному успешному вызову
каждого метода схем. Вторым аргументом передаётся адрес Schema Registry:
тогда харнесс ещё и зарегистрирует схему.

```sh
adb shell /data/local/tmp/connect_harness /data/local/tmp/libSimpleKafka1C_ARM64.so \
    http://192.168.41.202:8082
```

Проверка Schema Registry на устройстве требует доступного с телефона
реестра. Поднять его можно так:

```sh
docker run -d --name schema-registry -p 8082:8081 \
    -e SCHEMA_REGISTRY_HOST_NAME=schema-registry \
    -e SCHEMA_REGISTRY_KAFKASTORE_BOOTSTRAP_SERVERS=PLAINTEXT://192.168.41.202:9092 \
    -e SCHEMA_REGISTRY_LISTENERS=http://0.0.0.0:8081 \
    confluentinc/cp-schema-registry:8.2.0
```

Адрес `192.168.41.202` — адрес хоста в локальной сети: он же указан в
`advertised.listeners` брокера в `docker/kafka-share/docker-compose.yml`.

### Проверка TLS

Четвёртым аргументом харнессу передаётся путь к сертификату CA на
устройстве, пятым — адрес брокера с TLS-листенером. Сертификат передаётся
компоненте содержимым, а не файлом, поэтому на устройстве ничего
записывать не нужно:

```sh
adb push docker/kafka-share/certs/ca.pem /data/local/tmp/
adb shell /data/local/tmp/connect_harness /data/local/tmp/libSimpleKafka1C_ARM64.so \
    http://192.168.41.202:8082 /data/local/tmp/ca.pem 192.168.41.202:9094
```

Ожидаемый вывод — `InitProducer : result=1` и `Produce : value=0`: продюсер
поднялся по TLS и сообщение ушло в топик `test-topic-tls`.

Брокер с TLS-листенером поднимается вместе со стендом: порт 9094
(`docker/kafka-share/docker-compose.yml`), сертификаты для него генерируются
скриптом `docker/kafka-share/gen-certs.sh`.

## Диагностическая сборка

Для разовых расследований есть сборка с логированием жизненного цикла
компоненты в logcat, тег `SK1C`:

```sh
DIAGNOSTICS=ON OUTPUT=out/android-diag sh docker/android/build.sh
```

Сборка включается ключом `--build-arg DIAGNOSTICS=ON` и добавляет к обычной
одну зависимость — `liblog.so`. Патч диагностики лежит в
`docker/android/diagnostics/`, применяется поверх основного и на обычные
сборки не влияет.

Логи читаются так:

```sh
adb logcat -d -s SK1C
```

Так, например, выяснилось, что мобильная платформа вызывает `setMemManager` и
передаёт компоненте собственный менеджер памяти 1С — компоненте не нужен
резервный менеджер памяти.
