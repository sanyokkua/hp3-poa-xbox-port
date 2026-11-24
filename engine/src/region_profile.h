#ifndef HPPOA_REGION_PROFILE_H
#define HPPOA_REGION_PROFILE_H
/* An app contains statically translated code for exactly one executable:
 * Harry Potter and the Prisoner of Azkaban, NTSC-US, title 0x4541004C, XDK 5849. */
#define HPPOA_XBE_SIZE 3190784ull
#define HPPOA_REGION_NAME "NTSC-US"
#define HPPOA_XBE_SHA1 "51b09e01169fda86f2cb7e5ce45ee80a10268918"
#define HPPOA_ENTRY_POINT 0x000111DBu
/* The DSOUND library section (virtual address, virtual size): its MmGetPhysicalAddress callers are tagged for the APU. */
#define HPPOA_DSOUND_LO 0x002B66E0u
#define HPPOA_DSOUND_SIZE 37428u
#endif
