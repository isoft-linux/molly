# Testcase

Coverage:
* partition -clone-to-> file
* file -restore-to-> partition
* partition -to-> partition
* disk -to-> disk

## QEMU

Create partclone.qcow2 image
```
$ qemu-img create -f qcow2 partclone.qcow2 60G
```

Resize the image if space is not enough
```
$ qemu-img resize partclone.qcow2 +10G
```

Use 10G to install Windows XP
```
$ qemu-system-x86_64 -m 4G -enable-kvm -cdrom windows-xp-SP3.iso \
    -boot order=d -drive file=partclone.qcow2,format=qcow2
```

Use 20G to install iSOFTLinux v4.1
```
$ qemu-system-x86_64 -m 4G -enable-kvm -cdrom isoft-desktop-v4.1-x86_64.iso \
    -boot order=d -drive file=partclone.qcow2,format=qcow2
```

Create other several images for partition to file/partition testcase.
```
$ qemu-img create -f qcow2 hdb.qcow2 1G
$ qemu-img create -f qcow2 hdc.qcow2 2G
```

Boot system
```
$ qemu-system-x86_64 \
    -net nic,model=rtl8139 -net user,smb=/shared \
    -m 4G -enable-kvm \
    -hda partclone.qcow2 \
    -hdb hdb.qcow2 \
    -hdc hdc.qcow2
```

Choose Windows XP in the grub, and format hdb to ntfs by Windows' administration 
tools, then install some Applications to /dev/sdb1 partition. OK, reboot to 
iSOFTLinux system for cloning.

## partclone

```
$ partclone.ntfs -L sdb1-clone.log -d -c -s /dev/sdb1 -o sdb1.img
```

***NOTE***: It does not need to run partclone with root, just tell it ```-L``` 
Log FILE to, for example $HOME, the writable path.

```
$ partclone.info -L sdb1-info.log -s sdb1.img

Partclone v0.2.89 http://partclone.org
Display image information
initial main bitmap pointer 0x1ebc5c0
Initial image hdr: read bitmap table
check main bitmap pointer 0x1ebc5c0
print image_head
File system:  NTFS
Device size:    1.1 GB = 1046272 Blocks
Space in use: 717.8 MB = 700982 Blocks
Free Space:   353.6 MB = 345290 Blocks
Block size:   1024 Byte
```

Then reboot to Windows, deleted some files deliberately in /dev/sdb1 partition, 
some Applications are not able to launch ;-) reboot to iSOFTLinux again for 
restoring.

```
$ partclone.ntfs -L sdb1-restore.log -d -r -s sdb1.img -o /dev/sdb1
```

## dd

disk2disk

```
$ dd if=/dev/sdb of=/dev/sdc bs=4096 conv=noerror,sync
```
