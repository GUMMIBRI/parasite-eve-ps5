/* Catalog regression fixture: production menu code, isolated rendering/economy.
 * Inject stock cursor outcomes to exercise wrap, rapid input and page jumps. */
#include <assert.h>
#include <stdio.h>
#include "../mods/host/pe_main_menu.c"
#include "../mods/pe_economy/pe_catalog_menu.c"
int pe_in_battle(void) { return 0; }
const char *pe_mod_current(void) { return NULL; }
int pe_mod_active(const char *m) { (void)m; return 1; }
int g_MenuItemContextFlag, g_MenuBattleStatusOverlayActive;
MenuWidgetNode *g_MenuWidgetActiveListHead;
/* the host's Yes / No (pe_confirm): not used by the Catalog */
u8 g_MenuItemLabelBuffer[128];
int g_MenuItemPrimListHandles[2], g_MenuActionTextBase;
MenuConfirmCallback g_MenuConfirmCallback;
void Menu_DrawItemLabel(MenuWidgetNode *n) { (void)n; }
void Menu_DrawActionOptionList(MenuWidgetNode *n) { (void)n; }
int Menu_ConfirmDialogHandler(MenuWidgetNode *n, unsigned int f) { (void)n; (void)f; return 1; }
int Draw_MeasureTextWidth(u8 *t) { (void)t; return 0; }
/* the host's icon helpers: not used by the Catalog */
unsigned char *g_DrawPacketCursor, *g_DrawPacketArenaBase;
unsigned int g_DrawPrimColor, g_DrawColorShaded, *g_DrawOrderingTableEntry;
int g_DrawTextDimmed, g_TextCursorX, g_TextCursorY;
DrawGlyphDescriptor *Draw_LookupGlyphDescriptor(int i) { (void)i; return NULL; }
void Draw_OffsetCursor(int x, int y) { (void)x; (void)y; }
void Draw_AllocSprite(int i) { (void)i; }
static MenuWidgetNode panel, list;
static int next_row, next_top, next_adjust;
int Menu_StepScrollCursor(MenuWidgetNode *n, unsigned int b) {
    (void)b; n->cursor_y=next_row; n->scroll_y=next_top;
    n->scroll_adjust=next_adjust; return 1;
}
int MenuWidget_GridCellIndex(MenuWidgetNode *n) { return n->cursor_y; }
MenuWidgetNode *MenuWidget_GetCurrentNode(void) { return &list; }
void MenuWidget_SetCurrentNode(MenuWidgetNode *n) { (void)n; }
void MenuWidget_DestroyNode(MenuWidgetNode *n) { (void)n; }
MenuWidgetNode *MenuWidget_CreateSimpleNode(int m,MenuWidgetNode *p,MenuWidgetNode *a,int x) {
    (void)m;(void)p;(void)a;(void)x;return &panel;
}
MenuWidgetNode *MenuWidget_CreateNode(int m,MenuWidgetNode *p,MenuWidgetNode *a) {
    (void)m;(void)p;(void)a;return &list;
}
MenuWidgetSimpleDescriptor *MenuWidget_LookupSimpleDescriptor(unsigned int i) {
    static MenuWidgetSimpleDescriptor d = { 28, 24, 252, 0 }; (void)i; return &d;
}
void Draw_SetPrimCallback(MenuWidgetNode *n,int rows) {
    n->y_limit=rows; n->visible_rows=rows<8?rows:8;
}
void MenuWidget_DrawList(MenuWidgetNode *n,void (*f)(int)) { (void)n;(void)f; }
void Draw_SetTextDimmed(int d) { (void)d; }
void Draw_PrintRawText(u8 *s) { (void)s; }
void Menu_DrawItemList(MenuWidgetNode *n) { (void)n; }
void Menu_PlayErrorSound(void) {}
void Menu_PlayConfirmSound(void) {}
void Menu_PlayCancelSound(void) {}
int Menu_GetBattleEquipMode(void) { return 0; }
int pe_game_loaded(void) { return 1; }
int pe_loot_enabled(void) { return 1; }
uint32_t pe_loot_junk(void) { return 100; }
static ItemDataRecord equipped_record = { .iconId = 1, .itemId = 0x45, .kind = 1, .tailCount = 3 };
ItemDataRecord *pe_cat_record(int w) { (void)w;return &equipped_record; }
const char *pe_cat_item_name(int id) { (void)id;return "M9"; }
const char *pe_cat_effect_name(int w,int e) { (void)w;(void)e;return "Rate of Fire 2"; }
int pe_cat_slots(int w) { (void)w;return 3; }
int pe_cat_slot_effect(int w,int s) { (void)w;(void)s;return 0; }
int pe_cat_board_count(int w) { (void)w;return 20; }
int pe_cat_board_effect(int w,int i) { (void)w;return i+1; }
int pe_cat_unlocked(int w,int e) { (void)w;(void)e;return 1; }
uint32_t pe_cat_price(int w,int e) { (void)w;(void)e;return 10; }
int pe_cat_needs_tool(int w,int e) { (void)w;(void)e;return 0; }
int pe_cat_install(int w,int s,int e,int d) { (void)w;(void)s;(void)e;(void)d;return 0; }
int pe_cat_remove(int w,int s,int d) { (void)w;(void)s;(void)d;return 0; }
void pe_log(const char *t,const char *f,...) { (void)t;(void)f; }
void pe_mod_log(const char *t,const char *f,...) { (void)t;(void)f; }
int pe_hook_add(int i,PeHookFn f,void *u,int p,const char *m) {
    (void)i;(void)f;(void)u;(void)p;(void)m;return 0;
}
static void outcome(int row,int top,int adjust,int want_top,int want_adjust) {
    next_row=row;next_top=top;next_adjust=adjust;
    assert(pe_scroll_input(&list,0x4000)==1);
    assert(list.scroll_y==want_top && list.scroll_adjust==want_adjust);
    assert(list.cursor_y>=list.scroll_y && list.cursor_y<list.scroll_y+list.visible_rows);
}
int main(void) {
    s_cat.list=s_screen.list=&list; list.visible_rows_mirror=8;
    pe_set_step(STEP_EFFECT,0);
    outcome(20,0,0,13,0); /* Up wraps first to last. */
    outcome(0,13,0,0,0);  /* Down wraps last to first. */
    outcome(8,1,8,1,8);   /* Ordinary one-row animation stays smooth. */
    outcome(9,1,8,2,0);   /* Repeated input outruns an unfinished animation. */
    outcome(0,2,-8,0,0);  /* Upward page jump keeps selection visible. */
    outcome(20,13,0,13,0);
    list.scroll_adjust=8; pe_set_step(STEP_SLOT,2);
    assert(list.scroll_adjust==0 && list.scroll_y==0 && list.cursor_y==2);
    list.scroll_adjust=-8; pe_set_step(STEP_EFFECT,20);
    assert(list.scroll_adjust==0 && list.scroll_y==13 && list.cursor_y==20);
    pe_set_step(STEP_EFFECT,100);
    assert(list.cursor_y==0 && list.scroll_y==0);
    puts("Catalog scrolling checks passed: end wrapping, rapid input, page jumps, normal animation, step changes.");
    return 0;
}
