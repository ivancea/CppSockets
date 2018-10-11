#ifndef HTTP_H
#define HTTP_H

#include <vector>
#include <map>
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <windows.h>

#include "sockets.cpp"
#include "timers.h"
#include "strings.h"


namespace http{
    class request{
    protected:
        std::map<std::string,std::string> _fields;

    private:
        std::string _url;
        bool _secure;//HTTPS

    public:
        request():_secure(0){}
        bool isValidUrl()const{return isValidUrl(_url);}
        std::string getBaseUrl()const{
            for(int i=0; i<_url.size(); i++)
                if(_url[i]=='/') return _url.substr(0,i);
            return _url;
        }
        std::string getUrlDir()const{
            for(int i=0; i<_url.size(); i++)
                if(_url[i]=='/') return _url.substr(i,_url.size());
            return "/";
        }
        std::string getUrl()const{return _url;}
        void setUrl(std::string url){
            //url=tolower(url);
            if(url.substr(0,7)=="http://") url.erase(0,7);
            else if(url.substr(0,8)=="https://"){
                url.erase(0,8);
                _secure=true;
            }
            _url = (isValidUrl(url)?url:"");
        }
        bool isSecure(){return _secure;}
        void setSecure(bool secure){_secure = secure;}
        const std::map<std::string,std::string>& getFields()const{return _fields;}
        std::string getField(std::string field)const{
            field = tolower(field);
            for(auto it:_fields)
                if(it.first==field)
                    return it.second;
            return "";
        }
        void setField(std::string field, std::string value){
            if(field=="" || value=="") return;
            for(int i=0; i<field.size(); i++)
                if(field[i]=='\r'||field[i]=='\n'||field[i]==':'){
                    field.erase(i,field.size());
                    break;
                }
            for(int i=0; i<value.size(); i++)
                if(value[i]=='\r'||value[i]=='\n'){
                    value.erase(i,value.size());
                    break;
                }
            _fields[tolower(field)] = value;
        }


        static bool isValidUrl(std::string url){
            if(!contains(url,".")) return false;
            for(int i=0; i<url.size(); i++)
                if(url[i]=='.') break;
                else if(url[i]=='/') return false;
            for(int i=0; i<url.size(); i++)
                if(url[i]=='.' && i<url.size()-1)
                    if((url[i+1]<'a'||url[i+1]>'z')&&(url[i+1]<'A'&&url[i+1]>'Z')) return false;
            for(int i=0; i<url.size(); i++)
                if(url[i]=='\n' || url[i]=='\r') return false;
            return true;
        }
        virtual void parseHeader(std::string header){
            if(header.substr(header.size()-4,4)=="\r\n\r\n") header.erase(header.size()-4,4);
            else if(header.substr(header.size()-2,2)=="\r\n") header.erase(header.size()-2,2);
            std::vector<std::string> v = split(header,"\r\n");
            int i=0;
            if(v.size()>=1)
                if(contains(v[0],"HTTP/1.1",0)||contains(v[0],"POST ",0)||contains(v[0],"GET ",0))
                    ++i;
            for(; i<v.size(); i++){
                for(int j=0; j<v[i].size(); j++)
                    if(v[i][j]==':'){
                        setField(trim(v[i].substr(0,j)),trim(v[i].substr(j+1,v[i].size())));
                        break;
                    }
            }
        }
        virtual void clear(){
            _url="";
            _fields.clear();
        }
        virtual std::string toString()=0;
        virtual std::string getHeader()=0;

    };

    class GETRequest:public request{
        void makeRequest(){
            if(_fields["host"]=="")
                _fields["host"]=getBaseUrl();
        }

    public:
        GETRequest(){}
        GETRequest(string url){setUrl(url);}
        GETRequest(const GETRequest& r){
            setUrl(r.getUrl());
            _fields = r.getFields();
        }
        virtual std::string getHeader(){
            std::string temp;
            temp += "GET "+getUrlDir()+(isSecure()?" HTTP/1.1":" HTTP/1.1")+"\r\n"; // TODO: Fix
            for(auto it=_fields.begin(); it!=_fields.end(); it++)
                temp+=it->first+':'+it->second+"\r\n";
            return temp;
        }
        virtual string toString(){
            makeRequest();
            return getHeader()+"\r\n";
        }
    };

    class POSTRequest:public request{/*************************** POST ***************************/
        std::map<std::string,std::string> _body;

        void makeRequest(){
            if(_fields["host"]=="")
                _fields["host"]=getBaseUrl();
            if(_fields["content-type"]=="")
                _fields["content-type"]="application/x-www-form-urlencoded";
            //if(_fields["content-length"]=="" || !isNum(_fields["content-length"]) || parseString(_fields["content-length"])>getBody().size())
                _fields["content-length"]=to_string(getBody().size());
        }

    public:
        POSTRequest(){}
        POSTRequest(std::string url){setUrl(url);}
        POSTRequest(const POSTRequest& r){
            setUrl(r.getUrl());
            _fields = r.getFields();
            _body = r.getBodyFields();
        }
        std::string getBody()const{
            string t;
            for(auto it = _body.begin(); it!=_body.end(); it++){
                t+=it->first+(it->second==""?"":'='+it->second)+'&';
            }
            if(t.size()>0) t.erase(t.size()-1,1);
            return t;
        }
        const std::map<std::string,std::string>& getBodyFields()const{return _body;}
        void setBodyField(std::string field, std::string value=""){
            if(field=="") return;
            for(int i=0; i<field.size(); i++)
                if(field[i]=='='||field[i]=='&'){
                    field.erase(i,field.size());
                    break;
                }
            for(int i=0; i<value.size(); i++)
                if(value[i]=='='||value[i]=='&'){
                    value.erase(i,value.size());
                    break;
                }
            _body[field]=value;
        }
        std::string getBodyField(std::string field)const{
            for(auto it:_fields)
                if(it.first==field)
                    return it.second;
            return "";
        }
        void parseBodyField(std::string fieldAndValue){
            for(int i=0; i<fieldAndValue.size(); i++)
                if(fieldAndValue[i]=='='){
                    setBodyField(fieldAndValue.substr(0,i),fieldAndValue.substr(i+1,fieldAndValue.size()));
                }
        }
        virtual std::string getHeader(){
            std::string temp;
            temp += "POST "+getUrlDir()+(isSecure()?" HTTP/1.1":" HTTP/1.1")+"\r\n";
            for(auto it=_fields.begin(); it!=_fields.end(); it++)
                temp+=it->first+':'+it->second+"\r\n";
            return temp;
        }
        virtual std::string toString(){
            makeRequest();
            return getHeader()+"\r\n"+getBody();
        }
        virtual void clear(){
            request::clear();
            _body.clear();
        }
    };

    class RawPOSTRequest:public request{/*************************** RAW POST ***************************/
        std::string _body;

        void makeRequest(){
            if(_fields["host"]=="")
                _fields["host"]=getBaseUrl();
            if(_fields["content-type"]=="")
                _fields["content-type"]="application/x-www-form-urlencoded";
            //if(_fields["content-length"]=="" || !isNum(_fields["content-length"]) || parseString(_fields["content-length"])>getBody().size())
                _fields["content-length"]=to_string(getBody().size());
        }

    public:
        RawPOSTRequest(){}
        RawPOSTRequest(std::string url){setUrl(url);}
        RawPOSTRequest(const RawPOSTRequest& r){
            setUrl(r.getUrl());
            _fields = r.getFields();
            _body = r.getBody();
        }
        const std::string& getBody() const{
            return _body;
        }
        void setBody(std::string body){
            _body = body;
        }
        virtual std::string getHeader(){
            std::string temp;
            temp += "POST "+getUrlDir()+(isSecure()?" HTTP/1.1":" HTTP/1.1")+"\r\n";
            for(auto it=_fields.begin(); it!=_fields.end(); it++)
                temp+=it->first+':'+it->second+"\r\n";
            return temp;
        }
        virtual std::string toString(){
            makeRequest();
            return getHeader()+"\r\n"+getBody();
        }
        virtual void clear(){
            request::clear();
            _body.clear();
        }
    };

    class response:public request{
        int _code;
        std::string _body;
    public:
        response():_code(0){}
        response(int code):_code(code){}
        response(const response& r){
            _code = r.getCode();
            _body = r.getBody();
            _fields = r.getFields();
        }
        std::string getBody()const{return _body;}
        void setBody(string body){_body=body;}
        int getCode()const{return _code;}
        void setCode(int code){_code=code;}
        virtual std::string getHeader(){
            std::string temp;
            temp+="HTTP/1.1 "+to_string(_code)+" "+getCodeReason(_code)+"\r\n";
            for(auto it=_fields.begin(); it!=_fields.end(); it++)
                temp+=it->first+':'+it->second+"\r\n";
            return temp;
        }
        virtual std::string toString(){
            return getHeader()+"\r\n"+_body;
        }
        virtual void parseHeader(std::string header){
            for(int i=0; i<header.size()&&header.substr(i,2)!="\r\n"; i++)
                if(atoi(header.substr(i,3).c_str())>=100 && atoi(header.substr(i,3).c_str())<=999){
                    setCode(atoi(header.substr(i,3).c_str()));
                    break;
                }
            request::parseHeader(header);
        }
        static std::string getCodeReason(int code){
            switch(code){
                case 100:return "Continue";
                case 101:return "Switching Protocols";
                case 200:return "OK";
                case 201:return "Created";
                case 202:return "Accepted";
                case 203:return "Non-Authoritative Information";
                case 204:return "No Content";
                case 205:return "Reset Content";
                case 206:return "Partial Content";
                case 300:return "Multiple Choices";
                case 301:return "Moved Permanently";
                case 302:return "Found";
                case 303:return "See Other";
                case 304:return "Not Modified";
                case 305:return "Use Proxy";
                case 306:return "";
                case 307:return "Temporary Redirect";
                case 400:return "Bad Request";
                case 401:return "Unauthorized";
                case 402:return "Payment Required";
                case 403:return "Forbidden";
                case 404:return "Not Found";
                case 405:return "Method Not Allowed";
                case 406:return "Not Acceptable";
                case 407:return "Proxy Authentication Required";
                case 408:return "Request Timeout";
                case 409:return "Conflict";
                case 410:return "Gone";
                case 411:return "Length Required";
                case 412:return "Precondition Failed";
                case 413:return "Request Entity Too Large";
                case 414:return "Request URI Too Long";
                case 415:return "Unsupported Media Type";
                case 416:return "Resquested Range Not Satisfiable";
                case 417:return "Expectation Failed";
				case 451:return "Unavailable for legal reasons";
                case 500:return "Internal Server Error";
                case 501:return "Not Implemented";
                case 502:return "Bad Gateway";
                case 503:return "Service Unavaliable";
                case 504:return "Gateway Timeout";
                case 505:return "HTTP Version Not Supported";
                case -1: return "Error: Invalid url";
                case 1:return "Error: Invalid file";
                case 2:return "Error: Failed SSL charging BIO";
                case 3:return "Error: Failed SSL connection";
                case 4:return "Error: Failed SSL handshake";
                case 5:return "Error: Failed SSL receiving data";
                default:return "Error";
            }
        }

        virtual void clear(){
            request::clear();
            _code=0;
            _body="";
        }
    };

    response sendRequest(request &r){
        if(!r.isValidUrl()) return response(-1);
        static bool is_SSL_set = false;
        response t;
        std::string s, header, body;
        if(!r.isSecure()){
            if(!r.isValidUrl()) return response(6);
            TCPClient cl;
            if(!cl.connect(r.getBaseUrl(),80)) return response(7);
            if(!cl.send(r.toString())) return response(8);
            thSleep(100);
            for(int i=0; i<100; i++) if((s=cl.recv())!="") break; else if(i==99) return response(9); else thSleep(10);

            while(!contains(s,"\r\n\r\n")){
                header+=s;
                for(int i=0; i<100; i++) if((s=cl.recv())!="") break; else if(i==99) return response(10); else thSleep(10);
            }
            std::vector<std::string> v = split(s,"\r\n\r\n",2);
            header+=v[0];
            t.parseHeader(header);
            if(v.size()==2) s=v[1];
            else s.clear();
            bool chunked=false;
            try{
                t.getFields().at("content-length");
            }catch(std::out_of_range e){
                try{
                    chunked=t.getFields().at("transfer-encoding")=="chunked";
                }catch(std::out_of_range e){}
            }
            v.clear();
            if(chunked){
                int resto=0;
                while(true){
                    if(resto){
                        t.setBody(t.getBody()+s.substr(0,resto));
                        if(s.size()<resto){
                            resto-=s.size();
                            s.clear();
                        }else{
                            s.erase(0,2);
                            s.erase(0,resto);
                            resto=0;
                        }
                    }else{
                        v=split(s,"\r\n",2);
                        if(v.size()==2) s = v[1];
                        else s="";
                        int n = 0;
                        try{
                            n = stoi(v[0], nullptr, 16);
                        }catch(std::invalid_argument exc){
                            t.setCode(0);
                            return response(11);
                        }
                        if(n==0)
                            break;
                        t.setBody(t.getBody()+s.substr(0,n));
                        if(s.size()<n) resto = n-s.size();
                        else s.erase(0,2);
                        s.erase(0,n);
                    }
                    s+=cl.recv();
                }
            }else{
                int contentLength=0;
                try{
                    contentLength = atoi(t.getFields().at("content-length").c_str());
                }catch(std::out_of_range e){return response(12);}
                while(true){
                    if(contentLength==0) break;
                    t.setBody(t.getBody()+s.substr(0,contentLength));
                    if(s.size()<contentLength){
                        contentLength-=s.size();
                        s.clear();
                    }else break;
                    s+=cl.recv();
                }
            }
        }else{//HTTPS
            if(!is_SSL_set){
                CRYPTO_malloc_init();
                SSL_library_init();
                OpenSSL_add_all_algorithms();
            }

            SSL_CTX* ctx = SSL_CTX_new(SSLv23_client_method());
            SSL* ssl;
            BIO* bio = BIO_new_ssl_connect(ctx);
            if (bio == NULL) {
                SSL_CTX_free(ctx);
                return response(2);
            }
            BIO_get_ssl(bio, &ssl);
            SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);
            BIO_set_conn_hostname(bio, (r.getBaseUrl()+":443").c_str());

            if (BIO_do_connect(bio) <= 0) {
                BIO_free_all(bio);
                SSL_CTX_free(ctx);
                return response(3);
            }

            if (BIO_do_handshake(bio) <= 0) {
                BIO_free_all(bio);
                SSL_CTX_free(ctx);
                return response(4);
            }
            char buf[1024];
            memset(buf, 0, sizeof(buf));
            r.setField("connection","close");
            BIO_puts(bio, r.toString().c_str());
            while (1) {
                int x = BIO_read(bio, buf, sizeof(buf) - 1);
                if (x == 0) {
                   break;
                }
                else if (x < 0) {
                   if (!BIO_should_retry(bio)) {
                      BIO_free_all(bio);
                      SSL_CTX_free(ctx);
                      return response(5);
                   }
                }
                else{
                    buf[x] = 0;
                    if(!contains(buf,"\r\n\r\n")&&t.getCode()==0)
                        header+=buf;
                    else if(t.getCode()==0){
                        vector<string> v = split(buf,"\r\n\r\n",2);
                        t.parseHeader(header+v[0]);
                        if(v.size()==2) t.setBody(t.getBody()+v[1]);
                    }else{
                        t.setBody(t.getBody()+buf);
                    }
                }
            }
            BIO_free_all(bio);
            SSL_CTX_free(ctx);

         }
         return t;
    }

    response sendRequestAndBodyToFile(request &r, string name){
        if(!r.isValidUrl()) return response(-1);
        static bool is_SSL_set = false;
        response t;
        std::string s, header, body;
        std::ofstream f;
        f.open(name,ios::trunc|ios::binary);
        if(f.fail()) return response(1);

        if(!r.isSecure()){
            if(!r.isValidUrl()) return t;
            TCPClient cl;
            if(!cl.connect(r.getBaseUrl(),80)) return t;
            if(!cl.send(r.toString())) return t;
            thSleep(100);
            for(int i=0; i<100; i++) if((s=cl.recv())!="") break; else if(i==99) return t; else thSleep(10);

            while(!contains(s,"\r\n\r\n")){
                header+=s;
                for(int i=0; i<100; i++) if((s=cl.recv())!="") break; else if(i==99) return t; else thSleep(10);
            }
            std::vector<std::string> v = split(s,"\r\n\r\n",2);
            header+=v[0];
            if(v.size()==2) s=v[1];
            else s.clear();
            v = split(header,"\r\n");
            for(int i=0; i<v[0].size(); i++)
                if(atoi(v[0].substr(i,3).c_str())>=100 && atoi(v[0].substr(i,3).c_str())<=999){
                    t.setCode(atoi(v[0].substr(i,3).c_str()));
                    break;
                }
            for(int i=1; i<v.size(); i++){
                for(int j=0; j<v[i].size(); j++)
                    if(v[i][j]==':'){
                        t.setField(trim(v[i].substr(0,j)),trim(v[i].substr(j+1,v[i].size())));
                        break;
                    }
            }
            bool chunked=false;
            try{
                t.getFields().at("content-length");
            }catch(std::out_of_range e){
                try{
                    chunked=t.getFields().at("transfer-encoding")=="chunked";
                }catch(std::out_of_range e){}
            }
            v.clear();
            int totalBytes=0;
            if(chunked){
                int resto=0;
                while(true){
                    if(resto){
                        f << s.substr(0,resto);
                        totalBytes+=s.substr(0,resto).size();
                        if(s.size()<resto){
                            resto-=s.size();
                            s.clear();
                        }else{
                            s.erase(0,2);
                            s.erase(0,resto);
                            resto=0;
                        }
                    }else{
                        v=split(s,"\r\n",2);
                        if(v.size()==2) s = v[1];
                        else s="";
                        int n = 0;
                        try{
                            n = stoi(v[0], nullptr, 16);
                        }catch(invalid_argument exc){
                            t.setCode(0);
                            return response(11);
                        }
                        if(n==0) break;
                        f << s.substr(0,n);
                        totalBytes+=s.substr(0,n).size();
                        if(s.size()<n) resto = n-s.size();
                        else s.erase(0,2);
                        s.erase(0,n);
                    }
                    s+=cl.recv();
                }
            }else{
                int contentLength=0;
                try{
                    contentLength = atoi(t.getFields().at("content-length").c_str());
                }catch(out_of_range e){return t;}
                while(true){
                    if(contentLength==0) break;
                    f << s.substr(0,contentLength);
                    totalBytes+=s.substr(0,contentLength).size();
                    if(s.size()<contentLength){
                        contentLength-=s.size();
                        s.clear();
                    }else break;
                    s+=cl.recv();
                }
            }
            t.setBody("<Body fileName=\""+name+"\" size=\""+ to_string(totalBytes) +"\">");
        }else{//HTTPS
            if(!is_SSL_set){
                CRYPTO_malloc_init();
                SSL_library_init();
                OpenSSL_add_all_algorithms();
            }

            SSL_CTX* ctx = SSL_CTX_new(SSLv23_client_method());
            SSL* ssl;
            BIO* bio = BIO_new_ssl_connect(ctx);
            if (bio == NULL) {
                SSL_CTX_free(ctx);
                return response(2);
            }
            BIO_get_ssl(bio, &ssl);
            SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);
            BIO_set_conn_hostname(bio, (r.getBaseUrl()+":443").c_str());

            if (BIO_do_connect(bio) <= 0) {
                BIO_free_all(bio);
                SSL_CTX_free(ctx);
                return response(3);
            }

            if (BIO_do_handshake(bio) <= 0) {
                BIO_free_all(bio);
                SSL_CTX_free(ctx);
                return response(4);
            }
            char buf[1024];
            memset(buf, 0, sizeof(buf));
            r.setField("connection","close");
            BIO_puts(bio, r.toString().c_str());
            int totalBytes=0, cLength = -1;
            while (totalBytes!=cLength) {
                int x = BIO_read(bio, buf, sizeof(buf) - 1);
                if (x < 0) {
                   if (!BIO_should_retry(bio)) {
                      BIO_free_all(bio);
                      SSL_CTX_free(ctx);
                      return response(5);
                   }
                }else{
                    buf[x] = 0;
                    string sBuf = string(buf,1024);
                    size_t pos;
                    if((pos=sBuf.find("\r\n\r\n"))==sBuf.npos&&t.getCode()==0)
                        header+=sBuf;
                    else if(t.getCode()==0){
                        t.parseHeader(header+sBuf.substr(0,pos));
                        f << sBuf.substr(pos+4,x-pos-4);
                        totalBytes+=x-pos-4;
                        cLength = atoi(t.getField("content-length").c_str());
                    }else{
                        f.write(buf,x);
                        totalBytes+=x;
                    }
                }
            }
            BIO_free_all(bio);
            SSL_CTX_free(ctx);
            t.setBody("<Body fileName=\""+name+"\" size=\""+ to_string(totalBytes) +"\">");
        }
        return t;
    }
}

#endif
