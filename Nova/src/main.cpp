#include <SDL.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <ctime>
#include <cmath>
#include <sys/stat.h>

// NOVA: native SDL2 storefront UI (tabs on L1/R1). Catalog from catalog.json (or a built-in sample),
// details screen, confirm dialog, hash-verified downloads. Network/hash/storage live in
// platform_ps4.cpp + http_download.cpp; installing the saved .pkg is left to the user.

#include "platform.h"
#include "catalog.h"

#ifndef PS4_MARKET_CATALOG_PATH
#define PS4_MARKET_CATALOG_PATH "/data/ps4market/catalog.json"
#endif
#ifndef PS4_MARKET_BUNDLED_CATALOG_PATH
#define PS4_MARKET_BUNDLED_CATALOG_PATH "/app0/sce_sys/catalog.json"
#endif

struct Color { Uint8 r,g,b,a; };
static const Color BG{14,8,34,255}, PANEL{22,14,46,255}, PANEL2{40,24,80,255};
static const Color BLUE{124,58,237,255}, CYAN{192,132,252,255}, WHITE{240,232,255,255};
static const Color MUTED{158,142,198,255}, WARN{255,196,92,255}, DIM{6,2,16,200};

struct Item {
    std::string name,type,glyph,version,size,publisher,desc,url,sha256,titleId,region,minFw,release,coverUrl;
    Color a,b;
};

// 5x7 bitmap font: digits 0-9 first, then A-Z, then punctuation (see PUNCT_CHARS).
static const unsigned char FONT[36][7] = {
 {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},
 {30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
 {6,8,16,30,17,17,14},{31,1,2,4,8,8,8},
 {14,17,17,14,17,17,14},{14,17,17,15,1,2,12},
 {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},
 {14,17,16,16,16,17,14},{28,18,17,17,17,18,28},
 {31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
 {14,17,16,23,17,17,15},{17,17,17,31,17,17,17},
 {14,4,4,4,4,4,14},{7,2,2,2,2,18,12},
 {17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
 {17,27,21,21,17,17,17},{17,25,21,19,17,17,17},
 {14,17,17,17,17,17,14},{30,17,17,30,16,16,16},
 {14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
 {15,16,16,14,1,1,30},{31,4,4,4,4,4,4},
 {17,17,17,17,17,17,14},{17,17,17,17,17,10,4},
 {17,17,17,21,21,21,10},{17,17,10,4,10,17,17},
 {17,17,10,4,4,4,4},{31,1,2,4,8,16,31}
};
static const char PUNCT_CHARS[] = ".,/-:!>?_()+'";
static const unsigned char PUNCT[13][7] = {
 {0,0,0,0,0,12,12},{0,0,0,0,12,4,8},{1,1,2,4,8,16,16},{0,0,0,31,0,0,0},
 {0,12,12,0,12,12,0},{4,4,4,4,4,0,4},{16,8,4,2,4,8,16},{14,17,1,2,4,0,4},{0,0,0,0,0,0,31},
 {2,4,8,8,8,4,2},{8,4,2,2,2,4,8},{0,4,4,31,4,4,0},{4,4,8,0,0,0,0}
};
static const unsigned char* glyphFor(char c) {
    if(c>='0'&&c<='9') return FONT[c-'0'];
    if(c>='A'&&c<='Z') return FONT[10+c-'A'];
    const char* p = c ? strchr(PUNCT_CHARS,c) : nullptr;
    return p ? PUNCT[p-PUNCT_CHARS] : nullptr;
}
static void rect(SDL_Renderer* r,int x,int y,int w,int h,Color c) {
    SDL_SetRenderDrawColor(r,c.r,c.g,c.b,c.a);
    SDL_Rect q{x,y,w,h}; SDL_RenderFillRect(r,&q);
}
static void text(SDL_Renderer* r,int x,int y,const std::string& s,int scale,Color c) {
    for(char raw:s) {
        char ch=(raw>='a'&&raw<='z')?char(raw-32):raw;
        if(ch!=' ') {
            const unsigned char* g=glyphFor(ch);
            if(g) for(int row=0;row<7;row++) for(int col=0;col<5;col++)
                if(g[row]&(1<<(4-col))) rect(r,x+col*scale,y+row*scale,scale,scale,c);
        }
        x+=6*scale;
    }
}
static std::vector<std::string> wrap(const std::string& s,size_t width,size_t maxLines) {
    std::vector<std::string> lines; std::string cur, word;
    auto flushWord=[&]() {
        while(word.size()>width) { if(!cur.empty()) { lines.push_back(cur); cur.clear(); } lines.push_back(word.substr(0,width)); word.erase(0,width); }
        if(word.empty()) return;
        if(!cur.empty()&&cur.size()+1+word.size()>width) { lines.push_back(cur); cur.clear(); }
        cur+=(cur.empty()?"":" ")+word; word.clear();
    };
    for(char c:s) { if(c==' ') flushWord(); else word+=c; }
    flushWord(); if(!cur.empty()) lines.push_back(cur);
    if(lines.size()>maxLines) lines.resize(maxLines);
    return lines;
}
static std::string fmtMB(unsigned long long b) {
    char t[32]; snprintf(t,sizeof t,"%.1f MB",b/1048576.0); return t;
}

static Color mid(const Item& t) {
    return {(Uint8)((t.a.r+t.b.r)/2),(Uint8)((t.a.g+t.b.g)/2),(Uint8)((t.a.b+t.b.b)/2),255};
}
static Item makeItem(const CatalogEntry& e) {
    Item it; it.name=e.name; it.type=e.type.empty()?"APP":e.type; for(char& c:it.type) c=(char)toupper((unsigned char)c);
    it.version=e.version.empty()?"UNKNOWN":e.version; it.size=e.size.empty()?"UNKNOWN":e.size;
    it.publisher=e.publisher.empty()?"UNKNOWN":e.publisher;
    it.desc=e.description; it.url=e.url; it.sha256=e.sha256; it.titleId=e.title_id; it.region=e.region; it.minFw=e.min_fw; it.release=e.release; it.coverUrl=e.cover_url;
    // Initials for the tile art: first letter of the first two words (or first two letters).
    std::string g; bool startWord=true;
    for(char c:e.name) {
        if(isalnum((unsigned char)c)) { if(startWord||g.size()==0) g+=(char)toupper((unsigned char)c); startWord=false; }
        else startWord=true;
        if(g.size()==2) break;
    }
    if(g.size()<2) for(char c:e.name) if(isalnum((unsigned char)c)&&g.size()<2) { g+=(char)toupper((unsigned char)c); }
    it.glyph=g.empty()?"?":g;
    unsigned h=2166136261u; for(char c:e.name) { h^=(unsigned char)c; h*=16777619u; }
    it.a={(Uint8)(20+(h&0x7F)),(Uint8)(80+((h>>7)&0x5F)),(Uint8)(110+((h>>14)&0x5F)),255};
    it.b={(Uint8)(60+((h>>21)&0x6F)),(Uint8)(60+((h>>5)&0x5F)),(Uint8)(140+((h>>12)&0x3F)),255};
    return it;
}
static std::vector<CatalogEntry> sampleCatalog() {
    const char* d="Placeholder listing for UI testing. Add a catalog.json to list real items.";
    return {
        {"Pixel Drift","GAME","1.0.0","48 MB","Sample Studio",d,"",""},
        {"Neon Platform","GAME","0.9.2","62 MB","Sample Studio",d,"",""},
        {"File Utility","APP","2.1.0","6 MB","Sample Dev",d,"",""},
        {"Media Center","APP","1.4.0","21 MB","Sample Dev",d,"",""},
        {"Retro Emulator","TOOL","3.0.1","14 MB","Sample Dev",d,"",""},
        {"Aurora Theme","THEME","1.2.0","3 MB","Sample Artist",d,"",""},
        {"Starlight Racer","GAME","1.1.0","71 MB","Sample Studio",d,"",""},
        {"Void Runner","GAME","0.8.0","55 MB","Sample Studio",d,"",""},
        {"Block Garden","GAME","1.3.1","39 MB","Sample Studio",d,"",""},
        {"Echo Tactics","GAME","2.0.0","88 MB","Sample Studio",d,"",""},
        {"Cave Diver","GAME","1.0.4","44 MB","Sample Studio",d,"",""},
        {"Photo Viewer","APP","1.0.0","9 MB","Sample Dev",d,"",""}};
}

enum class Screen { Home, Details, Confirm, Result };
enum class Action { None, Left, Right, Up, Down, Select, Back, PrevTab, NextTab, Fav };

static Color lerp(Color a,Color b,int t,int d) { return {(Uint8)(a.r+(b.r-a.r)*t/d),(Uint8)(a.g+(b.g-a.g)*t/d),(Uint8)(a.b+(b.b-a.b)*t/d),255}; }
static void vgrad(SDL_Renderer* r,int x,int y,int w,int h,Color a,Color b) { for(int i=0;i<h;i+=3) rect(r,x,y+i,w,std::min(3,h-i),lerp(a,b,i,h)); }
static void box(SDL_Renderer* r,int x,int y,int w,int h,Color fill,Color line) {   // notched corners
    rect(r,x+2,y,w-4,h,fill); rect(r,x,y+2,w,h-4,fill);
    rect(r,x+2,y,w-4,1,line); rect(r,x+2,y+h-1,w-4,1,line); rect(r,x,y+2,1,h-4,line); rect(r,x+w-1,y+2,1,h-4,line);
    rect(r,x+1,y+1,1,1,line); rect(r,x+w-2,y+1,1,1,line); rect(r,x+1,y+h-2,1,1,line); rect(r,x+w-2,y+h-2,1,1,line);
}
static void glow(SDL_Renderer* r,int x,int y,int w,int h,Color c) {
    SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);
    for(int k=1;k<=5;k++) { Color g{c.r,c.g,c.b,(Uint8)(90-k*16)};
        rect(r,x-k,y-k,w+2*k,1,g); rect(r,x-k,y+h+k-1,w+2*k,1,g); rect(r,x-k,y-k,1,h+2*k,g); rect(r,x+w+k-1,y-k,1,h+2*k,g); }
    SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_NONE);
}
static int tw(const std::string& s,int sc) { return (int)s.size()*6*sc; }
static Color tint(Color c) { return lerp(c,Color{120,60,230,255},1,2); }
static const Color LINE{70,40,130,255};
static const char* TABS[6]={"GAMES","APPS","TOOLS","FAVORITES","DOWNLOADS","LIBRARY"};

static std::string clockStr() {
    time_t t=time(nullptr); tm* l=localtime(&t); if(!l) return "";
    int h=l->tm_hour%12; if(!h) h=12;
    char b[16]; snprintf(b,sizeof b,"%d:%02d %s",h,l->tm_min,l->tm_hour<12?"AM":"PM"); return b;
}
// 8x8 icons: 12 clock, 0 controller 1 grid 2 wrench 3 star 4 download 5 layers 6 search 7 heart 8 refresh 9 gear 10 bolt 11 info
static const unsigned char ICONS[13][8]={
 {0x00,0x7E,0xFF,0xDB,0xFF,0xE7,0x66,0x00},{0xEE,0xEE,0xEE,0x00,0xEE,0xEE,0xEE,0x00},{0x0E,0x06,0x1C,0x38,0x70,0xE0,0xC0,0x80},
 {0x18,0x18,0xFF,0x7E,0x3C,0x7E,0x66,0x00},{0x18,0x18,0x18,0x7E,0x3C,0x18,0x00,0xFF},{0x18,0x3C,0x7E,0xFF,0x00,0x7E,0x00,0x7E},
 {0x3C,0x42,0x42,0x42,0x3C,0x0C,0x06,0x03},{0x66,0xFF,0xFF,0xFF,0x7E,0x3C,0x18,0x00},{0x3C,0x42,0x81,0x87,0x81,0x81,0x42,0x3C},
 {0x18,0xDB,0x7E,0x66,0x66,0x7E,0xDB,0x18},{0x0C,0x18,0x30,0x7E,0x0C,0x18,0x30,0x20},{0x3C,0x7E,0xF7,0xFF,0xF7,0xF7,0x7E,0x3C},{0x3C,0x42,0x89,0x89,0x8F,0x81,0x42,0x3C}};
static void icon(SDL_Renderer* r,int x,int y,int k,int sc,Color c) {
    for(int row=0;row<8;row++) for(int col=0;col<8;col++) if(ICONS[k][row]&(0x80>>col)) rect(r,x+col*sc,y+row*sc,sc,sc,c);
}
static void drawTop(SDL_Renderer* r,int tab) {
    rect(r,0,0,1280,72,{12,7,30,255}); rect(r,0,71,1280,1,LINE);
    box(r,36,17,38,38,BLUE,CYAN); text(r,47,25,"N",3,WHITE);
    text(r,90,18,"NOVA",5,CYAN); rect(r,238,18,1,36,LINE);
    int x=258;
    for(int i=0;i<6;i++) { int w=tw(TABS[i],2)+52;
        if(i==tab) { glow(r,x,16,w,40,CYAN); box(r,x,16,w,40,PANEL2,CYAN); }
        icon(r,x+14,28,i,2,i==tab?WHITE:CYAN);
        text(r,x+38,29,TABS[i],2,i==tab?WHITE:MUTED); x+=w+4; }
    icon(r,1086,28,6,2,WHITE); rect(r,1118,18,1,36,LINE);
    std::string c=clockStr(); text(r,1244-tw(c,2),29,c,2,WHITE);
}
static void panel(SDL_Renderer* r,int x,int y,int w,int h,const char* title,int ic) {
    box(r,x,y,w,h,PANEL,LINE); icon(r,x+18,y+16,ic,3,CYAN); text(r,x+52,y+21,title,2,WHITE); rect(r,x+14,y+52,w-28,1,LINE);
}
static void drawQueue(SDL_Renderer* r,int x,int y,int w,const DownloadStatus& ds,const std::string& name) {
    if(ds.state==DlState::Idle) { text(r,x,y,"NO ACTIVE DOWNLOADS",1,MUTED); return; }
    text(r,x,y,(ds.state==DlState::Running?"DOWNLOADING ":"")+name.substr(0,20),1,WHITE);
    rect(r,x,y+20,w,10,PANEL2);
    if(ds.total) rect(r,x,y+20,(int)((double)w*ds.received/ds.total),10,ds.state==DlState::Failed?WARN:BLUE);
    if(ds.state==DlState::Running) text(r,x,y+40,fmtMB(ds.received)+(ds.total?" / "+fmtMB(ds.total):""),1,CYAN);
    if(ds.state==DlState::Done) { text(r,x,y+40,"VERIFIED. SAVED TO:",1,CYAN); text(r,x,y+58,ds.path.substr(0,46),1,MUTED); }
    if(ds.state==DlState::Failed) text(r,x,y+40,ds.message.substr(0,48),1,WARN);
}
static void waves(SDL_Renderer* r) {
    SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);
    for(int k=0;k<3;k++) for(int x=0;x<1280;x+=8) {
        int y=470+k*70+(int)(34*sin(x*0.006+k*1.7)+18*sin(x*0.015+k));
        rect(r,x,y,8,720-y,{120,60,230,(Uint8)(16+k*6)}); }
    SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_NONE);
}
// Original artwork: a synthwave city at dusk, varied per item by hashing its name.
static void scene(SDL_Renderer* r,int X,int Y,int W,int H,const Item& it) {
    unsigned h=2166136261u; for(char c:it.name) { h^=(unsigned char)c; h*=16777619u; }
    auto rnd=[&]() { h=h*1664525u+1013904223u; return (int)((h>>16)&0x7FFF); };
    const int hz=Y+176;
    vgrad(r,X+2,Y+2,W-4,hz-Y-2,lerp(Color{20,8,56,255},tint(it.a),1,3),lerp(Color{190,60,150,255},tint(it.b),1,4));
    const int cx=X+560+rnd()%200, cy=hz-48, rad=60;
    for(int dy=-rad;dy<=rad;dy+=2) {
        if(dy>8&&((dy-8)/8)%2==1) continue;
        int half=(int)sqrt((double)(rad*rad-dy*dy)); if(cy+dy+2>hz) continue;
        rect(r,cx-half,cy+dy,2*half,2,lerp(Color{255,210,120,255},Color{255,70,170,255},dy+rad,2*rad)); }
    for(int bx=X+340;bx<X+W-6;) {
        int bw=28+rnd()%26, bh=36+rnd()%112; if(bx+bw>X+W-4) bw=X+W-4-bx;
        rect(r,bx,hz-bh,bw,bh,{22,10,48,255}); rect(r,bx,hz-bh,bw,2,{90,45,150,255});
        for(int wy=hz-bh+8;wy<hz-6;wy+=10) for(int wx=bx+5;wx<bx+bw-5;wx+=9) if(rnd()%5==0) rect(r,wx,wy,3,4,{255,170,210,255});
        bx+=bw+4; }
    vgrad(r,X+2,hz,W-4,Y+H-2-hz,{36,12,72,255},{12,6,30,255});
    for(int k=0;k<5;k++) rect(r,X+2,hz+8+k*k*4,W-4,1,{110,50,190,255});          // road lines
    const int car=X+700, cby=Y+H-62;                                              // sports-car silhouette
    rect(r,car+14,cby,64,14,{30,14,66,255}); rect(r,car,cby+12,126,18,{40,18,86,255}); rect(r,car+6,cby+10,114,2,{150,90,230,255});
    rect(r,car+14,cby+26,24,12,{8,4,20,255}); rect(r,car+88,cby+26,24,12,{8,4,20,255}); rect(r,car+122,cby+16,6,5,{255,70,120,255});
    glow(r,car+122,cby+16,6,5,Color{255,70,120,255});
}
static void drawHero(SDL_Renderer* r,const Item* it,SDL_Texture* cover,int oy) {
    const int X=32,Y=92-oy,W=954,H=270;
    box(r,X,Y,W,H,PANEL,CYAN);
    if(!it) { text(r,X+40,Y+110,"NOTHING HERE YET",3,MUTED); return; }
    scene(r,X,Y,W,H,*it);
    if(cover) { SDL_Rect dst{X+710,Y+22,220,220}; SDL_RenderCopy(r,cover,nullptr,&dst); }
    SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);
    rect(r,X+2,Y+2,470,H-4,{10,5,26,185}); rect(r,X+472,Y+2,60,H-4,{10,5,26,90}); rect(r,X+532,Y+2,50,H-4,{10,5,26,40});
    SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_NONE);
    box(r,X+40,Y+34,tw(it->type,1)+44,24,{30,18,64,255},LINE); icon(r,X+50,Y+40,3,1,CYAN); text(r,X+64,Y+41,it->type,1,CYAN);
    std::string nm=it->name.substr(0,22); text(r,X+40,Y+78,nm,nm.size()>14?3:4,WHITE);
    int y=Y+124; for(const std::string& l:wrap(it->desc,32,3)) { text(r,X+40,y,l,2,{225,214,250,255}); y+=24; }
    box(r,X+40,Y+198,204,42,{20,10,46,255},CYAN); icon(r,X+58,Y+211,4,2,CYAN); text(r,X+86,Y+212,"VIEW DETAILS",2,CYAN);
}
static void drawTile(SDL_Renderer* r,int x,int y,const Item& t,SDL_Texture* cover,bool sel,bool fav) {
    if(sel) glow(r,x,y,150,137,CYAN);
    box(r,x,y,150,137,PANEL,sel?CYAN:LINE);
    vgrad(r,x+2,y+2,146,133,tint(t.a),tint(t.b));
    if(cover) { SDL_Rect dst{x+2,y+2,146,133}; SDL_RenderCopy(r,cover,nullptr,&dst); }
    else text(r,x+(150-tw(t.glyph,5))/2,y+30,t.glyph,5,WHITE);
    SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND); rect(r,x+2,y+101,146,34,{10,5,26,200}); SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_NONE);
    text(r,x+10,y+106,t.name.substr(0,23),1,WHITE); text(r,x+10,y+120,t.type,1,CYAN);
    if(fav) text(r,x+140-tw("FAV",1),y+120,"FAV",1,WARN);
}
static void sect(SDL_Renderer* r,int y,const std::string& title,const std::string& right) {
    rect(r,32,y,5,24,CYAN); text(r,48,y+4,title,2,WHITE); if(!right.empty()) text(r,986-tw(right,1),y+9,right,1,MUTED);
}
static void drawStatus(SDL_Renderer* r,const DownloadStatus& ds,const std::string& name) {
    if(ds.state==DlState::Idle) return;
    rect(r,0,694,1280,26,{12,7,30,255}); rect(r,0,694,1280,1,LINE);
    text(r,30,703,(ds.state==DlState::Running?"DOWNLOADING ":"")+name.substr(0,24),1,WHITE);
    if(ds.state==DlState::Running) {
        rect(r,330,703,400,10,PANEL2); if(ds.total) rect(r,330,703,(int)(400.0*ds.received/ds.total),10,BLUE);
        text(r,750,703,fmtMB(ds.received)+(ds.total?" / "+fmtMB(ds.total):""),1,CYAN);
    }
    if(ds.state==DlState::Done) text(r,330,703,"VERIFIED. SAVED TO: "+ds.path.substr(0,60),1,CYAN);
    if(ds.state==DlState::Failed) text(r,330,703,ds.message.substr(0,70),1,WARN);
}
static void drawHome(SDL_Renderer* ren,const std::vector<Item>& items,const std::vector<int>& view,int tab,int focus,
                     const std::vector<bool>& fav,const DownloadStatus& ds,const std::string& dlName,
                     const std::string& note,bool noteWarn,int nDone,const std::string& updated,const std::vector<SDL_Texture*>& covers) {
    vgrad(ren,0,0,1280,720,{14,8,34,255},{34,13,70,255}); waves(ren);
    const int vn=(int)view.size(), page=vn?focus/12:0, pages=vn?(vn+11)/12:1;
    const int row=vn?(focus%12)/6:0, oy=row?233:0;            // second row scrolls up when it has focus
    if(tab==4) {
        box(ren,32,92,954,270,PANEL,CYAN); text(ren,72,126,"DOWNLOADS",3,WHITE);
        drawQueue(ren,72,180,860,ds,dlName);
        text(ren,32,400,"FINISHED DOWNLOADS ARE LISTED IN LIBRARY",2,MUTED);
    } else {
        drawHero(ren,vn?&items[view[focus]]:nullptr,vn&&view[focus]<(int)covers.size()?covers[view[focus]]:nullptr,oy);
        const int dots=std::min(vn,5);
        for(int i=0;i<dots;i++) rect(ren,479-dots*9+i*18+(i==focus%5?-2:0),(378-oy)+(i==focus%5?-2:0),i==focus%5?12:8,i==focus%5?12:8,i==focus%5?CYAN:LINE);
        sect(ren,405-oy,"TRENDING","");
        if(vn>6) sect(ren,612-oy,"NEW RELEASES","PAGE "+std::to_string(page+1)+" / "+std::to_string(pages)+" >");
        if(vn==0) text(ren,48,460,tab==3?"PRESS SQUARE ON AN ITEM TO FAVORITE IT":tab==5?"FINISHED DOWNLOADS APPEAR HERE":"NO LISTINGS IN THIS TAB",2,MUTED);
        for(int i=0;i<12;i++) { int idx=page*12+i; if(idx>=vn) break;
            drawTile(ren,32+(i%6)*161,(i<6?433:640)-oy,items[view[idx]],view[idx]<(int)covers.size()?covers[view[idx]]:nullptr,idx==focus,fav[view[idx]]); }
    }
    rect(ren,0,0,1280,72,{12,7,30,255}); drawTop(ren,tab);     // top bar stays above scrolled content
    panel(ren,1008,100,242,264,"STORE INFO",11);
    text(ren,1030,170,"WELCOME TO NOVA!",2,WHITE); text(ren,1030,196,"DISCOVER. DOWNLOAD. PLAY.",1,MUTED);
    rect(ren,1022,222,214,1,LINE);
    const std::string cat=noteWarn?"ERROR":(note=="SAMPLE CATALOG"?"SAMPLE":updated), n1=std::to_string(items.size()), n2=std::to_string(nDone);
    const char* lab[3]={"TOTAL ITEMS","DOWNLOADED","LAST UPDATED"}; const std::string val[3]={n1,n2,cat}; const int ik[3]={0,4,12};
    for(int i=0;i<3;i++) { int y=242+i*38; icon(ren,1030,y-2,ik[i],2,CYAN); text(ren,1056,y+2,lab[i],1,MUTED);
        text(ren,1236-tw(val[i],1),y+2,val[i],1,noteWarn&&i==2?WARN:WHITE); }
    panel(ren,1008,422,242,204,"QUICK LINKS",10);
    const char* ql[4]={"MY FAVORITES","MY DOWNLOADS","UPDATES","SETTINGS"}; const int qi[4]={7,4,8,9};
    for(int i=0;i<4;i++) { int y=488+i*34; bool on=(i==0&&tab==3)||(i==1&&tab==4);
        icon(ren,1030,y,qi[i],2,CYAN); text(ren,1062,y+1,ql[i],2,on?WHITE:MUTED); }
    drawStatus(ren,ds,dlName);
}
static void drawDetails(SDL_Renderer* ren,const Item& it) {
    rect(ren,270,102,980,580,PANEL);
    rect(ren,270,102,7,580,BLUE);
    rect(ren,310,140,190,154,mid(it));
    text(ren,310+(190-(int)it.glyph.size()*24)/2+2,190,it.glyph,4,WHITE);
    text(ren,540,140,it.name.substr(0,30),(it.name.size()>22?3:4),WHITE);
    text(ren,540,184,it.type,2,CYAN);
    text(ren,540,224,"VERSION "+it.version,1,MUTED);
    text(ren,540,244,"SIZE "+it.size,1,MUTED);
    text(ren,540,264,"PUBLISHER "+it.publisher,1,MUTED);
    if(!it.titleId.empty()) {
        text(ren,540,284,"TITLE ID "+it.titleId,1,CYAN);
        text(ren,540,304,"REGION "+(it.region.empty()?std::string("UNKNOWN"):it.region),1,MUTED);
        text(ren,780,304,"MIN FW "+(it.minFw.empty()?std::string("UNKNOWN"):it.minFw),1,MUTED);
        if(!it.release.empty()) text(ren,540,324,"RELEASE "+it.release,1,MUTED);
    }
    int y=it.titleId.empty()?334:366; for(const std::string& l:wrap(it.desc,140,5)) { text(ren,310,y,l,1,WHITE); y+=16; }
    bool canDl=!it.url.empty()&&!it.sha256.empty();
    text(ren,310,440,it.titleId.empty()?"PACKAGE HASH (SHA-256)":"CATALOG SOURCE",1,CYAN);
    text(ren,310,460,canDl?it.sha256:(it.titleId.empty()?"NO HASH - DOWNLOAD DISABLED":"METADATA ONLY - DOWNLOAD DISABLED"),1,canDl?WHITE:WARN);
    rect(ren,310,560,230,48,canDl?BLUE:PANEL2); text(ren,334,578,canDl?"X DOWNLOAD":"CATALOG ONLY",2,canDl?WHITE:MUTED);
    rect(ren,560,560,170,48,PANEL2); text(ren,590,578,"O BACK",2,WHITE);
    rect(ren,0,696,1280,24,{8,18,38,255});
    text(ren,30,704,"ENTER / X DOWNLOAD",1,MUTED);
    text(ren,300,704,"BACKSPACE / O BACK",1,MUTED);
}
static void drawModal(SDL_Renderer* ren,const char* title,const std::string& l1,const char* l2,const char* hint) {
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
    rect(ren,0,0,1280,720,DIM);
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);
    rect(ren,340,230,600,240,PANEL);
    rect(ren,340,230,600,6,CYAN);
    text(ren,372,262,title,2,WHITE);
    text(ren,372,310,l1,1,MUTED);
    text(ren,372,334,l2,1,MUTED);
    text(ren,372,420,hint,1,CYAN);
}

int main(int argc,char** argv) {
    (void)argc; (void)argv;
    // Catalog: use catalog.json if present and valid; otherwise the built-in sample.
    std::vector<CatalogEntry> entries; std::string err, note="CATALOG LOADED"; bool noteWarn=false;
    if(!catalog_load_file(PS4_MARKET_CATALOG_PATH,entries,err)) {
        std::string bundledErr;
        if(catalog_load_file(PS4_MARKET_BUNDLED_CATALOG_PATH,entries,bundledErr)) {
            note="BUNDLED CATALOG";
        } else {
            entries=sampleCatalog();
            if(err=="CATALOG FILE NOT FOUND" && bundledErr=="CATALOG FILE NOT FOUND") note="SAMPLE CATALOG";
            else { note="BAD CATALOG: "+(err=="CATALOG FILE NOT FOUND"?bundledErr:err); noteWarn=true; }
        }
    }
    std::vector<Item> items; for(const CatalogEntry& e:entries) items.push_back(makeItem(e));
    const int n=(int)items.size();
    std::string updated="-";                       // date of catalog.json, shown in Store Info
    { struct stat st; if(!noteWarn&&note=="CATALOG LOADED"&&stat(PS4_MARKET_CATALOG_PATH,&st)==0) {
        time_t t=st.st_mtime; tm* l=localtime(&t); if(l) { char b[48]; snprintf(b,sizeof b,"%d/%d/%d",l->tm_mon+1,l->tm_mday,l->tm_year+1900); updated=b; } } }

#ifdef PS4_NATIVE
    // Same setup as the OpenOrbis SDL2 sample: 1920x1080 window surface + software renderer,
    // raw joystick API (the PS4 SDL port has no game-controller mapping), one UpdateWindowSurface per frame.
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_JOYSTICK)!=0) return 1;
    SDL_Window* win=SDL_CreateWindow("PS4 MARKET",SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,1920,1080,0);
    if(!win) { SDL_Quit(); return 2; }
    SDL_Surface* surf=SDL_GetWindowSurface(win);
    SDL_Renderer* ren=surf?SDL_CreateSoftwareRenderer(surf):nullptr;
    if(!ren) { SDL_DestroyWindow(win); SDL_Quit(); return 3; }
    SDL_RenderSetLogicalSize(ren,1280,720);          // the UI is laid out on 1280x720
    SDL_Joystick* pad=nullptr;
    if(SDL_NumJoysticks()>0) pad=SDL_JoystickOpen(0);
#else
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER)!=0) return 1;
    SDL_Window* win=SDL_CreateWindow("PS4 MARKET",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
        1280,720,SDL_WINDOW_SHOWN);
    if(!win) { SDL_Quit(); return 2; }
    SDL_Renderer* ren=SDL_CreateRenderer(win,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
    if(!ren) { SDL_DestroyWindow(win); SDL_Quit(); return 3; }
    for(int i=0;i<SDL_NumJoysticks();i++) if(SDL_IsGameController(i)) SDL_GameControllerOpen(i);
#endif

    std::vector<SDL_Texture*> covers(items.size(),nullptr);
    for(size_t i=0;i<items.size();++i) {
        char path[128]; snprintf(path,sizeof path,"/app0/sce_sys/covers/%04u.bmp",(unsigned)i);
        SDL_Surface* image=SDL_LoadBMP(path);
        if(image) { covers[i]=SDL_CreateTextureFromSurface(ren,image); SDL_FreeSurface(image); }
    }
    bool running=true; int focus=0, tab=0, dlIdx=-1; bool libRec=false; Screen screen=Screen::Home;
    std::vector<bool> fav(n,false); std::vector<int> view, lib;
    auto rebuild=[&]() {
        view.clear();
        for(int i=0;i<n;i++) { const std::string& t=items[i].type;
            bool ok=tab==0?t=="GAME":tab==1?t=="APP":tab==2?(t!="GAME"&&t!="APP"):tab==3?(bool)fav[i]
                   :tab==5?std::find(lib.begin(),lib.end(),i)!=lib.end():false;
            if(ok) view.push_back(i); }
        if(focus>=(int)view.size()) focus=0;
    };
    rebuild();
    std::string dlName, resultMsg; DownloadStatus ds; SDL_Event ev;
    while(running) {
        Action act=Action::None;
        while(SDL_PollEvent(&ev)) {
            if(ev.type==SDL_QUIT) running=false;
#ifdef PS4_NATIVE
            if(ev.type==SDL_JOYDEVICEADDED && !pad) pad=SDL_JoystickOpen(ev.jdevice.which);
            if(ev.type==SDL_JOYBUTTONDOWN) {      // button numbers as used by the OpenOrbis SDL2 sample
                switch(ev.jbutton.button) {
                    case 0:  act=Action::Select; break;   // Cross
                    case 1:  act=Action::Back;   break;   // Circle
                    case 13: act=Action::Up;     break;   // D-pad
                    case 14: act=Action::Down;   break;
                    case 15: act=Action::Left;   break;
                    case 16: act=Action::Right;  break;
                    case 4:  act=Action::PrevTab; break;  // L1
                    case 5:  act=Action::NextTab; break;  // R1
                    case 2:  act=Action::Fav;     break;  // Square
                    default: break;
                }
            }
#else
            if(ev.type==SDL_CONTROLLERDEVICEADDED) SDL_GameControllerOpen(ev.cdevice.which);
#endif
            if(ev.type==SDL_KEYDOWN) {
                switch(ev.key.keysym.sym) {
                    case SDLK_RIGHT: act=Action::Right; break;
                    case SDLK_LEFT:  act=Action::Left;  break;
                    case SDLK_UP:    act=Action::Up;    break;
                    case SDLK_DOWN:  act=Action::Down;  break;
                    case SDLK_RETURN: case SDLK_SPACE: act=Action::Select; break;
                    case SDLK_ESCAPE: case SDLK_BACKSPACE: act=Action::Back; break;
                    case SDLK_q: act=Action::PrevTab; break;
                    case SDLK_e: act=Action::NextTab; break;
                    case SDLK_f: act=Action::Fav; break;
                    default: break;
                }
            }
            if(ev.type==SDL_CONTROLLERBUTTONDOWN) {
                switch(ev.cbutton.button) {
                    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: act=Action::Right; break;
                    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:  act=Action::Left;  break;
                    case SDL_CONTROLLER_BUTTON_DPAD_UP:    act=Action::Up;    break;
                    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:  act=Action::Down;  break;
                    case SDL_CONTROLLER_BUTTON_A: act=Action::Select; break; // Cross
                    case SDL_CONTROLLER_BUTTON_B: act=Action::Back;   break; // Circle
                    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:  act=Action::PrevTab; break;
                    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: act=Action::NextTab; break;
                    case SDL_CONTROLLER_BUTTON_X: act=Action::Fav; break;
                    default: break;
                }
            }
        }
        const int vn=(int)view.size(); const int cur=vn?view[focus]:-1;
        switch(screen) {
        case Screen::Home:
            if(act==Action::NextTab||act==Action::PrevTab) { tab=(tab+(act==Action::NextTab?1:5))%6; focus=0; rebuild(); }
            else if(vn>0) {
                if(act==Action::Right) focus=(focus+1)%vn;
                if(act==Action::Left)  focus=(focus+vn-1)%vn;
                if(act==Action::Down)  focus=std::min(focus+6,vn-1);
                if(act==Action::Up)    focus=std::max(focus-6,0);
                if(act==Action::Select) screen=Screen::Details;
                if(act==Action::Fav) { fav[cur]=!fav[cur]; if(tab==3) rebuild(); }
            }
            // Circle on Home does nothing now (it used to quit the app, which looked like a crash).
            break;
        case Screen::Details:
            if(act==Action::Select) {
                if(!items[cur].url.empty()&&!items[cur].sha256.empty()) screen=Screen::Confirm;
                else { resultMsg="NO URL OR HASH LISTED FOR THIS ITEM"; screen=Screen::Result; }
            }
            if(act==Action::Back) screen=Screen::Home;
            break;
        case Screen::Confirm:
            if(act==Action::Select) {
                std::string e2; const Item& it=items[cur];
                if(platform_start_download(it.name.c_str(),it.url.c_str(),it.sha256.c_str(),e2)) {
                    dlName=it.name; dlIdx=cur; libRec=false; ds=DownloadStatus(); ds.state=DlState::Running;
                    screen=Screen::Home;
                } else { resultMsg=e2; screen=Screen::Result; }
            }
            if(act==Action::Back) screen=Screen::Details;
            break;
        case Screen::Result:
            if(act==Action::Select||act==Action::Back) screen=Screen::Home;
            break;
        }

        if(ds.state==DlState::Running) platform_poll_download(ds);
        if(ds.state==DlState::Done&&!libRec) { libRec=true; if(dlIdx>=0&&std::find(lib.begin(),lib.end(),dlIdx)==lib.end()) lib.push_back(dlIdx); rebuild(); }
        SDL_SetRenderDrawColor(ren,BG.r,BG.g,BG.b,BG.a); SDL_RenderClear(ren);
        drawHome(ren,items,view,tab,focus,fav,ds,dlName,note,noteWarn,(int)lib.size(),updated,covers);
        if(screen!=Screen::Home) {
            drawDetails(ren,items[cur]);
            if(screen==Screen::Confirm)
                drawModal(ren,"CONFIRM DOWNLOAD","ONLY INSTALL PACKAGES YOU ARE AUTHORIZED TO USE.",
                          "THE FILE IS DELETED IF ITS HASH DOES NOT MATCH.","X CONFIRM     O CANCEL");
            if(screen==Screen::Result) drawModal(ren,"DOWNLOAD STATUS",resultMsg,"","X OK");
        }
#ifdef PS4_NATIVE
        SDL_UpdateWindowSurface(win);
#else
        SDL_RenderPresent(ren);
#endif
    }
    for(SDL_Texture* t:covers) if(t) SDL_DestroyTexture(t);
    SDL_DestroyRenderer(ren); SDL_DestroyWindow(win); SDL_Quit();
    return 0;
}
