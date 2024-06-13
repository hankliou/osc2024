cd rootfs
find . | cpio -o -H newc > ../initramfs.cpio
cd ..
cp ./initramfs.cpio ../kernel