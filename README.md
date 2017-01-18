# Molly

The Qt frontend for partclone and dd.

## Build and Install
```
git submodule init
git submodule update --remote --rebase
```

```
cd partclone
libtoolize --force --copy
aclocal -I m4
autoconf
autoheader
automake --add-missing
./configure --prefix=/usr --enable-extfs --disable-reiserfs --enable-fat \
    --enable-hfsp --enable-btrfs --enable-ncursesw --enable-ntfs \
    --enable-exfat --enable-f2fs --enable-minix --disable-nilfs2 --enable-xfs \
    --sbindir=/usr/bin
```

## Static Analyzer
```
mkdir scan-build
cd scan-build
scan-build -k -v cmake .. -DCMAKE_INSTALL_PREFIX=/usr   \
    -DCMAKE_BUILD_TYPE=Debug
scan-build -k -v -v -v make -j4
```

