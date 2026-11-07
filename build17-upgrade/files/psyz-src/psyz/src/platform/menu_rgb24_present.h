/* SPDX-License-Identifier: MIT. Source-owned title companion, no private keys. */
#ifndef PE_MENU_RGB24_PRESENT_H
#define PE_MENU_RGB24_PRESENT_H
static PsyzRgb24MenuProducer title_menu_producer;
static void *title_menu_userdata;
static uint64_t title_menu_output_identity;
static bool title_menu_failed,title_menu_output_hd;
void Psyz_VideoSetRgb24MenuProducer(PsyzRgb24MenuProducer producer,void *userdata){
    if(title_menu_producer==producer&&title_menu_userdata==userdata)return;
    title_menu_producer=producer;title_menu_userdata=userdata;title_menu_output_identity=0;title_menu_failed=false;
}
static void TitleMenuLabelsReset(void){
    /* No external label surfaces exist in this fixed native-font selection. */
    title_menu_output_identity=0;
}
static void TitleMenuRetire(void){
    Psyz_VideoSetRgb24MenuProducer(NULL,NULL);TitleMenuLabelsReset();
}
const unsigned char *Psyz_VideoTitleLabelRGBA(unsigned label,int width,int height,
    int *pitch,uint64_t *generation){
    /* External PNG admission is intentionally disabled. Compressed size and
     * IHDR dimensions do not bound retained stb inflate scratch/work. Keep
     * this API fail-closed until a genuinely bounded decoder is selected.
     * Native cached4x font coverage remains the presentation source. */
    (void)label;(void)width;(void)height;
    if(pitch)*pitch=0;if(generation)*generation=1;
    return NULL;
}
static uint64_t TitleMenuProbe(const u8 *rgb){
    if(!title_menu_producer||title_menu_failed||UseSoftwareBackend()||!device||gpu_failed||display_size.x!=320||display_size.y!=240)return 0;
    return title_menu_producer(title_menu_userdata,rgb,320,240,display_area.y,NULL,0,4,0,0);
}
#endif
