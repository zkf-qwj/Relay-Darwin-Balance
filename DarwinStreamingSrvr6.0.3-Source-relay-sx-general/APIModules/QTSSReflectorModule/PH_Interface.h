#ifndef PH_INTERFACE_H
#define PH_INTERFACE_H
#define MAX_BUFFER_LEN 25000
#define URL_LEN 1024
#define  BUF_LEN 2048
#define MAXIDLETIME  2000
#include "OSThread.h"
extern "C" {
#include "fdevent.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
 #include <fcntl.h>
}

class PH_Interface: public OSThread
{
public:
    PH_Interface();
    ~PH_Interface();
    bool Configure(char * pBindHost ,char* port,int portsNum,int portsList[],char *pbindIp,int pseqNum);
    virtual void Entry();
    fdevents *ev;           /* Event handlers */   
    iosocket *sock;              /* iosocket for SCTP to Controller */
    int conntimeout;              /* Number of seconds to wait before retrying controller */   
    int polltimeout; 
    char recvBuf[MAX_BUFFER_LEN];
    
    int recv_len;
    char sendBuf[MAX_BUFFER_LEN];
    char *sendPtr;
    int send_len;
    int conn_statu;
    bool fresh_server;
   
    
    bool lock;
    
    int GetPortNum() {return N_port; }
    int* GetPortsList() {return ports;}
    char * GetBindIp() {return bindIp;}
    void Init();
    void UnInit();
    int GetSeqNum() {return seqNum;}
    void fresh_server_addr();
private:
    void Trigger();
    struct sockaddr_in servaddr;    /* ARTS Controller Address */  
    int N_port;
    int ports[BUF_LEN];
    char bindIp[BUF_LEN]; 
    int seqNum;  
    char server_ip[BUF_LEN];  
};


extern char** g_balanceHost;
extern int g_balanceNum;


typedef struct RES_DATA
{
    char url[URL_LEN];//camera url
    char ip[URL_LEN]; //streaming ip
    int  port;        //streaming port    
    char DeviceID[URL_LEN]; // camera deviceID
    int  statu;
    void *next;
    char error[URL_LEN];
    int64_t idelTime;
    int try_time;
}res_data_t;
extern res_data_t * res_data_list;

int insert_node(res_data_t **list,res_data_t **last_node,res_data_t *node);
res_data_t* get_node(res_data_t *list,const char *url,const char *DeveiceID);
int del_node(res_data_t **list,res_data_t **last_node,char *DeviceID,char *url);

void RegisterAndsendPort(PH_Interface *obj);

int sendStream(PH_Interface *obj,char *url,char *user,char *pwd,char *DeviceID,int module);
//int stopStream(PH_Interface *obj,char *DeviceID);
int stopStream(PH_Interface *obj,char *DeviceID,char *StopFlag);
extern PH_Interface *sPHInterface;
#endif
