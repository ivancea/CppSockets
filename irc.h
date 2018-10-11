#include "sockets.cpp"
#include "timers.h"
#include <map>

class irc{
private:
    TCPClient _cl;
    string  _user, _pass, _nick, _url, _channel;
    string _buffer;
    int _port;
public:
    irc(){}
    irc(string user, string pass, string nick, string url, int port=6667){
        start(user, pass, nick, url, port);
    }
    bool start(string user, string pass, string nick, string url, int port=6667){
        string s;
        if(!_cl.connect(url, port)) return false;
        _user = user;
        _pass = pass;
        _nick = nick;
        _url = url;
        if(pass!="")
            _cl.send("PASS " + pass + "\r\n");
        if(nick!="")
            _cl.send("NICK " + nick + "\r\n");
        if(user!="")
            _cl.send("USER " + user + " " + user + " " + url + " :" + nick + "\r\n");
        return true;
    }
    void connect(string channel){
        if(channel == "")
            return;
        _channel = channel;
        if(_channel[0]!='#')
            _channel = "#" + _channel;
        _cl.send("JOIN " + _channel + "\r\n");
        _cl.send("MODE " + _channel + "\r\n");
    }
    void sendMsg(string msg){
        if(_channel=="") return;
        _cl.send("PRIVMSG " + _channel + " :" + msg + "\r\n");
    }
    void send(string msg){
        _cl.send(msg + "\r\n");
    }
    bool receive(string& str){
        _buffer += _cl.recv();
        size_t p;
        if((p=_buffer.find("\r\n"))!=string::npos){
            str = _buffer.substr(0,p);
            _buffer.erase(0, p+2);
            return true;
        }
        return false;
    }
    string getUrl(){return _url;}
    string getChannel(){return _channel;}
    string getNick(){return _nick;}
    TCPClient* getClient(){return &_cl;}
};

class Irc{
private:
    TCPClient _cl;
    string _buffer;
    string  _url;
    unsigned short _port;

    bool _autoPong;

    void* _privMsgCallbackData;
    void (*_privMsgCallback)(Irc &irc, string receiver, string sender, string msg, void* data);
    void* _channelMsgCallbackData;
    void (*_channelMsgCallback)(Irc &irc, string channel, string sender, string msg, void* data);
    void* _userModeCallbackData;
    void (*_userModeCallback)(Irc &irc, string channel, string sender, string target, string mode, void* data);

public:
    Irc():_port(6667),_autoPong(true),_privMsgCallback(nullptr),_channelMsgCallback(nullptr){}
    Irc(string user, string pass, string nick, string url, unsigned short port=6667)
    :_port(6667),_autoPong(true),_privMsgCallback(nullptr),_channelMsgCallback(nullptr), _userModeCallback(nullptr){
        start(user, pass, nick, url, port);
    }

    string getUrl(){return _url;}
    unsigned short getPort(){return _port;}
    bool isAutoPongEnabled(){return _autoPong;}
    TCPClient& getClient(){return _cl;}

    void setAutoPong(bool autoPong){_autoPong = autoPong;}

    void setPrivMsgCallback(void (*privMsgCallback)(Irc &irc, string receiver, string sender, string msg, void* data), void* data = nullptr){
        _privMsgCallbackData = data;
        _privMsgCallback = privMsgCallback;
    }
    void setChannelMsgCallback(void (*channelMsgCallback)(Irc &irc, string channel, string sender, string msg, void* data), void* data = nullptr){
        _channelMsgCallbackData = data;
        _channelMsgCallback = channelMsgCallback;
    }
    void setUserModeCallback(void (*userModeCallback)(Irc &irc, string channel, string sender, string target, string mode, void* data), void* data = nullptr){
        _userModeCallbackData = data;
        _userModeCallback = userModeCallback;
    }

    bool start(string user, string pass, string nick, string url, unsigned short port=6667){
        string s;
        if(!_cl.connect(url, port)) return false;
        _url = url;
        if(pass!="")
            _cl.send("PASS " + pass + "\r\n");
        if(nick!="")
            _cl.send("NICK " + nick + "\r\n");
        if(user!="")
            _cl.send("USER " + user + " " + user + " " + url + " :" + nick + "\r\n");
        return true;
    }
    void connect(string channel){
        if(channel == "")
            return;
        if(channel[0]!='#')
            channel = "#" + channel;
        _cl.send("JOIN " + channel + "\r\n");
        _cl.send("MODE " + channel + "\r\n");
    }

    void sendChannelMsg(string channelOrUser, string msg){
        if(channelOrUser=="") return;
        _cl.send("PRIVMSG " + channelOrUser + " :" + msg + "\r\n");
    }
    void send(string msg){
        _cl.send(msg + "\r\n");
    }

    bool receive(string& str, bool enableCallbacks = true){
        _buffer += _cl.recv();
        size_t p;
        if((p=_buffer.find("\r\n"))!=string::npos){
            str = _buffer.substr(0,p);
            _buffer.erase(0, p+2);

            if(str.substr(0,4)=="PING"){
                if(_autoPong){
                    _cl.send("PONG "+str.substr(5, str.size())+"\r\n");
                }
                return true;
            }
            if(enableCallbacks && str[0]==':'){
                size_t pos = str.find('!');
                if(pos==string::npos)
                    return true;
                string sender = str.substr(1,pos-1);
                if(sender.find('.')!=string::npos)
                    return true;
                pos = str.find(' ');
                if(pos==string::npos)
                    return true;
                string command = str.substr(pos+1,str.size());
                if(command[0]>='0' && command[0]<='9')
                    return true;
                pos = command.find(' ');
                if(pos==string::npos)
                    return true;
                string args = command.substr(pos+1,command.size());
                command.erase(pos,command.size());
                if(command == "PRIVMSG"){
                    pos = args.find(':');
                    if(pos==string::npos)
                        return true;
                    if(args[0]=='#' && _channelMsgCallback!=nullptr){
                        _channelMsgCallback(*this, args.substr(0,pos-1), sender, args.substr(pos+1,args.size()), _channelMsgCallbackData);
                    }else if(args[0]!='#' && _privMsgCallback!=nullptr){
                        _privMsgCallback(*this, args.substr(0,pos-1), sender, args.substr(pos+1,args.size()), _privMsgCallbackData);
                    }
                    return true;
                }else if(command == "MODE" && _userModeCallback!=nullptr){
                    char channel[50], mode[50], target[50];
                    sscanf(args.c_str(), "%s %s %s", channel, mode, target);
                    _userModeCallback(*this, channel, sender, target, mode, _userModeCallbackData);
                }
            }

            return true;
        }
        return false;
    }

    map<string,bool> getNames(string channel, unsigned int msTimeout=0){
        map<string,bool> m;
        if(channel.size()==0) return m;
        if(channel[0]!='#') channel = "#"+channel;
        string t;
        send("NAMES " + channel);
        clock_t cl=clock();

        string buffer;
        size_t pos;
        do{
            thSleep(1);
            buffer += _cl.recv();
            if(msTimeout!=0 && ((clock()-cl)*1000)/CLOCKS_PER_SEC > msTimeout){
                _buffer += buffer;
                return m;
            }
        }while((pos=buffer.find(" 353 "))==string::npos || buffer.find("\r\n", pos)==string::npos);

        size_t first = buffer.rfind("\r\n", pos);
        if(first!=string::npos)
            _buffer += buffer.substr(0, first+2);
        first = buffer.find("\r\n", pos);
        if(first != buffer.size()-2)
            _buffer += buffer.substr(first+2, buffer.size());
        buffer.erase(first,buffer.size());

        first = buffer.find(':', pos)+1;
        do{
            pos = buffer.find(' ', first);
            bool jump = buffer[first]=='@' || buffer[first]=='+';
            if(pos==string::npos){
                m[buffer.substr(first+jump, buffer.size())] = (buffer[first]=='@');
            }else{
                m[buffer.substr(first+jump, pos-first-jump)] = (buffer[first]=='@');
            }
            first = pos+1;
        }while(pos!=string::npos);
        return m;
    }
};
