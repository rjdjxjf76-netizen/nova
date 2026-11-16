// Minimal software stand-in for SDL2, used ONLY to test-render the UI off-console.
#pragma once
#include <cstdint>
#include <cstdio>
#include <vector>
typedef uint8_t Uint8; typedef uint32_t Uint32;
#define SDL_INIT_VIDEO 1
#define SDL_INIT_GAMECONTROLLER 2
#define SDL_WINDOWPOS_CENTERED 0
#define SDL_WINDOW_SHOWN 0
#define SDL_RENDERER_ACCELERATED 0
#define SDL_RENDERER_PRESENTVSYNC 0
enum { SDL_BLENDMODE_NONE=0, SDL_BLENDMODE_BLEND=1 };
enum { SDL_QUIT=1, SDL_KEYDOWN, SDL_CONTROLLERBUTTONDOWN, SDL_CONTROLLERDEVICEADDED };
enum { SDLK_RIGHT=1,SDLK_LEFT,SDLK_UP,SDLK_DOWN,SDLK_RETURN,SDLK_SPACE,SDLK_ESCAPE,SDLK_BACKSPACE,SDLK_q,SDLK_e,SDLK_f };
enum { SDL_CONTROLLER_BUTTON_A=1,SDL_CONTROLLER_BUTTON_B,SDL_CONTROLLER_BUTTON_DPAD_UP,
       SDL_CONTROLLER_BUTTON_DPAD_DOWN,SDL_CONTROLLER_BUTTON_DPAD_LEFT,SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
       SDL_CONTROLLER_BUTTON_LEFTSHOULDER,SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,SDL_CONTROLLER_BUTTON_X };
struct SDL_Rect { int x,y,w,h; };
struct SDL_Window {}; struct SDL_Renderer {};
struct SDL_Event { int type; struct { struct { int sym; } keysym; } key; struct { int button; } cbutton; struct { int which; } cdevice; };
static std::vector<uint8_t> g_fb(1280*720*3);
static uint8_t g_c[4]; static int g_blend=0; static int g_frame=0, g_pi=0; static bool g_give=true;
// Scripted keys, one per frame: right, tab R1 x2 (APPS, TOOLS), favorite, tab R1 x3 (FAVORITES, DOWNLOADS, LIBRARY),
// tab L1, Circle on Home (must NOT quit), L1 x3 back to GAMES, open details
static int g_script[]={0,SDLK_RIGHT,SDLK_e,SDLK_e,SDLK_f,SDLK_e,SDLK_e,SDLK_e,SDLK_q,SDLK_ESCAPE,SDLK_q,SDLK_q,SDLK_q,SDLK_RETURN,0};
inline int SDL_Init(Uint32){return 0;}
inline SDL_Window* SDL_CreateWindow(const char*,int,int,int,int,Uint32){static SDL_Window w;return &w;}
inline SDL_Renderer* SDL_CreateRenderer(SDL_Window*,int,Uint32){static SDL_Renderer r;return &r;}
inline int SDL_NumJoysticks(){return 0;} inline int SDL_IsGameController(int){return 0;}
inline void* SDL_GameControllerOpen(int){return nullptr;}
inline int SDL_PollEvent(SDL_Event* e){
    if(!g_give){g_give=true;return 0;}
    g_give=false;
    if(g_pi>=(int)(sizeof(g_script)/sizeof(int))){e->type=SDL_QUIT;return 1;}
    int k=g_script[g_pi++]; if(!k){return 0;}
    e->type=SDL_KEYDOWN; e->key.keysym.sym=k; return 1;
}
inline int SDL_SetRenderDrawBlendMode(SDL_Renderer*,int m){g_blend=m;return 0;}
inline int SDL_SetRenderDrawColor(SDL_Renderer*,Uint8 r,Uint8 g,Uint8 b,Uint8 a){g_c[0]=r;g_c[1]=g;g_c[2]=b;g_c[3]=a;return 0;}
inline int SDL_RenderFillRect(SDL_Renderer*,const SDL_Rect* q){
    for(int y=q->y;y<q->y+q->h;y++) for(int x=q->x;x<q->x+q->w;x++)
        if(x>=0&&x<1280&&y>=0&&y<720) for(int i=0;i<3;i++){ uint8_t& d=g_fb[(y*1280+x)*3+i]; d=g_blend? (uint8_t)((g_c[i]*g_c[3]+d*(255-g_c[3]))/255) : g_c[i]; }
    return 0;}
inline int SDL_RenderClear(SDL_Renderer*){SDL_Rect q{0,0,1280,720};return SDL_RenderFillRect(nullptr,&q);}
inline void SDL_RenderPresent(SDL_Renderer*){
    char p[64]; snprintf(p,sizeof p,"/tmp/frames/f%02d.ppm",g_frame++);
    FILE* f=fopen(p,"wb"); fprintf(f,"P6\n1280 720\n255\n"); fwrite(g_fb.data(),1,g_fb.size(),f); fclose(f);}
inline void SDL_DestroyRenderer(SDL_Renderer*){} inline void SDL_DestroyWindow(SDL_Window*){} inline void SDL_Quit(){}
