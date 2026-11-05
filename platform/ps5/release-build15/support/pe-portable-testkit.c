/* SPDX-License-Identifier: MIT -- public candidate disables optional private seed. */
#include "pe-portable-testkit.h"
#if defined(PE_NATIVE_TESTKIT_ENABLED) && PE_NATIVE_TESTKIT_ENABLED
#error "Public candidate forbids private test-kit seeding"
#endif
int PePortable_SeedTestCard(const char *source,const char *destination){(void)source;(void)destination;return -1;}
