# Сборка внешней компоненты с использованием vcpkg
- Поставить https://github.com/microsoft/vcpkg по инструкции:
 ```
git clone https://github.com/microsoft/vcpkg
bootstrap-vcpkg
vcpkg integrate install
 ```

- Ставим пакеты Windows:
 ```
vcpkg install librdkafka --triplet x64-windows-static
vcpkg install avro-cpp --triplet x64-windows-static
vcpkg install protobuf --triplet x64-windows-static
vcpkg install abseil --triplet x64-windows-static
vcpkg install utf8-range --triplet x64-windows-static
vcpkg install boost-property-tree --triplet x64-windows-static
vcpkg install boost-algorithm --triplet x64-windows-static
vcpkg install boost-json --triplet x64-windows-static
vcpkg install snappy --triplet x64-windows-static
 ```

## librdkafka 2.16.0 через overlay-порт

Компонента собирается с librdkafka **2.16.0**. Пока в vcpkg порт отстаёт (2.15.1), версия
задаётся overlay-портом из каталога `vcpkg-overlay/librdkafka` этого репозитория. Команды
выполнять из корня vcpkg: если запустить их из каталога порта, vcpkg примет его `vcpkg.json`
за манифест.

Новая установка:
 ```
vcpkg install "librdkafka[curl,ssl,zlib,zstd]:x64-windows-static" --overlay-ports=<путь к репозиторию>\vcpkg-overlay
 ```

Если librdkafka уже установлена из стандартного порта — обновление (без `--no-dry-run`
команда только показывает план; в нём должна быть одна librdkafka):
 ```
vcpkg upgrade librdkafka:x64-windows-static --overlay-ports=<путь к репозиторию>\vcpkg-overlay
vcpkg upgrade librdkafka:x64-windows-static --overlay-ports=<путь к репозиторию>\vcpkg-overlay --no-dry-run
 ```

Когда порт vcpkg догонит 2.16.0, overlay-порт можно удалить.

Если после обновления пакетов vcpkg сборка падает с `missing and no known rule to make it`
по пути к библиотеке (например, zlib 1.3.2 переименовал `zlib.lib` в `zs.lib`), в кеше CMake
остался старый путь. Сбросить его: `cmake -U "ZLIB_*" out/build/x64-Release`.

В файле CMakeLists.txt в 4-й строке указать путь до библиотек VCPKG, должен оканчиваться на /vcpkg/installed/x64-windows-static.
Сборку можно выполнить в Visual Studio как CMake проект.

# Linux

- Ставим пакеты Linux:
 ```
vcpkg install librdkafka --triplet  x64-linux-static
vcpkg install protobuf --triplet x64-linux-static
vcpkg install abseil --triplet x64-linux-static
vcpkg install utf8-range --triplet x64-linux-static
vcpkg install boost-property-tree --triplet  x64-linux-static
vcpkg install boost-algorithm --triplet  x64-linux-static
vcpkg install boost-json --triplet  x64-linux-static
vcpkg install snappy --triplet  x64-linux-static
vcpkg install boost-filesystem --triplet  x64-linux-static
vcpkg install boost-iostreams --triplet  x64-linux-static
vcpkg install boost-program-options --triplet  x64-linux-static
vcpkg install boost-crc --triplet  x64-linux-static
vcpkg install boost-math --triplet  x64-linux-static
vcpkg install boost-format --triplet  x64-linux-static
vcpkg install boost-test --triplet  x64-linux-static
```

В версии для Linux необходимо avro-cpp собрать самостоятельно.
```
wget https://dlcdn.apache.org/avro/avro-1.12.0/avro-src-1.12.0.tar.gz
tar -zxvf avro-src-1.12.0.tar.gz
```
Файл **avro/lang/c++/CMakeLists.txt** необходимо заменить файлом **avro-cpp-linux/CMakeLists.txt** данного проекта. 
Сборка:
 ```
cd /home/source/avro-src-1.12.0/lang/c++
cmake -B . -S . -DCMAKE_TOOLCHAIN_FILE=/home/source/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-linux-static
make
```

В основном файле CMakeLists.txt проекта надо указать пути до файлов проекта avro и vcpkg. 

Далее сборка внешней компоненты:
 ```
cd /home/source/Simple-Kafka_Adapter
cmake -B . -S . -DCMAKE_TOOLCHAIN_FILE=/home/source/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-linux-static
make
```

# Android (мобильная платформа 1С)

Мобильная платформа «1С:Предприятие» загружает внешнюю компоненту архивом:
в архив обязательно входит `MANIFEST.XML`, который указывает платформе, какой
файл и для какой архитектуры процессора загружать. Поэтому для Android
собирается не отдельная библиотека, а архив с `.so` для каждой архитектуры.

Готовая сборка делается в контейнере — так же, как для RHEL 9:

```
sh docker/android/build.sh
```

```
pwsh docker/android/build.ps1
```

Подробности, параметры и порядок подключения архива в 1С — в
[docker/android/README.md](../docker/android/README.md).

Отдельно отметим требование мобильной платформы: библиотека компоненты под
Android обязана экспортировать `JNI_OnLoad`. Платформа загружает библиотеку
через `dlopen` и вызывает найденную через `dlsym` точку входа без проверки на
`NULL` — без этого экспорта приложение падает с `SIGSEGV` на строке
`ПодключитьВнешнююКомпоненту`. Символы перечислены в списке экспортируемых в
`src/version.script`, а `src/jnienv.cpp` подключается к сборке из главного
`CMakeLists.txt` — только под Android, потому что `jni.h` есть только в NDK.

Исходники проекта при Android-сборке не меняются: все платформенные правки
собраны в build-time патч `docker/android/patches/0001-android-core-build.patch`,
который применяется к копии исходников внутри контейнера сборки — так же, как
сделано для Linux-сборки.

### Что нужно для сборки вручную

Требуется Android NDK и vcpkg. Тулчейн NDK подключается к vcpkg через
`VCPKG_CHAINLOAD_TOOLCHAIN_FILE`. Перед сборкой нужно применить патч к
исходникам — он добавляет режим core и точку входа `JNI_OnLoad`:

```
patch -p1 -d . -i docker/android/patches/0001-android-core-build.patch
```

После этого собирается как обычно:

```
export ANDROID_NDK_HOME=/path/to/android-ndk
cmake -S . -B out/build/arm64 -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DVCPKG_TARGET_TRIPLET=arm64-android21 \
    -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-21 \
    -DANDROID_NATIVE_API_LEVEL=21 \
    -DANDROID_STL=c++_static
cmake --build out/build/arm64
```

Триплеты `arm64-android21` и `arm-neon-android21` лежат в
`vcpkg-overlay/triplets/`. Они отличаются от штатных `arm64-android` и
`arm-neon-android` только уровнем API: 21 (Android 5.0) — самая старая версия
Android, поддерживаемая мобильной платформой «1С:Предприятие».

Список пакетов vcpkg для этой сборки — в
[docker/android/README.md](../docker/android/README.md#пакеты-vcpkg). Важно:
vcpkg разделил Boost на отдельные пакеты, поэтому `boost-algorithm` и
`boost-property-tree` нужно ставить явно — в составе `boost-json` их нет.

### Ограничения мобильной сборки

Под Android компонента собирается в режиме **core**: без AVRO, Protobuf,
Schema Registry, TLS и SASL. Причины — эти зависимости не собираются под
NDK, а в Android нет ни системного OpenSSL, ни cyrus-sasl. Транспорт
только `plaintext`.

Режим core включается автоматически при целевой сборке под Android. Для других
платформ его можно включить принудительно:

```
cmake -S . -B out/build/core -DSIMPLEKAFKA_CORE=ON ...
```



