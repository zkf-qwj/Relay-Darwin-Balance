#ifndef COMMNICATION_H_
#define COMMNICATION_H_

#include <netinet/in.h>

#define				RECV_BUF_LEN			1024
#define				MAX_ARRAY_LEN			32
#define				SUCCESS					0
#define				FAIL					-1



class CCommnication
{
public:
	CCommnication();
	~CCommnication();
public:
	/*
	*function : init
	*return:
	*on success, 0 is returned 	
 	*/
	int init(char *ip,char* port,int portArry[],int portsNum,char*dst_ip,int seqNum);

	/*
 	*function : CreateConnect
 	*return:
 	*on success, 0 is returned
 	*/
	int CreateConnect();

	bool SendPort(int PortArr[], int PortCount, char *ip);

	bool SendStream(char* DeviceId,char *url,char*user,char*pwd,int module);
	bool StopStream(char* DeviceId);
    bool Register(int serialNumber);
    bool SendPersonNum(int num,char *deviceID);
	/*
 	*function : GetStream
 	*
 	*1):ip
 	*2):Port
	*3):DeviceId
 	* */
	int GetStream(void (*CallBack)(char *, int, char*,char*,char*),char *sdp,char*vid);
	char m_szLiveIp[MAX_ARRAY_LEN];
	char m_szLivePort[MAX_ARRAY_LEN];

	struct sockaddr_in m_stAddr;

	int m_iSocket;
};


#endif
