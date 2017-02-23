# Molly

Open source alternative for [Ghost](https://en.wikipedia.org/wiki/Ghost_(software)).

## Dependence

* [libpartclone](https://github.com/isoft-linux/libpartclone)
* [udisks-qt](https://github.com/isoft-linux/udisks-qt)

## Static Analyzer
```
mkdir scan-build
cd scan-build
scan-build -k -v cmake .. -DCMAKE_INSTALL_PREFIX=/usr   \
    -DCMAKE_BUILD_TYPE=Debug
scan-build -k -v -v -v make -j4
```

