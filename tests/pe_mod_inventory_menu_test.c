#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "pe_inventory.h"
#include "pe_mod_host.h"
#include "pe_mod_menu.h"
#include "pe1/field_pad.h"
#include "pe1/menu_equipment.h"
#include "pe1/menu_inventory.h"
#include "pe1/menu_state.h"
#include "pe1/menu_widget.h"
unsigned int g_FieldPadBits;
GameStateBlock g_GameStateBlock;
int g_MenuItemContextFlag, g_MenuActionSubmenuOpen, g_InvSelectedItemIndex;
int g_MenuEquipMode;
MenuWidgetNode *g_MenuWidgetActiveListHead;
static MenuWidgetNode main_list, panel, list, *current;
static int loaded=1, battle, battle_equip, opens, destroyed, header_closed, rows;
static PeHookFn hooks[PE_HOOK_COUNT];
SaveRuntimeState g_SaveRuntimeState;
static int *tips, toasts, auto_sort, cell;
int pe_menu_register_toggle(const char *p,const char *l,const char *d,int *v,PeMenuPerFrame t) {
    (void)p;(void)l;(void)d;(void)t; tips=v; return 0;
}
int MenuWidget_GridCellIndex(MenuWidgetNode *n) { (void)n; return cell; }
int Inv_GetAyaSlotLimit(void) { return 10; }
int pe_bag_sort_auto(void) { return auto_sort; }
void pe_toast(const char *t) { assert(strstr(t, "Square")); toasts++; }
int pe_hook_add(int id, PeHookFn fn, void *u, int p, const char *m) {
    (void)u; (void)p; (void)m; hooks[id]=fn; return 0;
}
int pe_game_loaded(void) { return loaded; }
int pe_in_battle(void) { return battle; }
int Menu_GetBattleEquipMode(void) { return battle_equip; }
void Menu_DrawItemList(MenuWidgetNode *n) { (void)n; }
void Menu_DrawSelectableEquipSlotList(MenuWidgetNode *n) { (void)n; }
int Menu_InventoryInputHandler(MenuWidgetNode *n, unsigned int f) { (void)n;(void)f;return 0; }
MenuWidgetNode *MenuWidget_GetCurrentNode(void) { return current; }
void MenuWidget_SetCurrentNode(MenuWidgetNode *n) { current=n; }
void MenuWidget_DestroyNode(MenuWidgetNode *n) { assert(n==&panel); destroyed++; current=n->parent; }
void MenuWidget_NavScrollTo(int id) { assert(id==0x1B); header_closed++; }
void Menu_CreateInvSwapView(MenuWidgetNode *n, int mode) {
    assert(n==&main_list && mode==0 && current==n); opens++; current=NULL;
}
void Draw_SetPrimCallback(MenuWidgetNode *n, int count) {
    PeHookListRows c={n,&count}; hooks[PE_HOOK_LIST_ROWS](&c,NULL);
    /* Catalog appends its row after the early hide hook. */
    rows=count+1;
}
static void frame(unsigned pad) { g_FieldPadBits=pad; hooks[PE_HOOK_FRAME](NULL,NULL); }
static void inventory(void) {
    memset(&main_list,0,sizeof main_list); memset(&panel,0,sizeof panel); memset(&list,0,sizeof list);
    main_list.draw=Menu_DrawItemList; panel.parent=&main_list; panel.mode=panel.selected_base=1;
    panel.update=Menu_InventoryInputHandler; list.parent=&panel;
    list.draw=Menu_DrawSelectableEquipSlotList; list.appearance.cursorTargetX=-1;
    current=&list; g_MenuWidgetActiveListHead=&main_list;
    g_GameStateFlags=4; g_InvSelectedItemIndex=-1;
}
int main(void) {
    pe_inventory_menu_register("test"); inventory(); g_MenuItemContextFlag=0x1FF;
    Draw_SetPrimCallback(&main_list,8); assert(rows==8 && g_MenuItemContextFlag==0x1BF);
    Draw_SetPrimCallback(&main_list,7); assert(rows==8);
    frame(0x80000000u); assert(opens==1 && destroyed==1 && header_closed==1);
    inventory(); frame(0x80000000u); assert(opens==1); /* held Square cannot reopen */
    frame(0); g_MenuActionSubmenuOpen=1; frame(0x80000000u); assert(opens==1);
    g_MenuActionSubmenuOpen=0; frame(0); g_InvSelectedItemIndex=0;
    frame(0x80000000u); assert(opens==1); g_InvSelectedItemIndex=-1;
    frame(0); list.appearance.cursorTargetX=0; frame(0x80000000u); assert(opens==1);
    list.appearance.cursorTargetX=-1; frame(0); g_MenuEquipMode=1;
    frame(0x80000000u); assert(opens==1); g_MenuEquipMode=0;
    frame(0); battle=1; frame(0x80000000u); assert(opens==1); battle=0;
    frame(0); battle_equip=1; frame(0x80000000u); assert(opens==1); battle_equip=0;
    frame(0); panel.selected_base=0x33; frame(0x80000000u); assert(opens==1);
    panel.selected_base=1; frame(0); frame(0x80000000u); assert(opens==2);
    hooks[PE_HOOK_SAVE_WRITE](NULL,NULL); assert(g_MenuItemContextFlag==0x1FF);
    frame(0); assert(g_MenuItemContextFlag==0x1BF && rows==8);
    g_GameStateFlags=0; frame(0); assert(g_MenuItemContextFlag==0x1FF);
    inventory(); g_MenuItemContextFlag=0x1BF;
    Draw_SetPrimCallback(&main_list,7); pe_inventory_menu_shutdown(); assert(g_MenuItemContextFlag==0x1BF);
    g_MenuItemContextFlag=0x1FF; Draw_SetPrimCallback(&main_list,8);
    g_MenuItemContextFlag=3; pe_inventory_menu_shutdown(); assert(g_MenuItemContextFlag==3);
    battle=1; g_MenuItemContextFlag=0x1FF; Draw_SetPrimCallback(&main_list,5);
    assert(g_MenuItemContextFlag==0x1FF && rows==6); battle=0;

    /* Tips: the bag's first opening this session, the Items row's and an empty slot's title. */
    assert(tips && *tips==1 && toasts==1);           /* shown once, on the first bag frame above */
    inventory(); frame(0); assert(toasts==1);
    u8 *title=NULL; cell=0; g_AyaInventoryItems[0]=0;
    hooks[PE_HOOK_HELP_TEXT](&title,NULL); assert(title);                      /* empty bag slot */
    title=NULL; g_AyaInventoryItems[0]=6; hooks[PE_HOOK_HELP_TEXT](&title,NULL); assert(!title);   /* an item: its own text */
    current=&main_list; g_MenuItemContextFlag=0x1BF; cell=0;
    title=NULL; hooks[PE_HOOK_HELP_TEXT](&title,NULL); assert(title);          /* the Items row */
    title=NULL; cell=1; hooks[PE_HOOK_HELP_TEXT](&title,NULL); assert(!title); /* another row */
    *tips=0; cell=0; title=NULL; hooks[PE_HOOK_HELP_TEXT](&title,NULL); assert(!title);
    puts("Inventory menu checks passed: Square edge/guards, row counts with Catalog, save/exit restoration, scripted masks, sort tips.");
    return 0;
}
