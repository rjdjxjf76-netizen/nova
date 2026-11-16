#include "catalog.h"
#include <cstdio>
#include <cstring>

namespace {
const size_t kMaxJson = 8u<<20, kMaxItems = 3000, kMaxField = 1024;

struct J {
    enum T { Null, Bool, Num, Str, Arr, Obj } t = Null;
    std::string s;                       // Str value, or number text
    std::vector<std::string> keys;       // Obj keys (parallel to a)
    std::vector<J> a;                    // Arr elements or Obj values
};
struct P { const char* p; const char* e; std::string err; int depth = 0; };

void skip(P& x) { while(x.p<x.e&&(*x.p==' '||*x.p=='\t'||*x.p=='\n'||*x.p=='\r')) x.p++; }
bool fail(P& x,const char* m) { if(x.err.empty()) x.err=m; return false; }

bool parseStr(P& x,std::string& out) {
    if(x.p>=x.e||*x.p!='"') return fail(x,"EXPECTED STRING");
    x.p++;
    while(x.p<x.e) {
        char c=*x.p++;
        if(c=='"') return true;
        if((unsigned char)c<0x20) return fail(x,"BAD CONTROL CHARACTER");
        if(c!='\\') { if(out.size()<kMaxField) out+=c; continue; }
        if(x.p>=x.e) break;
        char d=*x.p++;
        switch(d) {
            case '"': out+='"'; break;  case '\\': out+='\\'; break; case '/': out+='/'; break;
            case 'b': out+=' '; break;  case 'f': out+=' '; break;   case 'n': out+=' '; break;
            case 'r': out+=' '; break;  case 't': out+=' '; break;
            case 'u': {
                if(x.e-x.p<4) return fail(x,"BAD UNICODE ESCAPE");
                unsigned v=0;
                for(int i=0;i<4;i++) {
                    char h=*x.p++; v<<=4;
                    if(h>='0'&&h<='9') v|=h-'0'; else if(h>='a'&&h<='f') v|=h-'a'+10;
                    else if(h>='A'&&h<='F') v|=h-'A'+10; else return fail(x,"BAD UNICODE ESCAPE");
                }
                out+=(v>=32&&v<127)?(char)v:'?';     // the on-screen font is ASCII only
                break;
            }
            default: return fail(x,"BAD ESCAPE");
        }
    }
    return fail(x,"UNTERMINATED STRING");
}

bool parseVal(P& x,J& v) {
    skip(x);
    if(x.p>=x.e) return fail(x,"UNEXPECTED END");
    if(++x.depth>16) return fail(x,"NESTED TOO DEEP");
    struct D { int& d; ~D(){ --d; } } guard{x.depth};
    char c=*x.p;
    if(c=='{'||c=='[') {
        bool obj=(c=='{'); char close=obj?'}':']';
        v.t=obj?J::Obj:J::Arr; x.p++; skip(x);
        if(x.p<x.e&&*x.p==close) { x.p++; return true; }
        for(;;) {
            skip(x);
            if(obj) {
                std::string k; if(!parseStr(x,k)) return false;
                skip(x); if(x.p>=x.e||*x.p!=':') return fail(x,"EXPECTED :");
                x.p++; v.keys.push_back(k);
            }
            J child; if(!parseVal(x,child)) return false;
            v.a.push_back(std::move(child));
            if(v.a.size()>100000) return fail(x,"TOO MANY VALUES");
            skip(x);
            if(x.p<x.e&&*x.p==',') { x.p++; continue; }
            if(x.p<x.e&&*x.p==close) { x.p++; return true; }
            return fail(x,obj?"EXPECTED COMMA OR END OF OBJECT":"EXPECTED COMMA OR END OF LIST");
        }
    }
    if(c=='"') { v.t=J::Str; return parseStr(x,v.s); }
    auto lit=[&](const char* w,J::T t)->bool {
        size_t n=strlen(w);
        if((size_t)(x.e-x.p)>=n&&strncmp(x.p,w,n)==0) { x.p+=n; v.t=t; return true; }
        return false;
    };
    if(lit("true",J::Bool)||lit("false",J::Bool)||lit("null",J::Null)) return true;
    if(c=='-'||(c>='0'&&c<='9')) {
        const char* s=x.p; while(x.p<x.e&&strchr("+-0123456789.eE",*x.p)) x.p++;
        v.t=J::Num; v.s.assign(s,x.p); return true;
    }
    return fail(x,"UNEXPECTED CHARACTER");
}

std::string field(const J& o,const char* key) {
    for(size_t i=0;i<o.keys.size();i++)
        if(o.keys[i]==key&&(o.a[i].t==J::Str||o.a[i].t==J::Num)) return o.a[i].s;
    return "";
}
const J* member(const J& o,const char* key) {
    if(o.t!=J::Obj) return nullptr;
    for(size_t i=0;i<o.keys.size();i++) if(o.keys[i]==key) return &o.a[i];
    return nullptr;
}
std::string humanSize(const std::string& raw) {
    if(raw.empty()) return "UNKNOWN";
    char* end=nullptr;
    unsigned long long n=strtoull(raw.c_str(),&end,10);
    if(!end||*end) return raw;
    char b[64];
    if(n>=1024ull*1024*1024) snprintf(b,sizeof b,"%.1f GB",n/(1024.0*1024*1024));
    else if(n>=1024ull*1024) snprintf(b,sizeof b,"%.0f MB",n/(1024.0*1024));
    else snprintf(b,sizeof b,"%.0f KB",n/1024.0);
    return b;
}
bool appendDataEntry(const J& it,CatalogEntry& e) {
    if(it.t!=J::Obj) return false;
    e.name=field(it,"name");
    if(e.name.empty()) return false;
    e.type="GAME"; e.version=field(it,"version");
    e.size=humanSize(field(it,"size")); e.publisher="CATALOG";
    e.title_id=field(it,"title_id"); e.region=field(it,"region");
    e.min_fw=field(it,"min_fw"); e.release=field(it,"release");
    e.cover_url=field(it,"cover_url");
    // The DATA object's key is a package URL. Deliberately never import it.
    e.url.clear(); e.sha256.clear();
    e.description="PS4 game metadata";
    return true;
}
} // namespace

bool catalog_parse(const std::string& json,std::vector<CatalogEntry>& out,std::string& err) {
    if(json.size()>kMaxJson) { err="CATALOG TOO LARGE"; return false; }
    P x{json.data(),json.data()+json.size(),"",0};
    J root;
    if(!parseVal(x,root)) { err=x.err; return false; }
    skip(x);
    if(x.p!=x.e) { err="TRAILING DATA AFTER JSON"; return false; }

    std::vector<CatalogEntry> tmp;
    const J* data=member(root,"DATA");
    if(data && data->t==J::Obj) {
        if(data->a.size()>kMaxItems) { err="TOO MANY ITEMS"; return false; }
        for(size_t i=0;i<data->a.size();i++) {
            CatalogEntry e;
            if(appendDataEntry(data->a[i],e)) tmp.push_back(e);
        }
        if(tmp.empty()) { err="NO VALID GAME METADATA"; return false; }
    } else {
        const J* arr=nullptr;
        if(root.t==J::Arr) arr=&root;
        else if(root.t==J::Obj) {
            const J* items=member(root,"items");
            if(items&&items->t==J::Arr) arr=items;
        }
        if(!arr) { err="NO ITEMS ARRAY OR DATA OBJECT"; return false; }
        if(arr->a.size()>kMaxItems) { err="TOO MANY ITEMS"; return false; }
        for(size_t i=0;i<arr->a.size();i++) {
            const J& it=arr->a[i];
            if(it.t!=J::Obj) { err="ITEM "+std::to_string(i+1)+" IS NOT AN OBJECT"; return false; }
            CatalogEntry e;
            e.name=field(it,"name"); e.type=field(it,"type"); e.version=field(it,"version");
            e.size=field(it,"size"); e.publisher=field(it,"publisher");
            e.description=field(it,"description"); e.url=field(it,"url"); e.sha256=field(it,"sha256");
            e.title_id=field(it,"title_id"); e.region=field(it,"region");
            e.min_fw=field(it,"min_fw"); e.release=field(it,"release");
            e.cover_url=field(it,"cover_url");
            if(e.name.empty()) { err="ITEM "+std::to_string(i+1)+" HAS NO NAME"; return false; }
            tmp.push_back(e);
        }
    }
    out.swap(tmp);
    return true;
}

bool catalog_load_file(const char* path,std::vector<CatalogEntry>& out,std::string& err) {
    FILE* f=fopen(path,"rb");
    if(!f) { err="CATALOG FILE NOT FOUND"; return false; }
    std::string data; char buf[4096]; size_t n;
    while((n=fread(buf,1,sizeof buf,f))>0) { data.append(buf,n); if(data.size()>kMaxJson) break; }
    fclose(f);
    return catalog_parse(data,out,err);
}
