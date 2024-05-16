cd lab3_user_proc
cp *.img ../rootfs
cd ../rootfs
find . | cpio -o -H newc > ../initramfs.cpio
cd ..
cp ./initramfs.cpio ../kernel/