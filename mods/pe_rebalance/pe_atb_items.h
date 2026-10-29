#ifndef PE_ATB_ITEMS_H
#define PE_ATB_ITEMS_H
#include <stdint.h>
/* Owned by the half-bar feature; the PE rules page edits these together with
 * healing/cost settings, as in the recomp. No dependency on another mod. */
extern int g_pe_half_enabled;
extern int g_pe_half_allow[5];
extern uint32_t g_pe_quick_mask;
void pe_atb_items_register(const char *mod_id);
void pe_atb_items_shutdown(void);
#endif
