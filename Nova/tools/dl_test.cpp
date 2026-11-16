// CLI harness: dl_test <url> <out> <sha256>   (also: dl_test --sha <file>)
#include "http_download.h"
#include <cstdio>
#include <string>
#include <unistd.h>
int main(int argc,char** argv) {
    if(argc==3&&std::string(argv[1])=="--sha") {
        FILE* f=fopen(argv[2],"rb"); if(!f) return 2;
        Sha256 s; char b[7000]; size_t n; while((n=fread(b,1,sizeof b,f))>0) s.update(b,n);
        fclose(f); printf("%s\n",s.finalHex().c_str()); return 0;
    }
    if(argc!=4) return 64;
    HttpDownload d; std::string err;
    if(!d.begin(argv[1],argv[2],argv[3],err)) { printf("BEGIN_FAIL: %s\n",err.c_str()); return 2; }
    while(d.step()==HttpDownload::Running) usleep(500);
    bool ok=d.state()==HttpDownload::Done;
    printf("%s: %s (%llu bytes)\n",ok?"DONE":"FAIL",d.message().c_str(),(unsigned long long)d.received());
    return ok?0:1;
}
