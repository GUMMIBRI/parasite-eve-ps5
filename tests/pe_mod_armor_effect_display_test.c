#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "pe_mod_host.h"
#include "pe1/draw_state.h"
#include "pe1/inventory.h"
#include "pe1/menu_widget.h"
int g_TextCursorX=20,g_TextCursorY=30,g_InvItemUsableFlag;
ItemDataRecord *g_MenuActionItemData;
static ItemDataRecord item;
static MenuWidgetNode node;
static PeHookFn icon,help;
static unsigned calls;
static u8 glyphs[4];
int pe_hook_add(int id,PeHookFn fn,void *u,int p,const char *m) {
    (void)u;(void)p;(void)m;
    if(id==PE_HOOK_EQUIP_EFFECT) icon=fn;
    else {assert(id==PE_HOOK_HELP_TEXT);help=fn;} return 0;
}
void Draw_PrintRawText(u8 *s) {
    assert(g_TextCursorX==22 && g_TextCursorY==32);memcpy(glyphs,s,4);++calls;
}
MenuWidgetNode *MenuWidget_GetCurrentNode(void) { return &node; }
int MenuWidget_GridCellIndex(MenuWidgetNode *n) { return n->cursor_y; }
int Inv_RestoreSelection(unsigned int n) { assert(n==1);return 7; }
ItemDataRecord *Inv_LookupActiveListData(int n) { assert(n==7);return &item; }
void pe_armor_effect_display_register(const char *);
int main(void) {
    pe_armor_effect_display_register("test");
    static const char *badges[]={"BRG","SCV","LCK","SLV"};
    for(int armor=0;armor<2;++armor) for(int code=0;code<32;++code) {
        PeHookEquipEffect e={code,armor};unsigned before=calls;
        int handled=icon(&e,NULL),expected=armor && code>=17 && code<=20;
        assert(handled==expected && calls==before+expected);
        assert(g_TextCursorX==20 && g_TextCursorY==30);
        if(expected) for(int i=0;i<3;++i) assert(glyphs[i]==badges[code-17][i]-'A'+0x10);
    }
    g_MenuActionItemData=&item;item.tailCount=1;
    for(int mode=6;mode<=11;mode+=5) for(int code=17;code<=20;++code) {
        node.selected_base=mode;item.tailData[0]=code|0xA0;
        u8 old=0xFF,*text=&old;help(&text,NULL);assert(text!=&old && text[0]!=0xFF);
    }
    u8 old=0xFF,*text=&old;
    g_InvItemUsableFlag=1;help(&text,NULL);assert(text==&old);
    g_InvItemUsableFlag=0;item.tailData[0]=7;help(&text,NULL);assert(text==&old);
    node.cursor_y=1;help(&text,NULL);assert(text==&old);
    puts("Armor display checks passed: all four badges, vanilla/weapon passthrough, help names, cursor state, slot bounds.");
}
