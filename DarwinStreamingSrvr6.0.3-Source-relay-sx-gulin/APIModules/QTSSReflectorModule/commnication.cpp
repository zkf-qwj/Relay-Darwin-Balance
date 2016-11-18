#include "commnication.h"
#include <string>
#include "json/json.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
extern "C" {
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
}

static int*g_ports = NULL;
static int g_portsNum =0;
static char g_ip[256];
static int seqNumber =0;

CCommnication::CCommnication()
{
	memset(m_szLiveIp, 0, MAX_ARRAY_LEN);
	memset(m_szLivePort, 0, MAX_ARRAY_LEN);
	memset(&m_stAddr, 0, sizeof(m_stAddr));

	m_iSocket = 0;
}

CCommnication::~CCommnication()
{
    if(m_iSocket >0)
    {
        close(m_iSocket);
    }
  
}


bool CCommnication::Register(int number)
{
    Json::Value root;
	Json::FastWriter fast_writer;
	char temp[1024]={'\0'};
	if(number == 0 && seqNumber >0)
	{
	    number = seqNumber;
	    fprintf(stdout,"user cached seqNumber");
	 }
	
	root["flag"] = "register";
	root["DeviceType"] = 1;
	root["SerialNumber"] = number;
	
	
	
	int iStrLen = strlen(fast_writer.write(root).c_str());
	memcpy(temp,&iStrLen,4);
	memcpy(temp+4,fast_writer.write(root).c_str(),iStrLen);
	int iRet = send(m_iSocket, temp, iStrLen+4, 0);
	if(iRet == (iStrLen+4))
	{
	    fprintf(stdout,"Register success!\n");
		return true;
	}
	return false;
    
}

int CCommnication::init(char *ip,char * port,int PortArr[], int PortCount, char *dst_ip,int seqNum)
{
	if(ip!= NULL)
	{
	    strcpy(m_szLiveIp,ip);
	}

    if(port != NULL)
    {
	    strcpy(m_szLivePort,port);
	}
	fprintf(stdout,"g_ports:%x,g_portsNum:%d,PortCount:%d\n",g_ports,g_portsNum,PortCount);
	if(g_ports ==  NULL && g_portsNum <=0 && PortCount >0)
	{
	   g_ports = (int*)malloc(sizeof(int)*PortCount);
	   memcpy(g_ports,PortArr,sizeof(int)*PortCount);
	   memcpy(g_ip,dst_ip,strlen(dst_ip));
	   g_portsNum = PortCount;	  
	   fprintf(stdout,"copy data to static\n");  
	}
	
	if(seqNum >0)
	    seqNumber = seqNum;
	
	return 0;
}

int CCommnication::CreateConnect()
{
	m_iSocket = socket(AF_INET, SOCK_STREAM, 0);
	if(FAIL == m_iSocket)
	{
		return 1;
	}

	m_stAddr.sin_family = AF_INET;
	m_stAddr.sin_addr.s_addr = inet_addr(m_szLiveIp);
	m_stAddr.sin_port = htons(atoi(m_szLivePort));

	if(SUCCESS != connect(m_iSocket, (struct sockaddr *)&m_stAddr, sizeof(m_stAddr)))
	{
		return -2;
	}

	return 1;
}

bool CCommnication::SendPersonNum(int num,char *deviceID)
{
    if(deviceID == NULL)
        return false;
        
        
    char ctx[RECV_BUF_LEN]={'\0'};
    sprintf(ctx,"{\"person_number\":%d,\"camera_card\":\"%s\"}",num,deviceID);
    char temp[RECV_BUF_LEN]={'\0'};
    sprintf(temp,"POST /jumbo/shihai/Home/api HTTP/1.1\r\nHost: %s:%s\r\nAccept: */*\r\nContent-type: application/json\r\nContent-Length: %d\r\n\r\n%s",
        m_szLiveIp,m_szLivePort,strlen(ctx),ctx);
    
    int isend = send(m_iSocket, temp, strlen(temp), 0);
    if(isend == strlen(temp))
    {
        fprintf(stdout,"send success");
    }else
    {
        fprintf(stdout,"SendPersonNum failed");
        return false;
    }
    /*
    memset(temp,0,RECV_BUF_LEN);
    int irecv=recv(m_iSocket, temp, RECV_BUF_LEN, 0);
    if(irecv>0)
    {
        fprintf(stdout,"recv :%s\n",temp);
    }*/
    
    return true;
}

bool CCommnication::SendPort(int PortArr[], int PortCount, char *ip)
{
	char port[MAX_ARRAY_LEN];
	Json::Value root;
	Json::FastWriter fast_writer;	
	char temp[1024]={'\0'};
	
	root["flag"] = "SendPort";
	
	if(g_ports >0 && g_portsNum >0)
	{
	    ip = g_ip;
	    PortCount = g_portsNum;
	    PortArr = g_ports;
	    printf("get ports array from static\n");
    }	
    if(ip == NULL || PortArr == NULL)
    {
        return false;        
    }
    root["ip"] = ip;
    root["PortCount"] = PortCount;
    for(int i = 0; i < PortCount; i++)
    {
        memset(port, 0, MAX_ARRAY_LEN);
        sprintf(port, "port%d", i+1);
        root[port] = PortArr[i];
    }
	
	
	int iStrLen = strlen(fast_writer.write(root).c_str());
	memcpy(temp,&iStrLen,4);
	memcpy(temp+4,fast_writer.write(root).c_str(),iStrLen);
	int iRet = send(m_iSocket, temp, iStrLen+4, 0);
	if(iRet == (iStrLen+4))
	{
	    fprintf(stdout,"sendPort success!\n");
		return true;
	}
	return false;
}

bool CCommnication::SendStream(char* DeviceId,char*url,char*user,char*pwd,int module)
{
	Json::Value root;
	Json::FastWriter fter;
	char temp[1024]={'\0'};
    if(url != NULL && strlen(url) >0 && strstr(url,"rtsp"))
    {
        root["flag"]="rtsp";
        root["url"]=url;
        root["UserName"]=user;
        root["passwd"]= pwd;       
    }else if(url!=NULL && strlen(url) >0 && (strstr(url,"NVR") || strstr(url,"nvr")))
    {
        root["flag"] = "nvr";	
	    root["DeviceId"] = "196609";
    }else if(url!=NULL && strlen(url) >0 && (strstr(url,"WINDOWS") || strstr(url,"windows")))
    {
        root["flag"] = "windows";	
	    root["DeviceId"] = DeviceId;
    }   
    else
    {
	    root["flag"] = "StartStream";	
	    root["DeviceId"] = DeviceId;
	}
	
	root["module"]=module;

	int iStrLen = strlen(fter.write(root).c_str());
	
	memcpy(temp,&iStrLen,4);
	memcpy(temp+4,fter.write(root).c_str(),iStrLen);
	
	int iRet = send(m_iSocket, temp, iStrLen+4, 0);
	if(iRet == (iStrLen+4))
	{
		return true;
	}

	return false;
}

bool CCommnication::StopStream(char* DeviceId)
{
	Json::Value root;
	Json::FastWriter fter;
    char temp[1024]={'\0'};
	root["flag"] = "StopStream";	
	root["DeviceId"] = DeviceId;
	root["DeviceType"]=1;

	int iStrLen = strlen(fter.write(root).c_str());
	memcpy(temp,&iStrLen,4);
	memcpy(temp+4,fter.write(root).c_str(),iStrLen);
	int iRet = send(m_iSocket,temp, iStrLen+4, 0);
	if(iRet == (iStrLen+4))
	{
		return true;
	}

	return false;
}


int CCommnication::GetStream(void (*CallBack)(char *, int, char*,char*,char*),char *buf1,char*buf2)
{
	int iRet = 0;

	char RecvBuf[RECV_BUF_LEN] = {0};
retry:
	iRet = recv(m_iSocket, RecvBuf, RECV_BUF_LEN, 0);
	if(FAIL == iRet)
	{
		return 1;	
	}else if(0 == iRet){
		close(m_iSocket);
		return 2;
	}
	
	Json::Reader reader;
    Json::Value root;

	std::string flag;
    if(reader.parse(RecvBuf, root))
    {
		flag = root["flag"].asString();
	}
	printf("flag:%s\n",flag.c_str());
    if(0 == strcmp(flag.c_str(), "RequestPort"))
    {
        this->SendPort(NULL,0,NULL);
        goto retry;
    }
    else if (0 != strcmp(flag.c_str(), "RecvStream"))
	{
	    fprintf(stdout,"recv data ERROR,%s",flag.c_str());
		return 3;			
	}

	std::string Ip = root["ip"].asString();
	int iPort = root["port"].asInt();
	std::string iDeviceId = root["DeviceId"].asString();

    fprintf(stdout,"ip:%s,port:%d,id:%s\n",(char *)Ip.c_str(),iPort,(char *)iDeviceId.c_str());
	CallBack((char *)Ip.c_str(), iPort,(char *)iDeviceId.c_str(),buf1,buf2);

	return 0;
}




