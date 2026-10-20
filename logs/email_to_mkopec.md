**To:** mkopec@dasharo.com  
**Subject:** Seeking guidance: EDK II PeiDispatcher hang on ASRock SPC741D8 (Xeon SP / DDR5)

---

Hi Michał,

My name is Mahdi Sahebi. I have been working on porting Dasharo/coreboot with the EDK II UEFI payload to an ASRock SPC741D8-2L2T/BCM server (Intel Xeon SP Sapphire Rapids, DDR5, ASPEED AST2600 BMC). I started from the `asrock_spc741d8_v0.9.0` release tag and created a branch `edk2-server` on top of it.

I have been following the Dasharo codebase and have made some progress, but I am running into a silent crash in the EDK II PEI phase that I would really appreciate your guidance on. I want to make sure I understand the intended behavior before I go further with guesswork.

---

**The problem**

Coreboot boots fully and hands off to the EDK II DasharoPayloadPkg correctly. PeiCore starts its first pass, reaches `PeiDispatcher()`, and then the system hangs silently — no more VGA output, no serial, no reboot. The last VGA line I see before the hang is:

```
[1]u-Sec:8FE94, PD:8FA30,BM:0,PMI:0
```

This is printed from `MdeModulePkg/Core/Pei/PeiMain/PeiMain.c` immediately before the call to `PeiDispatcher()`. The system never returns from that call.

---

**What I found: two bugs**

**Bug 1 — VGA array index (already fixed in my branch)**

In `MdeModulePkg/Core/Pei/PeiMain/PeiMain.c` lines 43–45, there was an off-by-one: `p[1]` and `string[1]` were used in a loop where `p[i]` and `string[i]` were intended. This caused incorrect VGA writes on every debug character after the first. I fixed this and can see proper VGA output now.

```c
// Before:
p[1]      = VgaColor;
string[1] = Character;

// After:
p[i]      = VgaColor;
string[i] = Character;
```

**Bug 2 — `MemInfoCallback` size threshold in `BlSupportPei.c` (root crash cause)**

In `DasharoPayloadPkg/BlSupportPei/BlSupportPei.c` around line 347, `MemInfoCallback` has this guard:

```c
if (Base < BASE_4GB) {
    if (Size >= PEI_MEM_SIZE) {          // PEI_MEM_SIZE = 64 MB
        *UsableLowMemTop = Base + Size;
    }
}
```

On this Xeon SP DDR5 platform, the coreboot LBIO table appears to present low memory (< 4 GB) as many small fragments, none of which individually reaches 64 MB. As a result `UsableLowMemTop` remains 0 after iterating all regions.

In a DEBUG build the subsequent `ASSERT(UsableLowMemTop >= BASE_1MB + PEI_MEM_SIZE)` would catch this, but in RELEASE it is a no-op. Execution continues to:

```c
PeiMemBase = (UsableLowMemTop - PEI_MEM_SIZE) & (~(BASE_64KB - 1));
           = (0 - 0x4000000)
           = 0xFFFFFFFFFC000000   // UINT64 underflow
```

`PeiServicesInstallPeiMemory(0xFFFFFFFFFC000000, 64MB)` then crashes PeiCore silently.

I changed the guard to track the highest low-RAM region top regardless of individual region size:

```c
if (Base < BASE_4GB) {
    if ((Base + Size) > *UsableLowMemTop) {
        *UsableLowMemTop = Base + Size;
    }
}
```

I have built and committed this fix. The ROM is ready to flash. Before I do, I have a few questions.

---

**My questions**

1. Is the original `Size >= PEI_MEM_SIZE` guard intentional for a reason I am missing — for example, to avoid placing PEI memory in a region that is too fragmented to be useful? Or is this genuinely a bug for platforms where RAM is presented in smaller pieces?

2. On this Xeon SP platform with DDR5, what does the coreboot LBIO memory map typically look like below 4 GB? I do not yet have a DEBUG EDK II build running (serial `early_puts` is not implemented in `SecCore/SecMain.c`), so I am reading the memory layout indirectly. Is there a recommended way to dump the LBIO table from EDK II before PEI memory is installed?

3. Is there a known-working approach for serial debug on this board? The BMC is an AST2600 and IPMI SOL is accessible, but `DasharoPayloadPkg/SecCore/SecMain.c` does not implement `early_puts`. I plan to add it — is there a reference implementation from another Dasharo board I should follow?

4. Is there anything else on the `asrock_spc741d8_v0.9.0` + EDK II path that is known to need attention that I may be missing?

---

**Platform details**

| Item | Value |
|---|---|
| Board | ASRock SPC741D8-2L2T/BCM |
| CPU | Intel Xeon SP (Sapphire Rapids) |
| RAM | DDR5 |
| BMC | ASPEED AST2600 |
| SPI flash | 64 MB (16 MB BIOS region) |
| Coreboot base | `asrock_spc741d8_v0.9.0` |
| EDK II base | Dasharo edk2, branch `asrock` |
| FSP | EagleStreamFspBinPkg |

---

**Key file references**

- `DasharoPayloadPkg/BlSupportPei/BlSupportPei.c` — `MemInfoCallback`, lines 307–370
- `DasharoPayloadPkg/SecCore/SecMain.c` — `early_puts` (stub / unimplemented)
- `MdeModulePkg/Core/Pei/PeiMain/PeiMain.c` — VGA debug, lines 43–45 (fixed)
- `MdeModulePkg/Core/Pei/Dispatcher/Dispatcher.c` — no GUID print before dispatch

---

Any guidance you can offer would be very much appreciated. I am happy to share patches, logs, or the full analysis document I have been building if that would help.

Thank you for your time and for all the work you and the Dasharo team have put into this codebase.

Best regards,  
Mahdi Sahebi  
m.nejadsahebi@live.co.uk
