# Vendor Inspection — server.bin

**Target image:** `/home/mahdi/repositories/reconstruction/server/server.bin`
**Size:** 67108864 bytes (64 MiB)
**Date:** 2026-06-12

## Result

| Field | Value |
|-------|-------|
| **Vendor / OEM** | **Gooxi** |
| BIOS base | AMI Aptio V (AmiCrbPkg / AmiModulePkg / AmiIpmi2Pkg) |
| Reference platform | Intel **ArcherCity** (Eagle Stream, Xeon SP) |
| Board code | `G4DEL110` |
| Build branch | `bios_release_g4del` (Jenkins, VS2015 RELEASE) |

## Commands & Output

### File info
```console
$ ls -la /home/mahdi/repositories/reconstruction/server/server.bin
-rw-r--r-- 1 mahdi mahdi 67108864 Aug 20  2024 .../server.bin
$ stat -c %s /home/mahdi/repositories/reconstruction/server/server.bin
67108864
```

### Vendor string (decisive)
```console
$ strings -n 4 server.bin | grep -i gooxi | sort -u
r:\jenkins\workspace\bios_release_g4del\es\Build\ArcherCity\RELEASE_VS2015\IA32\Build\GooxiPei\DEBUG\GooxiPei.pdb
```

### Platform / board confirmation
```console
$ strings -n 4 server.bin | grep -iE 'gooxi|G4DEL|ArcherCity' | sort -u
ArcherCity
G4DEL110
r:\jenkins\workspace\bios_release_g4del\es\Build\ArcherCity\RELEASE_VS2015\IA32\Build\GooxiPei\DEBUG\GooxiPei.pdb
... (AMI Aptio PEI/CRB module PDB paths under ArcherCity) ...
```

## Conclusion

The image is a **Gooxi** server BIOS built on AMI Aptio V firmware for the Intel
ArcherCity (Eagle Stream / Xeon SP) reference platform, board `G4DEL110`.
The `GooxiPei.pdb` build path is the unambiguous vendor identifier.
