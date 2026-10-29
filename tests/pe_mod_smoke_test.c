#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "pe_game.h"
#include "pe_mod_host.h"
#include "pe_mod_menu.h"
#include "pe_economy.h"
#include "pe1/item_ids.h"
SaveRuntimeState g_SaveRuntimeState;
GameStateBlock g_GameStateBlock;
BattleEnemySlot g_BattleEnemySlots[7];
int g_BattleModeState;
static ItemDataRecord smoke;
static int loaded=1,battle,economy=1,limit=50;
static uint32_t money=100,price=15;
static PeHookFn hooks[PE_HOOK_COUNT];
static PeMenuPerFrame tick;
static PeMenuAction buy;
int pe_game_loaded(void) { return loaded; }
int pe_in_battle(void) { return battle; }
ItemDataRecord *pe_base_item(unsigned id) { assert(id==ITEM_ITEM24); return &smoke; }
int Inv_GetAyaSlotLimit(void) { return limit; }
int pe_loot_enabled(void) { return economy; }
uint32_t pe_loot_price(uint32_t base) { assert(base==15); return price; }
uint32_t pe_loot_junk(void) { return money; }
int pe_loot_spend_junk(uint32_t n) { if(money<n)return 0; money-=n; return 1; }
void pe_log(const char *tag,const char *fmt,...) { (void)tag;(void)fmt; }
void pe_toast(const char *text) { (void)text; }
int pe_hook_add(int id,PeHookFn fn,void *u,int p,const char *m) { (void)u;(void)p;(void)m;hooks[id]=fn;return 0; }
int pe_menu_register_tick(PeMenuPerFrame fn) { tick=fn;return 0; }
int pe_menu_register_info(const char *p,const char *l,const char *d,PeMenuText t) { (void)p;(void)l;(void)d;(void)t;return 0; }
int pe_menu_register_action(const char *p,const char *l,const char *d,PeMenuAction a) { (void)p;(void)l;(void)d;buy=a;return 0; }
void pe_smoke_register(const char *);
void pe_smoke_shutdown(void);
static void clear(void) {
    memset(g_AyaInventoryItems,0,sizeof g_AyaSaveState.inventory_items);
    memset(g_BattleEnemySlots,0,sizeof g_BattleEnemySlots);
    battle=0;loaded=1;economy=1;price=15;money=100;g_GameStateFlags=0;tick();
}
int main(void) {
    smoke.flags=0x40;pe_smoke_register("pe_economy");clear();assert(smoke.flags==0x42);
    void *text=NULL;PeHookTextLookup lookup={8,ITEM_ITEM24-1,&text};hooks[PE_HOOK_TEXT_LOOKUP](&lookup,NULL);
    assert(text && *(u8 *)text==0x22);
    buy();assert(money==100 && !g_AyaInventoryItems[0]);buy();assert(money==85 && g_AyaInventoryItems[0]==ITEM_ITEM24);
    clear();price=9;buy();price=10;buy();assert(money==100);buy();assert(money==90);
    clear();money=14;buy();buy();assert(money==14 && !g_AyaInventoryItems[0]);
    clear();for(int i=0;i<50;++i)g_AyaInventoryItems[i]=6;buy();buy();assert(money==100);
    clear();economy=0;buy();buy();assert(money==100 && !g_AyaInventoryItems[0]);
    clear();battle=1;buy();buy();assert(money==100 && !g_AyaInventoryItems[0]);
    clear();buy();tick();buy();assert(money==100); /* confirmation cancelled by gameplay */
    clear();int item=ITEM_ITEM24;battle=1;g_BattleModeState=1;
    hooks[PE_HOOK_ITEM_USE](&item,NULL);assert(g_BattleModeState==4);
    economy=0;g_BattleModeState=1;hooks[PE_HOOK_ITEM_USE](&item,NULL);assert(g_BattleModeState==4);
    clear();battle=1;g_BattleModeState=1;
    g_BattleEnemySlots[6].active=1;g_BattleEnemySlots[6].combatant.hpAlive=10;
    g_BattleEnemySlots[6].combatant.statusFlags2=0x40000;tick();assert(smoke.flags==0x40);
    for(int i=0;i<49;++i)g_AyaInventoryItems[i]=6; /* consumed grenade freed last slot */
    hooks[PE_HOOK_ITEM_USE](&item,NULL);assert(g_BattleModeState==1 && g_AyaInventoryItems[49]==ITEM_ITEM24);
    g_BattleEnemySlots[6].combatant.hpAlive=0;tick();assert(smoke.flags==0x42);
    hooks[PE_HOOK_ITEM_USE](&item,NULL);assert(g_BattleModeState==4); /* dead boss cannot block */
    g_BattleEnemySlots[6].combatant.hpAlive=10;g_BattleEnemySlots[6].active=0;
    g_BattleModeState=1;hooks[PE_HOOK_ITEM_USE](&item,NULL);assert(g_BattleModeState==4);
    item=ITEM_MEDICINE_1;g_BattleModeState=1;hooks[PE_HOOK_ITEM_USE](&item,NULL);assert(g_BattleModeState==1);
    item=ITEM_ITEM24;battle=0;hooks[PE_HOOK_ITEM_USE](&item,NULL);assert(g_BattleModeState==1);
    loaded=0;battle=1;hooks[PE_HOOK_ITEM_USE](&item,NULL);assert(g_BattleModeState==1);
    pe_smoke_shutdown();assert(smoke.flags==0x40);
    puts("Passed native Smoke Grenade checks: escape/boss/dead-enemy rules, blocked-use refund, labels, item flags, purchase pricing/confirmation/capacity/guards and cleanup.");
    return 0;
}
