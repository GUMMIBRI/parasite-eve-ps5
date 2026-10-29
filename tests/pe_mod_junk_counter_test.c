#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "pe_economy.h"
#include "pe_mod_host.h"
#include "pe_mod_menu.h"
#include "pe1/menu_widget.h"
#include "pe1/draw_state.h"
int g_TextCursorX, g_TextCursorY, g_DrawTextDimmed;
static PeHookFn draw;
static int *toggle, enabled=1, loaded=1, panels, label_x,label_y,digit_x,digit_y;
static unsigned balance, printed;
static int digit_count;
static MenuWidgetNode inventory, parent;
int pe_game_loaded(void) { return loaded; }
int pe_loot_enabled(void) { return enabled; }
uint32_t pe_loot_junk(void) { return balance; }
MenuWidgetNode *MenuWidget_FindByModeAndSelectedBase(int m,int s) {
    assert(m==2 && s==1);return &inventory;
}
int pe_menu_register_toggle(const char *p,const char *l,const char *d,int *f,PeMenuPerFrame t) {
    (void)l;(void)d;assert(!strcmp(p,"Inventory") && !t);toggle=f;return 0;
}
int pe_hook_add(int id,PeHookFn fn,void *u,int p,const char *m) {
    (void)u;(void)p;(void)m;assert(id==PE_HOOK_INVENTORY_HEADER);draw=fn;return 0;
}
void Draw_SetTextDimmed(int n) { g_DrawTextDimmed=n; }
void Draw_PrintRawText(u8 *s) {
    assert(s[0]==0x19 && s[4]==0xFF);label_x=g_TextCursorX;label_y=g_TextCursorY;
}
void Draw_EmitDigitSprite(int n) {
    assert(n>=0 && n<=9);if(!digit_count) { digit_x=g_TextCursorX;digit_y=g_TextCursorY; }
    ++digit_count;printed=printed*10+n;
}
void Draw_AllocColorGradient(int w,int h,u8 *p,int t) {
    (void)p;(void)t;assert(w==118 && h==16 && g_TextCursorX==28);++panels;
}
void pe_junk_counter_register(const char *);
int main(void) {
    pe_junk_counter_register("test");assert(*toggle==1);
    parent.x=28;inventory.parent=&parent;
    MenuWidgetNode header={0};header.x=154;header.y=164;header.visible_rows=16;
    g_TextCursorX=269;g_TextCursorY=170;g_DrawTextDimmed=1;
    unsigned values[]={0,9,10,999,99999};
    for(unsigned i=0;i<5;++i) {
        balance=values[i];printed=0;digit_count=0;draw(&header,NULL);
        assert(printed==balance && digit_count>=1 && digit_count<=5);
        assert(label_x==38 && label_y==166 && digit_y==170);
        assert(digit_x>label_x+32 && digit_x+digit_count*5==136);
        assert(g_TextCursorX==269 && g_TextCursorY==170 && g_DrawTextDimmed==1);
    }
    header.y=84;digit_count=0;draw(&header,NULL);assert(label_y==86 && digit_y==90);
    /* An odd bag on its last page: the header sits beside the last item; the counter stays below. */
    inventory.has_scroll=1;inventory.y_limit=5;inventory.visible_rows=5;inventory.scroll_y=0;
    digit_count=0;draw(&header,NULL);assert(label_y==102 && digit_y==106);
    inventory.y_limit=12;inventory.visible_rows=8;inventory.scroll_y=2;   /* not the last page: native spot */
    digit_count=0;draw(&header,NULL);assert(label_y==86 && digit_y==90);
    inventory.has_scroll=0;
    int before=panels;*toggle=0;draw(&header,NULL);assert(panels==before);
    *toggle=1;enabled=0;draw(&header,NULL);assert(panels==before);
    enabled=1;loaded=0;draw(&header,NULL);assert(panels==before);
    loaded=1;inventory.parent=NULL;draw(&header,NULL);assert(panels==before);
    puts("Junk counter checks passed: zero/full balance, placement, odd bags, drawing state, toggle, economy/load guards.");
}
