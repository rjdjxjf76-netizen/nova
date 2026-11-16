#include "platform.h"
#include "http_download.h"
#include <cctype>
#include <sys/stat.h>
#include <sys/types.h>

// Where verified packages are saved. Override with -DPS4_MARKET_DL_DIR="\"/some/dir\"".
#ifndef PS4_MARKET_DL_DIR
#define PS4_MARKET_DL_DIR "/data/ps4market"
#endif

static HttpDownload g_dl;

static std::string fileNameFor(const char* name) {
    std::string f;
    for(const char* p=name;*p;++p) f+=isalnum((unsigned char)*p)?(char)tolower((unsigned char)*p):'_';
    return f.empty()?"download.pkg":f+".pkg";
}

bool platform_start_download(const char* name,const char* url,const char* sha256,std::string& err) {
    if(!url||!url[0]||!sha256||!sha256[0]) { err="NO URL OR HASH LISTED FOR THIS ITEM"; return false; }
    mkdir(PS4_MARKET_DL_DIR,0777);          // fine if it already exists
    std::string out=std::string(PS4_MARKET_DL_DIR)+"/"+fileNameFor(name);
    return g_dl.begin(url,out,sha256,err);
}

void platform_poll_download(DownloadStatus& s) {
    switch(g_dl.step()) {
        case HttpDownload::Idle:    s.state=DlState::Idle; break;
        case HttpDownload::Running: s.state=DlState::Running; break;
        case HttpDownload::Done:    s.state=DlState::Done; s.path=g_dl.outPath(); break;
        case HttpDownload::Failed:  s.state=DlState::Failed; break;
    }
    s.received=g_dl.received(); s.total=g_dl.total(); s.message=g_dl.message();
}
// Installing the verified .pkg is deliberately NOT done here: it needs the console's
// package-install API, which this project has not been able to build or test against.
