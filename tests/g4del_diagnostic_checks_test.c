#include <Library/G4delDiagnosticChecks.h>

#include <assert.h>
#include <stdio.h>

static void test_ranges(void)
{
  assert(G4delDiagRangeValid(0x1000, 0x1000));
  assert(!G4delDiagRangeValid(0, 0));
  assert(!G4delDiagRangeValid(UINT64_MAX - 1, 4));
  assert(G4delDiagRangesOverlap(0x1000, 0x1000, 0x1800, 0x1000));
  assert(!G4delDiagRangesOverlap(0x1000, 0x1000, 0x2000, 0x1000));
  assert(G4delDiagRangesOverlap(UINT64_MAX - 1, 4, 0, 2));
}

static void test_hobs(void)
{
  assert(G4delDiagHobHeaderValid(1, 16, 32, 0xffff, 8));
  assert(G4delDiagHobHeaderValid(0xffff, 8, 8, 0xffff, 8));
  assert(!G4delDiagHobHeaderValid(1, 7, 32, 0xffff, 8));
  assert(!G4delDiagHobHeaderValid(1, 33, 32, 0xffff, 8));
  assert(!G4delDiagHobHeaderValid(0xffff, 16, 16, 0xffff, 8));
  assert(G4delDiagHobHeaderReason(1, 8, 8, 0xffff, 8) == G4delDiagHobHeaderValidReason);
  assert(G4delDiagHobHeaderReason(1, 7, 32, 0xffff, 8) == G4delDiagHobHeaderTooShort);
  assert(G4delDiagHobHeaderReason(1, 16, 8, 0xffff, 8) == G4delDiagHobHeaderExceedsRemaining);
  assert(G4delDiagHobHeaderReason(0xffff, 16, 32, 0xffff, 8) == G4delDiagHobEndMarkerWrongLength);
  assert(G4delDiagHobHeaderReason(0xffff, 8, 7, 0xffff, 8) == G4delDiagHobHeaderExceedsRemaining);
}

static void test_acpi_checksum(void)
{
  const uint8_t valid[] = { 1, 2, 253 };
  const uint8_t invalid[] = { 1, 2, 254 };
  assert(G4delDiagChecksumValid(valid, sizeof(valid)));
  assert(!G4delDiagChecksumValid(invalid, sizeof(invalid)));
  assert(!G4delDiagChecksumValid(NULL, 0));
}

static void test_framebuffer(void)
{
  assert(G4delDiagFramebufferValid(0x94000000, 0x300000, 1024, 768, 1024, 4));
  assert(!G4delDiagFramebufferValid(0x94000000, 0x1000, 1024, 768, 1024, 4));
  assert(!G4delDiagFramebufferValid(UINT64_MAX - 1, 4, 640, 480, 640, 4));
  assert(!G4delDiagFramebufferValid(0x1000, 0x10000, 640, 480, 639, 4));
}

static void test_boot_gate(void)
{
  assert(G4delDiagBootAllowed(0, false));
  assert(!G4delDiagBootAllowed(1, false));
  assert(G4delDiagBootAllowed(1, true));
}

int main(void)
{
  test_ranges();
  test_hobs();
  test_acpi_checksum();
  test_framebuffer();
  test_boot_gate();
  puts("G4DEL diagnostic validator tests: PASS");
  return 0;
}
