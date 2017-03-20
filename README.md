# molly

Open source alternative for [Ghost](https://en.wikipedia.org/wiki/Ghost_(software)).

[partclone](https://github.com/Thomas-Tsai/partclone) and [dd](https://en.wikipedia.org/wiki/Dd_(Unix))'s [Qt](https://www.qt.io/) frontend.

## Dependence

* [libpartclone](https://github.com/isoft-linux/partclone)
* [udisks-qt](https://github.com/isoft-linux/udisks-qt)
* [isoft-os-prober](https://github.com/isoft-linux/isoft-os-prober)

## Static Analyzer

```
mkdir scan-build
cd scan-build
scan-build -k -v cmake .. -DCMAKE_INSTALL_PREFIX=/usr   \
    -DCMAKE_BUILD_TYPE=Debug
scan-build -k -v -v -v make -j4
```

