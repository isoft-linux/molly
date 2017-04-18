# [molly](http://ghost-movie.wikia.com/wiki/Molly_Jenson)

Open source alternative for [Ghost](https://en.wikipedia.org/wiki/Ghost_(software)).

[partclone](https://github.com/Thomas-Tsai/partclone) and [dd](https://en.wikipedia.org/wiki/Dd_(Unix))'s [Qt](https://www.qt.io/) frontend.

## Dependence

* [libpartclone](https://github.com/isoft-linux/partclone)
* [udisks-qt](https://github.com/isoft-linux/udisks-qt)
* [isoft-os-prober](https://github.com/isoft-linux/isoft-os-prober)

## I18n

```
lupdate src/*.cpp -ts translations/zh_CN.ts
```

## Static Analyzer

```
mkdir scan-build
cd scan-build
scan-build -k -v cmake .. -DCMAKE_INSTALL_PREFIX=/usr   \
    -DCMAKE_BUILD_TYPE=Debug
scan-build -k -v -v -v make -j4
```

## Dynamic Sanitizer

### ASan && UBSan for checking memory leak, out-of-bounds and other undefined behavior issues.

```
mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr    \
    -DCMAKE_CXX_FLAGS="-Wall -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -fPIE"   \
    -DCMAKE_BUILD_TYPE=Debug
make -j4
```

### TSan for checking potential deadlock and data race issues.

```
mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr    \
    -DCMAKE_CXX_FLAGS="-Wall -fsanitize=thread -fno-omit-frame-pointer -fPIE"   \
    -DCMAKE_BUILD_TYPE=Debug
make -j4
```
