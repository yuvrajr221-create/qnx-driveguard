# QNX 8.0 from a phone: build log and tutorial

We built DriveGuard for the QNX Global Hackathon with only an Android phone: a browser, and GitHub Codespaces as the Linux host. This is the path that worked for us in November 2026 (QNX SDP 8.0, baseline 8.0.0, QNX Software Center 2.0.4), including the problems we hit.

## 0. What you need

- A GitHub account (we used a 4-core Codespace)
- A myQNX account on qnx.com
- An x86_64 Ubuntu host. SDP 8.0 does not support ARM hosts, so Termux on a phone or an ARM cloud VM will not work. Ubuntu 24.04 worked for us.

## 1. Get and deploy the license

1. Open the QNX Everywhere page, accept the agreement and submit.
2. Open the myQNX License Manager (qnx.com/account/dashboard). Tick the license, open the small menu next to the header checkbox and choose "Accept Licenses".
3. Deploy the license to yourself: use the "Users" column ("Click to deploy to...") or the menu item "Deploy Selected Products to Users", and enter your own email.
4. Careful: the Allocate / Deallocate panel next to "Managers" is something else and does not deploy the license. Open the license details. "Product deployed to the following users" must list your email. Without this, the installer download stays locked.

## 2. Get the Software Center installer into the Codespace

Download "QNX Software Center ... Linux Hosts" (about 105 MB) from the QNX download center on your phone.

- `wget` in the Codespace does not work: QNX redirects it to the login page.
- Uploading the file from a phone browser to Codespaces did not work for us.
- What worked: upload the file to Google Drive from the phone, set sharing to "Anyone with the link", then in the Codespace run `pip install gdown` and `gdown FILE_ID -O ~/qnx.run`. Delete the file and turn sharing off afterwards.

## 3. Install the Software Center

```
chmod +x ~/qnx.run
~/qnx.run -- -d
```

The installer is a makeself archive. `-d` stops it from starting the GUI. Accept the license and press Enter for the default path (`~/qnx`). Check it:

```
~/qnx/qnxsoftwarecenter/qnxsoftwarecenter_clt -version
```

## 4. Install SDP 8.0 from the command line

The tool logs a password given on the command line, so put your login in a file that only you can read, outside any git repo:

```
umask 077
printf -- '-myqnx.user\nYOUR_EMAIL\n-myqnx.password\nYOUR_PASSWORD\n' > "$HOME/.qsc_opts"
```

Do not paste interactive `read` prompts and the next commands in one block, the prompt can swallow the next line.

```
QSC="$HOME/qnx/qnxsoftwarecenter/qnxsoftwarecenter_clt"
$QSC @"$HOME/.qsc_opts" -mirrorBaseline qnx800
$QSC @"$HOME/.qsc_opts" -addLicenseKey YOUR-LICENSE-KEY
$QSC -listAccessible | head
nohup $QSC @"$HOME/.qsc_opts" -destination "$HOME/qnx800" -installBaseline com.qnx.qnx800 > ~/sdp_install.log 2>&1 &
```

- `-mirror` is deprecated, use `-mirrorBaseline`.
- `-syncLicenseKeys` found nothing for us. `-addLicenseKey` with the key from the license details worked.
- The install took about 4 minutes.

## 5. First program

```
source ~/qnx800/qnxsdp-env.sh
qcc -Vgcc_ntox86_64 hello.c -o hello
file hello
```

`file` should show the QNX loader `ldqnx-64.so.2`.

## 6. Boot QNX in QEMU

```
sudo apt-get install -y qemu-system-x86
sudo chmod 666 /dev/kvm
ssh-keygen -q -t ed25519 -N '' -f ~/.ssh/id_ed25519
mkdir -p ~/qnxvm && cd ~/qnxvm
mkqnximage --type=qemu --arch=x86_64 --ssh-ident=$HOME/.ssh/id_ed25519.pub --cpu=2 --ram=2G --build
```

The image build works. The `--run` step needs `brctl` and a tap network, which we did not set up in the container, so we start QEMU ourselves with user-mode networking:

```
qemu-system-x86_64 -smp 2 -m 2G -enable-kvm -cpu host \
  -kernel output/ifs.bin \
  -drive file=output/disk-qemu,format=raw,if=ide \
  -netdev user,id=n0,hostfwd=tcp::2200-:22 \
  -device virtio-net-pci,netdev=n0 \
  -display none -serial file:qemu_serial.log &
```

Problems we hit:

- Attach the disk as IDE. With `virtio-blk` the boot stops at "Can't mount /boot".
- Port 2222 is already used by the Codespace's own sshd. We forward port 2200.
- `/dev/kvm` loses its permission when the Codespace restarts, and the VM stops with it. Repeat the `chmod` and the QEMU command.
- The image uses a virtio network driver (`vtnet0`), so the QEMU network device must be `virtio-net-pci`.

After about 30 seconds, `qemu_serial.log` should end with "Startup complete". Then:

```
ssh -p 2200 -i ~/.ssh/id_ed25519 root@localhost uname -a
```

It prints `QNX noname 8.0.0 ...`.

## 7. Copy and run your program on QNX

```
ssh -p 2200 -i ~/.ssh/id_ed25519 root@localhost "cat > /tmp/hello && chmod +x /tmp/hello && /tmp/hello" < ~/hello
```

## Limits

- QEMU on shared cloud CPUs gives timing numbers that are not hardware numbers.
- Codespaces stop when idle and free hours are limited.
- We tried this on one setup only.
