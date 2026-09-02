# Coreboot CrossGCC Setup Guide

## Step 1: Install All Dependencies

Copy and run this command to install all required packages:

```bash
sudo apt update && sudo apt install -y build-essential bison flex texinfo libgmp-dev libmpfr-dev libmpc-dev wget curl git make gnat autoconf automake libtool pkg-config
```


## Step 2: Build CrossGCC Toolchain

```bash
make crossgcc-clean
make crossgcc -j$(nproc) MPCSOURCE=https://ftp.gnu.org/gnu/mpc/mpc-1.3.1.tar.gz
```



## Step 3: Verify Installation

```bash
ls -la util/crossgcc/xgcc/bin/i386-elf-gcc
util/crossgcc/xgcc/bin/i386-elf-gcc --version
```
