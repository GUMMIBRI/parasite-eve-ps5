#ifndef PE_PANACEA_H
#define PE_PANACEA_H
/* Settings owned by the rebalance mod, shared by its effects and recipes. */
extern int g_pe_panacea;
void pe_panacea_register(const char *mod_id);
void pe_panacea_shutdown(void);
void pe_combine_register(const char *mod_id);
#endif
