#include "pe1/field_pulled_sprite.h"
#include "pe1/room_orbit_spin_burst.h"
#include "pe1/scene_e22_ember_burst.h"
#include "../../src/overlays/scene_e19_cruiser/e19_externs.h"

/* This must agree in every overlay as well as geometry.c: an int zero in
 * Windows x64's eighth argument slot does not initialize a 64-bit pointer. */
_Static_assert(_Generic(&FieldFx_DrawPulledSprite,
    void (*)(GteShortVector *, int, int, int, int, int, int, RenderColor *, int): 1,
    default: 0), "Pulled sprite color must be a pointer in every caller");

static int native_pulled_sprite_packets(void)
{
    struct { FieldStripPacket packet; u8 guard[16]; } buffer;
    static RenderGpuTag ordering[4096];
    RenderBufferPrefix saved=D_800B0E38;
    RenderEffectParameters params=g_FieldFxSpriteParams;
    int saved_slot=g_ActiveDrawSlot, saved_offset=g_DrawPacketOffset;
    GteMatrix *saved_view=g_GeomScroll.words.viewMatrix;
    GteMatrix view={.m={{4096,0,0},{0,4096,0},{0,0,4096}}};
    GteShortVector position={0,0,512};
    RenderColor tint={64,32,16,0};
    g_GeomScroll.words.viewMatrix=&view;
    Psyz_GteCtrlWrite(26,256); gte_SetGeomOffset(160,120);
    g_FieldFxSpriteParams=(RenderEffectParameters){.extent_x=32,.extent_y=32,.tpage=0x100};
    for(int slot=0;slot<2;++slot) for(int colored=0;colored<2;++colored)
    for(int intensity=0;intensity<=192;intensity+=64) {
        memset(&buffer,0xa5,sizeof buffer);
        for(unsigned i=0;i<4096;++i) ordering[i]=(RenderGpuTag){.address=0xffffff};
        D_800B0E38.ordering[slot]=ordering;
        D_800B0E38.packets[slot]=(char *)&buffer;
        g_ActiveDrawSlot=slot;g_DrawPacketOffset=0;
        FieldFx_DrawPulledSprite(&position,4096,4096,0x4d,0x123,1,
                                 intensity,colored?&tint:0,0xa00);
        FieldStripPacket *p=&buffer.packet;
        if(p->r0!=(colored?intensity/2:intensity) ||
           p->g0!=(colored?intensity/4:intensity) ||
           p->b0!=(colored?intensity/8:intensity) ||
           p->clut!=0x123 || p->code!=0x2e || p->tag.length!=9 ||
           ordering[14].address!=(uintptr_t)p || g_DrawPacketOffset!=sizeof(*p))return 0;
        for(unsigned i=0;i<sizeof buffer.guard;++i)if(buffer.guard[i]!=0xa5)return 0;
    }
    D_800B0E38=saved;g_FieldFxSpriteParams=params;
    g_ActiveDrawSlot=saved_slot;g_DrawPacketOffset=saved_offset;
    g_GeomScroll.words.viewMatrix=saved_view;
    puts("Pulled sprites: pointer ABI, null/tinted color, fade levels and both draw buffers passed");
    return 1;
}

int CdRom_InitScreenState(void);
void Render_ResetScene(int width,int height);
int Draw_InitDisplay(int mode);
static int original_display_start(void)
{
    Pe1GameState saved=g_GameState;
    GeomScrollRecord scroll=g_GeomScroll;
    const int modes[]={0,1,2,3};
    g_GameState.flags=0;
    Render_ResetScene(320,224);
    for(unsigned i=0;i<4;++i) {
        CdRom_InitScreenState();
        g_SceneDispatchToken=0x12345678;
        g_GameState.scene_init_subphase=0xA5;
        if(Draw_InitDisplay(modes[i]) || g_ActiveDrawSlot || g_ScreenTint.fadeMode!=1 ||
           g_ScreenTint.frame!=2 || g_ScreenTint.duration!=2 ||
           g_SceneInitSubphase || g_GameState.bank_asset_source!=g_BootWorkArea ||
           g_GameState.scene_load_scratch!=g_SceneLoadScratchArea ||
           g_DrawFrameBuffers[0].frontBufferBase!=g_RenderFrontBufferBase[0] ||
           g_DrawFrameBuffers[1].frontBufferBase!=g_RenderBackBufferBase[0] ||
           g_SceneDispatchToken!=(modes[i]==1?0xA80830C8u:modes[i]==3?0xA80651C8u:0x12345678u)) return 0;
        size_t glyph_bytes=TEXTBOX_GLYPH_PACKET_CAPACITY*sizeof(TextboxGlyphPacket);
        if((u8 *)D_800B0E38.other_buffers[2]!=(u8 *)D_800B0E38.other_buffers[1]+glyph_bytes ||
           g_RenderScratchBufferBase[0]!=(u8 *)D_800B0E38.other_buffers[2]+glyph_bytes ||
           g_RenderAnimBufferBase[1]!=g_RenderAnimBufferBase[0]+RENDER_ANIM_BUFFER_BYTES ||
           (u8 *)g_GameState.scene_process_slots!=g_RenderAnimBufferBase[1]+RENDER_ANIM_BUFFER_BYTES)return 0;
        RenderBufferPrefix display_layout=D_800B0E38;
        Boot_InitMemoryLayout();
        if(memcmp(&display_layout,&D_800B0E38,sizeof(display_layout)))return 0;
    }
    g_GameState=saved; g_GeomScroll=scroll;
    puts("Original display start: four modes, memory layout, two fade frames and scene-token selection passed");
    return 1;
}

/* RGB sequences from executing USA Render_SetCDDCSlot 80068E24 in MIPS. */
static int original_startup_fade(void)
{
    static const struct { unsigned duration, count; u8 rgb[8][3]; } cases[]={
        {0,2,{{0,200,255},{255,128,0}}},
        {1,2,{{0,200,255},{255,128,0}}},
        {2,3,{{0,200,255},{255,128,0},{255,128,0}}},
        {4,5,{{0,200,255},{85,176,170},{170,152,85},{255,128,0},{255,128,0}}},
        {7,8,{{0,200,255},{42,188,213},{85,176,170},{127,164,128},{170,152,85},{212,140,43},{255,128,0},{255,128,0}}},
    };
    GeomScrollRecord saved_scroll=g_GeomScroll;
    RenderBufferPrefix saved_buffers=D_800B0E38;
    int saved_slot=g_ActiveDrawSlot;
    RenderGpuTag ordering[2][4];
    int valid=1;
    for(unsigned slot=0;slot<2;++slot) for(unsigned stop=0;stop<2;++stop)
    for(unsigned blend=0;blend<4;++blend)
    for(unsigned c=0;c<sizeof(cases)/sizeof(cases[0]);++c) {
        CdRom_InitScreenState();
        for(unsigned i=0;i<2;++i) {
            const RenderTintTile *tile=&g_ScreenTint.tiles[i];
            if(tile->tag.length!=3 || tile->command!=0x62 || tile->r!=255 ||
               tile->g!=255 || tile->b!=255 || tile->x || tile->y ||
               tile->width!=320 || tile->height!=224) valid=0;
            D_800B0E38.ordering[i]=ordering[i];
        }
        g_ActiveDrawSlot=slot;
        g_ScreenTint.fadeMode=stop?6:2; g_ScreenTint.blendMode=blend;
        g_ScreenTint.duration=cases[c].duration;
        g_ScreenTint.targetR=255; g_ScreenTint.targetG=128; g_ScreenTint.targetB=0;
        g_ScreenTint.startR=0; g_ScreenTint.startG=200; g_ScreenTint.startB=255;
        for(unsigned frame=0;frame<cases[c].count;++frame) {
            for(unsigned i=0;i<2;++i) for(unsigned j=0;j<4;++j)
                ordering[i][j]=(RenderGpuTag){0xFFFFFF,0x9876};
            unsigned inactive=(g_ScreenTint.fadeMode&3)==0;
            Render_SetCDDCSlot();
            if(inactive) {
                if(ordering[slot][3].address!=0xFFFFFF) valid=0;
            } else {
                const RenderTintTile *tile=&g_ScreenTint.tiles[slot];
                const RenderTintMode *mode=&g_ScreenTint.modes[slot];
                if(ordering[slot][3].address!=(uintptr_t)mode ||
                   mode->tag.address!=(uintptr_t)tile || tile->tag.address!=0xFFFFFF ||
                   mode->tag.length!=1 || tile->tag.length!=3 ||
                   mode->command!=(0xE1000400u|(blend<<5)) ||
                   memcmp(&tile->r,cases[c].rgb[frame],3)) valid=0;
            }
            for(unsigned i=0;i<2;++i) for(unsigned j=0;j<4;++j) {
                if(ordering[i][j].length!=0x9876 ||
                   ((i!=slot || j!=3) && ordering[i][j].address!=0xFFFFFF)) valid=0;
            }
        }
        if(g_ScreenTint.fadeMode!=(stop?0:1) ||
           g_ScreenTint.frame!=(cases[c].duration?cases[c].duration:1)) valid=0;
    }
    g_GeomScroll=saved_scroll; D_800B0E38=saved_buffers; g_ActiveDrawSlot=saved_slot;
    if(!valid) fprintf(stderr,"Original startup fade RGB/native links differ from MIPS reference\n");
    return valid;
}

static int native_screen_tint_packets(void)
{
    static RenderGpuTag ordering[4096];
    struct { RenderTintTile tile; RenderTintMode mode; u8 guard[16]; } buffer;
    RenderBufferPrefix saved=D_800B0E38;
    int saved_slot=g_ActiveDrawSlot, saved_offset=g_DrawPacketOffset;
    u8 rgb[3]={200,100,50};
    for(unsigned slot=0;slot<2;++slot) {
        D_800B0E38.ordering[slot]=ordering;
        D_800B0E38.packets[slot]=(char *)&buffer;
        g_ActiveDrawSlot=slot;
        for(unsigned depth=0;depth<4096;++depth) {
            ordering[depth].address=0xffffff;
            ordering[depth].length=0;
        }
        for(int blend=0;blend<5;++blend) for(unsigned depth=0;depth<4096;++depth) {
            memset(&buffer,0xa5,sizeof(buffer));
            g_DrawPacketOffset=0;
            ordering[depth].address=0xffffff;
            if(STRIP_OT(depth)!=&ordering[depth]) return 0;
            FieldFx_DrawScreenTint(rgb,64,blend==4?255:blend,depth);
            RenderTintTile *tile=&buffer.tile;
            if(tile->tag.address!=0xffffff || tile->tag.length!=3 ||
               tile->r!=100 || tile->g!=50 || tile->b!=25 ||
               tile->x || tile->y || tile->width!=320 || tile->height!=240) return 0;
            if(blend==4) {
                if(ordering[depth].address!=(uintptr_t)tile || tile->command!=0x60 ||
                   g_DrawPacketOffset!=sizeof(*tile)) return 0;
                const u8 *unused=(const u8 *)&buffer.mode;
                for(unsigned i=0;i<sizeof(buffer.mode);++i) if(unused[i]!=0xa5) return 0;
            } else {
                if(ordering[depth].address!=(uintptr_t)&buffer.mode ||
                   buffer.mode.tag.address!=(uintptr_t)tile || buffer.mode.tag.length!=1 ||
                   buffer.mode.command!=(u32)(0xe1000200|GetTPage(0,blend,0,0)) ||
                   tile->command!=0x62 ||
                   g_DrawPacketOffset!=sizeof(*tile)+sizeof(buffer.mode)) return 0;
            }
            for(unsigned i=0;i<sizeof(buffer.guard);++i) if(buffer.guard[i]!=0xa5) return 0;
            ordering[depth].address=0xffffff;
        }
        for(unsigned depth=0;depth<4096;++depth)
            if(ordering[depth].address!=0xffffff || ordering[depth].length) return 0;
        memset(&buffer,0xa5,sizeof(buffer)); g_DrawPacketOffset=0;
        FieldFx_DrawScreenTint(rgb,64,0,4096);
        if(g_DrawPacketOffset!=sizeof(RenderTintTile)) return 0;
        for(unsigned depth=0;depth<4096;++depth)
            if(ordering[depth].address!=0xffffff || ordering[depth].length) return 0;
    }
    D_800B0E38=saved; g_ActiveDrawSlot=saved_slot; g_DrawPacketOffset=saved_offset;
    puts("Original screen tint: all 4096 OT depths, both slots, blend modes, RGB and packet guards passed");
    return 1;
}

static int native_effect_model_packets(void)
{
    _Alignas(16) u8 blob[128]={0}, packets[512];
    static RenderGpuTag ordering[4096];
    RenderBufferPrefix saved=D_800B0E38;
    int saved_slot=g_ActiveDrawSlot, saved_offset=g_DrawPacketOffset;
    GteMatrix view={.m={{4096,0,0},{0,4096,0},{0,0,4096}}};
    GteMatrix *saved_view=g_GeomScroll.words.viewMatrix;
    g_GeomScroll.words.viewMatrix=&view;
    GteMatrix placement=view; placement.t[2]=512;
    FieldTexturedModel *model=(FieldTexturedModel *)blob;
    model->header.flatTris=model->header.shadedTris=1;
    model->header.flatQuads=model->header.shadedQuads=1;
    model->header.colorOffset=104; model->header.colorCount=4;
    FieldModelTri *tri=model->header.tris;
    FieldModelQuad *quad=(FieldModelQuad *)(tri+2);
    for(unsigned i=0;i<2;++i) {
        for(unsigned j=0;j<3;++j) { tri[i].vertex[j]=72+8*j; tri[i].uv[j]=0x1000+j; }
        for(unsigned j=0;j<4;++j) { quad[i].vertex[j]=72+8*j; quad[i].uv[j]=0x2000+j; }
    }
    FieldModelVertex *v=(FieldModelVertex *)(blob+72);
    for(unsigned j=0;j<4;++j) {
        v[j].x=(j&1)?32:-32; v[j].y=(j&2)?32:-32;
        v[j].color=104+4*j;
        ((u32 *)(blob+104))[j]=0x00302010+j;
    }
    Psyz_GteCtrlWrite(26,256); Psyz_GteCtrlWrite(29,1365); Psyz_GteCtrlWrite(30,1024);
    gte_SetGeomOffset(160,120);
    FieldFx_ModelSetTexture(0x123,0x456);
    const u16 factors[]={0,64,128,512,65535};
    const u32 expected_colors[5][4]={
        {0,0,0,0},
        {0x181008,0x181008,0x181009,0x181009},
        {0x302010,0x302011,0x302012,0x302013},
        {0xc08040,0xc08044,0xc08048,0xc0804c},
        {0xffffff,0xffffff,0xffffff,0xffffff},
    };
    u8 original_model[sizeof(blob)];memcpy(original_model,blob,sizeof(blob));
    for(unsigned slot=0;slot<2;++slot) for(unsigned semi=0;semi<2;++semi) for(unsigned factor=0;factor<5;++factor) {
        FieldFx_ModelSaveColors((char *)blob);
        FieldFx_ModelScaleColors((char *)blob,factors[factor]);
        if(memcmp(blob+104,expected_colors[factor],16))return 0;
        memset(packets,0xa5,sizeof(packets));
        memset(ordering,0,sizeof(ordering));
        for(unsigned i=0;i<4096;++i) ordering[i].address=0xffffff;
        D_800B0E38.ordering[slot]=ordering; D_800B0E38.packets[slot]=(char *)packets;
        g_ActiveDrawSlot=slot; g_DrawPacketOffset=0;
        FieldFx_ModelSetSemiTrans(semi); g_FieldFxModelDepthBias=0;
        FieldFx_ModelDraw(model,&placement);
        size_t expected=sizeof(FieldModelFt3Packet)+sizeof(FieldModelGt3Packet)+sizeof(FieldModelFt4Packet)+sizeof(FieldModelGt4Packet);
        if(g_DrawPacketOffset!=expected) { fprintf(stderr,"effect model bytes %d expected %zu\n",g_DrawPacketOffset,expected); return 0; }
        unsigned sizes[4]={sizeof(FieldModelFt3Packet),sizeof(FieldModelGt3Packet),sizeof(FieldModelFt4Packet),sizeof(FieldModelGt4Packet)};
        unsigned lengths[4]={7,9,9,12}, codes[4]={0x24,0x34,0x2c,0x3c};
        size_t offset=0; uintptr_t previous=0xffffff;
        for(unsigned i=0;i<4;++i) {
            RenderGpuTag *tag=(RenderGpuTag *)(packets+offset);
            if(tag->address!=previous || tag->length!=lengths[i] || packets[offset+sizeof(*tag)+3]!=(codes[i]|(semi?2:0))) return 0;
            u32 rendered_color;memcpy(&rendered_color,packets+offset+sizeof(*tag),4);
            if((rendered_color&0xffffff)!=expected_colors[factor][0])return 0;
            previous=(uintptr_t)tag; offset+=sizes[i];
        }
        unsigned used=0;
        for(unsigned depth=0;depth<4096;++depth) if(ordering[depth].address!=0xffffff) {
            if(ordering[depth].address!=previous) return 0;
            ++used;
        }
        if(used!=1) return 0;
        for(size_t i=expected;i<sizeof(packets);++i) if(packets[i]!=0xa5) return 0;
        FieldFx_ModelRestoreColors((char *)blob);
        if(memcmp(blob,original_model,sizeof(blob)))return 0;
    }
    D_800B0E38=saved; g_ActiveDrawSlot=saved_slot; g_DrawPacketOffset=saved_offset;
    g_GeomScroll.words.viewMatrix=saved_view;
    puts("Original effect model: all packet types, both slots, transparency and save/scale/draw/restore saturation passed");
    return 1;
}

static int native_oriented_sprite_packets(void)
{
    struct { FieldStripPacket packet; u8 guard[16]; } buffer;
    static RenderGpuTag ordering[4096];
    RenderBufferPrefix saved=D_800B0E38;
    int saved_slot=g_ActiveDrawSlot, saved_offset=g_DrawPacketOffset;
    GteMatrix *saved_view=g_GeomScroll.words.viewMatrix;
    GteMatrix view={.m={{4096,0,0},{0,4096,0},{0,0,4096}}};
    g_GeomScroll.words.viewMatrix=&view;
    FieldOrientedSprite sprite={.position={0,0,512},.scale={4096,4096,4096},
        .rgb={80,40,20},.cell=0x23,.clut=0x12,.brightness=128};
    Psyz_GteCtrlWrite(26,256); Psyz_GteCtrlWrite(29,1365); Psyz_GteCtrlWrite(30,1024);
    gte_SetGeomOffset(160,120);
    FieldFx_SetSpriteCellSize(2,3);
    if(g_FieldFxCellQuad[0].x!=-32 || g_FieldFxCellQuad[0].y!=-48 ||
       g_FieldFxCellQuad[3].x!=32 || g_FieldFxCellQuad[3].y!=48) return 0;
    for(unsigned slot=0;slot<2;++slot) for(unsigned preset=0;preset<4;++preset)
    for(unsigned color=0;color<2;++color) for(unsigned blend=0;blend<5;++blend) {
        memset(&buffer,0xa5,sizeof(buffer));
        memset(ordering,0,sizeof(ordering));
        for(unsigned i=0;i<4096;++i) ordering[i].address=0xffffff;
        D_800B0E38.ordering[slot]=ordering;
        D_800B0E38.packets[slot]=(char *)&buffer;
        g_ActiveDrawSlot=slot; g_DrawPacketOffset=0;
        FieldFx_SetSpriteTexturePreset(preset); FieldFx_SetSpriteColorMode(color?256:16);
        FieldFx_SetSpriteBlendMode(blend);
        FieldFx_DrawOrientedSprite(&sprite);
        FieldStripPacket *p=&buffer.packet;
        if(g_DrawPacketOffset!=sizeof(*p) || p->tag.length!=9 || p->tag.address!=0xffffff ||
           p->code!=(blend?0x2e:0x2c) || p->r0!=80 || p->g0!=40 || p->b0!=20 ||
           p->u0!=48 || p->u1!=49 || p->u2!=48 || p->u3!=49 ||
           p->v0!=32 || p->v1!=32 || p->v2!=34 || p->v3!=34 ||
           p->tpage!=GetTPage(color,blend?blend-1:0,preset<3?0x340:0x380,preset==1?0x160:0x100) ||
           p->clut!=GetClut(32,(int[]){0x1d7,0x1db,0x1d6,0x1c8}[preset]+1)) return 0;
        unsigned used=0;
        for(unsigned depth=0;depth<4096;++depth) if(ordering[depth].address!=0xffffff) {
            if(ordering[depth].address!=(uintptr_t)p) return 0;
            ++used;
        }
        if(used!=1) return 0;
        for(unsigned i=0;i<sizeof(buffer.guard);++i) if(buffer.guard[i]!=0xa5) return 0;
    }
    struct { FieldGt4Packet packet; u8 guard[16]; } rotated;
    Psyz_GteCtrlWrite(27,1); Psyz_GteCtrlWrite(28,0);
    GteShortVector anchor={0,0,512}; GteRotation rotation={1024,0,0,0};
    RenderColor first={80,40,20,0},second={60,30,10,0};
    RenderEffectParameters saved_params=g_FieldFxSpriteParams;
    g_FieldFxSpriteParams.tpage=0x100;g_FieldFxSpriteParams.depth=0;
    for(unsigned slot=0;slot<2;++slot) for(unsigned mode=0;mode<5;++mode) {
        memset(&rotated,0xa5,sizeof(rotated));
        for(unsigned i=0;i<4096;++i)ordering[i]=(RenderGpuTag){.address=0xffffff};
        D_800B0E38.ordering[slot]=ordering;D_800B0E38.packets[slot]=(char *)&rotated;
        g_ActiveDrawSlot=slot;g_DrawPacketOffset=0;
        FieldFx_DrawRotatedQuad(&anchor,&rotation,100,100,7,11,12,16,123,&first,&second,128,mode==4?255:mode);
        FieldGt4Packet *p=&rotated.packet;
        if(g_DrawPacketOffset!=sizeof(*p) || p->tag.length!=12 || p->tag.address!=0xffffff ||
            p->code!=(mode==4?0x3c:0x3e) || p->r0!=80 || p->g0!=40 || p->b0!=20 ||
            p->r1!=80 || p->r2!=60 || p->g2!=30 || p->b2!=10 || p->r3!=60 ||
            p->u0!=7 || p->u1!=7 || p->u2!=18 || p->u3!=18 ||
            p->v0!=11 || p->v2!=11 || p->v1!=26 || p->v3!=26 || p->clut!=123 ||
            p->tpage!=(mode==4?0x100:0x100|GetTPage(0,mode,0,0))) return 0;
        unsigned linked=0;
        for(unsigned depth=0;depth<4096;++depth)if(ordering[depth].address!=0xffffff) {
            if(ordering[depth].address!=(uintptr_t)p)return 0;
            ++linked;
        }
        if(linked!=1)return 0;
        for(unsigned i=0;i<sizeof(rotated.guard);++i)if(rotated.guard[i]!=0xa5)return 0;
    }
    GteShortVector old_center=g_RoomM005BeaconCenter,old_offset=g_RoomM005BeaconOffset;
    int old_alt=g_FieldFxAltPaletteFlag,old_age=g_FieldFxCurrentAge;
    FieldFxTaskContext context={0},*old_context=g_FieldFxTaskContext;
    g_FieldFxTaskContext=&context;
    g_RoomM005BeaconCenter=(GteShortVector){0,0,512};g_RoomM005BeaconOffset=(GteShortVector){0};
    _Alignas(void *) u8 drift_arena[128];
    FieldFx_InitCallbackList((char *)drift_arena,sizeof(RoomM005DriftingSpriteState),1,RoomM005_BeaconDriftCallback);
    RoomM005DriftingSpriteState *drift=FieldFx_AcquireCallbackSlot((char *)drift_arena);
    *drift=(RoomM005DriftingSpriteState){1024,0,0,0,0,0,0};
    FieldFxCallbackListEntry *entry=(void *)((u8 *)drift-sizeof(FieldFxCallbackListEntry));
    for(unsigned slot=0;slot<2;++slot)for(unsigned kind=3;kind<=4;++kind)
    for(unsigned alt=0;alt<2;++alt)for(unsigned age=0;age<=6;++age) {
        memset(&rotated,0xa5,sizeof(rotated));
        for(unsigned i=0;i<4096;++i)ordering[i]=(RenderGpuTag){.address=0xffffff};
        D_800B0E38.ordering[slot]=ordering;D_800B0E38.packets[slot]=(char *)&rotated;
        g_ActiveDrawSlot=slot;g_DrawPacketOffset=0;
        g_FieldFxSpriteParams.palette=kind;g_FieldFxAltPaletteFlag=alt;entry->age=age;
        if(FieldFx_DrawCallbackList((char *)drift_arena)!=1)return 0;
        FieldGt4Packet *p=&rotated.packet;
        u8 intensity=rcos((age<<10)/6)/24;
        unsigned u=(age&1)<<7,v=((age<<3)&16)+32;
        if(p->tag.length!=12 || p->tag.address!=0xffffff || p->code!=0x3e ||
            p->r0!=intensity || p->g0!=intensity || p->b0!=intensity || p->r3!=intensity ||
            p->clut!=GetClut(0,g_FieldFxPaletteTable[kind]+(kind==4&&alt?4:0)) ||
            p->u0!=u || p->u2!=(u8)(u+126) || p->v0!=v || p->v1!=v+14)return 0;
        unsigned linked=0;
        for(unsigned depth=0;depth<4096;++depth)if(ordering[depth].address!=0xffffff) {
            if(ordering[depth].address!=(uintptr_t)p)return 0;
            ++linked;
        }
        if(linked!=1 || g_DrawPacketOffset!=sizeof(*p))return 0;
        for(unsigned i=0;i<sizeof(rotated.guard);++i)if(rotated.guard[i]!=0xa5)return 0;
    }
    g_FieldFxTaskContext=old_context;g_FieldFxCurrentAge=old_age;
    g_FieldFxAltPaletteFlag=old_alt;g_RoomM005BeaconCenter=old_center;g_RoomM005BeaconOffset=old_offset;
    g_FieldFxSpriteParams=saved_params;
    D_800B0E38=saved; g_ActiveDrawSlot=saved_slot; g_DrawPacketOffset=saved_offset;
    g_GeomScroll.words.viewMatrix=saved_view;
    puts("Original sprites: both buffers, presets/blends, native rotated-quad payloads and M005 drift ages/palettes passed");
    return 1;
}

static int native_ring_packets(void)
{
    struct RingPair { FieldRingPacket packet; RenderTintMode mode; };
    struct { struct RingPair pairs[4]; u8 guard[16]; } buffer;
    static RenderGpuTag ordering[4096];
    GteShortVector points[8]={0};
    FieldShadedRing ring={.source=points,.color0={80,40,20},.color1={200,100,50},
        .mode=4,.extent0=16,.extent1=32,.intensity=64};
    RenderBufferPrefix saved=D_800B0E38;
    int saved_slot=g_ActiveDrawSlot, saved_offset=g_DrawPacketOffset;
    GteMatrix *saved_view=g_GeomScroll.words.viewMatrix;
    GteMatrix view={.m={{4096,0,0},{0,4096,0},{0,0,4096}}};
    g_GeomScroll.words.viewMatrix=&view;
    Psyz_GteCtrlWrite(26,256); Psyz_GteCtrlWrite(29,1365); Psyz_GteCtrlWrite(30,1024);
    gte_SetGeomOffset(160,120);
    FieldFx_BuildRingGeometry(&ring);
    if(points[0].x || points[0].y!=32 || points[4].x || points[4].y!=16) return 0;
    for(unsigned slot=0;slot<2;++slot) for(unsigned transform=0;transform<2;++transform)
    for(unsigned blend=0;blend<5;++blend) {
        memset(&buffer,0xa5,sizeof(buffer)); memset(ordering,0,sizeof(ordering));
        for(unsigned i=0;i<4096;++i) ordering[i].address=0xffffff;
        D_800B0E38.ordering[slot]=ordering; D_800B0E38.packets[slot]=(char *)&buffer;
        g_ActiveDrawSlot=slot; g_DrawPacketOffset=0;
        FieldFx_SetSpriteBlendMode(blend);
        GteMatrix placement=view; placement.t[2]=512;
        FieldFx_DrawShadedRing(&ring,&placement,transform);
        if(g_DrawPacketOffset!=sizeof(buffer.pairs)) return 0;
        uintptr_t previous=0xffffff;
        for(unsigned i=0;i<4;++i) {
            FieldRingPacket *p=&buffer.pairs[i].packet;
            RenderTintMode *m=&buffer.pairs[i].mode;
            if(p->tag.address!=previous || p->tag.length!=8 ||
               m->tag.address!=(uintptr_t)p || m->tag.length!=1 ||
               m->command!=(u32)(0xe1000000|((blend?blend-1:0)<<5)) ||
               p->c0.word!=(u32)((blend?0x3a:0x38)<<24|25<<16|50<<8|100) ||
               p->c1.word!=(u32)(25<<16|50<<8|100) ||
               p->c2.word!=(u32)(10<<16|20<<8|40) || p->c3.word!=p->c2.word) return 0;
            previous=(uintptr_t)m;
        }
        unsigned used=0;
        for(unsigned i=0;i<4096;++i) if(ordering[i].address!=0xffffff) {
            if(ordering[i].address!=previous) return 0;
            ++used;
        }
        if(used!=1) return 0;
        for(unsigned i=0;i<sizeof(buffer.guard);++i) if(buffer.guard[i]!=0xa5) return 0;
    }
    D_800B0E38=saved; g_ActiveDrawSlot=saved_slot; g_DrawPacketOffset=saved_offset;
    g_GeomScroll.words.viewMatrix=saved_view;
    puts("Original rings: builder, both slots/transforms, all blend modes, paired packet links and guards passed");
    return 1;
}

static int native_effect_collision_coordinates(void)
{
    BattleEntity player={0}, *saved=D_8009D254;
    GteShortVector quad[4]={{0,0,0},{16,0,0},{0,0,16},{16,0,16}};
    g_PlayerEntity=(FieldActor *)(&player);
    for(int x=-2;x<=18;++x) for(int z=-2;z<=18;++z) {
        GteShortVector point={x,99,z};
        player.posX.parts.integer=x; player.posY.parts.integer=99; player.posZ.parts.integer=z;
        if(FieldFx_IsPlayerInTriangleXZ(quad)!=FieldFx_IsPointInTriangleXZ(&point,quad) ||
           FieldFx_IsPlayerInQuadXZ(quad)!=FieldFx_IsPointInQuadXZ(&point,quad)) return 0;
    }
    FieldActor actor={0};FieldActorState actor_state={0};RenderMatrix matrices[21]={0};
    actor.state=&actor_state;actor.render_object.matrices=matrices;
    GteShortVector input={-7,9,11,0},output={0},angles={0};
    for(unsigned index=0;index<=20;index+=20)for(unsigned turn=0;turn<2;++turn) {
        RenderMatrix *matrix=&matrices[index];
        memset(matrix,0,sizeof(*matrix));
        matrix->rotation[0][turn?2:0]=4096;matrix->rotation[1][1]=4096;
        matrix->rotation[2][turn?0:2]=turn?-4096:4096;
        matrix->translation[0]=40000+index;matrix->translation[1]=-50000;matrix->translation[2]=65530;
        FieldFx_TransformMatrixPoint((RoomFxTransformOwner *)&actor,index,&input,&output);
        if(output.x!=(s16)(40000+index+(turn?11:-7)) || output.y!=(s16)-49991 ||
            output.z!=(s16)(turn?65537:65541))return 0;
        FieldFx_GetTransformLookAngles((RoomFxTransformOwner *)&actor,index,&angles);
        if(angles.x || angles.y!=(turn?3072:2048) || angles.z)return 0;
        if(((RoomM005BeaconPool *)&actor)->matrix!=(GteMatrix *)matrices)return 0;
    }
    actor_state.flags=0x1234;
    ((RoomM005BeaconActor *)&actor_state)->flags|=0x4000;
    if(actor_state.flags!=0x5234)return 0;
    u8 action=1;actor_state.action_state=&action;
    if(((RoomM005BeaconPool *)&actor)->node->state!=&action)return 0;
    g_PlayerEntity=(FieldActor *)(saved);
    puts("Original effect collision: player/native coordinate paths agree across 441 triangle/quad positions");
    return 1;
}

static int original_glow_resident_constants(void)
{
    const char *path=getenv("PE_TEST_DISC");
    if(!path) return !getenv("PE_REQUIRE_GAME_ASSETS");
    const u32 fan92[]={0x800d751c,0,0,0,0,0,0,0,8,0,0,0,0,0x800e17c8};
    const u32 fan93[]={0x800d751c,0x800d7764,0,0,0,0,0,0,0xc0008,0,0,0,0,0x800e1840};
    const u32 slow99[]={0x800da5d4,0x800da934,0,0,0,0,0,0,0x80008,0,0,0,0,0x800e1b84};
    const u32 detox100[]={0x800daca4,0x800db0d0,0,0,0,0,0,0,0xc000c,0,0,0,0,0x800e1be0};
    const u32 scan101[]={0x800db6bc,0,0,0,0,0,0,0,28,0,0,0,0,0x800e1c9c};
    const u32 slow102[]={0x800dba9c,0x800dbe6c,0,0,0,0,0,0,0x8000c,0,0,0,0,0x800e1d30};
    const u32 barrier103[]={0x800dc058,0x800dc750,0,0,0,0,0,0,0x80014,0,0,0,0,0x800e1e04};
    const u32 pe94[]={0x800d751c,0x800d7764,0x800d7b70,0x0,0x0,0x0,0x0,0x0,0xc0008,0x10,0x0,0x0,0x0,0x800e189c};
    const u32 pe95[]={0x800d8388,0x800d7764,0x800d751c,0x0,0x0,0x0,0x0,0x0,0xc0008,0x8,0x0,0x0,0x0,0x800e1958};
    const u32 pe96[]={0x800d868c,0x800d8b6c,0x800d7764,0x0,0x0,0x0,0x0,0x0,0x40008,0xc,0x0,0x0,0x0,0x800e19e8};
    const u32 pe97[]={0x800d927c,0x800d96f4,0x0,0x0,0x0,0x0,0x0,0x0,0xc0008,0x0,0x0,0x0,0x0,0x800e1a7c};
    const u32 pe98[]={0x800df9b0,0x800d9fd4,0x0,0x0,0x0,0x0,0x0,0x0,0x80008,0x0,0x0,0x0,0x0,0x800e1b28};
    const u32 pe104[]={0x800dca80,0x800dce94,0x0,0x0,0x0,0x0,0x0,0x0,0xc0008,0x0,0x0,0x0,0x0,0x800e1ec4};
    const u32 pe106[]={0x800ddd70,0x800de0a8,0x800dea30,0x0,0x0,0x0,0x0,0x0,0x100008,0xc,0x0,0x0,0x0,0x800e204c};
    const u32 pe105[]={0x800dd19c,0x800dd76c,0x0,0x0,0x0,0x0,0x0,0x0,0x10000c,0x0,0x0,0x0,0x0,0x800e1f80};
    const u32 pe107[]={0x800df6ac,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x4,0x0,0x0,0x0,0x0,0x800e2124};
    const u32 hit85[]={0x800d4c24,0x800df87c,0x0,0x0,0x0,0x0,0x0,0x0,0x40004,0x0,0x0,0x0,0x0,0x800e13f0};
    const u32 hit86[]={0x800d4c24,0x800d4ea4,0x0,0x0,0x0,0x0,0x0,0x0,0x40004,0x0,0x0,0x0,0x0,0x800e14f4};
    const u32 hit87[]={0x800d4c24,0x800d4ea4,0x0,0x0,0x0,0x0,0x0,0x0,0x40004,0x0,0x0,0x0,0x0,0x800e15a0};
    const u32 hit88[]={0x800d5290,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0xc,0x0,0x0,0x0,0x800e21a8,0x800e15fc};
    const u32 hit89[]={0x800d5a00,0x800d5eb4,0x0,0x0,0x0,0x0,0x0,0x0,0xc0010,0x0,0x0,0x0,0x800e21a8,0x800e164c};
    const u32 hit90[]={0x800d5290,0x800d5eb4,0x800d5a00,0x0,0x0,0x0,0x0,0x0,0xc000c,0x10,0x0,0x0,0x0,0x800e16fc};
    const u32 hit91[]={0x800d6514,0x800d6a1c,0x800d6e3c,0x0,0x0,0x0,0x0,0x0,0x14000c,0x4,0x0,0x0,0x800e21d0,0x800e1760};
    struct { u32 address; const void *value; size_t size; } rows[]={
        {0x800e14bc,hit85,56},
        {0x800e13f0,g_FieldFxTaskProgramCmd85.script,44},
        {0x800e1568,hit86,56},
        {0x800e14f4,g_FieldFxTaskProgramCmd86.script,36},
        {0x800e15c4,hit87,56},
        {0x800e15a0,g_FieldFxTaskProgramCmd87.script,36},
        {0x800e1614,hit88,56},
        {0x800e15fc,g_FieldFxTaskProgramCmd88.script,24},
        {0x800e16c4,hit89,56},
        {0x800e164c,g_FieldFxTaskProgramCmd89.script,32},
        {0x800e1728,hit90,56},
        {0x800e16fc,g_FieldFxTaskProgramCmd90.script,44},
        {0x800e1790,hit91,56},
        {0x800e1760,g_FieldFxTaskProgramCmd91.script,48},
        {0x800e141c,g_FallingSpriteColorTrack0,40},
        {0x800e1444,g_FallingSpriteColorTrack1,40},
        {0x800e146c,g_BouncingSpriteColorTrack2,40},
        {0x800e1494,g_RotatingEffectColorTrack,40},
        {0x800e1518,g_PulsingStarFanColorTrack1,40},
        {0x800e1540,g_PulsingStarFanColorTrack0,40},
        {0x800e166c,g_CosineSparkColorTrack,40},
        {0x800e1694,g_RisingSpriteColorTrack,48},
        {0x800c22c0,&g_FieldFxCosineEffectTint,4},
        {0x800c22c4,&g_FieldFxArcingEmitterTint,4},
        {0x800c22c8,&g_FieldFxSpiralSpriteTint,4},
        {0x800c22cc,&g_FieldFxRingPulseTint,4},
        {0x800c22d0,&g_FieldFxBouncingSpriteTint,4},
        {0x800e2014,pe105,56},
        {0x800e1f80,g_FieldFxTaskProgramCmd105.script,36},
        {0x800e216c,pe107,56},
        {0x800e2124,g_FieldFxTaskProgramCmd107.script,24},
        {0x800e1fa4,g_StarFanRingGlowColorTrack,40},
        {0x800e1fcc,g_StarFanRingGlowSpinColorTrack,32},
        {0x800e1fec,g_ConvergingSpriteColorTrack,40},
        {0x800e213c,g_FadeParticleColorTrack,40},
        {0x800e2164,g_FadeEmitterParticleKinds,8},
        {0x800c22f0,&g_FieldFxZeroOffset,8},
        {0x800e18c0,g_ArcingEffectColorTrack,48},
        {0x800e18f0,g_ArcingSparkColorTrack,48},
        {0x800e1988,g_OrbitTrailColorTrack,40},
        {0x800e1a14,g_VerticalEffectColorTrack,48},
        {0x800e1aa0,g_HelicalTrailColorTrack,40},
        {0x800e1ac8,g_DampedSparkColorTrack,40},
        {0x800e1920,pe94,sizeof(pe94)},
        {0x800e189c,g_FieldFxTaskProgramCmd94.script,36},
        {0x800e19b0,pe95,sizeof(pe95)},
        {0x800e1958,g_FieldFxTaskProgramCmd95.script,48},
        {0x800e1a44,pe96,sizeof(pe96)},
        {0x800e19e8,g_FieldFxTaskProgramCmd96.script,44},
        {0x800e1af0,pe97,sizeof(pe97)},
        {0x800e1a7c,g_FieldFxTaskProgramCmd97.script,36},
        {0x800e1b4c,pe98,sizeof(pe98)},
        {0x800e1b28,g_FieldFxTaskProgramCmd98.script,36},
        {0x800e1ee8,g_DriftingEffectColorTrack,48},
        {0x800e1f18,g_TiltingSpriteColorTrack,48},
        {0x800e207c,g_SettlingSpriteColorTrack,48},
        {0x800e20ac,g_SettlingEmitterColorTrack,32},
        {0x800e20cc,g_FlashBurstColorTrack,32},
        {0x800e1f48,pe104,sizeof(pe104)},
        {0x800e1ec4,g_FieldFxTaskProgramCmd104.script,36},
        {0x800e20ec,pe106,sizeof(pe106)},
        {0x800e204c,g_FieldFxTaskProgramCmd106.script,48},

        {0x800e1c64,detox100,sizeof(detox100)},
        {0x800e1be0,g_FieldFxTaskProgramCmd100.script,36},
        {0x800e1c04,g_CosineEffectColorTrack,40},
        {0x800e1c2c,g_SineSweepColorTrack,56},
        {0x800c22bc,&g_FieldFxGlowFountainTint,4},
        {0x800c22dc,&g_FieldFxArcingSparkTint,4},
        {0x800c22e0,&g_FieldFxOrbitHelicalTint,4},
        {0x800c22e4,&g_FieldFxDampedSparkTint,4},
        {0x800c22e8,&g_FieldFxSparkOuterTint,4},
        {0x800c22ec,&g_FieldFxSparkInnerTint,4},
        {0x800e1ba8,slow99,sizeof(slow99)},
        {0x800e1b84,g_FieldFxTaskProgramCmd99.script,36},
        {0x800e1cf8,scan101,sizeof(scan101)}, {0x800e1dcc,slow102,sizeof(slow102)},
        {0x800e1c9c,g_FieldFxTaskProgramCmd101.script,44},
        {0x800e1d30,g_FieldFxTaskProgramCmd102.script,48},
        {0x800e1e8c,barrier103,sizeof(barrier103)},
        {0x800e1e04,g_FieldFxTaskProgramCmd103.script,32},
        {0x800e1e24,g_TwinModelColorTrack,64},
        {0x800e1e64,g_RisingJitterColorTrack,40},
        {0x800e1cc8,g_DiamondParticleColorTrack,48},
        {0x800e1d64,g_RingFlashColorTrack,32},
        {0x800e1d84,g_RingFlashInnerColorTrack,32},
        {0x800e1da4,g_JitterSpriteColorTrack,40},
        {0x800e1808,fan92,sizeof(fan92)}, {0x800e1864,fan93,sizeof(fan93)},
        {0x800e17c8,g_FieldFxTaskProgramCmd92.script,24},
        {0x800e1840,g_FieldFxTaskProgramCmd93.script,36},
        {0x800c22d4,&g_FieldFxFanSweepSparkTint,4},
        {0x800c22d8,&g_FieldFxFanSweepGlowTint,4},
        {0x800e17e0,g_EmberColorTrack,40},
        {0x800c2154,&g_GlowEffectLayersSpinA,sizeof(GteShortVector)},
        {0x800c215c,&g_GlowLayers1SpinB,sizeof(GteShortVector)},
        {0x800c2164,&g_GlowLayers1Scale,sizeof(GteVector)},
        {0x800c2184,&g_GlowEffectBLayersSpinA,sizeof(GteShortVector)},
        {0x800c218c,&g_GlowEffectBSpinB,sizeof(GteShortVector)},
        {0x800c2194,&g_GlowEffectBLayersScale,sizeof(GteVector)},
        {0x800c21a4,&g_GlowEffectCLayersSpinA,sizeof(GteShortVector)},
        {0x800c21ac,&g_GlowEffectCLayersSpinB,sizeof(GteShortVector)},
        {0x800c21b4,&g_GlowLayers3Scale,sizeof(GteVector)},
        {0x800c21d4,&g_FourLayerGlowSpinA,sizeof(GteShortVector)},
        {0x800c21dc,&g_FourLayerGlowSpinB,sizeof(GteShortVector)},
        {0x800c21e4,&g_FourLayerGlowSpinC,sizeof(GteShortVector)},
        {0x800c21ec,&g_FourLayerGlowSpinD,sizeof(GteShortVector)},
        {0x800c21f4,&g_FourLayerGlowScale,sizeof(GteVector)},
        {0x800c214c,&g_GlowEffectPointOffset,sizeof(GteShortVector)},
        {0x800c2174,&g_GlowEffectSpriteRotation,sizeof(GteShortVector)},
        {0x800e085c,g_GlowEffectSlotSizes,7*sizeof(u16)},
        {0x800e0888,g_GlowEffectScaleByAction,16*sizeof(s16)},
        {0x800e08a8,g_GlowEffectFlareOffsets,8*sizeof(GteShortVector)},
        {0x800e08e8,g_GlowEffectHaloOffsets,8*sizeof(GteShortVector)},
        {0x800e0928,g_GlowEffectScript,25*sizeof(int)},
        {0x800c217c,&g_GlowEffectBPointOffset,sizeof(GteShortVector)},
        {0x800e09c8,g_GlowEffectBSlotSizes,5*sizeof(u16)},
        {0x800e09f0,g_GlowEffectBScaleByAction,16*sizeof(s16)},
        {0x800e0a10,g_GlowEffectBFlareOffsets,8*sizeof(GteShortVector)},
        {0x800e0a50,g_GlowEffectBScript,13*sizeof(int)},
        {0x800e0ab4,g_GlowEffectCSlotSizes,4*sizeof(u16)},
        {0x800e0ad8,g_GlowEffectCScaleByAction,16*sizeof(s16)},
        {0x800e0af8,g_GlowEffectCFlareOffsets,8*sizeof(GteShortVector)},
        {0x800e0b38,g_GlowEffectCScript,12*sizeof(int)},
        {0x800c21c4,&g_FourLayerGlowPointOffsetA,sizeof(GteShortVector)},
        {0x800c21cc,&g_FourLayerGlowPointOffsetB,sizeof(GteShortVector)},
        {0x800c2204,&g_FourLayerGlowSpriteRotation,sizeof(GteShortVector)},
        {0x800e0bbc,g_FourLayerGlowSlotSizes,7*sizeof(u16)},
        {0x800e0be8,g_FourLayerGlowScaleByAction,16*sizeof(s16)},
        {0x800e0c08,g_FourLayerGlowFlareOffsets,8*sizeof(GteShortVector)},
        {0x800e0c48,g_FourLayerGlowHaloOffsets,8*sizeof(GteShortVector)},
        {0x800e0c88,g_FourLayerGlowScript,25*sizeof(int)},
        {0x800e0d40,g_PointSpriteSlotSizes,7*sizeof(u16)},
        {0x800e0d6c,g_PointSpriteScript,22*sizeof(int)},
        {0x800c22a0,&g_FieldFxBlackColor,sizeof(RenderColor)},
        {0x800e1204,g_FieldFxPaletteTable,6*sizeof(u16)},
        {0x800e11e4,g_FieldFxTpageSlots,sizeof(g_FieldFxTpageSlots)},
        {0x800c2258,&D_800C2258,sizeof(GteShortVector)},
        {0x800c2260,&g_FieldFxZeroVector,sizeof(GteShortVector)},
        {0x800c2268,&g_FieldFxZeroRotation,sizeof(GteRotation)},
        {0x800c2290,&g_FieldFxUnitScale,sizeof(GteVector)},
        {0x800e1210,g_FieldFxStripPacketCounts,6*sizeof(u16)},
        {0x800e121c,g_FieldFxShapeVertexTable[0],4*sizeof(GteShortVector)},
        {0x800e123c,g_FieldFxShapeVertexTable[1],8*sizeof(GteShortVector)},
        {0x800e127c,g_FieldFxShapeVertexTable[2],8*sizeof(GteShortVector)},
        {0x800e12bc,g_FieldFxShapeVertexTable[3],8*sizeof(GteShortVector)},
        {0x800e12fc,g_FieldFxShapeVertexTable[4],8*sizeof(GteShortVector)},
        {0x800e133c,g_FieldFxShapeVertexTable[5],16*sizeof(GteShortVector)}
    };
    PeDisc disc={0}; PeDiscFile file;
    u8 header[32], raw[128];
    if(!pe_disc_open(&disc,path)) return 0;
    if(!pe_disc_find(&disc,"SLUS_006.62",&file) ||
       !pe_disc_read(&disc,&file,0,header,sizeof(header))) { pe_disc_close(&disc); return 0; }
    u32 base=(u32)header[24]|(u32)header[25]<<8|(u32)header[26]<<16|(u32)header[27]<<24;
    for(unsigned i=0;i<sizeof(rows)/sizeof(rows[0]);++i) {
        if(rows[i].address<base || !pe_disc_read(&disc,&file,0x800ull+rows[i].address-base,raw,rows[i].size) ||
           memcmp(raw,rows[i].value,rows[i].size)) { pe_disc_close(&disc); return 0; }
    }
    const u32 resident_ids[8]={0x800e086c,0x800e09d4,0x800e0abc,0x800e0bcc,0x800e0f50,0x800e0e9c,0x800e0fe0,0x800e0d50};
    if(!pe_disc_read(&disc,&file,0x800ull+0x80094188u-base,raw,sizeof(resident_ids)) ||
       memcmp(raw,resident_ids,sizeof(resident_ids))) { pe_disc_close(&disc); return 0; }
    u32 task_descriptor;
    const u32 task_methods[]={0x800d4860,0x800d4620,0x800d4698,0x800d4704,0x800d413c,0x800d4850,0};
    if(!pe_disc_read(&disc,&file,0x800ull+0x80094188u-base+85*4,&task_descriptor,4) || task_descriptor!=0x800e13d4 ||
        !pe_disc_read(&disc,&file,0x800ull+task_descriptor-base,raw,sizeof(task_methods)) ||
        memcmp(raw,task_methods,sizeof(task_methods))) {pe_disc_close(&disc);return 0;}
    PeDiscFile archive;u32 room_range[2];u8 room_bytes[0x2000];
    if(!pe_disc_find(&disc,"PE.IMG",&archive) ||
        !pe_disc_read(&disc,&file,0x800ull+0x80093378u-base+4*8,room_range,sizeof(room_range))) {pe_disc_close(&disc);return 0;}
    u64 room_sector=(u64)room_range[0]+(room_range[1]&255)+((room_range[1]>>8)&4095);
    if(!pe_disc_read(&disc,&archive,room_sector*2048,room_bytes,sizeof(room_bytes))) {pe_disc_close(&disc);return 0;}
    struct {u32 address;const void *value;size_t size;} room_rows[]={
        {0x8018eff4,&g_RoomM005OrbiterJointAnchor,8},
        {0x8018effc,&g_RoomM005CrossFlareFlashAnchor,8},
        {0x8018f004,&g_RoomM005CrossFlareTwinAnchor,8},
        {0x8018f00c,&g_RoomM005BeaconTint,4},
        {0x8018f010,&g_RoomM005JointBeaconAnchor,8},
        {0x80190ad4,RoomM005_Rec2,sizeof(RoomM005_Rec2)},
        {0x80190af4,g_RoomM005CrossFlareColorTrack,sizeof(g_RoomM005CrossFlareColorTrack)},
        {0x80190b80,g_RoomScriptRecordWords,20},
    };
    for(unsigned i=0;i<sizeof(room_rows)/sizeof(room_rows[0]);++i)
        if(memcmp(room_bytes+(room_rows[i].address-0x8018efe8u),room_rows[i].value,room_rows[i].size)) {pe_disc_close(&disc);return 0;}
    const unsigned orbit_maps[]={12,13,27,30};u8 orbit_reference[0xd10-12];
    for(unsigned m=0;m<4;++m){
        unsigned map=orbit_maps[m];
        if(!pe_disc_read(&disc,&file,0x800ull+0x80093378u-base+(map-1)*8,room_range,sizeof(room_range)))return 0;
        room_sector=(u64)room_range[0]+(room_range[1]&255)+((room_range[1]>>8)&4095);
        if(!pe_disc_read(&disc,&archive,room_sector*2048,room_bytes,sizeof(room_bytes)))return 0;
        if(!m)memcpy(orbit_reference,room_bytes+12,sizeof(orbit_reference));
        else if(memcmp(orbit_reference,room_bytes+12,sizeof(orbit_reference)))return 0;
        struct {u32 callbacks[8];u16 sizes[8];u32 initialize,script;} descriptor;
        memcpy(&descriptor,room_bytes+(0x8018fcb4u-0x8018efe8u),sizeof(descriptor));
        for(unsigned i=0;i<8;++i){
            if(descriptor.callbacks[i]!=(i?0u:0x8018f20cu) || descriptor.sizes[i]!=(i?0u:20u) ||
               g_RoomOrbitFlareEffects.table.callbacks[i]!=(i?NULL:RoomEffect_OrbitFlareController) ||
               g_RoomOrbitFlareEffects.table.sizes[i]!=(i?0u:sizeof(RoomOrbitFlareState)))return 0;
        }
        if(descriptor.initialize!=0x8018fc54u || descriptor.script!=0x8018fc78u ||
           g_RoomOrbitFlareEffects.initialize!=RoomLib_SetArgs3 ||
           memcmp(room_bytes+(descriptor.script-0x8018efe8u),g_RoomOrbitFlareScript,sizeof(g_RoomOrbitFlareScript)))return 0;
    }
    pe_disc_close(&disc);
    puts("Original orbit flare descriptors: M012/M013/M027/M030 identical module bytes, callbacks, sizes, initializer and all 30 script halfwords match the supplied USA disc");
    puts("Original glow constants: resident PM IDs, spins, scales, offsets, slot sizes and authored script match the supplied USA disc");
    return 1;
}

static int native_glow_layer_packets(void)
{
    struct { FieldStripPacket packets[4]; u8 guard[16]; } buffer;
    static RenderGpuTag ordering[4096];
    RenderBufferPrefix saved=D_800B0E38;
    int saved_slot=g_ActiveDrawSlot, saved_offset=g_DrawPacketOffset;
    GteMatrix *saved_view=g_GeomScroll.words.viewMatrix;
    GteMatrix view={.m={{4096,0,0},{0,4096,0},{0,0,4096}}};
    g_GeomScroll.words.viewMatrix=&view;
    FieldGlowLayers glow={.depth=128,.z=1024,.matrix=view};
    int (*draws[])(void *,void *,void *)={GlowEffect_DrawFlareLayers,
        GlowEffectB_DrawFlareLayers,GlowEffectC_DrawFlareLayers,FourLayerGlow_DrawFlareLayers};
    FieldGlowSprite *sprites[]={&g_GlowEffectSprite2,&g_GlowEffectBSprite2,&g_GlowEffectCSprite1,&g_FourLayerGlowSprite2};
    Psyz_GteCtrlWrite(26,256); Psyz_GteCtrlWrite(29,1365); Psyz_GteCtrlWrite(30,1024);
    gte_SetGeomOffset(160,120);
    for(unsigned slot=0;slot<2;++slot) for(unsigned kind=0;kind<4;++kind) {
        memset(&buffer,0xa5,sizeof(buffer)); memset(ordering,0,sizeof(ordering));
        for(unsigned i=0;i<4096;++i) ordering[i].address=0xffffff;
        *sprites[kind]=(FieldGlowSprite){.r=80,.g=80,.b=80,.cell=0xae,.clut=7,.offset=-50};
        D_800B0E38.ordering[slot]=ordering; D_800B0E38.packets[slot]=(char *)&buffer;
        g_ActiveDrawSlot=slot; g_DrawPacketOffset=0;
        draws[kind](NULL,NULL,&glow);
        unsigned count=kind==3?4:2;
        if(g_DrawPacketOffset!=count*sizeof(FieldStripPacket)) { fprintf(stderr,"glow kind %u slot %u bytes %d expected %zu\n",kind,slot,g_DrawPacketOffset,count*sizeof(FieldStripPacket)); return 0; }
        unsigned linked=0, seen=0;
        for(unsigned depth=0;depth<4096;++depth) {
            uintptr_t address=ordering[depth].address;
            while(address!=0xffffff) {
                if(address<(uintptr_t)buffer.packets || address>=(uintptr_t)&buffer.packets[count] ||
                   (address-(uintptr_t)buffer.packets)%sizeof(FieldStripPacket) || linked>=count) return 0;
                FieldStripPacket *packet=(FieldStripPacket *)address;
                unsigned bit=1u<<(unsigned)(packet-buffer.packets);
                if(seen&bit) return 0;
                seen|=bit;
                unsigned brightness=(kind==3 && packet<&buffer.packets[2])?40:80;
                if(packet->tag.length!=9 || packet->code!=0x2e ||
                   packet->r0!=brightness || packet->g0!=brightness || packet->b0!=brightness) {
                    fprintf(stderr,"glow kind %u packet %td RGB %u/%u/%u code %x\n",kind,packet-buffer.packets,packet->r0,packet->g0,packet->b0,packet->code); return 0;
                }
                ++linked; address=packet->tag.address;
            }
        }
        if(linked!=count) return 0;
        const u8 *bytes=(const u8 *)&buffer;
        for(size_t i=count*sizeof(FieldStripPacket);i<sizeof(buffer);++i) if(bytes[i]!=0xa5) return 0;
    }
    /* M269's authored orbit trail passes NULL for a zero-scale endpoint.
     * Compare every emitted byte with the equivalent nonnull colour path. */
    struct { _Alignas(RenderGpuTag) u8 bytes[512]; u8 guard[32]; } line_buffer;
    u8 reference[512], colour[4]={120,80,40,0}, black[4]={0,0,0,0};
    GteShortVector from={-30,0,1024,0},to={30,0,1024,0};
    RenderEffectParameters old_params=g_FieldFxSpriteParams;
    memset(&g_FieldFxSpriteParams,0,sizeof(g_FieldFxSpriteParams));
    for(unsigned slot=0;slot<2;++slot)for(unsigned mode=0;mode<2;++mode)for(unsigned tail=0;tail<2;++tail){
        int used=0;
        for(unsigned null_endpoint=0;null_endpoint<2;++null_endpoint){
            memset(&line_buffer,0xa5,sizeof(line_buffer));
            for(unsigned depth=0;depth<4096;++depth)ordering[depth].address=0xffffff;
            D_800B0E38.ordering[slot]=ordering;D_800B0E38.packets[slot]=(char *)&line_buffer;
            g_ActiveDrawSlot=slot;g_DrawPacketOffset=0;
            gte_ldrotmatrix(&view);gte_ldtransmatrix(&view);
            FieldFx_DrawGlowLine(&from,&to,tail?colour:(null_endpoint?NULL:colour),
                tail?(null_endpoint?NULL:black):colour,tail?128:0,tail?0:128,mode?255:1);
            FieldLineG2Packet *line=(void *)line_buffer.bytes;
            if(line->r0!=(tail?120:0) || line->g0!=(tail?80:0) || line->b0!=(tail?40:0) ||
                line->r1!=(tail?0:120) || line->g1!=(tail?0:80) || line->b1 ||
                g_DrawPacketOffset>sizeof(line_buffer.bytes) || g_DrawPacketOffset<=sizeof(*line))return 0;
            for(unsigned k=0;k<sizeof(line_buffer.guard);++k)if(line_buffer.guard[k]!=0xa5)return 0;
            if(!null_endpoint){used=g_DrawPacketOffset;memcpy(reference,line_buffer.bytes,sizeof(reference));}
            else if(used!=g_DrawPacketOffset || memcmp(reference,line_buffer.bytes,sizeof(reference)))return 0;
        }
    }
    g_FieldFxSpriteParams=old_params;
    D_800B0E38=saved; g_ActiveDrawSlot=saved_slot; g_DrawPacketOffset=saved_offset;
    g_GeomScroll.words.viewMatrix=saved_view;
    puts("Original glow layers: three paired variants and four-layer variant, both slots, native chains and guards passed");
    puts("Original M269 zero-scale glow endpoint: NULL and nonnull emit identical packets, both buffers and blend modes, guards passed");
    return 1;
}

static unsigned object_init_calls, object_draw_calls, object_update_calls;
static FieldFxObject *expected_effect_object;
static int object_slot_init(void *object, void *slot, void *data)
{
    FieldFxSlot *entry=slot;
    if(object!=expected_effect_object || entry!=&expected_effect_object->slots[0] ||
       data!=expected_effect_object->data || entry->counter || entry->flag!=1) abort();
    memcpy(data,&object,sizeof(object)); ++object_init_calls;
    return 0;
}
static int object_slot_draw(void *object, void *slot, void *data)
{
    void *saved=NULL; memcpy(&saved,data,sizeof(saved));
    if(saved!=object || object!=expected_effect_object || slot!=&expected_effect_object->slots[0]) abort();
    ++object_draw_calls; return 0;
}
static int object_slot_update(void *object, void *slot, void *data)
{
    object_slot_draw(object,slot,data); --object_draw_calls;
    FieldFxSlot *entry=slot;
    if(entry->counter==1) entry->flag=2;
    ++object_update_calls; return 0;
}
static int native_object_script_cycle(void)
{
    struct { FieldFxObject object; u8 guard[16]; } buffer;
    FieldActor actor={0};
    FieldFxHandler init[]={object_slot_init},draw[]={object_slot_draw},update[]={object_slot_update};
    u16 sizes[]={sizeof(void *)};
    int commands[]={0x100009,0x10000,0x20002,-1};
    memset(&buffer,0xa5,sizeof(buffer));
    FieldFxObject *object=&buffer.object;
    object->prefix.asset_type=7; object->prefix.actor=&actor;
    expected_effect_object=object;
    void **script=FieldFx_GetSlot(object);
    if(script!=(void **)&object->state.commands || object->prefix.asset_type!=7 ||
       object->prefix.actor!=&actor || object->prefix.owner_kind || object->prefix.live_slots) return 0;
    for(size_t i=0;i<sizeof(object->data);++i) if(object->data[i]) return 0;
    for(unsigned i=0;i<64;++i) if(object->slots[i].flag || object->slots[i].counter || object->slots[i].data_offset) return 0;
    object->state.commands=commands;
    object_init_calls=object_draw_calls=object_update_calls=0;
    if(FieldFx_RunScript(object,init,sizes) || object_init_calls!=1 ||
       object->state.regs[0]!=9 || object->state.slot_count!=1 || object->state.pc!=3 || object->state.wait!=2) return 0;
    FieldFx_SetScriptContext(&actor);
    if(FieldFx_GetScriptContext()!=&actor || FieldFx_RegPtr(0)!=&object->state.regs[0] ||
       FieldFx_VarPtr(0)!=&object->state.vars[0]) return 0;
    if(FieldFx_Register(object,draw) || object_draw_calls!=1) return 0;
    if(FieldFx_UpdateSlots(object,update) || object_update_calls!=1 || object->slots[0].counter!=1) return 0;
    if(FieldFx_RunScript(object,init,sizes) || object->state.wait!=1 || object_init_calls!=1) return 0;
    if(FieldFx_UpdateSlots(object,update) || object_update_calls!=2 || object->slots[0].flag || object->state.slot_count) return 0;
    if(FieldFx_RunScript(object,init,sizes) || object->state.wait) return 0;
    if(FieldFx_RunScript(object,init,sizes)!=-1 || !object->state.halted || object->state.pc!=3) return 0;
    FieldFx_GetSlot(object);
    int immediate[]={0x10000,-1}; object->state.commands=immediate;
    if(FieldFx_RunScript(object,init,sizes) || !FieldFx_IsLastSlotHalted()) return 0;
    if(FieldFx_FreeAllSlots()!=-1 || FieldFx_IsLastSlotHalted() || object->state.slot_count) return 0;
    for(unsigned i=0;i<sizeof(buffer.guard);++i) if(buffer.guard[i]!=0xa5) return 0;
    puts("Original object scripts: native reset, init/draw/update/free callbacks, wait/halt, context and guards passed");
    return 1;
}

static int authored_fan_sweep_cycles(void)
{
    PmTables old_tables=g_PmTables;Pe1GameState old_game=g_GameState;u32 old_flags=g_GameStateFlags;
    FieldActor *old_player=g_PlayerEntity;FieldFxTaskContext *old_context=g_FieldFxTaskContext;
    FieldFxObjectPrefix *old_owner=g_FieldFxOwner;FieldFxTaskSlot *old_task=g_FieldFxActiveTaskSlot;
    int old_age=g_FieldFxCurrentAge,old_slot=g_ActiveDrawSlot,old_offset=g_DrawPacketOffset;
    RenderBufferPrefix old_buffers=D_800B0E38;GteMatrix *old_view=g_GeomScroll.words.viewMatrix;
    GteShortVector old_sine=g_SineEffectAnchor,old_anchor=g_FanSweepSparkAnchor,old_diamond=g_DiamondParticleAnchor;void *old_trail=g_FanSweepSparkTrailPool;
    s32 *old_projection=g_GeomScroll.state.position.projectionDistance;
    GteShortVector old_spiral=g_SpiralEffectAnchor;u16 old_spiral_radius=g_SpiralEffectRadius;
    int old_phase=g_SparkEmitterPhaseSeed,old_floor=g_RoomFloorY->y;
    GteShortVector old_orbit=g_OrbitingEffectAnchor,old_helical=g_HelicalEffectAnchor;
    GteShortVector old_vertical=g_VerticalEffectAnchor,old_tilting=g_TiltingSpriteAnchor,old_settling=g_SettlingSpriteAnchor;
    GteShortVector old_converging=g_ConvergingSpriteAnchor;
    s16 old_fade_intensity=g_FadeEmitterIntensity;
    char *old_orbit_pool=g_OrbitingEffectParticlePool,*old_helical_pool=g_HelicalEffectParticlePool;
    s16 old_model_anchor_a[3];memcpy(old_model_anchor_a,g_SpinningModelAnchorA,sizeof(old_model_anchor_a));
    u8 *old_model=D_800F32D8,*old_twin_a=D_800F3418,*old_twin_b=g_FieldFxTwinModelB,*old_flash=g_FieldFxModel83ACD504,*old_spiral_model=g_FieldFxModel6DEAE684;
    s16 old_model_anchor[3];memcpy(old_model_anchor,g_SpinningModelAnchorB,6);
    u32 old_random=Psyz_RandState();
    uint64_t hash=UINT64_C(14695981039346656037);
    const u32 seeds[]={0,1,0x12345678,0xffffffff};const int ages[]={0,1,17,18,19};
    const s16 coordinates[]={0,1,-32768,32767,-1000};
    for(unsigned seed=0;seed<4;++seed)for(unsigned age=0;age<5;++age)for(unsigned coordinate=0;coordinate<5;++coordinate){
        s16 data[]={coordinates[coordinate],coordinates[coordinate],coordinates[coordinate],(s16)0xa5a5};
        srand(seeds[seed]);g_FieldFxCurrentAge=ages[age];u32 result=FieldFx_EmberSpark(1,data,0);u8 output[12];
        memcpy(output,&result,4);memcpy(output+4,data,8);
        for(unsigned i=0;i<12;++i)hash=(hash^output[i])*UINT64_C(1099511628211);
    }
    if(hash!=UINT64_C(0x1026cb69f2dfeabd)){fprintf(stderr,"Ember CPU hash %016llx\n",(unsigned long long)hash);return 0;}
    /* Unhooked USA executable/ROM captures include all original particle
     * bytes and the BIOS RNG state, including halfword wrap boundaries. */
    hash=UINT64_C(14695981039346656037);
    const struct {FieldFxTaskCallback callback;unsigned size;} hit_particles[]={
        {FieldFx_FallingSprite,16},{FieldFx_ScatterBurstSprite,16},
        {FieldFx_CosineEffectB,8},{FieldFx_RisingSprite,16},
        {FieldFx_SpiralSprite,16},{FieldFx_BouncingSpinSprite,16}
    };
    const int hit_ages[]={0,1,7,8,12,16,17,20,24,25,31,40,255,65535};
    const u16 radii[]={0,1,255,65535};
    for(unsigned callback=0;callback<6;++callback)for(unsigned seed=0;seed<4;++seed)
    for(unsigned age=0;age<14;++age)for(unsigned variant=0;variant<4;++variant){
        _Alignas(void *) u8 data[16];
        for(unsigned i=0;i<hit_particles[callback].size;++i)data[i]=i*37+hit_ages[age]*3+variant*73;
        if(callback==2){s16 duration=12+(seeds[seed]&3);memcpy(data+6,&duration,2);}
        g_SpiralEffectRadius=radii[variant];g_SpiralEffectAnchor=(GteShortVector){-32768,32767,-123,0};
        srand(seeds[seed]);g_FieldFxCurrentAge=hit_ages[age];
        u32 result=hit_particles[callback].callback(1,data,0),random=Psyz_RandState();
        const void *parts[]={&result,data,&random};const unsigned lengths[]={4,hit_particles[callback].size,4};
        for(unsigned part=0;part<3;++part)for(unsigned i=0;i<lengths[part];++i)
            hash=(hash^((const u8 *)parts[part])[i])*UINT64_C(1099511628211);
    }
    if(hash!=UINT64_C(0x46f703bc59498352)){fprintf(stderr,"Resident hit/death CPU hash %016llx\n",(unsigned long long)hash);return 0;}
    puts("Original resident hit/death: 1344 unhooked USA CPU/ROM update states, all particle bytes, halfword wrap and RNG passed");
    hash=UINT64_C(14695981039346656037);
    FieldFxTaskCallback callbacks[]={FieldFx_DiamondParticle,FieldFx_DiamondEmitter,FieldFx_RingFlash,FieldFx_JitterSprite};
    const unsigned sizes[]={12,28,12,8};
    const int emitter_ages[]={0,1,7,8,20,24,25,31,32,52,53,71,72};
    for(unsigned callback=0;callback<4;++callback)for(unsigned seed=0;seed<4;++seed)for(unsigned age=0;age<13;++age){
        union {_Alignas(void *) u8 bytes[28];RenderDiamondEmitter emitter;} data;
        _Alignas(void *) char pool[1024]={0};FieldFxTaskSlot task={.end=pool};
        for(unsigned i=0;i<sizes[callback];++i)data.bytes[i]=i*37+emitter_ages[age]*3;
        g_FieldFxActiveTaskSlot=&task;g_FieldFxCurrentAge=emitter_ages[age];srand(seeds[seed]);
        if(FieldFx_InitCallbackList(pool,12,10,FieldFx_DiamondParticle)<0)return 0;
        u32 result=callbacks[callback](1,data.bytes,0),random=Psyz_RandState();
        const void *parts[]={&result,data.bytes,&random};const unsigned lengths[]={4,sizes[callback],4};
        for(unsigned part=0;part<3;++part)for(unsigned i=0;i<lengths[part];++i)
            hash=(hash^((const u8 *)parts[part])[i])*UINT64_C(1099511628211);
    }
    if(hash!=UINT64_C(0xf5ac1564e3f2d91d)){fprintf(stderr,"Scan/Confuse CPU hash %016llx\n",(unsigned long long)hash);return 0;}
    const struct {FieldFxTaskCallback callback;unsigned size;int stage;} slow_cases[]={
        {FieldFx_SpinningModelEmitterB,8,-1},{FieldFx_DescendingSprite,8,-1},
        {FieldFx_DescendingSpriteEmitter,8,-1},{FieldFx_SpinningModelFlareB,16,0},
        {FieldFx_SpinningModelFlareB,16,1}
    };
    const int slow_ages[]={0,1,3,4,5,18,19,23,24,25,39,40,69,70};
    hash=UINT64_C(14695981039346656037);
    for(unsigned callback=0;callback<5;++callback)for(unsigned seed=0;seed<4;++seed)for(unsigned age=0;age<14;++age){
        _Alignas(void *) u8 data[16],pool[2048];FieldFxTaskSlot task={.end=(char *)pool};
        for(unsigned i=0;i<slow_cases[callback].size;++i)data[i]=i*37+slow_ages[age]*3;
        if(callback==0)memcpy(data,&seeds[seed],4);
        if(slow_cases[callback].stage>=0){s16 stage=slow_cases[callback].stage;memcpy(data+6,&stage,2);}
        g_SpinningModelAnchorB[0]=-123;g_SpinningModelAnchorB[1]=-77;g_SpinningModelAnchorB[2]=1300;
        memset(pool,0x5a,sizeof(pool));g_FieldFxActiveTaskSlot=&task;g_FieldFxCurrentAge=slow_ages[age];srand(seeds[seed]);
        if(FieldFx_InitCallbackList((char *)pool,16,24,FieldFx_SpinningModelFlareB)<0)return 0;
        u32 result=slow_cases[callback].callback(1,data,0),random=Psyz_RandState();
        const void *parts[]={&result,data,&random};const unsigned lengths[]={4,slow_cases[callback].size,4};
        for(unsigned part=0;part<3;++part)for(unsigned i=0;i<lengths[part];++i)
            hash=(hash^((const u8 *)parts[part])[i])*UINT64_C(1099511628211);
        FieldFxCallbackList *list=(void *)pool;
        for(unsigned slot=0;slot<24;++slot){FieldFxCallbackListEntry *entry=(void *)(list->entries+slot*list->stride);
            for(unsigned i=0;i<4;++i)hash=(hash^((u8 *)entry)[i])*UINT64_C(1099511628211);
            for(unsigned i=0;i<16;++i)hash=(hash^entry->state[i])*UINT64_C(1099511628211);
        }
    }
    if(hash!=UINT64_C(0xed6c59607498484e)){fprintf(stderr,"Slow CPU hash %016llx\n",(unsigned long long)hash);return 0;}
    const struct {FieldFxTaskCallback callback;unsigned size,stride,count;} detox_cases[]={
        {FieldFx_CosineEffect,8,8,24},{FieldFx_CosineEmitter,12,8,24},
        {FieldFx_SineSweepTriangle,16,16,16},{FieldFx_SineEmitter,12,16,16}
    };
    const int detox_ages[]={0,1,7,8,16,17,22,25,50,51,63,64,73,80};
    hash=UINT64_C(14695981039346656037);
    for(unsigned callback=0;callback<4;++callback)for(unsigned seed=0;seed<4;++seed)for(unsigned age=0;age<14;++age){
        _Alignas(void *) u8 data[16],pool[2048];FieldFxTaskSlot task={.end=(char *)pool};
        for(unsigned i=0;i<detox_cases[callback].size;++i)data[i]=i*37+detox_ages[age]*3;
        if(callback==0){s16 duration=22+(seeds[seed]&3);memcpy(data+6,&duration,2);}
        memset(pool,0x5a,sizeof(pool));g_FieldFxActiveTaskSlot=&task;g_FieldFxCurrentAge=detox_ages[age];srand(seeds[seed]);
        if(FieldFx_InitCallbackList((char *)pool,detox_cases[callback].stride,detox_cases[callback].count,detox_cases[callback].callback)<0)return 0;
        u32 result=detox_cases[callback].callback(1,data,0),random=Psyz_RandState();
        const void *parts[]={&result,data,&random};const unsigned lengths[]={4,detox_cases[callback].size,4};
        for(unsigned part=0;part<3;++part)for(unsigned i=0;i<lengths[part];++i)
            hash=(hash^((const u8 *)parts[part])[i])*UINT64_C(1099511628211);
        FieldFxCallbackList *list=(void *)pool;
        for(unsigned slot=0;slot<detox_cases[callback].count;++slot){FieldFxCallbackListEntry *entry=(void *)(list->entries+slot*list->stride);
            for(unsigned i=0;i<4;++i)hash=(hash^((u8 *)entry)[i])*UINT64_C(1099511628211);
            for(unsigned i=0;i<detox_cases[callback].stride;++i)hash=(hash^entry->state[i])*UINT64_C(1099511628211);
        }
    }
    if(hash!=UINT64_C(0xe7f3d5cc149b7a78)){fprintf(stderr,"Detox CPU hash %016llx\n",(unsigned long long)hash);return 0;}
    /* USA CPU captures stop at LoadAverageCol, the GTE boundary. Negative
     * inherited particle times select the header exactly as retail does. */
    const struct {int time,offset,first,second;} color_cases[]={
        {INT32_MIN,0,4096,0},{-32768,0,33558528,-33554432},
        {-406,0,419840,-415744},{-1,0,5120,-1024},{0,8,4096,0},
        {1,8,3926,170},{23,8,171,3925},{24,16,4096,0},
        {25,16,2048,2048},{63,32,1152,2944},{64,32,1024,3072},
        {72,32,0,4096},{100,32,0,4096},{INT32_MAX,32,0,4096}
    };
    hash=UINT64_C(14695981039346656037);
    for(unsigned i=0;i<sizeof(color_cases)/sizeof(color_cases[0]);++i){
        _Alignas(u32) u8 track[48];RenderColor actual={0},expected={0};
        memcpy(track,g_DiamondParticleColorTrack,sizeof(track));memset(track,0,8);
        FieldFx_SampleColorTrack(track,&actual,color_cases[i].time);
        LoadAverageCol(track+color_cases[i].offset,
                       track+color_cases[i].offset+8,
                       color_cases[i].first,color_cases[i].second,&expected);
        if(memcmp(&actual,&expected,3))return 0;
        for(unsigned j=0;j<sizeof(track);++j)hash=(hash^track[j])*UINT64_C(1099511628211);
    }
    if(hash!=UINT64_C(0x3d88066c0df41791))return 0;
    GteMatrix view={.m={{4096,0,0},{0,4096,0},{0,0,4096}}};
    RenderMatrix matrix={.rotation={{4096,0,0},{0,4096,0},{0,0,4096}},.translation={0,0,1200}};
    RenderMatrix matrices[29];for(unsigned i=0;i<29;++i)matrices[i]=matrix;
    FieldActorState state={0};FieldActor player={.state=&state,.pos_z=1200*65536};player.render_object.matrices=matrices;
    static RenderGpuTag ordering[4096];
    struct {_Alignas(void *) u8 bytes[65536];u8 guard[16];} packets;
    s32 projection=256;u32 sound[32]={0};
    g_PlayerEntity=&player;g_GeomScroll.words.viewMatrix=&view;
    g_GeomScroll.state.position.projectionDistance=&projection;g_AkaoBgmHandle[0]=sound;
    g_GameState.bank_value_f8=100;g_GameState.bank_value_fa=1000;g_GameState.bank_value_f6=48;g_GameState.bank_value_f7=100;
    g_GameStateFlags=0x80;g_GameState.flags&=~8u;g_GameState.tim_load_state=0;
    Psyz_GteCtrlWrite(26,256);Psyz_GteCtrlWrite(29,1365);Psyz_GteCtrlWrite(30,1024);gte_SetGeomOffset(160,120);
    FieldFx_InitTexturePageTable();
    /* Consecutive outlines must reserve four native packets each, not the
     * PS1's fixed 64 bytes; otherwise the next outline overwrites its links. */
    for(unsigned draw=0;draw<2;++draw){
        memset(&packets,0xa5,sizeof(packets));
        for(unsigned i=0;i<4096;++i)ordering[i]=(RenderGpuTag){.address=0xffffff};
        g_ActiveDrawSlot=draw;g_DrawPacketOffset=0;g_FieldFxCurrentAge=0;
        D_800B0E38.ordering[draw]=ordering;D_800B0E38.packets[draw]=(char *)packets.bytes;
        g_DiamondParticleAnchor=(GteShortVector){0,0,1200,0};
        gte_ldrotmatrix(&view);gte_ldtransmatrix(&view);
        RenderDiamondParticle particle={.angle=128,.size=64,.color_time=2};
        for(unsigned i=0;i<3;++i)if(FieldFx_DiamondParticle(2,&particle,0))return 0;
        if(g_DrawPacketOffset!=12*(sizeof(RenderLinePacket)+sizeof(RenderTintMode)))return 0;
        for(unsigned i=g_DrawPacketOffset;i<sizeof(packets);++i)if(((u8 *)&packets)[i]!=0xa5)return 0;
    }
    _Alignas(16) u8 model_bytes[64]={0};FieldTexturedModel *model=(void *)model_bytes;
    model->header.flatTris=1;model->header.colorOffset=52;model->header.colorCount=3;
    for(unsigned i=0;i<3;++i){model->header.tris[0].vertex[i]=28+i*8;model->header.tris[0].uv[i]=0x1000+i;
        FieldModelVertex *v=(void *)(model_bytes+28+i*8);v->x=i==0?-20:i==1?20:0;v->y=i==2?20:-20;v->color=52+i*4;
        u32 color=0x00302010+i;memcpy(model_bytes+52+i*4,&color,4);}
    _Alignas(16) u8 twin_model_bytes[64];memcpy(twin_model_bytes,model_bytes,sizeof(twin_model_bytes));
    D_800F32D8=D_800F3418=g_FieldFxModel83ACD504=model_bytes;g_FieldFxTwinModelB=twin_model_bytes;
    _Alignas(16) u8 spiral_model_bytes[64];memcpy(spiral_model_bytes,model_bytes,sizeof(spiral_model_bytes));
    g_FieldFxModel6DEAE684=spiral_model_bytes;
    g_RoomFloorY->y=0;player.render_object.target_z=1200;
    const unsigned commands[]={92,93,101,102,99,100,103,94,95,96,97,98,104,106,105,107,85,86,87,88,89,90,91},variables[]={7,8,16,17,13,15,18,9,10,11,12,13,19,21,20,22,0,1,2,3,4,5,6};
    for(unsigned spell=0;spell<19;++spell){
        unsigned command=g_BattleActionEffectIds.bytes[spell];
        if(command<85 || command>=189 || !g_FieldFxTaskPrograms[command-85])return 0;
    }
    if(g_FieldFxTaskPrograms[18]!=&g_FieldFxTaskProgramCmd103 ||
       g_FieldFxTaskProgramCmd103.table.callbacks[0]!=FieldFx_TwinModelEffect ||
       g_FieldFxTaskProgramCmd103.table.sizes[0]!=sizeof(FieldFxTwinModel))return 0;
    for(unsigned variant=0;variant<sizeof(commands)/sizeof(commands[0]);++variant){
        struct {PmPrimarySlot slots[11];u8 guard[16];} bank={0};memset(bank.guard,0xa5,16);
        g_PmTables=(PmTables){g_PmCmdHandlerArray,bank.slots,NULL};srand(1);
        if(Scene_LoadRoomAssets(commands[variant],&player)!=0)return 0;
        if((commands[variant]==94 && bank.slots[0].task.tasks.table->sizes[0]<sizeof(FieldFanSweep)) ||
           (commands[variant]==95 && bank.slots[0].task.tasks.table->sizes[2]<sizeof(FieldFanSweep)))return 0;
        /* Retail hit/status/death call sites create these without send. */
        if(commands[variant]!=88 && commands[variant]!=89 && commands[variant]!=91 && Pm_SendCmd(0,0,0,0,0,0))return 0;
        unsigned draws=0,particles=0;
        for(unsigned frame=0;frame<200;++frame){
            for(unsigned i=0;i<4096;++i)ordering[i]=(RenderGpuTag){.address=0xffffff};
            memset(packets.guard,0xa5,16);g_ActiveDrawSlot=frame&1;g_DrawPacketOffset=0;
            D_800B0E38.ordering[g_ActiveDrawSlot]=ordering;D_800B0E38.packets[g_ActiveDrawSlot]=(char *)packets.bytes;
            if(Pm_Start(0) || Pm_Exec(0))return 0;
            draws+=g_DrawPacketOffset!=0;
            FieldFxTaskContext *ctx=&bank.slots[0].task.tasks;
            for(unsigned slot=0;slot<8;++slot)if(ctx->slots[slot].id!=0xffff && ctx->slots[slot].end){
                FieldFxTaskCallback callback=ctx->table->callbacks[ctx->slots[slot].id];
                {
                    FieldFxCallbackList *list=(void *)ctx->slots[slot].end;
                    for(unsigned i=0;i<(unsigned)list->count;++i)particles+=((FieldFxCallbackListEntry *)(list->entries+i*list->stride))->active!=0;
                }
            }
            if(ctx->used>sizeof(ctx->arena) || g_DrawPacketOffset<0 || g_DrawPacketOffset>sizeof(packets.bytes))return 0;
            for(unsigned i=0;i<4096;++i){uintptr_t at=ordering[i].address;unsigned count=0;while(at && at!=0xffffff){
                if(at<(uintptr_t)packets.bytes || at+sizeof(RenderGpuTag)>(uintptr_t)packets.bytes+g_DrawPacketOffset || ++count>512)return 0;
                at=((RenderGpuTag *)at)->address;
            }}
            for(unsigned i=0;i<16;++i)if(bank.guard[i]!=0xa5 || packets.guard[i]!=0xa5)return 0;
        }
        if(!draws || !particles || bank.slots[0].header.state || bank.slots[0].task.tasks.count ||
           bank.slots[0].task.tasks.variables[6]!=variables[variant]){fprintf(stderr,"Resident task %u: draws=%u particles=%u state=%u count=%u variable=%d\n",commands[variant],draws,particles,bank.slots[0].header.state,bank.slots[0].task.tasks.count,bank.slots[0].task.tasks.variables[6]);return 0;}
    }
    g_PmTables=old_tables;g_GameState=old_game;g_GameStateFlags=old_flags;g_PlayerEntity=old_player;
    g_FieldFxTaskContext=old_context;g_FieldFxOwner=old_owner;g_FieldFxActiveTaskSlot=old_task;g_FieldFxCurrentAge=old_age;
    D_800B0E38=old_buffers;g_GeomScroll.words.viewMatrix=old_view;g_ActiveDrawSlot=old_slot;g_DrawPacketOffset=old_offset;
    g_SineEffectAnchor=old_sine;g_FanSweepSparkAnchor=old_anchor;g_FanSweepSparkTrailPool=old_trail;g_DiamondParticleAnchor=old_diamond;g_SparkEmitterPhaseSeed=old_phase;srand(old_random);
    D_800F32D8=old_model;D_800F3418=old_twin_a;g_FieldFxTwinModelB=old_twin_b;g_FieldFxModel83ACD504=old_flash;
    g_OrbitingEffectAnchor=old_orbit;g_HelicalEffectAnchor=old_helical;
    g_VerticalEffectAnchor=old_vertical;g_TiltingSpriteAnchor=old_tilting;g_SettlingSpriteAnchor=old_settling;
    g_ConvergingSpriteAnchor=old_converging;g_FadeEmitterIntensity=old_fade_intensity;
    g_SpiralEffectAnchor=old_spiral;g_SpiralEffectRadius=old_spiral_radius;g_FieldFxModel6DEAE684=old_spiral_model;
    g_GeomScroll.state.position.projectionDistance=old_projection;
    g_OrbitingEffectParticlePool=old_orbit_pool;g_HelicalEffectParticlePool=old_helical_pool;
    memcpy(g_SpinningModelAnchorA,old_model_anchor_a,sizeof(old_model_anchor_a));
    g_RoomFloorY->y=old_floor;memcpy(g_SpinningModelAnchorB,old_model_anchor,6);
    puts("Original resident particles: 100 ember + 208 Scan/Confuse + 280 Slow + 224 Detox unhooked USA CPU/BIOS states passed");
    puts("Original resident effects: 23 complete authored programs x 200 PM frames, native pointer payloads, models, nested pools, both draw buffers and expiry passed");
    return 1;
}

static int authored_m005_effect_cycle(void)
{
    PmPrimarySlot bank[11]={0};FieldActor actor={0};FieldActorState state={0};u8 action=1;
    RenderMatrix matrices[21]={0};
    for(unsigned i=0;i<21;++i) {
        for(unsigned axis=0;axis<3;++axis)matrices[i].rotation[axis][axis]=4096;
        matrices[i].translation[1]=-100;matrices[i].translation[2]=1024;
    }
    actor.state=&state;actor.mode=2;actor.render_object.matrices=matrices;
    state.action_state=&action;state.control10.command_value=1;
    PmTables old_tables=g_PmTables;Pe1GameState old_game=g_GameState;u32 old_flags=g_GameStateFlags;
    RenderBufferPrefix old_buffers=D_800B0E38;GteMatrix *old_view=g_GeomScroll.words.viewMatrix;
    int old_slot=g_ActiveDrawSlot,old_offset=g_DrawPacketOffset,old_age=g_FieldFxCurrentAge;
    void *old_program=g_FieldFxTaskPrograms[32];u32 old_random=Psyz_RandState();
    FieldFxTaskContext *old_context=g_FieldFxTaskContext;FieldFxObjectPrefix *old_owner=g_FieldFxOwner;
    FieldFxTaskSlot *old_task=g_FieldFxActiveTaskSlot;
    GteMatrix view={.m={{4096,0,0},{0,4096,0},{0,0,4096}}};
    static RenderGpuTag ordering[4096];
    struct { _Alignas(void *) u8 bytes[65536];u8 guard[16]; } packets;
    g_PmTables=(PmTables){g_PmCmdHandlerArray,bank,NULL};g_FieldFxTaskPrograms[32]=&g_RoomM005Effects;
    g_GameStateFlags=0x80;g_GameState.flags&=~8u;g_GameState.tim_load_state=0;
    struct {SceneAssetBlob header;SceneAssetDirectory directory;} archive={0};
    archive.header.directoryOffset=sizeof(SceneAssetBlob);
    g_GameState.loaded_scene_archive=&archive.header;
    g_GeomScroll.words.viewMatrix=&view;
    Psyz_GteCtrlWrite(26,256);Psyz_GteCtrlWrite(27,1);Psyz_GteCtrlWrite(28,0);
    Psyz_GteCtrlWrite(29,1365);Psyz_GteCtrlWrite(30,1024);gte_SetGeomOffset(160,120);
    FieldFx_InitTexturePageTable();srand(1);
    memset(g_RoomM005ScriptAnchorView-4,0,20);
    if(Scene_LoadRoomAssets(117,&actor)!=0 || Pm_SendCmd(0,0,0,0,0,0)!=0)return 0;
    unsigned drawn=0,linked=0,ring_links=0,blend_links=0;
    for(unsigned frame=0;frame<76;++frame) {
        memset(&packets,0xa5,sizeof(packets));
        for(unsigned depth=0;depth<4096;++depth)ordering[depth]=(RenderGpuTag){.address=0xffffff};
        g_ActiveDrawSlot=frame&1;g_DrawPacketOffset=0;
        D_800B0E38.ordering[g_ActiveDrawSlot]=ordering;D_800B0E38.packets[g_ActiveDrawSlot]=(char *)packets.bytes;
        if(frame==30 && Pm_SendCmd(0,0,1,0,0,0)!=0)return 0;
        if(Pm_Start(0)!=0 || Pm_Exec(0)!=0 || bank[0].task.tasks.count!=((frame<41 || frame>=62)?1:2))return 0;
        if(g_DrawPacketOffset<0 || g_DrawPacketOffset>sizeof(packets.bytes))return 0;
        drawn+=g_DrawPacketOffset!=0;
        for(unsigned depth=0;depth<4096;++depth) {
            uintptr_t address=ordering[depth].address;unsigned chain=0;
            while(address!=0xffffff) {
                if(address<(uintptr_t)packets.bytes || address+sizeof(RenderGpuTag)>(uintptr_t)packets.bytes+g_DrawPacketOffset ||
                    address%_Alignof(RenderGpuTag) || ++chain>512)return 0;
                RenderGpuTag *tag=(void *)address;
                if(tag->length!=9 && tag->length!=8 && tag->length!=1)return 0;
                ring_links+=tag->length==8;blend_links+=tag->length==1;
                ++linked;address=tag->address;
            }
        }
        for(unsigned i=0;i<16;++i)if(packets.guard[i]!=0xa5)return 0;
    }
    RoomM005OrbiterState *orbiter=(void *)bank[0].task.tasks.slots[0].start;
    if(!drawn || !linked || !ring_links || !blend_links || orbiter->armed!=1 || orbiter->frame!=32 ||
        bank[0].task.tasks.slots[0].id!=0xffff || bank[0].task.tasks.slots[1].id!=1 ||
        bank[0].task.tasks.slots[1].age!=35 || bank[0].task.tasks.variables[0]!=1)return 0;
    s16 *anchor_words=(void *)g_RoomM005ScriptAnchorView,*fade_words=(void *)g_RoomM005ScriptFadeView;
    if(anchor_words[0] || anchor_words[1]!=-100 || anchor_words[2]!=1024 ||
        fade_words[0] || fade_words[1]!=-100 || fade_words[2]!=964)return 0;
    g_FieldFxCurrentAge=35;
    if(RoomM005_CrossFlare(1,bank[0].task.tasks.slots[1].start,bank[0].task.tasks.argument)!=1)return 0;
    if(Pm_Stop(0,&actor,0)!=0 || bank[0].header.state)return 0;
    g_FieldFxTaskContext=old_context;g_FieldFxOwner=old_owner;g_FieldFxActiveTaskSlot=old_task;g_FieldFxCurrentAge=old_age;
    g_PmTables=old_tables;g_GameState=old_game;g_GameStateFlags=old_flags;g_FieldFxTaskPrograms[32]=old_program;
    D_800B0E38=old_buffers;g_GeomScroll.words.viewMatrix=old_view;g_ActiveDrawSlot=old_slot;g_DrawPacketOffset=old_offset;
    srand(old_random);memset(g_RoomM005ScriptAnchorView-4,0,20);
    puts("Original M005: 76 PM frames, real orbiter/cross-flare script, overlapping tasks, OT rings/blends and arena guards passed");
    return 1;
}

static int authored_orbit_flare_cycle(void)
{
    PmPrimarySlot bank[11]={0};FieldActor actor={0},player={0};FieldActorState state={0};u8 action=1;
    RenderMatrix matrices[26]={0};
    for(unsigned i=0;i<26;++i){
        for(unsigned axis=0;axis<3;++axis)matrices[i].rotation[axis][axis]=4096;
        matrices[i].translation[0]=20;matrices[i].translation[2]=1024;
    }
    actor.state=&state;actor.mode=2;actor.render_object.matrices=matrices;
    actor.render_object.target_x=20;actor.render_object.target_z=1024;
    state.action_state=&action;player.pos_x=10000<<16;player.pos_z=10000<<16;
    PolygonVertex polygon[]={{0,-1100,0,-1100},{0,1100,0,-1100},{0,1100,0,1100},{0,-1100,0,1100}};
    PmTables old_tables=g_PmTables;Pe1GameState old_game=g_GameState;u32 old_flags=g_GameStateFlags;
    FieldActor *old_player=g_PlayerEntity;PolygonVertex *old_floor=g_FloorPolygonVertices;
    u16 old_count=g_FloorPolygonVertexCount;
    RenderBufferPrefix old_buffers=D_800B0E38;GteMatrix *old_view=g_GeomScroll.words.viewMatrix;
    int old_slot=g_ActiveDrawSlot,old_offset=g_DrawPacketOffset,old_age=g_FieldFxCurrentAge;
    void *old_program=g_FieldFxTaskPrograms[30];u32 old_random=Psyz_RandState();
    int old_args[3];memcpy(old_args,g_RoomScriptArgs,sizeof(old_args));
    FieldFxTaskContext *old_context=g_FieldFxTaskContext;FieldFxObjectPrefix *old_owner=g_FieldFxOwner;
    FieldFxTaskSlot *old_task=g_FieldFxActiveTaskSlot;
    const u32 patterns[]={0,1,0x7fff,0x8000,0xffff,0x80000000,0xffffffff,0x010c0000,0xa5a55a5a,0x12345678,0x7fffffff,0x600000};
    uint64_t init_hash=UINT64_C(14695981039346656037);
    const int modes[]={0,1,-1};
    for(unsigned m=0;m<3;++m)for(unsigned p=0;p<12;++p){
        intptr_t result=RoomLib_SetArgs3(modes[m],(s32)patterns[p],(s32)patterns[(p+1)%12],(s32)patterns[(p+2)%12]);
        const u32 words[]={result==(intptr_t)g_RoomScriptArgs,(u32)g_RoomScriptArgs[0],(u32)g_RoomScriptArgs[1],(u32)g_RoomScriptArgs[2]};
        for(unsigned j=0;j<4;++j)for(unsigned b=0;b<4;++b)
            init_hash=(init_hash^((words[j]>>(b*8))&255))*UINT64_C(1099511628211);
    }
    if(init_hash!=UINT64_C(0x3c0af86f8810cdb5))return 0;
    uint64_t drift_hash=UINT64_C(14695981039346656037);
    const int drift_modes[]={0,1,3},ages[]={0,1,13,14,15,32767},positions[]={0,1,-1,32767,-32768,1234};
    const u32 seeds[]={1,0xffffffff};
    for(unsigned m=0;m<3;++m)for(unsigned a=0;a<6;++a)for(unsigned p=0;p<6;++p)for(unsigned seed=0;seed<2;++seed){
        RoomFxDriftParticle drift={.x=positions[p],.y=(s16)-positions[p],.z=(s16)(positions[p]^0x5555),
            .pad6=(s16)0xa55a,.radius=0x12345678};
        srand(seeds[seed]);g_FieldFxCurrentAge=ages[a];
        u32 result=RoomEffect_OrbitFlareDrift(drift_modes[m],&drift,0);
        for(unsigned b=0;b<4;++b)drift_hash=(drift_hash^((result>>(b*8))&255))*UINT64_C(1099511628211);
        for(unsigned b=0;b<sizeof(drift);++b)drift_hash=(drift_hash^((u8 *)&drift)[b])*UINT64_C(1099511628211);
        u32 random=Psyz_RandState();
        for(unsigned b=0;b<4;++b)drift_hash=(drift_hash^((random>>(b*8))&255))*UINT64_C(1099511628211);
    }
    if(drift_hash!=UINT64_C(0xf3429d82d5c5a69d))return 0;
    GteMatrix view={.m={{4096,0,0},{0,4096,0},{0,0,4096}}};
    static RenderGpuTag ordering[4096];
    struct {_Alignas(void *) u8 bytes[65536];u8 guard[16];} packets;
    g_PmTables=(PmTables){g_PmCmdHandlerArray,bank,NULL};g_FieldFxTaskPrograms[30]=&g_RoomOrbitFlareEffects;
    g_PlayerEntity=&player;g_FloorPolygonVertices=polygon;g_FloorPolygonVertexCount=4;
    FieldActorState player_state={0};player.state=&player_state;
    FieldFxObjectPrefix owner={.actor=&actor};FieldFxTaskContext context={0};
    g_FieldFxOwner=&owner;g_FieldFxTaskContext=&context;g_FieldFxCurrentAge=0;
    uint64_t controller_hash=UINT64_C(14695981039346656037);
    const u16 frames[]={0,1,15,16,51,52,32767,65535};
    const int flight_positions[]={0,1000,-1000,1100,-1100,3000},angles[]={0,1024,2048,3072,-1},radii[]={0,64,-64};
    for(unsigned phase=1;phase<=3;++phase)for(unsigned f=0;f<8;++f)for(unsigned p=0;p<6;++p)
    for(unsigned angle=0;angle<5;++angle)for(unsigned r=0;r<3;++r)for(unsigned near=0;near<2;++near)for(unsigned flags=0;flags<2;++flags){
        RoomOrbitFlareState flight={.x=flight_positions[p],.y=20,.z=flight_positions[p],.reserved06=-5,
            .state=phase,.frame=frames[f],.soundHandle=-1,.angle=angles[angle],.height=100,.spread=60};
        RoomOrbitFlareSpawn spawn={radii[r],512,1};
        context.flags=flags;state.core_flags=0x01000000;player_state.flags=0;
        player.pos_x=(s32)((u32)(near?flight_positions[p]:10000)<<16);player.pos_z=player.pos_x;
        u32 result=RoomEffect_OrbitFlareController(1,&flight,(intptr_t)&spawn);
        for(unsigned b=0;b<4;++b)controller_hash=(controller_hash^((result>>(b*8))&255))*UINT64_C(1099511628211);
        for(unsigned b=0;b<sizeof(flight);++b)controller_hash=(controller_hash^((u8 *)&flight)[b])*UINT64_C(1099511628211);
        const u32 words[]={state.core_flags,player_state.flags};
        for(unsigned j=0;j<2;++j)for(unsigned b=0;b<4;++b)
            controller_hash=(controller_hash^((words[j]>>(b*8))&255))*UINT64_C(1099511628211);
    }
    if(controller_hash!=UINT64_C(0xa1ad64689555871d)){fprintf(stderr,"Orbit controller hash %016llx\n",(unsigned long long)controller_hash);return 0;}
    player.pos_x=player.pos_z=10000<<16;
    g_GameStateFlags=0x80;g_GameState.flags&=~8u;g_GameState.tim_load_state=0;
    struct {SceneAssetBlob header;SceneAssetDirectory directory;} archive={0};
    archive.header.directoryOffset=sizeof(SceneAssetBlob);g_GameState.loaded_scene_archive=&archive.header;
    g_GeomScroll.words.viewMatrix=&view;
    Psyz_GteCtrlWrite(26,256);Psyz_GteCtrlWrite(27,1);Psyz_GteCtrlWrite(28,0);
    Psyz_GteCtrlWrite(29,1365);Psyz_GteCtrlWrite(30,1024);gte_SetGeomOffset(160,120);
    FieldFx_InitTexturePageTable();srand(1);
    int init=Scene_LoadRoomAssets(115,&actor),send=init?init:Pm_SendCmd(0,0,0,64,512,1);
    if(init || send){fprintf(stderr,"Orbit init=%d send=%d\n",init,send);return 0;}
    if(g_RoomScriptArgs[0]!=64 || g_RoomScriptArgs[1]!=512 || g_RoomScriptArgs[2]!=1 ||
       bank[0].task.tasks.argument!=(intptr_t)g_RoomScriptArgs){fprintf(stderr,"Orbit args %d %d %d\n",g_RoomScriptArgs[0],g_RoomScriptArgs[1],g_RoomScriptArgs[2]);return 0;}
    unsigned drawn=0,linked=0,seen=0,maximum=0;
    for(unsigned frame=0;frame<160;++frame){
        memset(&packets,0xa5,sizeof(packets));
        for(unsigned depth=0;depth<4096;++depth)ordering[depth]=(RenderGpuTag){.address=0xffffff};
        g_ActiveDrawSlot=frame&1;g_DrawPacketOffset=0;
        D_800B0E38.ordering[g_ActiveDrawSlot]=ordering;D_800B0E38.packets[g_ActiveDrawSlot]=(char *)packets.bytes;
        int start=Pm_Start(0),exec=start?start:Pm_Exec(0);
        if(start || exec){fprintf(stderr,"Orbit frame %u start=%d exec=%d\n",frame,start,exec);return 0;}
        for(unsigned i=0;i<8;++i)if(bank[0].task.tasks.slots[i].id!=0xffff){
            seen|=1u<<i;
            if(bank[0].task.tasks.slots[i].age>maximum)maximum=bank[0].task.tasks.slots[i].age;
        }
        if(g_DrawPacketOffset<0 || g_DrawPacketOffset>sizeof(packets.bytes))return 0;
        drawn+=g_DrawPacketOffset!=0;
        for(unsigned depth=0;depth<4096;++depth){
            uintptr_t address=ordering[depth].address;unsigned chain=0;
            while(address!=0xffffff){
                if(address<(uintptr_t)packets.bytes || address+sizeof(RenderGpuTag)>(uintptr_t)packets.bytes+g_DrawPacketOffset ||
                   address%_Alignof(RenderGpuTag) || ++chain>1024)return 0;
                RenderGpuTag *tag=(void *)address;++linked;address=tag->address;
            }
        }
        for(unsigned i=0;i<16;++i)if(packets.guard[i]!=0xa5)return 0;
    }
    if(!drawn || !linked || seen!=7 || maximum<52 || bank[0].task.tasks.count || action!=4){
        fprintf(stderr,"Orbit lifecycle drawn=%u linked=%u seen=%x age=%u count=%u action=%u\n",drawn,linked,seen,maximum,bank[0].task.tasks.count,action);return 0;
    }
    if(Pm_Stop(0,&actor,0)!=0 || bank[0].header.state)return 0;
    g_FieldFxTaskContext=old_context;g_FieldFxOwner=old_owner;g_FieldFxActiveTaskSlot=old_task;g_FieldFxCurrentAge=old_age;
    g_PmTables=old_tables;g_GameState=old_game;g_GameStateFlags=old_flags;g_FieldFxTaskPrograms[30]=old_program;
    g_PlayerEntity=old_player;g_FloorPolygonVertices=old_floor;g_FloorPolygonVertexCount=old_count;
    D_800B0E38=old_buffers;g_GeomScroll.words.viewMatrix=old_view;g_ActiveDrawSlot=old_slot;g_DrawPacketOffset=old_offset;
    srand(old_random);memcpy(g_RoomScriptArgs,old_args,sizeof(old_args));
    puts("Original orbit flare: 36 initializer, 216 drift and 8640 flight/end USA CPU states (BIOS rand boundary controlled) passed");
    puts("Original orbit flare: 160 real PM frames, three authored controller slots, drift list draw/update, native OT guards and complete expiry passed");
    return 1;
}

static int original_room_effect_core_flags(void)
{
    const u32 seeds[]={0,1,0xffffffff,0x80000000,0xc0ffffff,0x07060000,0x3f000000,0x40000000};
    int (*handlers[])(PmSlotHeader *)={RoomLib_CloseTarget,RoomEffect_DropFieldClose,RoomEffect_DropFieldRegister};
    FieldActor *old_player=g_PlayerEntity;FieldFxState *old_state=g_FieldFxScriptState;
    FieldFxObject *old_object=g_FieldFxScriptObject;FieldFxSlot *old_slots=g_FieldFxSlotTable;
    char *old_data=g_FieldFxScriptData;int *old_commands=g_FieldFxCommandTable;
    uint64_t hash=UINT64_C(14695981039346656037);unsigned cases=0;
    for(unsigned method=0;method<3;++method)for(unsigned status=0;status<8;++status)for(unsigned seed=0;seed<8;++seed){
        FieldActorState core;memset(&core,0xa5,sizeof(core));FieldActor actor={0},other={0};u8 phase=1;
        core.core_flags=seeds[seed];core.control10.command_value=status==2?0:status==3?-1:1;
        core.flags=0x07060000;core.action_state=&phase;
        actor.state=status==1?NULL:&core;g_PlayerEntity=status==7?&actor:&other;
        FieldFxObject object={0};memset(&object,0xa5,8);object.prefix.actor=status==0?NULL:&actor;
        object.prefix.asset_type=status==5?7:status==6?14:0;
        FieldActorState expected=core;int running=status>=2;
        if(method<2 && running)expected.core_flags &= 0xc0ffffff;
        if(method==2 && status==4)expected.core_flags &= 0xbfffffff;
        u32 result=handlers[method]((PmSlotHeader *)&object);u8 output[21];
        memcpy(output,&result,4);memcpy(output+4,&object,8);memcpy(output+12,&core.core_flags,4);memcpy(output+16,&core.flags,4);output[20]=phase;
        for(unsigned i=0;i<sizeof(output);++i)hash=(hash^output[i])*UINT64_C(1099511628211);
        if(memcmp(&core,&expected,sizeof(core)) || phase!=(method<2 && running?4:1))return 0;
        ++cases;
    }
    g_PlayerEntity=old_player;g_FieldFxScriptState=old_state;g_FieldFxScriptObject=old_object;
    g_FieldFxSlotTable=old_slots;g_FieldFxScriptData=old_data;g_FieldFxCommandTable=old_commands;
    if(cases!=192 || hash!=UINT64_C(0x15ee6173ba0071b5)){fprintf(stderr,"Room effect core flags %u %016llx\n",cases,(unsigned long long)hash);return 0;}
    /* The actor outlives its animation event while E08 effect slots drain. */
    int (*close_handlers[])(PmSlotHeader *)={RoomLib_CloseTarget,RoomEffect_DropFieldClose,SceneE08_OrbClose,SceneE08_SlotsClose,SceneE08_LayeredSpriteClose};
    for(unsigned method=0;method<5;++method)for(unsigned active=0;active<2;++active){
        FieldActorState core={0};FieldActor actor={0};FieldFxObject object={0};u8 phase=1;
        core.core_flags=0xffffffffu;core.control10.command_value=1;
        core.action_state=active?&phase:NULL;actor.state=&core;object.prefix.actor=&actor;
        FieldActor other={0};g_PlayerEntity=&other;
        if(close_handlers[method]((PmSlotHeader *)&object)!=0 ||
           ((PmSlotHeader *)&object)->state!=4 || core.core_flags!=0xc0ffffffu ||
           phase!=(active?4:1) || core.action_state!=(active?&phase:NULL))return 0;
    }
    g_PlayerEntity=old_player;
    puts("Original effect closure: five handlers preserve teardown with active or expired animation events passed");
    puts("Original room effect closure/registration: 192 unhooked USA CPU states, native owner/player/dead/null modes and every adjacent event byte preserved passed");
    return 1;
}

static int original_m086_states(void)
{
    FieldFxTaskContext *old_context=g_FieldFxTaskContext;FieldFxTaskSlot *old_task=g_FieldFxActiveTaskSlot;
    FieldFxObjectPrefix *old_owner=g_FieldFxOwner;FieldActor *old_player=g_PlayerEntity;
    GteMatrix *old_view=g_GeomScroll.words.viewMatrix;int old_age=g_FieldFxCurrentAge,old_floor=g_RoomFloorY->y;u32 old_random=Psyz_RandState();
    GteMatrix view={.m={{4096,0,0},{0,4096,0},{0,0,4096}}};g_GeomScroll.words.viewMatrix=&view;
    FieldFxTaskContext context={0};FieldFxObjectPrefix owner={0};FieldActor actor={0},player={0};FieldActorState state={0};Combatant player_state={0};
    owner.actor=&actor;actor.state=&state;player.state=(void *)&player_state;player.render_object.hit_cylinder.radius=63;
    g_FieldFxOwner=&owner;g_PlayerEntity=&player;g_FieldFxTaskContext=&context;g_RoomFloorY->y=0;
    FieldFxTaskCallback callbacks[]={RoomM086_SeekerCallback,RoomM086_SeekerController,RoomM086_BounceGlintCallback,RoomM086_BounceGlint};
    const unsigned sizes[]={20,28,20,8};const u32 seeds[]={0,1,0x12345678,0xffffffff};
    const int ages[]={0,1,2,3,4,7,8,23,24,31,32,70},timers[]={-1,0,1,7,8,23,24,31,32,63,64,32767};
    uint64_t hash=UINT64_C(14695981039346656037);
    for(unsigned callback=0;callback<4;++callback)for(unsigned seed=0;seed<4;++seed)for(unsigned variant=0;variant<12;++variant){
        _Alignas(void *) u8 data[44],pool[2048];FieldFxTaskSlot task={.end=(char *)pool};int params[]={4096,100,256,4};
        memset(data,0xa5,sizeof(data));for(unsigned j=0;j<sizes[callback];++j)data[j]=j*37+variant*3;
        if(callback<3){s16 words[]={timers[variant],variant%(callback<2?3:2)};memcpy(data+sizes[callback]-4,words,4);}
        if(callback==0){s16 point[]={0,-100,0};memcpy(data,point,6);}
        if(callback==1){s16 point[]={123,variant&1?-400:-100,-77};memcpy(data,point,6);}
        context.flags=variant&1;state.core_flags=0x01234567;player_state.stateFlags=0x10203040;
        g_FieldFxCurrentAge=ages[variant];g_FieldFxActiveTaskSlot=&task;srand(seeds[seed]);memset(pool,0x5a,sizeof(pool));
        if(FieldFx_InitCallbackList((char *)pool,20,24,RoomM086_SeekerCallback)<0)return 0;
        u32 result=callbacks[callback](1,data,(intptr_t)params),random=Psyz_RandState();
        const void *parts[]={&result,data,&random,&state.core_flags,&player_state.stateFlags};unsigned lengths[]={4,sizes[callback],4,4,4};
        for(unsigned part=0;part<5;++part)for(unsigned j=0;j<lengths[part];++j)hash=(hash^((const u8 *)parts[part])[j])*UINT64_C(1099511628211);
        FieldFxCallbackList *list=(void *)pool;
        for(unsigned slot=0;slot<24;++slot){FieldFxCallbackListEntry *entry=(void *)(list->entries+slot*list->stride);
            for(unsigned j=0;j<4;++j)hash=(hash^((u8 *)entry)[j])*UINT64_C(1099511628211);
            for(unsigned j=0;j<20;++j)hash=(hash^entry->state[j])*UINT64_C(1099511628211);
        }
        for(unsigned j=sizes[callback];j<sizeof(data);++j)if(data[j]!=0xa5)return 0;
    }
    if(hash!=UINT64_C(0x3c2b77bfca9f9902)){fprintf(stderr,"M086 CPU hash %016llx\n",(unsigned long long)hash);return 0;}
    int old_params[7];memcpy(old_params,g_RoomM086ControllerParams,16);memcpy(old_params+4,g_RoomM086BounceGlintParams,12);
    if(RoomM086_SetControllerParams(1,-1,100,256)!=(intptr_t)g_RoomM086ControllerParams || RoomM086_SetControllerParams(2,4,0,0)!=(intptr_t)g_RoomM086ControllerParams ||
       memcmp(g_RoomM086ControllerParams,(int[]){-1,100,256,4},16) || RoomM086_SetBounceGlintParams(1,-2,3,4)!=(intptr_t)g_RoomM086BounceGlintParams ||
       memcmp(g_RoomM086BounceGlintParams,(int[]){4096,3,4},12))return 0;
    memcpy(g_RoomM086ControllerParams,old_params,16);memcpy(g_RoomM086BounceGlintParams,old_params+4,12);
    g_FieldFxTaskContext=old_context;g_FieldFxActiveTaskSlot=old_task;g_FieldFxOwner=old_owner;g_PlayerEntity=old_player;
    g_GeomScroll.words.viewMatrix=old_view;g_FieldFxCurrentAge=old_age;g_RoomFloorY->y=old_floor;srand(old_random);
    puts("Original M086: 192 unhooked USA CPU/ROM BIOS/GTE update states, all particle slots, RNG, contact flags, guards and native parameter initializers passed");return 1;
}

static int original_paired_glow_states(void)
{
    static const s16 ages[]={-1,0,1,7,15,16,17,23,31,32,33,32767};
    static const s16 values[]={-32768,-1,0,1,32767},intensity[]={4,5,128,32767,-1};
    FieldFxState script={0},*saved=g_FieldFxScriptState;
    uint64_t hash=UINT64_C(14695981039346656037);unsigned cases=0;
    FieldFxHandler handlers[]={g_RoomUpdateList__paired[1],g_RoomUpdateList__paired[4],
        g_RoomUpdateList__paired[3],g_RoomInitList__paired[3]};
    g_FieldFxScriptState=&script;
    for(unsigned callback=0;callback<4;++callback)for(unsigned a=0;a<12;++a)for(unsigned j=0;j<5;++j){
        struct {u64 before;RoomFxPairedEmitterState data;u64 after;} guard;
        guard.before=guard.after=UINT64_C(0x1234567887654321);
        u8 *bytes=(u8 *)&guard.data;for(unsigned i=0;i<sizeof(guard.data);++i)bytes[i]=(u8)(i*37+j*3);
        RoomFxPairedEmitterState *emitter=&guard.data;RoomDoubleSpriteFxParams *sprite=(void *)bytes;
        emitter->timer=values[j];emitter->intensity=intensity[j];emitter->phase=values[4-j];
        sprite->scale=values[j];sprite->depth=(u16)intensity[j];sprite->seed.vector.y=values[4-j];
        RoomFxControl control={7,1,ages[a]};RoomLibFxMatrixState clock={0};clock.hit=-7;
        FieldActor actor={0};FieldFxObjectPrefix object={0};object.actor=&actor;
        script.current_context=&clock;
        int returned=handlers[callback](&object,&control,bytes);
        if(returned || guard.before!=UINT64_C(0x1234567887654321) || guard.after!=UINT64_C(0x1234567887654321)){
            g_FieldFxScriptState=saved;return 0;
        }
        u8 fields[6];memcpy(fields,&emitter->timer,6);
        const u8 *parts[]={callback<2?fields:bytes,(u8 *)&control,(u8 *)&clock.hit};
        const size_t sizes[]={callback<2?6:20,4,2};
        for(unsigned part=0;part<3;++part)for(unsigned i=0;i<sizes[part];++i)hash=(hash^parts[part][i])*UINT64_C(1099511628211);
        ++cases;
    }
    g_FieldFxScriptState=saved;
    if(cases!=240 || hash!=UINT64_C(0x18c13e84019080a0)){fprintf(stderr,"Paired glow states: %u %llx\n",cases,(unsigned long long)hash);return 0;}
    puts("Original paired glow: 240 MIPS-reference cases for both emitter ticks, sprite growth/fade and reset, native context and guards passed");return 1;
}

/* Isolate the authored command flow using the original no-op initializer
 * for each type, as in the USA interpreter corpus. No update/draw is run. */
static int original_paired_glow_program_flow(void)
{
    FieldFxState *old_state=g_FieldFxScriptState;FieldFxObject *old_object=g_FieldFxScriptObject;
    FieldFxSlot *old_slots=g_FieldFxSlotTable;char *old_data=g_FieldFxScriptData;int *old_commands=g_FieldFxCommandTable;
    FieldActor actor={0};FieldActorState core={0};actor.state=&core;core.control10.command_value=1;
    FieldFxObject object={0};object.prefix.actor=&actor;
    FieldFxHandler init[6];for(unsigned i=0;i<5;++i)init[i]=g_RoomInitList__paired[2];init[5]=(FieldFxHandler)(intptr_t)-1;
    g_RoomPairedGlowHandlers.initialize((PmSlotHeader *)&object);
    uint64_t hash=UINT64_C(14695981039346656037);u32 seen=0;
    for(unsigned frame=0;frame<80;++frame){
        FieldFx_RunScript(&object,init,(u16 *)g_RoomSpawnLayout__paired);
        for(unsigned i=0;i<64;++i)if(object.slots[i].flag)seen|=1u<<object.slots[i].handler_id;
        const u32 words[]={(u16)object.state.pc,(u16)object.state.wait,(u32)object.state.regs[0],
            (u8)object.state.slot_count,(u8)object.state.halted,seen};
        for(unsigned i=0;i<6;++i)for(unsigned byte=0;byte<4;++byte)hash=(hash^((words[i]>>(byte*8))&255))*UINT64_C(1099511628211);
    }
    g_FieldFxScriptState=old_state;g_FieldFxScriptObject=old_object;g_FieldFxSlotTable=old_slots;g_FieldFxScriptData=old_data;g_FieldFxCommandTable=old_commands;
    if(seen!=0x1b || hash!=UINT64_C(0xebce7b7762f7a29a)){fprintf(stderr,"Paired glow flow %x %llx\n",seen,(unsigned long long)hash);return 0;}
    puts("Original paired glow program: 80 USA interpreter frames, PC/wait/registers/count/halt and actual type mask 0x1b passed");return 1;
}

static int original_line_burst_states(void)
{
    FieldFxTaskContext *old_context=g_FieldFxTaskContext;FieldFxTaskSlot *old_task=g_FieldFxActiveTaskSlot;
    FieldFxObjectPrefix *old_owner=g_FieldFxOwner;FieldActor *old_player=g_PlayerEntity;
    GteMatrix *old_view=g_GeomScroll.words.viewMatrix;int old_age=g_FieldFxCurrentAge,old_floor=g_RoomFloorY->y;u32 old_random=Psyz_RandState();
    GteMatrix view={.m={{4096,0,0},{0,4096,0},{0,0,4096}}};g_GeomScroll.words.viewMatrix=&view;
    FieldFxTaskContext context={0};FieldFxObjectPrefix owner={0};FieldActor actor={0},player={0};FieldActorState state={0};Combatant player_state={0};
    owner.actor=&actor;actor.state=&state;player.state=(void *)&player_state;player.render_object.hit_cylinder.radius=63;
    g_FieldFxOwner=&owner;g_PlayerEntity=&player;g_FieldFxTaskContext=&context;g_RoomFloorY->y=0;
    FieldFxTaskCallback callbacks[]={RoomEffect_LineBurstParticle,RoomEffect_LineBurstController};
    const unsigned sizes[]={20,36};const u32 seeds[]={0,1,0x12345678,0xffffffff};
    const int timers[]={0,1,2,3,5,6,7,15,23,24,31,32,32766,32767};
    PolygonVertex *old_polygon=g_FloorPolygonVertices;u16 old_count=g_FloorPolygonVertexCount;
    PolygonVertex polygon[]={{0,-2000,0,-2000},{0,2000,0,-2000},{0,2000,0,2000},{0,-2000,0,2000}};
    g_FloorPolygonVertices=polygon;g_FloorPolygonVertexCount=4;
    RenderMatrix matrices[20];for(unsigned i=0;i<20;++i)matrices[i]=(RenderMatrix){.rotation={{4096,0,0},{0,4096,0},{0,0,4096}}};
    actor.render_object.matrices=matrices;
    uint64_t hash=UINT64_C(14695981039346656037);
    for(unsigned callback=0;callback<2;++callback)for(unsigned seed=0;seed<4;++seed)for(unsigned variant=0;variant<28;++variant){
        _Alignas(void *) u8 data[44],pool[2048];FieldFxTaskSlot task={.end=(char *)pool};RoomLineBurstParams params={.duration=48,.countdown=8};
        int age=timers[variant%14];
        memset(data,0xa5,sizeof(data));for(unsigned j=0;j<sizes[callback];++j)data[j]=j*37+variant*3;
        s16 point[]={0,-100,0,0};memcpy(data,point,8);
        if(callback==0){s16 words[]={3,-7,5,2,variant%3,age};memcpy(data+8,words,12);}
        else {s16 position[]={variant<14?0:3000,0,0,0},words[]={variant%4,age,64,8192,128,variant%3};memcpy(data+16,position,8);memcpy(data+24,words,12);}
        context.flags=variant&1;state.core_flags=0x01234567;player_state.stateFlags=0x10203040;
        g_FieldFxCurrentAge=age;g_FieldFxActiveTaskSlot=&task;srand(seeds[seed]);memset(pool,0x5a,sizeof(pool));
        if(FieldFx_InitCallbackList((char *)pool,20,24,RoomEffect_LineBurstParticle)<0)return 0;
        u32 result=callbacks[callback](1,data,(intptr_t)&params),random=Psyz_RandState();
        const void *parts[]={&result,data,&random,&state.core_flags,&player_state.stateFlags};unsigned lengths[]={4,sizes[callback],4,4,4};
        for(unsigned part=0;part<5;++part)for(unsigned j=0;j<lengths[part];++j)hash=(hash^((const u8 *)parts[part])[j])*UINT64_C(1099511628211);
        FieldFxCallbackList *list=(void *)pool;
        for(unsigned slot=0;slot<24;++slot){FieldFxCallbackListEntry *entry=(void *)(list->entries+slot*list->stride);
            for(unsigned j=0;j<4;++j)hash=(hash^((u8 *)entry)[j])*UINT64_C(1099511628211);
            for(unsigned j=0;j<20;++j)hash=(hash^entry->state[j])*UINT64_C(1099511628211);
        }
        for(unsigned j=sizes[callback];j<sizeof(data);++j)if(data[j]!=0xa5)return 0;
    }
    if(hash!=UINT64_C(0xed4f850d3f8ab98e)){fprintf(stderr,"Line burst CPU hash %016llx\n",(unsigned long long)hash);return 0;}
    RoomSlotRec saved_slots[8];memcpy(saved_slots,RoomLib_SlotTable,sizeof(saved_slots));
    for(unsigned i=0;i<8;++i){
        intptr_t address=RoomLib_SlotSet(0,i,-123,456);RoomSlotRec *slot=(void *)address;
        if(slot!=&RoomLib_SlotTable[i] || slot->h0!=-123 || slot->h2 || slot->h4!=456)return 0;
        if(RoomLib_SlotSet(1,i,48,8)!=address || slot->w8!=48 || slot->wC!=8)return 0;
    }
    memcpy(RoomLib_SlotTable,saved_slots,sizeof(saved_slots));
    g_FloorPolygonVertices=old_polygon;g_FloorPolygonVertexCount=old_count;
    g_FieldFxTaskContext=old_context;g_FieldFxActiveTaskSlot=old_task;g_FieldFxOwner=old_owner;g_PlayerEntity=old_player;
    g_GeomScroll.words.viewMatrix=old_view;g_FieldFxCurrentAge=old_age;g_RoomFloorY->y=old_floor;srand(old_random);
    puts("Original line burst: 224 unhooked USA CPU/ROM BIOS/GTE update states, all phases, particle slots, RNG, contact flags, guards and all eight native parameter slots passed");return 1;
}

static int original_dropped_flare_states(void)
{
    FieldFxTaskContext *old_context=g_FieldFxTaskContext;FieldFxTaskSlot *old_task=g_FieldFxActiveTaskSlot;
    FieldFxObjectPrefix *old_owner=g_FieldFxOwner;FieldActor *old_player=g_PlayerEntity;
    GteMatrix *old_view=g_GeomScroll.words.viewMatrix;int old_age=g_FieldFxCurrentAge,old_floor=g_RoomFloorY->y;u32 old_random=Psyz_RandState();
    GteMatrix view={.m={{4096,0,0},{0,4096,0},{0,0,4096}}};g_GeomScroll.words.viewMatrix=&view;
    FieldFxTaskContext context={0};FieldFxObjectPrefix owner={0};FieldActor actor={0},player={0};FieldActorState state={0};Combatant player_state={0};
    owner.actor=&actor;actor.state=&state;player.state=(void *)&player_state;player.render_object.hit_cylinder.radius=63;
    g_FieldFxOwner=&owner;g_PlayerEntity=&player;g_FieldFxTaskContext=&context;g_RoomFloorY->y=0;
    FieldFxTaskCallback callbacks[]={RoomEffect_DroppedFlareParticle,RoomEffect_DroppedFlareEmitter};
    const unsigned sizes[]={20,16};const u32 seeds[]={0,1,0x12345678,0xffffffff};
    const int ages[]={0,1,2,3,7,8,15,16,23,24,31,64},timers[]={-1,0,1,7,8,15,16,23,24,31,32766,32767};
    uint64_t hash=UINT64_C(14695981039346656037);
    for(unsigned callback=0;callback<2;++callback)for(unsigned seed=0;seed<4;++seed)for(unsigned variant=0;variant<30;++variant){
        _Alignas(void *) u8 data[44],pool[2048];FieldFxTaskSlot task={.end=(char *)pool};int params[]={128,variant&2?4096:8192};
        int age=ages[variant%12];if(callback==1 && age==1)continue;
        memset(data,0xa5,sizeof(data));for(unsigned j=0;j<sizes[callback];++j)data[j]=j*37+variant*3;
        s16 point[]={variant<20?0:1234,variant&1?-100:0,0};memcpy(data,point,6);
        if(callback==0){s16 velocity[]={3,-7,5,0},words[]={variant%5,timers[variant%12]};memcpy(data+8,velocity,8);memcpy(data+16,words,4);}
        context.flags=variant&1;state.core_flags=0x01234567;player_state.stateFlags=0x10203040;
        g_FieldFxCurrentAge=age;g_FieldFxActiveTaskSlot=&task;srand(seeds[seed]);memset(pool,0x5a,sizeof(pool));
        if(FieldFx_InitCallbackList((char *)pool,20,24,RoomEffect_DroppedFlareParticle)<0)return 0;
        u32 result=callbacks[callback](1,data,(intptr_t)params),random=Psyz_RandState();
        const void *parts[]={&result,data,&random,&state.core_flags,&player_state.stateFlags};unsigned lengths[]={4,sizes[callback],4,4,4};
        for(unsigned part=0;part<5;++part)for(unsigned j=0;j<lengths[part];++j)hash=(hash^((const u8 *)parts[part])[j])*UINT64_C(1099511628211);
        FieldFxCallbackList *list=(void *)pool;
        for(unsigned slot=0;slot<24;++slot){FieldFxCallbackListEntry *entry=(void *)(list->entries+slot*list->stride);
            for(unsigned j=0;j<4;++j)hash=(hash^((u8 *)entry)[j])*UINT64_C(1099511628211);
            for(unsigned j=0;j<20;++j)hash=(hash^entry->state[j])*UINT64_C(1099511628211);
        }
        for(unsigned j=sizes[callback];j<sizeof(data);++j)if(data[j]!=0xa5)return 0;
    }
    if(hash!=UINT64_C(0x162712f297cfbbdc)){fprintf(stderr,"Dropped flare CPU hash %016llx\n",(unsigned long long)hash);return 0;}
    int old_params[2];memcpy(old_params,g_RoomDroppedFlareArgs,8);
    intptr_t wide=sizeof(intptr_t)>4?(intptr_t)UINT64_C(0x1234567887654321):0x7654321;
    if(RoomEffect_DroppedFlareSetArgs(7,wide,-8192,99)!=(intptr_t)g_RoomDroppedFlareArgs ||
       g_RoomDroppedFlareArgs[0]!=(s32)(u32)wide || g_RoomDroppedFlareArgs[1]!=-8192)return 0;
    memcpy(g_RoomDroppedFlareArgs,old_params,8);
    g_FieldFxTaskContext=old_context;g_FieldFxActiveTaskSlot=old_task;g_FieldFxOwner=old_owner;g_PlayerEntity=old_player;
    g_GeomScroll.words.viewMatrix=old_view;g_FieldFxCurrentAge=old_age;g_RoomFloorY->y=old_floor;srand(old_random);
    puts("Original dropped flare: 228 unhooked USA CPU/ROM BIOS/GTE update states, particle slots, RNG, contact flags, guards and native initializer (emitter sound path checked by PM) passed");return 1;
}

static int original_m089_states(void)
{
    FieldFxTaskContext *old_context=g_FieldFxTaskContext;FieldFxTaskSlot *old_task=g_FieldFxActiveTaskSlot;
    FieldFxObjectPrefix *old_owner=g_FieldFxOwner;FieldActor *old_player=g_PlayerEntity;
    GteMatrix *old_view=g_GeomScroll.words.viewMatrix;int old_age=g_FieldFxCurrentAge,old_floor=g_RoomFloorY->y;u32 old_random=Psyz_RandState();
    GteMatrix view={.m={{4096,0,0},{0,4096,0},{0,0,4096}}};g_GeomScroll.words.viewMatrix=&view;
    FieldFxTaskContext context={0};FieldFxObjectPrefix owner={0};FieldActor actor={0},player={0};FieldActorState state={0};Combatant player_state={0};
    owner.actor=&actor;actor.state=&state;player.state=(void *)&player_state;player.render_object.hit_cylinder.radius=63;
    g_FieldFxOwner=&owner;g_PlayerEntity=&player;g_FieldFxTaskContext=&context;g_RoomFloorY->y=0;
    FieldFxTaskCallback callbacks[]={RoomM089_SpiralTrailCallback,RoomM089_SpiralTrail,RoomM089_SpinModel,RoomM089_SwirlRiseSprite,RoomM089_ModelPulse};
    const unsigned sizes[]={56,8,4,16,12};const u32 seeds[]={0,1,0x12345678,0xffffffff};
    const int ages[]={0,1,2,3,4,7,8,9,24,31,40,64},timers[]={-1,0,1,7,8,15,16,23,24,31,32,32767};
    uint64_t hash=UINT64_C(14695981039346656037);
    for(unsigned callback=0;callback<5;++callback)for(unsigned seed=0;seed<4;++seed)for(unsigned variant=0;variant<12;++variant){
        _Alignas(void *) u8 data[64],pool[2048];FieldFxTaskSlot task={.end=(char *)pool};s16 params[]={123,-400,-77,0};
        memset(data,0xa5,sizeof(data));for(unsigned j=0;j<sizes[callback];++j)data[j]=j*37+variant*3;
        if(callback==0){s16 values[]={timers[variant],variant%2};memcpy(data+48,values,4);}
        if(callback==2){s16 values[]={timers[variant],variant%3};memcpy(data,values,4);}
        if(callback==3){s16 phase=variant%2;memcpy(data+8,&phase,2);}
        if(callback==4){s16 point[]={variant<8?0:1234,0,0},values[]={variant%3,variant};memcpy(data,point,6);memcpy(data+8,values,4);}
        context.flags=variant&1;state.core_flags=0x01234567;player_state.stateFlags=0x10203040;
        g_FieldFxCurrentAge=ages[variant];g_FieldFxActiveTaskSlot=&task;srand(seeds[seed]);memset(pool,0x5a,sizeof(pool));
        if(FieldFx_InitCallbackList((char *)pool,56,24,RoomM089_SpiralTrailCallback)<0)return 0;
        u32 result=callbacks[callback](1,data,(intptr_t)params),random=Psyz_RandState();
        const void *parts[]={&result,data,&random,&state.core_flags,&player_state.stateFlags};unsigned lengths[]={4,sizes[callback],4,4,4};
        for(unsigned part=0;part<5;++part)for(unsigned j=0;j<lengths[part];++j)hash=(hash^((const u8 *)parts[part])[j])*UINT64_C(1099511628211);
        FieldFxCallbackList *list=(void *)pool;
        for(unsigned slot=0;slot<24;++slot){FieldFxCallbackListEntry *entry=(void *)(list->entries+slot*list->stride);
            for(unsigned j=0;j<4;++j)hash=(hash^((u8 *)entry)[j])*UINT64_C(1099511628211);
            for(unsigned j=0;j<56;++j)hash=(hash^entry->state[j])*UINT64_C(1099511628211);
        }
        for(unsigned j=sizes[callback];j<sizeof(data);++j)if(data[j]!=0xa5)return 0;
    }
    if(hash!=UINT64_C(0x3345e804cde1fcfa)){fprintf(stderr,"M089 CPU hash %016llx\n",(unsigned long long)hash);return 0;}
    short old_anchor[3];memcpy(old_anchor,g_RoomM089Anchor,6);
    if(RoomM089_SetAnchor(1,65535,32768,65537)!=(intptr_t)g_RoomM089Anchor || memcmp(g_RoomM089Anchor,(s16[]){-1,-32768,1},6))return 0;
    if(RoomM089_SetAnchor(2,1,2,3)!=(intptr_t)g_RoomM089Anchor || memcmp(g_RoomM089Anchor,(s16[]){-1,-32768,1},6))return 0;
    memcpy(g_RoomM089Anchor,old_anchor,6);
    g_FieldFxTaskContext=old_context;g_FieldFxActiveTaskSlot=old_task;g_FieldFxOwner=old_owner;g_PlayerEntity=old_player;
    g_GeomScroll.words.viewMatrix=old_view;g_FieldFxCurrentAge=old_age;g_RoomFloorY->y=old_floor;srand(old_random);
    puts("Original M089: 240 unhooked USA CPU/ROM BIOS/GTE update states, all particle slots, RNG, contact flags, guards and native parameter initializers passed");return 1;
}

static int original_three_particle_program(void);

static int original_three_particle_states(void)
{
    FieldFxState *old_script=g_FieldFxScriptState;FieldFxState script={0};
    FieldActor *old_player=g_PlayerEntity;FieldActor player={0};FieldFxObjectPrefix owner={0};
    PolygonVertex *old_polygon=g_FloorPolygonVertices;u16 old_count=g_FloorPolygonVertexCount;
    PolygonVertex polygon[4]={{.x=-300,.z=-300},{.x=300,.z=-300},{.x=300,.z=300},{.x=-300,.z=300}};
    RoomDebrisClock clock={0};script.current_context=&clock;g_FieldFxScriptState=&script;g_PlayerEntity=&player;
    const u32 seeds[]={0,1,0x12345678,0xffffffff};u32 old_rng=Psyz_RandState();
    const s16 matrices[3][9]={{4096,0,0,0,4096,0,0,0,4096},{0,0,4096,0,4096,0,-4096,0,0},{2048,1024,0,-1024,3072,0,0,0,4096}};
    const s32 poses[4][3]={{0,0,0},{123,-99,1500},{-32768,32767,-1},{32767,-32768,32768}};
    const s16 angles[]={0,1,1024,-1024,32767,-32768};
    uint64_t hash=UINT64_C(14695981039346656037);unsigned cases=0;
#define THREE_HASH(ptr,length) do {const u8 *bytes=(const u8 *)(ptr);for(unsigned byte=0;byte<(length);++byte)hash=(hash^bytes[byte])*UINT64_C(1099511628211);}while(0)
    for(unsigned matrix=0;matrix<3;++matrix)for(unsigned pose=0;pose<4;++pose)for(unsigned angle=0;angle<6;++angle)for(unsigned seed=0;seed<4;++seed){
        struct {u32 before;RoomDebrisTriplet data;u32 after;} record;memset(&record,0xa5,sizeof(record));
        memset(&clock.view,0,sizeof(clock.view));memcpy(clock.view.matrix.m,matrices[matrix],18);memcpy(clock.view.matrix.t,poses[pose],12);
        intptr_t vars[]={200,-1,40,angles[angle],pose&1,1000,8,0,0};memcpy(script.vars,vars,sizeof(vars));script.regs[1]=matrix&1;
        srand(seeds[seed]);RoomLib_InitThreeTransformParticles(&owner,NULL,&record.data);u32 random=Psyz_RandState();
        THREE_HASH(&record.data,sizeof(record.data));THREE_HASH(&random,4);++cases;
        if(record.before!=0xa5a5a5a5 || record.after!=0xa5a5a5a5)return 0;
    }
    if(hash!=UINT64_C(0x234309fa30aaa62d)){fprintf(stderr,"Three particle init %u %016llx\n",cases,(unsigned long long)hash);return 0;}
    const s32 xs[]={0,1,-1,0x7fff0000,INT32_MIN,INT32_MAX};
    const s16 speeds[]={0,1,300,-1,32767,-32768},limits[]={-32768,0,1000,32767},accelerations[]={-32768,-1,1,32767};
    const s16 update_angles[]={-32768,-1,0,1,1023,32767},remaining[]={0,1,3,-1};
    const u16 life[]={0,1,19,20,21,65535},swings[]={0,1,1000,32767,32768,65535},phases[]={0,32767,65535};
    const s8 countdown[]={-128,-1,0,1,2,127};const u8 sizes[]={0,6,7,124,128,252,255};const s16 velocities[]={0,1,-1,32767,-32768};
    g_FloorPolygonVertices=polygon;g_FloorPolygonVertexCount=4;player.render_object.hit_cylinder.radius=63;
    hash=UINT64_C(14695981039346656037);cases=0;
    for(unsigned variant=0;variant<16;++variant)for(unsigned angle=0;angle<6;++angle)for(unsigned seed=0;seed<4;++seed){
        struct {u32 before;RoomDebrisTriplet data;u32 after;} record;memset(&record,0xa5,sizeof(record));RoomDebrisTriplet *data=&record.data;
        data->remaining=remaining[variant%4];data->speed=speeds[variant%6];data->acceleration=accelerations[variant%4];data->swingLimit=limits[variant%4];data->keepAlive=variant&1;
        for(unsigned i=0;i<3;++i){
            data->mode[i]=(variant+i)%3;data->landed[i]=(variant>>i)&1;data->active[i]=(variant+i)%3-1;data->countdown[i]=countdown[(variant+i)%6];data->size[i]=sizes[(variant+i)%7];
            data->life[i]=life[(variant+i)%6];data->swing[i]=swings[(variant+i)%6];data->phase[i]=phases[(variant+i)%3];
            for(unsigned axis=0;axis<3;++axis){((s32 *)&data->position[i])[axis]=xs[(variant+i+axis)%6];((s16 *)&data->velocity[i])[axis]=velocities[(variant+i+axis)%5];}
        }
        RoomFxControl control={37,67,update_angles[angle]};memset(&clock,0,sizeof(clock));clock.landedFlag=variant&1;
        player.pos_x=(variant&1?32767:0)*65536;player.pos_z=(variant&1?-32768:0)*65536;
        srand(seeds[seed]);RoomLib_UpdateDebrisTriplet(&owner,&control,data);u32 random=Psyz_RandState();
        THREE_HASH(data,sizeof(*data));THREE_HASH(&control,4);THREE_HASH(&clock.landedFlag,2);THREE_HASH(&random,4);++cases;
        if(record.before!=0xa5a5a5a5 || record.after!=0xa5a5a5a5)return 0;
    }
    if(hash!=UINT64_C(0xff1932b53e5636f9)){fprintf(stderr,"Three particle update %u %016llx\n",cases,(unsigned long long)hash);return 0;}
    const s16 cooldowns[]={-32768,-1,0,1,2,32767},reloads[]={-32768,0,8,32767};
    FieldActor contact_actor={0};FieldActorState contact_state={0};Combatant player_core={0};u8 action=0;
    contact_actor.state=&contact_state;contact_state.action_state=&action;owner.actor=&contact_actor;player.state=(FieldActorState *)&player_core;
    hash=UINT64_C(14695981039346656037);cases=0;
    for(unsigned cool=0;cool<6;++cool)for(unsigned reload=0;reload<4;++reload)for(unsigned landed=0;landed<3;++landed)for(unsigned halt=0;halt<3;++halt)for(unsigned mode=0;mode<3;++mode){
        memset(&clock,0xa5,sizeof(clock));clock.reload=reloads[reload];clock.cooldown=cooldowns[cool];clock.landedFlag=landed;
        script.slot_count=halt;script.halted=halt?1:0;contact_state.core_flags=0x01234567;contact_state.control10.command_value=1;action=mode;player_core.stateFlags=0x10203040;
        RoomFxControl control={37,0xa5,0x1234};RoomLib_UpdateThreeParticleTimer(&owner,&control,&clock);
        THREE_HASH(&clock.reload,6);THREE_HASH(&control,4);THREE_HASH(&contact_state.core_flags,4);THREE_HASH(&player_core.stateFlags,4);++cases;
    }
#undef THREE_HASH
    g_FieldFxScriptState=old_script;g_PlayerEntity=old_player;g_FloorPolygonVertices=old_polygon;g_FloorPolygonVertexCount=old_count;srand(old_rng);
    if(hash!=UINT64_C(0xdcf26f313369619d)){fprintf(stderr,"Three particle timer %u %016llx\n",cases,(unsigned long long)hash);return 0;}
    puts("Original three particles: 288 USA CPU/GTE initializers independent of stack fill and 384 updates plus 648 contact timer states from unhooked CPU/BIOS, full 180-byte states, RNG, collision and guards passed");
    return 1;
}

static int original_drop_field_states(void)
{
    const u8 fills[]={0,1,0xa5,0xff};const unsigned initializers[]={2,3,6,12},updates[]={2,3,4,6,9,10};
    const s16 ages[]={-32768,-1,0,1,7,8,20,21,60,61,120,121,32767},ys[]={0,1,-1,-32768,32767};
    const u32 steps[]={0,1,0xffffffff,0x80000000,0x7fffffff};
    uint64_t hash=UINT64_C(14695981039346656037);unsigned cases=0;
    _Alignas(void *) u8 data[512];u8 control[4];
#define DROP_HASH(ptr,length) do {const u8 *bytes=(const u8 *)(ptr);for(unsigned b=0;b<(length);++b)hash=(hash^bytes[b])*UINT64_C(1099511628211);}while(0)
    for(unsigned id=0;id<4;++id)for(unsigned fill=0;fill<4;++fill){
        memset(data,fills[fill],sizeof(data));g_RoomDropFieldSpawnData[initializers[id]](NULL,control,data);
        DROP_HASH(data,sizeof(data));++cases;
    }
    for(unsigned id=0;id<6;++id)for(unsigned fill=0;fill<4;++fill)for(unsigned age=0;age<13;++age){
        memset(data,fills[fill],sizeof(data));memset(control,0xa5,sizeof(control));memcpy(control+2,ages+age,2);
        g_RoomDropFieldUpdateList[updates[id]](NULL,control,data);
        DROP_HASH(data,sizeof(data));DROP_HASH(control,sizeof(control));++cases;
    }
    FieldFxState *old_state=g_FieldFxScriptState;FieldFxState script={0};RoomDropFieldState context={0};
    g_FieldFxScriptState=&script;script.current_context=&context;
    for(unsigned step=0;step<5;++step)for(unsigned y=0;y<5;++y)for(unsigned fill=0;fill<4;++fill){
        context.initialFallStep=(s32)steps[step];memset(data,fills[fill],sizeof(data));memcpy(data+2,ys+y,2);
        g_RoomDropFieldSpawnData[5](NULL,control,data);DROP_HASH(data,sizeof(data));++cases;
    }
    g_FieldFxScriptState=old_state;
#undef DROP_HASH
    if(cases!=428 || hash!=UINT64_C(0xb3bad0b238c533aa)){fprintf(stderr,"Drop field CPU hash %u %016llx\n",cases,(unsigned long long)hash);return 0;}
    puts("Original DropField: 428 unhooked USA CPU initializer/update states, signed positions, word/halfword wrap, timers, phases and whole-buffer guards passed");
    return 1;
}

static int original_spark_barrage_cycles(void)
{
    PmTables old_tables=g_PmTables;Pe1GameState old_game=g_GameState;
    u32 old_flags=g_GameStateFlags,old_rng=Psyz_RandState();RoomFloorY old_floor=*g_RoomFloorY;
    RenderBufferPrefix old_buffers=D_800B0E38;int old_slot=g_ActiveDrawSlot,old_offset=g_DrawPacketOffset;
    GteMatrix *old_view=g_GeomScroll.words.viewMatrix,view={.m={{4096,0,0},{0,4096,0},{0,0,4096}}};
    s32 *old_projection=g_GeomScroll.words.projectionDistance,projection=256;
    FieldActor *old_player=g_PlayerEntity;
    FieldFxObjectPrefix *old_owner=g_FieldFxOwner;FieldFxTaskContext *old_context=g_FieldFxTaskContext;
    FieldFxTaskSlot *old_task=g_FieldFxActiveTaskSlot;
    static RenderGpuTag ordering[4096];
    struct {_Alignas(16) u8 bytes[65536];u8 guard[32];} packets;
    const unsigned commands[]={124,125,127};unsigned cycles=0;
    for(unsigned kind=0;kind<3;++kind)for(unsigned draw=0;draw<2;++draw){
        struct {PmPrimarySlot slots[11];u8 guard[32];} bank={0};
        RenderMatrix matrices[24]={0};
        for(unsigned i=0;i<24;++i){for(unsigned j=0;j<3;++j)matrices[i].rotation[j][j]=4096;
            matrices[i].translation[0]=i*20-200;matrices[i].translation[1]=-100;
            matrices[i].translation[2]=1200;}
        FieldActorState core={.control10.command_value=1};u8 action=1;
        FieldActor actor={.state=&core,.mode=2};core.action_state=&action;actor.render_object.matrices=matrices;
        Combatant battle={0};FieldActor player={.state=(FieldActorState *)&battle};g_PlayerEntity=&player;
        player.pos_x=0;player.pos_y=0;player.pos_z=1200*65536;
        player.render_object.matrices=matrices;
        g_PmTables=(PmTables){g_PmCmdHandlerArray,bank.slots,NULL};
        g_GameStateFlags=0x80;g_GameState.flags&=~8u;g_GameState.tim_load_state=0;g_GameState.cd_range_read_mode=0;
        g_GeomScroll.words.viewMatrix=&view;g_GeomScroll.words.projectionDistance=&projection;g_RoomFloorY->raw=0;
        Psyz_GteCtrlWrite(26,256);Psyz_GteCtrlWrite(29,1365);Psyz_GteCtrlWrite(30,1024);gte_SetGeomOffset(160,120);
        FieldFx_InitTexturePageTable();srand(kind+1);memset(bank.guard,0xa5,sizeof(bank.guard));
        if(Scene_LoadRoomAssets(commands[kind],&actor) || Pm_SendCmd(0,0,0,17,0,0))return 0;
        unsigned seen=0,drawn=0,frame=0;FieldFxTaskContext *context=&bank.slots[0].task.tasks;
        for(;frame<600;++frame){
            for(unsigned i=0;i<4096;++i)ordering[i]=(RenderGpuTag){.address=0xffffff};
            memset(packets.guard,0xa5,sizeof(packets.guard));g_ActiveDrawSlot=draw;g_DrawPacketOffset=0;
            D_800B0E38.ordering[draw]=ordering;D_800B0E38.packets[draw]=(char *)packets.bytes;
            if(Pm_Start(0) || Pm_Exec(0))return 0;
            for(unsigned i=0;i<8;++i)if(context->slots[i].id!=0xffff)seen|=1u<<context->slots[i].id;
            if(g_DrawPacketOffset)++drawn;
            if(g_DrawPacketOffset<0 || g_DrawPacketOffset>sizeof(packets.bytes))return 0;
            for(unsigned i=0;i<4096;++i){uintptr_t at=ordering[i].address;unsigned links=0;
                while(at && at!=0xffffff){
                    if(at<(uintptr_t)packets.bytes || at+sizeof(RenderGpuTag)>(uintptr_t)packets.bytes+g_DrawPacketOffset ||
                        at%_Alignof(RenderGpuTag) || ++links>512)return 0;
                    at=((RenderGpuTag *)at)->address;
                }
            }
            for(unsigned i=0;i<32;++i)if(bank.guard[i]!=0xa5 || packets.guard[i]!=0xa5)return 0;
            g_AkaoMessageQueueCount=0;
            if(!bank.slots[0].header.state || bank.slots[0].header.state==4)break;
        }
        if(frame==600 || seen!=(kind==2?1u:3u) || !drawn || context->count){
            fprintf(stderr,"M188 program%u draw%u frames%u seen%x drawn%u count%d\n",
                commands[kind],draw,frame,seen,drawn,context->count);return 0;
        }
        if(Pm_Stop(0,&actor,0))return 0;
        ++cycles;
    }
    g_PmTables=old_tables;g_GameState=old_game;g_GameStateFlags=old_flags;srand(old_rng);*g_RoomFloorY=old_floor;
    D_800B0E38=old_buffers;g_ActiveDrawSlot=old_slot;g_DrawPacketOffset=old_offset;
    g_GeomScroll.words.viewMatrix=old_view;g_GeomScroll.words.projectionDistance=old_projection;
    g_PlayerEntity=old_player;g_FieldFxOwner=old_owner;g_FieldFxTaskContext=old_context;g_FieldFxActiveTaskSlot=old_task;
    printf("Original spark barrage: %u authored task cycles, all five attacks, both draw buffers, natural expiry and pool/packet/OT guards passed\n",cycles);
    return 1;
}

static int original_twelve_element_cycles(const int *authored_script)
{
    PmTables old_tables=g_PmTables;Pe1GameState old_game=g_GameState;
    u32 old_flags=g_GameStateFlags,old_rng=Psyz_RandState();RoomFloorY old_floor=*g_RoomFloorY;
    RenderBufferPrefix old_buffers=D_800B0E38;int old_slot=g_ActiveDrawSlot,old_offset=g_DrawPacketOffset;
    GteMatrix *old_view=g_GeomScroll.words.viewMatrix,view={.m={{4096,0,0},{0,4096,0},{0,0,4096}}};
    s32 *old_projection=g_GeomScroll.words.projectionDistance,projection=320;
    PolygonVertex *old_polygon=g_FloorPolygonVertices;u16 old_polygon_count=g_FloorPolygonVertexCount;
    FieldActor *old_player=g_PlayerEntity;
    FieldFxState *old_state=g_FieldFxScriptState;FieldFxObject *old_object=g_FieldFxScriptObject;
    FieldFxSlot *old_slots=g_FieldFxSlotTable;char *old_data=g_FieldFxScriptData;int *old_commands=g_FieldFxCommandTable;
    static RenderGpuTag ordering[4096];
    struct {_Alignas(16) u8 bytes[65536];u8 guard[32];} packets;
    PolygonVertex polygon[4]={{.x=-2000,.z=-2000},{.x=2000,.z=-2000},{.x=2000,.z=2000},{.x=-2000,.z=2000}};
    for(unsigned ended=0;ended<2;++ended)for(unsigned draw=0;draw<2;++draw){
        struct {PmPrimarySlot slots[11];u8 guard[32];} bank={0};
        RenderMatrix matrix={.rotation={{4096,0,0},{0,4096,0},{0,0,4096}},.translation={0,-100,1200}};
        FieldActorState core={.control10.command_value=1};u8 action=2;
        FieldActor actor={.state=&core};core.action_state=&action;actor.render_object.matrices=&matrix;
        Combatant player_core={0};FieldActor player={.state=(FieldActorState *)&player_core};
        player.pos_x=8000*65536;player.pos_z=8000*65536;g_PlayerEntity=&player;
        g_PmTables=(PmTables){g_PmCmdHandlerArray,bank.slots,NULL};
        g_GameStateFlags=0x80;g_GameState.flags&=~8u;g_GameState.tim_load_state=0;g_GameState.cd_range_read_mode=0;
        g_GeomScroll.words.viewMatrix=&view;g_GeomScroll.words.projectionDistance=&projection;g_RoomFloorY->raw=0;
        g_FloorPolygonVertices=polygon;g_FloorPolygonVertexCount=4;
        Psyz_GteCtrlWrite(26,320);Psyz_GteCtrlWrite(29,1365);Psyz_GteCtrlWrite(30,1024);gte_SetGeomOffset(160,112);
        FieldFx_InitTexturePageTable();srand(draw+1);memset(bank.guard,0xa5,sizeof(bank.guard));
        if(Scene_LoadRoomAssets(11,&actor))return 0;
        FieldFxObject *object=&bank.slots[0].effect;
        if(memcmp(object->state.commands,authored_script,16))return 0;
        const intptr_t vars[]={10,60,100,20,0,64,4096};
        for(unsigned v=0;v<7;++v)if(Pm_SendCmd(0,0,v,vars[v],0,0))return 0;
        unsigned seen=0,active=0,drawn=0,frame=0;
        for(;frame<240;++frame){
            for(unsigned i=0;i<4096;++i)ordering[i]=(RenderGpuTag){.address=0xffffff};
            memset(packets.guard,0xa5,sizeof(packets.guard));g_ActiveDrawSlot=draw;g_DrawPacketOffset=0;
            D_800B0E38.ordering[draw]=ordering;D_800B0E38.packets[draw]=(char *)packets.bytes;
            if(ended && frame==10)core.action_state=NULL;
            if(Pm_Start(0) || Pm_Exec(0))return 0;
            for(unsigned i=0;i<64;++i)if(object->slots[i].flag==1){
                FieldFxSlot *slot=&object->slots[i];seen|=1u<<slot->handler_id;
                if(slot->handler_id==1){RoomLibTick12Rec *work=(void *)(object->data+slot->data_offset);
                    for(unsigned j=0;j<12;++j)if(work->state[j]==1)active|=1u<<j;}
            }
            if(g_DrawPacketOffset)++drawn;
            if(g_DrawPacketOffset<0 || g_DrawPacketOffset>sizeof(packets.bytes))return 0;
            for(unsigned i=0;i<4096;++i){uintptr_t at=ordering[i].address;unsigned links=0;
                while(at && at!=0xffffff){
                    if(at<(uintptr_t)packets.bytes || at+sizeof(RenderGpuTag)>(uintptr_t)packets.bytes+g_DrawPacketOffset ||
                        at%_Alignof(RenderGpuTag) || ++links>512)return 0;
                    at=((RenderGpuTag *)at)->address;
                }
            }
            for(unsigned i=0;i<32;++i)if(bank.guard[i]!=0xa5 || packets.guard[i]!=0xa5)return 0;
            g_AkaoMessageQueueCount=0;
            if(!bank.slots[0].header.state)break;
        }
        if(frame==240 || seen!=3 || active!=4095 || !drawn || object->state.slot_count || bank.slots[0].header.owner){
            fprintf(stderr,"Twelve actual assets draw%u frame%u seen%x active%x drawn%u slots%d\n",draw,frame,seen,active,drawn,object->state.slot_count);return 0;
        }
    }
    g_PmTables=old_tables;g_GameState=old_game;g_GameStateFlags=old_flags;srand(old_rng);*g_RoomFloorY=old_floor;
    D_800B0E38=old_buffers;g_ActiveDrawSlot=old_slot;g_DrawPacketOffset=old_offset;
    g_GeomScroll.words.viewMatrix=old_view;g_GeomScroll.words.projectionDistance=old_projection;
    g_FloorPolygonVertices=old_polygon;g_FloorPolygonVertexCount=old_polygon_count;g_PlayerEntity=old_player;
    g_FieldFxScriptState=old_state;g_FieldFxScriptObject=old_object;g_FieldFxSlotTable=old_slots;
    g_FieldFxScriptData=old_data;g_FieldFxCommandTable=old_commands;
    puts("Original twelve-element actual assets: 4 complete PM cycles, live and completed animation events, both buffers, all twelve elements, natural expiry and pool/packet/OT guards passed");
    return 1;
}

static int original_orbit_set_cycles(void)
{
    PmTables old_tables=g_PmTables;Pe1GameState old_game=g_GameState;
    u32 old_flags=g_GameStateFlags,old_rng=Psyz_RandState();RoomFloorY old_floor=*g_RoomFloorY;
    RenderBufferPrefix old_buffers=D_800B0E38;int old_slot=g_ActiveDrawSlot,old_offset=g_DrawPacketOffset;
    GteMatrix *old_view=g_GeomScroll.words.viewMatrix,view={.m={{4096,0,0},{0,4096,0},{0,0,4096}}};
    s32 *old_projection=g_GeomScroll.words.projectionDistance,projection=256;
    FieldFxState *old_state=g_FieldFxScriptState;FieldFxObject *old_object=g_FieldFxScriptObject;
    FieldFxSlot *old_slots=g_FieldFxSlotTable;char *old_data=g_FieldFxScriptData;int *old_commands=g_FieldFxCommandTable;
    static RenderGpuTag ordering[4096];
    struct {_Alignas(16) u8 bytes[65536];u8 guard[32];} packets;
    unsigned cycles=0;
    for(unsigned ended=0;ended<2;++ended)for(unsigned mode=0;mode<2;++mode)for(unsigned draw=0;draw<2;++draw){
        struct {PmPrimarySlot slots[11];u8 guard[32];} bank={0};
        RenderMatrix matrix={0};for(unsigned j=0;j<3;++j)matrix.rotation[j][j]=4096;
        matrix.translation[0]=mode?123:-123;matrix.translation[1]=-100;matrix.translation[2]=1200;
        FieldActorState core={.control10.command_value=1};u8 action=1;
        FieldActor owner={.state=&core};core.action_state=&action;owner.render_object.matrices=&matrix;
        g_PmTables=(PmTables){g_PmCmdHandlerArray,bank.slots,NULL};
        g_GameStateFlags=0x80;g_GameState.flags&=~8u;g_GameState.tim_load_state=0;g_GameState.cd_range_read_mode=0;
        g_GeomScroll.words.viewMatrix=&view;g_GeomScroll.words.projectionDistance=&projection;g_RoomFloorY->y=0;
        Psyz_GteCtrlWrite(26,256);Psyz_GteCtrlWrite(29,1365);Psyz_GteCtrlWrite(30,1024);gte_SetGeomOffset(160,120);
        FieldFx_InitTexturePageTable();srand(mode+1);memset(bank.guard,0xa5,sizeof(bank.guard));
        if(Scene_LoadRoomAssets(36,&owner) || Pm_SendCmd(0,0,0,mode,0,0))return 0;
        unsigned seen=0,drawn=0,frame=0;FieldFxObject *object=&bank.slots[0].effect;
        for(;frame<600;++frame){
            for(unsigned i=0;i<4096;++i)ordering[i]=(RenderGpuTag){.address=0xffffff};
            memset(packets.guard,0xa5,sizeof(packets.guard));g_ActiveDrawSlot=draw;g_DrawPacketOffset=0;
            D_800B0E38.ordering[draw]=ordering;D_800B0E38.packets[draw]=(char *)packets.bytes;
            if(ended && frame==10)core.action_state=NULL;
            if(Pm_Start(0) || Pm_Exec(0))return 0;
            for(unsigned i=0;i<64;++i)if(object->slots[i].flag==1)seen|=1u<<object->slots[i].handler_id;
            if(g_DrawPacketOffset)++drawn;
            if(g_DrawPacketOffset<0 || g_DrawPacketOffset>sizeof(packets.bytes))return 0;
            for(unsigned i=0;i<4096;++i){uintptr_t at=ordering[i].address;unsigned links=0;
                while(at && at!=0xffffff){
                    if(at<(uintptr_t)packets.bytes || at+sizeof(RenderGpuTag)>(uintptr_t)packets.bytes+g_DrawPacketOffset ||
                        at%_Alignof(RenderGpuTag) || ++links>512)return 0;
                    at=((RenderGpuTag *)at)->address;
                }
            }
            for(unsigned i=0;i<32;++i)if(bank.guard[i]!=0xa5 || packets.guard[i]!=0xa5)return 0;
            /* Observe original sound staging; this graphics fixture has no audio timer. */
            g_AkaoMessageQueueCount=0;
            if(!bank.slots[0].header.state)break;
        }
        if(frame==600 || seen!=(mode?15u:3u) || !drawn || object->state.slot_count || !object->state.halted || bank.slots[0].header.owner){
            fprintf(stderr,"M187 orbit mode%u draw%u frames%u seen%x drawn%u slots%d halted%d\n",
                mode,draw,frame,seen,drawn,object->state.slot_count,object->state.halted);return 0;
        }
        ++cycles;
    }
    g_PmTables=old_tables;g_GameState=old_game;g_GameStateFlags=old_flags;srand(old_rng);*g_RoomFloorY=old_floor;
    D_800B0E38=old_buffers;g_ActiveDrawSlot=old_slot;g_DrawPacketOffset=old_offset;
    g_GeomScroll.words.viewMatrix=old_view;g_GeomScroll.words.projectionDistance=old_projection;
    g_FieldFxScriptState=old_state;g_FieldFxScriptObject=old_object;g_FieldFxSlotTable=old_slots;
    g_FieldFxScriptData=old_data;g_FieldFxCommandTable=old_commands;
    printf("Original M187 orbit: %u complete authored cycles, live and completed animation events, both sprite sides and draw buffers, all four effect types, natural expiry and pool/packet/OT guards passed\n",cycles);
    return 1;
}

static int original_room_glow_orb(void)
{
    const u32 seeds[]={0,1,2,3,0x12345678,0xffffffff,0x80000000,0x7fffffff};
    const u8 fills[]={0,1,0xa5,0xff},flags[]={0,1,2,255};
    const s8 timers[]={-128,-1,0,1,19,20,21,127};const s16 depths[]={-32768,-1,0,8,9,128,32767,0x1234};
    uint64_t hash=UINT64_C(14695981039346656037);unsigned cases=0;
#define GLOW_HASH(ptr,len) do {const u8 *bytes=(const u8 *)(ptr);for(size_t j=0;j<(len);++j)hash=(hash^bytes[j])*UINT64_C(1099511628211);++cases;}while(0)
    FieldActor *old_player=g_PlayerEntity,*old_owner=g_RoomGlowOrbOwner;
    GteMatrix old_axes=g_RoomGlowOrbAxes,old_origin=g_RoomGlowOrbOrigin;
    RoomFxSpritePacket old_sprite=g_RoomGlowOrbSprite,old_flash=g_RoomGlowOrbFlashSprite;
    FieldFxState *old_state=g_FieldFxScriptState;FieldFxObject *old_object=g_FieldFxScriptObject;
    FieldFxSlot *old_slots=g_FieldFxSlotTable;char *old_data=g_FieldFxScriptData;int *old_commands=g_FieldFxCommandTable;
    FieldActor actor={0},other={0};FieldFxObject object={0};RenderMatrix matrices[50];object.prefix.actor=&actor;actor.render_object.matrices=matrices;
    for(unsigned player=0;player<2;++player)for(unsigned seed=0;seed<8;++seed){
        u32 words[400];for(unsigned i=0;i<400;++i)words[i]=seeds[seed]+i*0x1020304u;memcpy(matrices,words,sizeof(matrices));
        g_PlayerEntity=player?&actor:&other;RoomEffect_GlowOrbCaptureOwner(&object,NULL,NULL);
        u8 data[68];u32 owner=g_RoomGlowOrbOwner==&actor;memcpy(data,&owner,4);memcpy(data+4,&g_RoomGlowOrbAxes,32);memcpy(data+36,&g_RoomGlowOrbOrigin,32);GLOW_HASH(data,sizeof(data));
    }
    for(unsigned f=0;f<4;++f){
        memset(&object,fills[f],sizeof(object));object.prefix.actor=NULL;
        memset(&g_RoomGlowOrbSprite,fills[f],sizeof(g_RoomGlowOrbSprite));memset(&g_RoomGlowOrbFlashSprite,fills[f],sizeof(g_RoomGlowOrbFlashSprite));
        RoomEffect_GlowOrbSetup((PmSlotHeader *)&object);u8 data[36];u32 script=object.state.commands==g_RoomGlowOrbScript;
        memcpy(data,&script,4);memcpy(data+4,&g_RoomGlowOrbSprite,12);memcpy(data+16,&g_RoomGlowOrbFlashSprite,12);memcpy(data+28,&object,8);GLOW_HASH(data,sizeof(data));
        FieldFxState state=object.state;state.commands=NULL;FieldFxState zero={0};
        if(memcmp(&state,&zero,sizeof(state)))return 0;
        for(unsigned i=0;i<sizeof(object.slots);++i)if(((u8 *)object.slots)[i])return 0;
        for(unsigned i=0;i<sizeof(object.data);++i)if(((u8 *)object.data)[i])return 0;
    }
    g_FieldFxScriptState=&object.state;
    for(unsigned i=0;i<8;++i){RoomGlowOrbSpawner data;memset(&data,0xa5,sizeof(data));object.state.regs[4]=seeds[i];RoomEffect_GlowOrbSpawnerInit(&object,NULL,&data);GLOW_HASH(&data,sizeof(data));}
    for(unsigned i=0;i<4;++i){u8 control[]={0xa5,flags[i]};RoomEffect_GlowOrbControllerUpdate(&object,control,NULL);GLOW_HASH(control,sizeof(control));}
    for(unsigned i=0;i<4;++i){RoomGlowOrbFlash data;memset(&data,fills[i],sizeof(data));RoomEffect_GlowOrbFlashInit(&object,NULL,&data);GLOW_HASH(&data,sizeof(data));}
    for(unsigned t=0;t<8;++t)for(unsigned d=0;d<8;++d)for(unsigned f=0;f<4;++f){
        RoomGlowOrbFlash flash;memset(&flash,0xa5,sizeof(flash));flash.timer=timers[t];flash.depth=depths[d];u8 control[]={0xa5,flags[f]};
        RoomEffect_GlowOrbFlashUpdate(&object,control,&flash);u8 data[16];memcpy(data,&flash,14);memcpy(data+14,control,2);GLOW_HASH(data,sizeof(data));
    }
#undef GLOW_HASH
    if(cases!=292 || hash!=UINT64_C(0x8fb5ea8e9d0d22fb)){
        fprintf(stderr,"Room glow CPU states %u %016llx\n",cases,(unsigned long long)hash);return 0;}
    /* Run the original four-type script through its real PM lifecycle. */
    PmTables old_tables=g_PmTables;PmCommand *old_class=g_PmCmdHandlerArray[8];
    Pe1GameState old_game=g_GameState;u32 old_flags=g_GameStateFlags;
    RenderBufferPrefix old_buffers=D_800B0E38;int old_slot=g_ActiveDrawSlot,old_offset=g_DrawPacketOffset;
    GteMatrix *old_view=g_GeomScroll.words.viewMatrix;GteMatrix view={.m={{4096,0,0},{0,4096,0},{0,0,4096}}};
    s32 *old_projection=g_GeomScroll.words.projectionDistance,projection=256;
    PolygonVertex *old_polygon=g_FloorPolygonVertices;u16 old_count=g_FloorPolygonVertexCount;RoomFloorY old_floor=*g_RoomFloorY;
    for(unsigned mode=0;mode<2;++mode){
    PolygonVertex polygon[4]={{.x=-2000,.z=-2000},{.x=2000,.z=-2000},{.x=2000,.z=2000},{.x=-2000,.z=2000}};
    struct {PmPrimarySlot slots[11];u8 guard[16];} bank={0};struct {_Alignas(16) u8 bytes[65536];u8 guard[16];} packets;
    static RenderGpuTag ordering[4096];FieldActorState state={.control10.command_value=1},player_state={0};u8 action=1;
    actor=(FieldActor){.state=&state};state.action_state=&action;actor.render_object.matrices=matrices;
    other=(FieldActor){.state=&player_state,.pos_x=10000*65536,.pos_z=10000*65536};g_PlayerEntity=&other;
    memset(matrices,0,sizeof(matrices));for(unsigned i=0;i<50;++i){for(unsigned j=0;j<3;++j)matrices[i].rotation[j][j]=4096;matrices[i].translation[2]=1200;}
    g_PmCmdHandlerArray[8]=&g_RoomGlowOrbHandlers;g_PmTables=(PmTables){g_PmCmdHandlerArray,bank.slots,NULL};
    g_GameStateFlags=0x80;g_GameState.flags&=~8u;g_GameState.tim_load_state=0;g_GameState.cd_range_read_mode=0;
    g_GeomScroll.words.viewMatrix=&view;g_GeomScroll.words.projectionDistance=&projection;
    g_FloorPolygonVertices=polygon;g_FloorPolygonVertexCount=4;g_RoomFloorY->y=0;
    Psyz_GteCtrlWrite(26,256);Psyz_GteCtrlWrite(29,1365);Psyz_GteCtrlWrite(30,1024);gte_SetGeomOffset(160,120);FieldFx_InitTexturePageTable();
    memset(bank.guard,0xa5,sizeof(bank.guard));g_RoomGlowOrbHandlers.init();
    if(Scene_LoadRoomAssets(8,&actor)!=0 || bank.slots[0].header.owner!=&actor)return 0;
    if(Pm_SendCmd(0,0,0,64,0,0) || Pm_SendCmd(0,0,1,mode,0,0) || Pm_SendCmd(0,0,2,0,0,0))return 0;
    if(Pm_SendCmd(0,0,8,(intptr_t)&actor,0,0) || bank.slots[0].effect.state.vars[8]!=(intptr_t)&actor)return 0;
    unsigned seen=0,drawn=0,linked=0;
    for(unsigned frame=0;frame<360;++frame){
        for(unsigned i=0;i<4096;++i)ordering[i]=(RenderGpuTag){.address=0xffffff};
        memset(packets.guard,0xa5,sizeof(packets.guard));g_ActiveDrawSlot=frame&1;g_DrawPacketOffset=0;
        D_800B0E38.ordering[g_ActiveDrawSlot]=ordering;D_800B0E38.packets[g_ActiveDrawSlot]=(char *)packets.bytes;
        if(frame==12){
            if(!FieldFx_StartSlot(&bank.slots[0].effect,1,(u16 *)g_RoomSpawnLayout,g_RoomInitList))return 0;
            seen|=2;
        }
        if(Pm_Start(0) || Pm_Exec(0))return 0;
        for(unsigned i=0;i<64;++i)if(bank.slots[0].effect.slots[i].flag==1)seen|=1u<<bank.slots[0].effect.slots[i].handler_id;
        if(g_DrawPacketOffset)++drawn;
        if(g_DrawPacketOffset>sizeof(packets.bytes))return 0;
        for(unsigned i=0;i<4096;++i){uintptr_t at=ordering[i].address;unsigned count=0;while(at && at!=0xffffff){
            if(at<(uintptr_t)packets.bytes || at+sizeof(RenderGpuTag)>(uintptr_t)packets.bytes+g_DrawPacketOffset || at%_Alignof(RenderGpuTag) || ++count>128)return 0;
            ++linked;at=((RenderGpuTag *)at)->address;
        }}
        for(unsigned i=0;i<16;++i)if(bank.guard[i]!=0xa5 || packets.guard[i]!=0xa5)return 0;
    }
    if(!drawn || !linked || (seen&14)!=14 || action!=4 || bank.slots[0].header.state || bank.slots[0].effect.state.slot_count){
        fprintf(stderr,"Room glow lifecycle seen=%x drawn=%u linked=%u action=%u state=%u count=%d\n",seen,drawn,linked,action,bank.slots[0].header.state,bank.slots[0].effect.state.slot_count);return 0;}
    RoomGlowOrb touching={.state=1,.size=4096,.depth=128,.z=1200,
        .matrix={.m={{4096,0,0},{0,0,4096},{0,-4096,0}}}};
    FieldFxObject hit_object={0};hit_object.prefix.actor=&actor;u8 hit_slot[2]={2,1};
    other.pos_x=other.pos_y=0;other.pos_z=1200*65536;player_state.flags=0;
    state.core_flags=0x12345678;state.flags=0x76543210;g_DrawPacketOffset=0;
    if(RoomEffect_GlowOrbDraw(&hit_object,hit_slot,&touching) || hit_slot[1]!=2 ||
       state.core_flags!=0x92345678 || state.flags!=0x76543210 || player_state.flags!=0x4000){fprintf(stderr,"Glow contact slot=%u core=%x flags=%x player=%x\n",hit_slot[1],state.core_flags,state.flags,player_state.flags);return 0;}
    }
    g_RoomGlowOrbHandlers.unload();
    g_PmTables=old_tables;g_PmCmdHandlerArray[8]=old_class;g_GameState=old_game;g_GameStateFlags=old_flags;
    D_800B0E38=old_buffers;g_ActiveDrawSlot=old_slot;g_DrawPacketOffset=old_offset;g_GeomScroll.words.viewMatrix=old_view;g_GeomScroll.words.projectionDistance=old_projection;
    g_FloorPolygonVertices=old_polygon;g_FloorPolygonVertexCount=old_count;*g_RoomFloorY=old_floor;
    g_PlayerEntity=old_player;g_RoomGlowOrbOwner=old_owner;g_RoomGlowOrbAxes=old_axes;g_RoomGlowOrbOrigin=old_origin;g_RoomGlowOrbSprite=old_sprite;g_RoomGlowOrbFlashSprite=old_flash;
    g_FieldFxScriptState=old_state;g_FieldFxScriptObject=old_object;g_FieldFxSlotTable=old_slots;g_FieldFxScriptData=old_data;g_FieldFxCommandTable=old_commands;
    puts("Original room glow orb: 292 unhooked USA CPU capture/setup/spawner/controller/flash states, full native model pointers and two scripts x 360 real PM frames with orb/flash draw, slot expiry and packet guards passed");
    return 1;
}

static int authored_glow_script_cycle(void)
{
    PmCommand *classes[]={&g_GlowEffectClass,&g_GlowEffectBClass,&g_GlowEffectCClass,&g_FourLayerGlowClass,&g_PointSpriteClass,&g_BurstEffectClass,&g_BurstEffectBClass,&g_BurstEffectCClass};
    unsigned commands[]={0,1,2,3,7,5,4,6};
    for(unsigned variant=0;variant<8;++variant) for(unsigned player_action=1;player_action<=7;++player_action) {
    struct { PmPrimarySlot primary[11]; u8 guard[16]; } buffer={0};
    RenderMatrix matrices[20]={0};
    FieldActor owner={0}; FieldActorState owner_state={0}; u8 owner_action=1;
    BattleEntity player={0}, *saved_player=D_8009D254;
    Combatant core={0}; BattleAction action={0};
    for(unsigned i=0;i<20;++i) {
        for(unsigned j=0;j<3;++j) matrices[i].rotation[j][j]=4096;
        matrices[i].translation[2]=1024;
    }
    owner.render_object.matrices=matrices; owner.pos_z=1024*65536;
    FieldActor targets_data[2]={0}; FieldActor *targets[]={&targets_data[0],&targets_data[1],NULL};
    for(unsigned i=0;i<2;++i) {
        targets_data[i].render_object.target_x=20+i*40;
        targets_data[i].render_object.target_y=0;
        targets_data[i].render_object.target_z=1024+i*100;
    }
    owner.state=&owner_state; owner_state.control10.command_value=1;
    owner_state.action_state=&owner_action;
    player.core=&core; core.action=&action; action.actionCode.actionId=player_action;
    g_PlayerEntity=(FieldActor *)(&player);
    FieldFxObject *object=&buffer.primary[0].effect;
    object->prefix.asset_type=7; object->prefix.actor=&owner;
    memset(buffer.guard,0xa5,sizeof(buffer.guard));
    RenderBufferPrefix saved=D_800B0E38;
    int saved_slot=g_ActiveDrawSlot, saved_offset=g_DrawPacketOffset;
    GteMatrix *saved_view=g_GeomScroll.words.viewMatrix;
    GteMatrix view={.m={{4096,0,0},{0,4096,0},{0,0,4096}}};
    g_GeomScroll.words.viewMatrix=&view;
    s32 *saved_projection=g_GeomScroll.words.projectionDistance, projection=256;
    g_GeomScroll.words.projectionDistance=&projection;
    static RenderGpuTag ordering[4096];
    _Alignas(16) u8 packets[32768];
    Psyz_GteCtrlWrite(26,256); Psyz_GteCtrlWrite(29,1365); Psyz_GteCtrlWrite(30,1024);
    gte_SetGeomOffset(160,120);
    g_FieldFxLoadedTexturePage=0;
    PmTables previous=g_PmTables; Pe1GameState previous_game=g_GameState;
    u32 previous_flags=g_GameStateFlags;
    g_PmTables=(PmTables){g_PmCmdHandlerArray,buffer.primary,NULL};
    if(g_PmCmdHandlerArray[commands[variant]]!=classes[variant]) do { fprintf(stderr,"authored effect failure line %d variant %u action %u\n",__LINE__,variant,player_action); return 0; } while(0);
    g_GameStateFlags=0x80; g_GameState.flags&=~8u; g_GameState.tim_load_state=0;
    g_GameState.cd_range_read_mode=0;
    classes[variant]->init();
    if(Scene_LoadRoomAssets(commands[variant],&owner)!=0 || buffer.primary[0].header.owner!=&owner ||
       buffer.primary[0].header.command!=commands[variant]) do { fprintf(stderr,"authored effect failure line %d variant %u action %u\n",__LINE__,variant,player_action); return 0; } while(0);
    unsigned seen=0, drawn=0;
    if(variant>=5 && Pm_SendCmd(0,0,1,(intptr_t)targets,0,0)) do { fprintf(stderr,"authored effect failure line %d variant %u action %u\n",__LINE__,variant,player_action); return 0; } while(0);
    if(Pm_SendCmd(0,0,0,variant==4?0:1,0x1234,0x5678)) do { fprintf(stderr,"authored effect failure line %d variant %u action %u\n",__LINE__,variant,player_action); return 0; } while(0);
    for(unsigned frame=0;frame<160;++frame) {
        unsigned slot=frame&1;
        for(unsigned i=0;i<4096;++i) { ordering[i].address=0xffffff; ordering[i].length=0; }
        D_800B0E38.ordering[slot]=ordering; D_800B0E38.packets[slot]=(char *)packets;
        g_ActiveDrawSlot=slot; g_DrawPacketOffset=0;
        if(variant<4 && frame==20 && Pm_SendCmd(0,0,0,variant==4?0:1,0,0)) do { fprintf(stderr,"authored effect failure line %d variant %u action %u\n",__LINE__,variant,player_action); return 0; } while(0);
        if(variant!=4 && frame==40 && Pm_SendCmd(0,0,0,2,0,0)) do { fprintf(stderr,"authored effect failure line %d variant %u action %u\n",__LINE__,variant,player_action); return 0; } while(0);
        if(Pm_Start(0) || Pm_Exec(0)) do { fprintf(stderr,"authored effect failure line %d variant %u action %u\n",__LINE__,variant,player_action); return 0; } while(0);
        if(frame==0 && owner_action!=((variant==4)?1:2)) do { fprintf(stderr,"authored effect failure line %d variant %u action %u\n",__LINE__,variant,player_action); return 0; } while(0);
        for(unsigned i=0;i<64;++i) if(object->slots[i].flag==1) {
            seen|=1u<<object->slots[i].handler_id;
            if(variant==4 && object->slots[i].handler_id==1) {
                FieldFxRadialParticles *data=(void *)(object->data+object->slots[i].data_offset);
                if(data->count!=(player_action==3?8:16)) do { fprintf(stderr,"authored effect failure line %d variant %u action %u\n",__LINE__,variant,player_action); return 0; } while(0);
            }
            if(variant==4 && object->slots[i].handler_id==4) {
                FieldFxScatteredParticles *data=(void *)(object->data+object->slots[i].data_offset);
                if(data->points.count!=(player_action==3?4:16)) do { fprintf(stderr,"authored effect failure line %d variant %u action %u\n",__LINE__,variant,player_action); return 0; } while(0);
            }
        }
        if(g_DrawPacketOffset) ++drawn;
        if(g_DrawPacketOffset>sizeof(packets)) do { fprintf(stderr,"authored effect failure line %d variant %u action %u\n",__LINE__,variant,player_action); return 0; } while(0);
        for(unsigned i=0;i<sizeof(buffer.guard);++i) if(buffer.guard[i]!=0xa5) do { fprintf(stderr,"authored effect failure line %d variant %u action %u\n",__LINE__,variant,player_action); return 0; } while(0);
    }
    if(seen!=(unsigned[]){0x3f,0xf,0x7,0x3f,0x3f,0xf,0x7,0x3}[variant] || !object->state.halted || object->state.slot_count || !drawn || buffer.primary[0].header.state || buffer.primary[0].header.owner) {
        fprintf(stderr,"authored effect %u handlers %x halted %d slots %d drawn %u\n",variant,seen,object->state.halted,object->state.slot_count,drawn); do { fprintf(stderr,"authored effect failure line %d variant %u action %u\n",__LINE__,variant,player_action); return 0; } while(0);
    }
    if(Scene_LoadRoomAssets(commands[variant],&owner)!=0) do { fprintf(stderr,"authored effect failure line %d variant %u action %u\n",__LINE__,variant,player_action); return 0; } while(0);
    if(variant>=5 && Pm_SendCmd(0,0,1,(intptr_t)targets,0,0)) do { fprintf(stderr,"authored effect failure line %d variant %u action %u\n",__LINE__,variant,player_action); return 0; } while(0);
    if(Pm_SendCmd(0,0,0,variant==4?0:1,0,0) || Pm_Exec(0) || !object->state.slot_count) do { fprintf(stderr,"authored effect failure line %d variant %u action %u\n",__LINE__,variant,player_action); return 0; } while(0);
    if(Pm_Stop(0,&player,0) || buffer.primary[0].header.state!=2) do { fprintf(stderr,"authored effect failure line %d variant %u action %u\n",__LINE__,variant,player_action); return 0; } while(0);
    if(Pm_Stop(0,&owner,0) || buffer.primary[0].header.state || buffer.primary[0].header.owner) do { fprintf(stderr,"authored effect failure line %d variant %u action %u\n",__LINE__,variant,player_action); return 0; } while(0);
    classes[variant]->unload(); g_PmTables=previous;
    g_GameState=previous_game; g_GameStateFlags=previous_flags;
    g_PlayerEntity=(FieldActor *)(saved_player); D_800B0E38=saved;
    g_ActiveDrawSlot=saved_slot; g_DrawPacketOffset=saved_offset; g_GeomScroll.words.viewMatrix=saved_view;
    g_GeomScroll.words.projectionDistance=saved_projection;
    }
    puts("Original authored effects: eight resident PM IDs and native factory allocations, seven actions, send/start/exec/close/release passed");
    return 1;
}

