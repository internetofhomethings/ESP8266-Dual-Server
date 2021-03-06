#ifndef __USER_WEBSERVER_H__
#define __USER_WEBSERVER_H__

#define SERVER_PORT 9703
#define SERVER_SSL_PORT 443

#define URLSize 10

typedef enum Result_Resp {
    RespFail = 0,
    RespSuc,
} Result_Resp;

typedef enum ProtocolType {
    GET = 0,
    POST,
    GET_FAVICON
} ProtocolType;

typedef enum _ParmType {
    SWITCH_STATUS = 0,
    INFOMATION,
    WIFI,
    SCAN,
	REBOOT,
    DEEP_SLEEP,
    LIGHT_STATUS,
    CONNECT_STATUS,
    USER_BIN,
	GET_SENSORS
} ParmType;

typedef struct URL_Frame {
    enum ProtocolType Type;
    char pSelect[URLSize];
    char pCommand[URLSize];
    char pFilename[URLSize];
} URL_Frame;

typedef struct URL_Param {
    enum ProtocolType Type;
    char pParam[URLSize][URLSize];
    char pParVal[URLSize][URLSize];
    int nPar;
} URL_Param;

typedef struct _rst_parm {
    ParmType parmtype;
    struct espconn *pespconn;
} rst_parm;

void user_webserver_init(uint32 port);

#endif
