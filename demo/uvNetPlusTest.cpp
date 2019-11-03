// uvHttpClient.cpp : 定义控制台应用程序的入口点。
//

#include <stdio.h>
#include <tchar.h>
#include <windows.h>
#include "uvnetplus.h"
#include "Log.h"
#include <string>
#include <thread>
using namespace std;
using namespace uvNetPlus;

const int svrport = 8080;

//////////////////////////////////////////////////////////////////////////
////////////       TCP客户端         /////////////////////////////////////

struct clientData {
    int tid;
    int finishNum;
};

void OnClientReady(CTcpSocket* skt){
    clientData* data = (clientData*)skt->userData;
    Log::debug("client %d ready", data->tid);
}

void OnClientConnect(CTcpSocket* skt, string err){
    clientData* data = (clientData*)skt->userData;
    if(err.empty()){
        char buff[1024] = {0};
        sprintf(buff, "client%d %d",data->tid, data->finishNum+1);
        skt->Send(buff, strlen(buff));
        skt->Send(" 111", 4);
    } else {
        Log::error("client%d connect err: %s", data->tid, err.c_str());
        skt->Delete();
    }
}

void OnClientRecv(CTcpSocket* skt, char *data, int len){
    clientData* c = (clientData*)skt->userData;
    c->finishNum++;
    Log::debug("client%d recv[%d]: %s", c->tid, c->finishNum, data);
#if 1
    skt->Delete();
#else
    if(c->finishNum < c->tid) {
        char buff[1024] = {0};
        sprintf(buff, "client%d %d",c->tid, c->finishNum+1);
        skt->Send(buff, strlen(buff));
        skt->Send(" AAA", 4);
    } else {
        skt->Delete();
    }
#endif
}

void OnClientDrain(CTcpSocket* skt){
    clientData* data = (clientData*)skt->userData;
    Log::debug("client%d drain", data->tid);
}

void OnClientClose(CTcpSocket* skt){
    clientData* data = (clientData*)skt->userData;
    Log::debug("client%d close", data->tid);
    delete data;
}

void OnClientEnd(CTcpSocket* skt){
    clientData* data = (clientData*)skt->userData;
    Log::debug("client%d end", data->tid);
}

void OnClientError(CTcpSocket* skt, string err){
    clientData* data = (clientData*)skt->userData;
    Log::error("client%d error: %s ", data->tid, err.c_str());
}

void testClient()
{
    CNet* net = CNet::Create();
    for (int i=0; i<100; i++) {
        clientData* data = new clientData;
        data->tid = i+1;
        data->finishNum = 0;
        CTcpSocket* client = CTcpSocket::Create(net, (void*)data);
        client->OnRecv = OnClientRecv;
        client->OnDrain = OnClientDrain;
        client->OnCLose = OnClientClose;
        client->OnEnd = OnClientEnd;
        client->OnError = OnClientError;
        client->OnConnect = OnClientConnect;
        client->Connect("127.0.0.1", svrport );
    }
}

//////////////////////////////////////////////////////////////////////////
/////////////     tcp客户端连接池      ///////////////////////////////////

static void OnConnectRequest(CTcpRequest* req, CTcpSocket* skt, bool connected){
    clientData* data = (clientData*)req->usr;
    delete req;
    if(!connected) {
        //新建连接
        skt->OnReady   = OnClientReady;
        skt->OnConnect = OnClientConnect;
        skt->OnRecv    = OnClientRecv;
        skt->OnDrain   = OnClientDrain;
        skt->OnCLose   = OnClientClose;
        skt->OnEnd     = OnClientEnd;
        skt->OnError   = OnClientError;
        skt->autoRecv  = true;
        skt->copy      = true;
        skt->userData  = data;
    } else {
        //连接池取出的连接
        clientData* ddd = (clientData*)skt->userData;
        data->finishNum = ddd->finishNum;
        skt->userData  = data;
        skt->Send("123456789", 9);
        skt->Send("987654321", 9);
    }
}

static void testTcpPool(){
    std::thread t([&](){
        CNet* net = CNet::Create();
        CTcpConnPool* pool = CTcpConnPool::Create(net, OnConnectRequest);
        pool->maxConns = 10;
        pool->maxIdle = 10;
        for (int i=0; i<100; i++) {
            Log::debug("new request %d", i);
            clientData* data = new clientData;
            data->tid = i+1;
            data->finishNum = 0;
            pool->Request("127.0.0.1", svrport, "", data, true, true);
        }
    });
    t.detach();
}

//////////////////////////////////////////////////////////////////////////

struct sclientData {
    int id;
};

void OnSvrCltRecv(CTcpSocket* skt, char *data, int len){
    sclientData *c = (sclientData*)skt->userData;
    Log::debug("server client%d recv: %s ", c->id, data);
    char buff[1024] = {0};
    sprintf(buff, "server client%d answer",c->id);
    skt->Send(buff, strlen(buff));
    skt->Send("123456", 6);
}

void OnSvrCltDrain(CTcpSocket* skt){
    sclientData *c = (sclientData*)skt->userData;
    Log::debug("server client%d drain", c->id);
}

void OnSvrCltClose(CTcpSocket* skt){
    sclientData *c = (sclientData*)skt->userData;
    Log::debug("server client%d close", c->id);
    delete c;
}

void OnSvrCltEnd(CTcpSocket* skt){
    sclientData *c = (sclientData*)skt->userData;
    Log::debug("server client%d end", c->id);
}

void OnSvrCltError(CTcpSocket* skt, string err){
    sclientData *c = (sclientData*)skt->userData;
    Log::error("server client%d error:%s ",c->id, err.c_str());
}

void OnTcpConnection(CTcpServer* svr, std::string err, CTcpSocket* client) {
    if(!err.empty()){
        Log::error("tcp server error: %s ", err.c_str());
        return;
    }
    Log::debug("tcp server recv new connection");
    static int sid = 1;
    sclientData *c = new sclientData();
    c->id = sid++;
    client->userData = c;
    client->OnRecv = OnSvrCltRecv;
    client->OnDrain = OnSvrCltDrain;
    client->OnCLose = OnSvrCltClose;
    client->OnEnd = OnSvrCltEnd;
    client->OnError = OnSvrCltError;
}

void OnTcpSvrClose(CTcpServer* svr, std::string err) {
    if(err.empty()) {
        Log::debug("tcp server closed");
    } else {
        Log::error("tcp server closed with error: %s", err.c_str());
    }
}

void OnTcpError(CTcpServer* svr, std::string err) {
    Log::error("tcp server error: %s", err.c_str());
}

void OnTcpListen(CTcpServer* svr, std::string err) {
    if(err.empty()) {
        Log::debug("tcp server start success");
        //testClient();
        testTcpPool();
    } else {
        Log::error("tcp server start failed: %s ", err.c_str());
    }
}

void testServer()
{
    CNet* net = CNet::Create();
    CTcpServer* svr = CTcpServer::Create(net, OnTcpConnection);
    svr->OnClose = OnTcpSvrClose;
    svr->OnError = OnTcpError;
    svr->OnListen = OnTcpListen;
    svr->Listen("0.0.0.0", svrport);
}

//////////////////////////////////////////////////////////////////////////
static void OnHttpError(Http::CHttpRequest *request, string error) {

}
static void OnHttpResponse(Http::CHttpRequest *request, Http::CIncomingMessage* response) {
    Log::debug("%d %s", response->statusCode, response->statusMessage.c_str());
    Log::debug(response->rawHeaders.c_str());
    Log::debug(response->content.c_str());
    if(response->complete)
        request->Delete();
}

void testHttpRequest()
{
    CNet* net = CNet::Create();
    Http::CHttpClientEnv *http = new Http::CHttpClientEnv(net, 10, 10);
    for (int i=0; i<100; i++) {
         Http::CHttpRequest* req = http->Request();
         req->OnError    = OnHttpError;
         req->OnResponse = OnHttpResponse;
         //req->host = "www.baidu.com";
         req->host = "127.0.0.1";
         req->protocol = PROTOCOL::HTTP;
         //req->method = Http::METHOD::GET;
         req->method = Http::METHOD::POST;
         //req->path = "/s?wd=http+content+len";
         req->path = "/imageServer/image?name=111111.jpg&type=1";
         req->version = Http::VERSION::HTTP1_1;
         req->SetHeader("myheader","????????\0");
         string content = "123456789\0";
         req->End(content.c_str(), content.length());
    }
}

static void OnHttpRequest(Http::CHttpServer *server, Http::CIncomingMessage *request, Http::CHttpResponse *response) {
    Log::debug("%d %s %d", request->method, request->url.c_str(), request->version);
    for(auto &h:request->headers) {
        Log::debug("%s: %s", h.first.c_str(), h.second.c_str());
    }
    Log::debug("Content len: %d", request->contentLen);
    Log::debug("Chunked: %d", request->chunked);
    Log::debug("Keep alive: %d", request->keepAlive);
    Log::debug("Complete: %d", request->complete);
    Log::debug(request->rawHeaders.c_str());

    response->statusCode = 200;
    response->SetHeader("myheader", "1111111111111");
    response->End("123456789",9);
}

void testHttpServer()
{
    CNet* net = CNet::Create();
    Http::CHttpServer *svr = Http::CHttpServer::Create(net);
    svr->OnRequest = OnHttpRequest;
    svr->Listen("0.0.0.0", svrport);
}

//////////////////////////////////////////////////////////////////////////

int _tmain(int argc, _TCHAR* argv[])
{
    Log::open(Log::Print::both, Log::Level::debug, "./log.txt");

    //testServer();
    testHttpServer();
    //testHttpRequest();

	Sleep(INFINITE);
	return 0;
}

