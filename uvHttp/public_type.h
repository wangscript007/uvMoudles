#ifndef _PUBLIC_TYPE_
#define _PUBLIC_TYPE_

#ifndef C99
#include "stdbool.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define OPTIONS "OPTIONS"
#define HEAD    "HEAD"
#define GET     "GET"
#define POST    "POST"
#define PUT     "PUT"
#define DELETE  "DELETE"
#define TRACE   "TRACE"
#define CONNECT "CONNECT"

typedef struct _config_
{
	int  keep_alive_secs;
	int  max_sockets;
	int  max_free_sockets;
}config_t;

typedef struct _http_ http_t;
typedef struct _response_ response_t;

typedef struct _request_ {
	const char* url;		    //一个完整的http地址("http://"可以省略)
	const char* method;		    //http请求方法
	const char* host;			//http请求优先使用host作为目标地址，当host为空时从url中获取目标地址
	int         keep_alive;     //0表示Connection为close，非0表示keep-alive
	int         chunked;        //POST使用 0表示不使用chuncked，非0表示Transfer-Encoding: "chunked"
	int         content_length; //POST时需要标注内容的长度
	void*       user_data;      //设置用户数据
	response_t* res;
}request_t;

typedef struct _response_ {
	const char* version;
	int         status;
	int         keep_alive;     //0表示Connection为close，非0表示keep-alive
	int         chunked;        //POST使用 0表示不使用chuncked，非0表示Transfer-Encoding: "chunked"
	int         content_length; //POST时需要标注内容的长度
	request_t*  req;
}response_t;

/** 发送http请求回调函数，参数为错误码和请求句柄 */
typedef void(*request_cb)(int, request_t*);
/** 设置收到应答的内容 */
typedef void(*response_data)(request_t*, char*, int);
/** 设置应答接收完成的处理 */
typedef void(*response_cb)(request_t*);

#ifdef __cplusplus
}
#endif
#endif
