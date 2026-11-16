#include "http_download.h"
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

// ---------------------------------------------------------------- net layer
namespace net {
static int openConnect(const std::string& host, const std::string& port, std::string& err) {
    addrinfo hints; memset(&hints,0,sizeof hints);
    hints.ai_family=AF_INET; hints.ai_socktype=SOCK_STREAM;
    addrinfo* res=nullptr;
    if(getaddrinfo(host.c_str(),port.c_str(),&hints,&res)!=0||!res) { err="COULD NOT RESOLVE HOST"; return -1; }
    int fd=socket(res->ai_family,res->ai_socktype,res->ai_protocol);
    if(fd<0) { freeaddrinfo(res); err="SOCKET FAILED"; return -1; }
    fcntl(fd,F_SETFL,fcntl(fd,F_GETFL,0)|O_NONBLOCK);
    int rc=connect(fd,res->ai_addr,res->ai_addrlen);
    freeaddrinfo(res);
    if(rc<0&&errno!=EINPROGRESS) { close(fd); err="CONNECT FAILED"; return -1; }
    return fd;
}
static int connectState(int fd) {            // 1 connected, 0 pending, -1 failed
    fd_set w; FD_ZERO(&w); FD_SET(fd,&w); timeval tv={0,0};
    if(select(fd+1,nullptr,&w,nullptr,&tv)<=0) return 0;
    int e=0; socklen_t l=sizeof e; getsockopt(fd,SOL_SOCKET,SO_ERROR,&e,&l);
    return e?-1:1;
}
static long sendSome(int fd,const char* b,size_t n) {   // -2 would block, -1 error
#ifdef MSG_NOSIGNAL
    ssize_t r=send(fd,b,n,MSG_NOSIGNAL);
#else
    ssize_t r=send(fd,b,n,0);
#endif
    if(r>=0) return (long)r;
    return (errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR)?-2:-1;
}
static long recvSome(int fd,char* b,size_t n) {         // 0 EOF, -2 would block, -1 error
    ssize_t r=recv(fd,b,n,0);
    if(r>=0) return (long)r;
    return (errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR)?-2:-1;
}
static void closeSock(int fd) { close(fd); }
} // namespace net
// ---------------------------------------------------------------------------

using clk = std::chrono::steady_clock;
static const int kConnectSecs=10, kIdleSecs=20;

HttpDownload::HttpDownload(): state_(Idle), phase_(Connect), fd_(-1), file_(nullptr),
    sent_(0), received_(0), total_(0), last_(clk::now()) {}
HttpDownload::~HttpDownload() { cleanup(state_==Running); }

void HttpDownload::cleanup(bool removePart) {
    if(fd_>=0) { net::closeSock(fd_); fd_=-1; }
    if(file_) { fclose(file_); file_=nullptr; }
    if(removePart && !part_.empty()) remove(part_.c_str());
}
HttpDownload::State HttpDownload::fail(const char* why) {
    cleanup(true); msg_=why; state_=Failed; return state_;
}

static bool isHex64(const std::string& s) {
    if(s.size()!=64) return false;
    for(char c:s) if(!isxdigit((unsigned char)c)) return false;
    return true;
}

bool HttpDownload::begin(const std::string& url,const std::string& outPath,
                         const std::string& expectedSha256,std::string& err) {
    if(state_==Running) { err="A DOWNLOAD IS ALREADY RUNNING"; return false; }
    cleanup(false);
    state_=Idle; msg_.clear(); received_=0; total_=0; sent_=0; hdr_.clear();
    sha_=Sha256();
    if(url.compare(0,7,"http://")!=0) {
        err=(url.compare(0,8,"https://")==0)?"HTTPS NOT SUPPORTED IN THIS BUILD":"ONLY HTTP URLS ARE SUPPORTED";
        return false;
    }
    if(!isHex64(expectedSha256)) { err="A VALID SHA-256 IS REQUIRED"; return false; }
    expected_.clear(); for(char c:expectedSha256) expected_+=(char)tolower((unsigned char)c);

    std::string rest=url.substr(7), hostport, path="/";
    size_t slash=rest.find('/');
    if(slash==std::string::npos) hostport=rest; else { hostport=rest.substr(0,slash); path=rest.substr(slash); }
    std::string host=hostport, port="80";
    size_t colon=hostport.rfind(':');
    if(colon!=std::string::npos) { host=hostport.substr(0,colon); port=hostport.substr(colon+1); }
    if(host.empty()||port.empty()||path.find_first_of(" \r\n")!=std::string::npos) { err="BAD URL"; return false; }

    out_=outPath; part_=outPath+".part";
    req_="GET "+path+" HTTP/1.0\r\nHost: "+hostport+"\r\nUser-Agent: PS4Market/3\r\nAccept: */*\r\nConnection: close\r\n\r\n";
    fd_=net::openConnect(host,port,err);
    if(fd_<0) return false;
    phase_=Connect; state_=Running; last_=clk::now();
    return true;
}

bool HttpDownload::parseHeaders() {
    int code=0;
    if(sscanf(hdr_.c_str(),"HTTP/%*d.%*d %d",&code)!=1) { fail("BAD SERVER RESPONSE"); return false; }
    if(code!=200) {
        char b[48]; snprintf(b,sizeof b,"SERVER REPLIED %d",code); fail(b); return false;
    }
    size_t pos=hdr_.find("\r\n");
    while(pos!=std::string::npos) {
        size_t s=pos+2, e=hdr_.find("\r\n",s);
        if(e==std::string::npos||e==s) break;
        std::string line=hdr_.substr(s,e-s), low;
        for(char c:line) low+=(char)tolower((unsigned char)c);
        if(low.compare(0,15,"content-length:")==0) total_=strtoull(line.c_str()+15,nullptr,10);
        if(low.compare(0,18,"transfer-encoding:")==0 && low.find("chunked")!=std::string::npos) {
            fail("CHUNKED ENCODING NOT SUPPORTED"); return false;
        }
        pos=e;
    }
    if(total_>kMaxBytes) { fail("FILE TOO LARGE"); return false; }
    file_=fopen(part_.c_str(),"wb");
    if(!file_) { fail("CANNOT OPEN OUTPUT FILE"); return false; }
    return true;
}

bool HttpDownload::writeBody(const char* d,size_t n) {
    if(!n) return true;
    if((total_&&received_+n>total_)||received_+n>kMaxBytes) { fail("BAD SERVER RESPONSE"); return false; }
    if(fwrite(d,1,n,file_)!=n) { fail("DISK WRITE FAILED"); return false; }
    sha_.update(d,n); received_+=n;
    return true;
}

bool HttpDownload::consume(const char* d,size_t n) {
    if(phase_==Headers) {
        hdr_.append(d,n);
        size_t end=hdr_.find("\r\n\r\n");
        if(end==std::string::npos) {
            if(hdr_.size()>16384) { fail("BAD SERVER RESPONSE"); return false; }
            return true;
        }
        std::string body=hdr_.substr(end+4);
        hdr_.resize(end+2);                 // keep one CRLF so the last header line terminates
        if(!parseHeaders()) return false;
        phase_=Body;
        return writeBody(body.data(),body.size());
    }
    return writeBody(d,n);
}

HttpDownload::State HttpDownload::finish() {
    if(phase_!=Body) return fail("BAD SERVER RESPONSE");
    if(total_&&received_!=total_) return fail("CONNECTION CLOSED EARLY");
    if(fclose(file_)!=0) { file_=nullptr; return fail("DISK WRITE FAILED"); }
    file_=nullptr;
    if(sha_.finalHex()!=expected_) return fail("HASH MISMATCH - FILE DELETED");
    if(fd_>=0) { net::closeSock(fd_); fd_=-1; }
    if(rename(part_.c_str(),out_.c_str())!=0) return fail("COULD NOT SAVE FILE");
    msg_="VERIFIED"; state_=Done; return state_;
}

HttpDownload::State HttpDownload::step() {
    if(state_!=Running) return state_;
    auto now=clk::now();
    auto idle=[&](int secs){ return std::chrono::duration_cast<std::chrono::seconds>(now-last_).count()>=secs; };
    if(phase_==Connect) {
        int c=net::connectState(fd_);
        if(c<0) return fail("CONNECTION REFUSED OR FAILED");
        if(c==0) return idle(kConnectSecs)?fail("CONNECTION TIMED OUT"):state_;
        phase_=Send; last_=now;
    }
    if(phase_==Send) {
        while(sent_<req_.size()) {
            long r=net::sendSome(fd_,req_.data()+sent_,req_.size()-sent_);
            if(r==-1) return fail("NETWORK ERROR");
            if(r==-2) break;
            sent_+=(size_t)r;
        }
        if(sent_<req_.size()) return idle(kIdleSecs)?fail("TIMED OUT"):state_;
        phase_=Headers; last_=now;
    }
    static char buf[65536];
    for(int i=0;i<8;i++) {                  // at most 512 KB per frame
        long r=net::recvSome(fd_,buf,sizeof buf);
        if(r==-2) return idle(kIdleSecs)?fail("TIMED OUT"):state_;
        if(r<0) return fail("NETWORK ERROR");
        last_=now;
        if(r==0) return finish();
        if(!consume(buf,(size_t)r)) return state_;
    }
    return state_;
}
