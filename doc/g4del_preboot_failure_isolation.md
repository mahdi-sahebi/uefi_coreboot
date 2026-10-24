# G4DEL power-on failure: controlled ROM comparison

Updated 2026-10-24. This report tracks a new failure that occurs before any
coreboot, VGA, or SOL checkpoint from the latest image. It supersedes the
earlier assumption that the next board failure must be in EDK2. The earlier
[DXE investigation](../payloads/external/edk2/workspace/mahdi-sahebi/doc/g4del_dxe_investigation.md)
still describes the prior, powered-on captures.

## Observed result and evidence limits

The board owner reports that the G4DEL BMC is reachable but cannot bring the
host to the on state with `build/coreboot_08-02.rom`: fans do not start, and
there is no VGA or SOL. They report that the chip readback matches that ROM.
After reflashing `/home/uefi/workspace/coreboot/build/coreboot_07-26.rom`,
host power and fans returned. No BMC event log, power-fault code, POST trace,
flash-command transcript, or readback file is available in this workspace.
The readback and board outcomes above are user reports, not measurements made
by the image-generation script. The board owner confirms Redfish access is
configured, but its endpoint has not yet been supplied to this workspace.
Neither `ipmitool` nor `flashrom` is installed here. No live BMC
measurement has been collected for this report.

| Reference | SHA-256 | Reported board result |
| --- | --- | --- |
| [Working ROM](/home/uefi/workspace/coreboot/build/coreboot_07-26.rom) | `50001ff4303e7852b14f9e0ec7013d8e0db6cc9737db2f64f94f201bd4a0789d` | Host powers on, fans turn, SOL was available |
| [Failed ROM](/home/uefi/workspace/test/uefi_coreboot/build/coreboot_08-02.rom) | `cb0aae73fcaefdbf0d80d2e3ce05b6cc21709ea21e2dfb7b7323a7399a345d66` | BMC reachable; host power fails; no fans, SOL, or VGA |

Both ROMs are exactly 67,108,864 bytes. The failing ROM's descriptor, GbE,
ME, and Device Exp1 regions each match the 64 MiB factory
`/home/uefi/workspace/gooxie_bios/server.bin` byte for byte. This corrects
the earlier status report that the factory image was absent. The working and
failed ROMs have *identical ME and SMMSTORE bytes*. Their FSP-T and FSP-S
CBFS files match; both use the same source `Fsp.fd`. The extracted FSP-M
CBFS bytes differ, so no claim of bit-identical FSP-M placement is made.
The bootblock occupies the same top-of-flash address in both ROMs, but its
bytes differ because the board target was rebuilt.

The two descriptors differ at 13 bytes: `0x81`, `0x82`, `0x83`, `0x85`,
`0x86`, `0x87`, `0x89`, `0x8a`, `0x8b`, `0x91`, `0x92`, `0x93`, and `0x1d6`.
`ifdtool -p lbg -d` reports **HAP set** and host permissions `0xffffff00`
for the working descriptor; the failing factory descriptor reports **HAP
clear** and host permissions `0x05a0fb00`. The working build selected
`IFDTOOL_DISABLE_ME=y` and `UNLOCK_FLASH_REGIONS=y`; the failed build selected
`DO_NOT_TOUCH_DESCRIPTOR_REGION=y` and disabled both transformations.
This is the strongest static pre-boot difference. It is still a hypothesis:
the board's Boot Guard or BMC power policy has not been measured. Intel
documents that Boot Guard enforcement can stop BIOS execution before the
initial boot block; that mechanism must not be inferred from an absent SOL
trace alone. [Intel's Boot Guard description](https://www.intel.com/content/www/us/en/developer/articles/technical/software-security-guidance/resources/key-usage-in-integrated-firmware-images.html).

The present `gooxi/g4del` board target, dual-socket FSP settings, BIOS
contents, and additional logging also differ from the older ASRock-based
image. EDK2 code executes after host power, coreboot, and payload handoff;
it cannot produce a checkpoint while the BMC cannot power the host.
The CBFS-embedded configuration confirms that the working ROM selected
`asrock/spc741d8`, `MAX_SOCKET=1`, and EDK2 `origin/feature/serial`; the failed
ROM selects `gooxi/g4del`, `MAX_SOCKET=2`, and EDK2
`origin/feature/dxe-log`. Both select SOL I/O `0x2f8`. The corresponding
CBFS revisions are `0175444e946c-dirty` and `dd645160bc52` respectively.

## Reproducible diagnostic images

Run from `/home/uefi/workspace/test/uefi_coreboot`:

```sh
python3 util/g4del_power_isolation.py
```

The script accepts only the two pinned source hashes above. It checks the
descriptor signature, size, ME equality, SMMSTORE equality, exact descriptor
delta, and each generated image's allowed changed regions. It runs `cbfstool`
on every image to check the 64 MiB FMAP, separate 256 KiB SMMSTORE and CBFS
bootblock/FSP/payload entries; it runs `ifdtool` to decode HAP and host flash
permissions. Generated images and a JSON manifest are written under ignored
`build/g4del_isolation/`. They are not committed or automatically flashed.
The candidates below were generated and checked during the investigation, but
the local `build/` directory was subsequently removed. Regeneration requires
the SHA-256-matching 08-02 ROM, the working 07-26 ROM, and built `cbfstool`
and `ifdtool`. The 08-02 ROM is currently absent from this workspace.

| Image in `build/g4del_isolation/` | SHA-256 | Bytes changed relative to the named base | Purpose |
| --- | --- | --- | --- |
| `08-02_with_07-26_descriptor.rom` | `6f5261f5434c68753b8925e878d71847b8fe32c7e086bffbe730b397088def5b` | 13 descriptor bytes from failed 08-02 | Combined HAP + permission test; run first |
| `08-02_hap_only.rom` | `0a74e8c1b6a15182b99e68ae75eaca697990ea0dd7a49edded25e67795715099` | HAP byte `0x1d6` from failed 08-02 | Test HAP with factory permissions |
| `08-02_unlock_only.rom` | `712fba4fb180c8f5236c6c6f5cc7d5b83d4d1f0dc9fbeed81daed01fc098b9f7` | 12 permission bytes from failed 08-02 | Test legacy permissions with HAP clear |
| `08-02_with_07-26_nonbios.rom` | `b3f8c7912ce8e0a1ebc977b59f1bb247134483a1f6ee3b3696ef07d88b449260` | Descriptor, GbE and Device Exp1 from failed 08-02 | New BIOS with the complete working non-BIOS prefix |
| `07-26_with_08-02_descriptor.rom` | `3fb28216df34cbd88151b829daef9d36cf6abde89c5f64e1df92a6bd0e0f9ff0` | Descriptor from working 07-26 | Factory descriptor with otherwise working ROM |
| `07-26_with_08-02_nonbios.rom` | `84b6ef0156593d850afe3910da4d8e81e654e5c9357c91b775aa07aff76ca48b` | Descriptor, GbE and Device Exp1 from working 07-26 | Working BIOS with complete current non-BIOS prefix |
| `07-26_with_factory_gbe_pt.rom` | `6e083c39f211b070b488012016f766d3d46ab679ad9be4edb2d79201c520f3e4` | GbE and Device Exp1 from working 07-26 | Isolate factory data restoration from descriptor policy |

Every image retains the identical ME and SMMSTORE bytes from both references.
The first image is a **diagnostic** use of the previously working unlocked
descriptor, not a proposal to keep flash regions unlocked permanently. The
HAP-only candidate retains the factory lock permissions.
These images change assembled bytes directly, so an 08-02-based image still
contains the 08-02 CBFS configuration text even when its descriptor has been
substituted. Decode the actual descriptor with `ifdtool`; do not infer its
HAP or permission state from the embedded Kconfig text.

## Board test order and decision points

Use the existing recovery programmer and the same full-chip flash procedure
for all tests. Before each power attempt, record the candidate filename and
SHA-256, read the entire chip back, compare it to that candidate, and record
the readback SHA-256. Keep the proven 07-26 ROM available for recovery.
For example, after the operator's full-chip readback:

```sh
sha256sum build/g4del_isolation/08-02_with_07-26_descriptor.rom readback.bin
cmp build/g4del_isolation/08-02_with_07-26_descriptor.rom readback.bin
```

1. Baseline is already reported: 07-26 powers on after the failed 08-02
   attempt. Save a new BMC power-state and SEL snapshot for this baseline.
2. Test `08-02_with_07-26_descriptor.rom`. If host power returns, test
   `08-02_hap_only.rom` next, then `08-02_unlock_only.rom` only if needed.
   A HAP-only success identifies a narrower possible permanent setting; a
   combined-only success requires further power/boot-policy investigation.
3. If the combined-descriptor image does not power on, test
   `07-26_with_08-02_descriptor.rom` and
   `07-26_with_08-02_nonbios.rom`. If the first also fails, investigate the
   descriptor policy. If it powers on, the current BIOS or interaction with
   restored GbE/Device Exp1 is implicated.
4. Use `07-26_with_factory_gbe_pt.rom` to separate factory-region effects;
   `08-02_with_07-26_nonbios.rom` isolates the new BIOS against all working
   non-BIOS bytes. If the new BIOS remains the differentiator, compare the
   Gooxi board target's bootblock, romstage and FSP-M configuration against
   the working ASRock-based build before examining EDK2.
5. Once host power and coreboot output return, capture SOL from reset and BMC
   video. Require FSP, SEC, PEI, DXE, menu input and Shell interaction for
   final acceptance. A powered-on image that still lacks SOL may have a UART
   routing problem; the fan and BMC state must be recorded separately.

For each candidate, save the BMC chassis status and system event log both
before and after the power attempt, any OEM POST/power-fault code shown by the
BMC, the timestamp, flash programmer command and result, full-chip readback
SHA-256, fan behavior, VGA observation and raw SOL capture. The board owner
uses Redfish. Discover the system and chassis member URLs from the service
root and collections; do not assume that their member IDs are `1` or
`System.Embedded.1`. With a private netrc file readable only by the operator,
these requests are read only:

```sh
G4DEL_REDFISH=https://BMC_ADDRESS
G4DEL_NETRC=/path/to/private/bmc.netrc
curl --fail --silent --show-error --netrc-file "$G4DEL_NETRC" \
  "$G4DEL_REDFISH/redfish/v1" -o service-root.json
curl --fail --silent --show-error --netrc-file "$G4DEL_NETRC" \
  "$G4DEL_REDFISH/redfish/v1/Systems" -o systems.json
curl --fail --silent --show-error --netrc-file "$G4DEL_NETRC" \
  "$G4DEL_REDFISH/redfish/v1/Chassis" -o chassis.json
```

Save the discovered System and Chassis members before and after each power
attempt. The System member normally contains `PowerState`, `Status`, and
possibly `BootProgress`; the Chassis member may expose fan or power status.
Follow their `LogServices` links and save each service's `Entries` collection
for event and POST evidence. Use the BMC's trusted CA certificate or local
trust store for TLS; if it uses a self-signed certificate, record how its
identity was checked before relying on the response. Keep credentials and
raw BMC inventory outside the repository.

No live SEL, POST, or power measurements are available yet. The result of
each generated candidate is **pending**. The generated manifest was removed
with `build/`; regenerate it from the pinned source ROMs before board testing.
Source fixes and a final descriptor policy should follow the discriminating
board result, not the static image comparison alone.

Record each attempt in this form so the power result stays tied to the exact
image:

| Timestamp | Candidate SHA-256 | Readback SHA-256 | BMC before/after power state | Fan/VGA/SOL result | SEL and POST evidence path |
| --- | --- | --- | --- | --- | --- |
| 2026-10-24, reported baseline | `50001ff4303e7852b14f9e0ec7013d8e0db6cc9737db2f64f94f201bd4a0789d` | User reports verified | Host powers on after failed flash | Fans/SOL previously observed | Awaiting capture |
| 2026-10-24, reported failure | `cb0aae73fcaefdbf0d80d2e3ce05b6cc21709ea21e2dfb7b7323a7399a345d66` | User reports verified | BMC reachable, host power fails | No fans/VGA/SOL | Awaiting capture |
| First controlled test | `6f5261f5434c68753b8925e878d71847b8fe32c7e086bffbe730b397088def5b` | Pending | Pending | Pending | Pending |
