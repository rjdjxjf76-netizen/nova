#include "catalog.h"
#include <cstdio>
int main(int argc,char** argv) {
    if(argc!=2) return 64;
    std::vector<CatalogEntry> v; std::string err;
    if(!catalog_load_file(argv[1],v,err)) { printf("ERR %s\n",err.c_str()); return 1; }
    printf("OK %zu\n",v.size());
    for(auto& e:v) printf("%s|%s|%s|%s|%s\n",e.name.c_str(),e.type.c_str(),e.size.c_str(),e.url.c_str(),e.sha256.c_str());
    return 0;
}
