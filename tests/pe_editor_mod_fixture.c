/* Synthetic fixture/oracle using the real host serializer. No game/save data.
 * Compile with pe_save.c and the same includes as run_pe_save_test.ps1.
 * write <file>: native fixture; read <file>: verify the Rust editor's output. */
#define main host_regression_main
#include "pe_mod_save_test.c"
#undef main
#include "../mods/pe_qol/pe_inventory.h"

static struct { uint16_t count, pad, ids[32]; } actual_keys;
static uint32_t actual_sort[6];
static uint8_t opaque[512];
static struct {
    uint32_t junk;
    uint16_t steal_bank, hits[7], rare_dry, rare_carry;
    uint32_t rng;
    uint16_t charm_left;
    uint8_t preset, disabled, econ_since, permit_since;
} actual_loot;
_Static_assert(sizeof actual_loot == 36, "pe_economy.loot v1");
static const PeSaveRecord loot_record = {"pe_economy.loot",1,&actual_loot,sizeof actual_loot};
static const PeSaveRecord actual_key_record = {"pe_qol.keys",1,&actual_keys,sizeof actual_keys};
static const PeSaveRecord actual_sort_record = {"pe_qol.sort",1,actual_sort,sizeof actual_sort};
static const PeSaveRecord opaque_record = {"other_author.future",9,opaque,sizeof opaque};
int main(int argc, char **argv) {
    assert(argc==3);
    for (int i=0;i<512;++i) opaque[i]=(uint8_t)(i*7+3);
    actual_keys.count=2; actual_keys.ids[0]=0x38; actual_keys.ids[1]=0xc8;
    actual_sort[1]=0x800923f8u; actual_sort[2]=0x80092458u;
    s_current=NULL;
    assert(!pe_save_register(&actual_key_record));
    assert(!pe_save_register(&actual_sort_record));
    assert(!pe_save_register(&opaque_record));
    int with_junk = strstr(argv[1], "junk") != NULL;
    if (with_junk) {
        actual_loot.junk=4321;actual_loot.rare_dry=17;actual_loot.preset=2;actual_loot.econ_since=3;
        assert(!pe_save_register(&loot_record));
    }
    if (!strncmp(argv[1],"write",5)) {
        pe_inventory_save(s_card,0x1e90,&actual_keys,sizeof actual_keys,0x59454b50u,0x6b657973u);
        pe_inventory_save(s_card,0x1ef0,actual_sort,sizeof actual_sort,0x54524f53u,0x736f7274u);
        pe_save_on_write(s_card);
        FILE *f=fopen(argv[2],"wb");assert(f);assert(fwrite(s_card,1,sizeof s_card,f)==sizeof s_card);fclose(f);
    } else {
        assert(!strcmp(argv[1],"read") || !strcmp(argv[1],"read-junk"));
        FILE *f=fopen(argv[2],"rb");assert(f);assert(fread(s_card,1,sizeof s_card,f)==sizeof s_card);fclose(f);
        memset(&actual_keys,0,sizeof actual_keys);memset(actual_sort,0,sizeof actual_sort);memset(opaque,0,sizeof opaque);
        pe_save_on_read(s_card);
        if (with_junk) {
            assert(actual_loot.junk==12345 && actual_loot.rare_dry==17 && actual_loot.preset==2 && actual_loot.econ_since==3);
        } else {
            assert(actual_keys.count==2 && actual_keys.ids[0]==0x39 && actual_keys.ids[1]==0xcc);
            assert(actual_sort[0]==1);
        }
        for (int i=0;i<512;++i) assert(opaque[i]==(uint8_t)(i*7+3));
        puts("Native host accepted editor records; unknown payload survived.");
    }
    return 0;
}