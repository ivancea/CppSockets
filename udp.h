#ifndef UDP_H
#define UDP_H

#include <string>
#include <windows.h>
#include <ws2tcpip.h>

struct UDP__WSA{
    UDP__WSA(){
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2,2), &wsaData);
    }
    ~UDP__WSA(){
        WSACleanup();
    }
}UDP__WSA__;

bool validIp(std::string ip){
    if(ip.size()<7||ip.size()>15) return false;
    ip+='.';
    for(int i=0; i<ip.size();){
        if(ip[i]=='.'){
            if(i==0) return false;
            int n = atoi(ip.substr(0,i).c_str());
            if(n<0||n>255) return false;
            ip.erase(0,i+1);
            i=0;
        }else ++i;
    }

    return true;
}

unsigned int resolveAddress(std::string addr){
    if(addr=="255.255.255.255")
        return INADDR_BROADCAST;
    unsigned int ip=0;
    if((ip=inet_addr(addr.c_str()))!=INADDR_NONE)
        return ip;
    addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    addrinfo* result = NULL;
    if (getaddrinfo(addr.c_str(), NULL, &hints, &result) == 0)
        if (result){
            ip = ((sockaddr_in*)result->ai_addr)->sin_addr.s_addr;
            freeaddrinfo(result);
            return ip;
        }
    return 0;
}

int UDPSend(std::string ip, unsigned short port, std::string msg){
    if(!validIp(ip)) return 1;
    int iResult;

    SOCKET SendSocket = INVALID_SOCKET;
    sockaddr_in RecvAddr;

    SendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(SendSocket == INVALID_SOCKET){
        WSACleanup();
        return 3;
    }
    RecvAddr.sin_family = AF_INET;
    RecvAddr.sin_port = htons(port);
    RecvAddr.sin_addr.s_addr = resolveAddress(ip.c_str());
    if(RecvAddr.sin_addr.s_addr == 0)
        return false;
    iResult = sendto(SendSocket, msg.c_str(), msg.size(), 0, (SOCKADDR *) &RecvAddr, sizeof (RecvAddr));
    if(iResult == SOCKET_ERROR){
        closesocket(SendSocket);
        WSACleanup();
        return 4;
    }

    iResult = closesocket(SendSocket);
    if(iResult == SOCKET_ERROR){
        return 5;
    }

    return 0;
}

bool UDPRecv(std::string *msg, std::string *ip, int port, bool blocking=false, size_t maxSize=1024){
    if(maxSize==0 || (msg==nullptr && ip==nullptr)){
        return false;
    }

    SOCKET RecvSocket;
    sockaddr_in RecvAddr;
    sockaddr_in SenderAddr;

    int SenderAddrSize = sizeof(SenderAddr);

    char RecvBuf[maxSize];
    int BufLen = maxSize;

    RecvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(RecvSocket == INVALID_SOCKET){
        return false;
    }
    RecvAddr.sin_family = AF_INET;
    RecvAddr.sin_port = htons(port);
    RecvAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(RecvSocket, (SOCKADDR *) & RecvAddr, sizeof (RecvAddr)) != 0){
        return false;
    }

    if(!blocking){
        DWORD nonBlocking = 1;
        if(ioctlsocket(RecvSocket,FIONBIO,&nonBlocking )!=0){
            return false;
        }
    }

    if((BufLen=recvfrom(RecvSocket, RecvBuf, BufLen, 0, (SOCKADDR*)&SenderAddr, &SenderAddrSize))==SOCKET_ERROR){
        return false;
    }

    closesocket(RecvSocket);

    if(ip!=nullptr)
        *ip = inet_ntoa(SenderAddr.sin_addr);
    if(msg!=nullptr)
        *msg = std::string(RecvBuf,BufLen);

    return true;
}

#endif // UDP_H
