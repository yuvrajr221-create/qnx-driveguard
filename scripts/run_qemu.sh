cd ~/qnxvm
if [ -w /dev/kvm ]; then ACC="-enable-kvm -cpu host"; else ACC="-accel tcg -cpu max"; fi
nohup qemu-system-x86_64 -smp 2 -m 2G $ACC \
  -kernel output/ifs.bin \
  -drive file=output/disk-qemu,format=raw,if=ide \
  -netdev user,id=n0,hostfwd=tcp::2200-:22 \
  -device virtio-net-pci,netdev=n0 \
  -display none -serial file:qemu_serial.log > qemu_run.log 2>&1 &
