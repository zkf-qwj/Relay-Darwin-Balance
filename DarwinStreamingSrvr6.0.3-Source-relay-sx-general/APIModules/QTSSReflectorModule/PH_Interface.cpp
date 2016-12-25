#include "PH_Interface.h"
#include "json/json.h"

#include "QTSS.h"
#include "QTSSARTSReflectorAccessLog.h"

enum CONN_STATUE
{
	CONNING = 0,
	CONNTED = 1,
	DISCONN = 2            
};
enum debug_level_t {
	NONE_ARTS_MODULE = 0,
	INFO_ARTS_MODULE,
	DEBUG_ARTS_MODULE
};

enum BALANCE_STATUE
{
	IDEL=0,
	REQUESTPORT=1,
	SENDSTREAMRESP=2,
	RECVSTREAMRESP=3,
	STOPSTREAM = 4,
	RECVSTOPSTREAM
};


PH_Interface::PH_Interface():OSThread()
{
	ev = fdevent_init(4096, FDEVENT_HANDLER_LINUX_SYSEPOLL);    
	polltimeout =1000;
	conn_statu = DISCONN;
	sock = NULL;    
	memset(bindIp,0,sizeof(bindIp));
	memset(ports,0,sizeof(ports));
	N_port = 0;
	conn_statu = DISCONN;
	qtss_printf("new PH_Interface\n");
	fresh_server = false;

}
void PH_Interface::Init()
{
	sock = iosocket_init();
	sendPtr = sendBuf;
	//fresh_server = false;
}

void PH_Interface::UnInit()
{
	iosocket_free(sock);
	sendPtr = sendBuf;
	send_len =0;
	recv_len =0;
}

PH_Interface::~PH_Interface()
{   
	fdevent_free(ev);
}

bool PH_Interface::Configure(char * pBindHost ,char*port,int portsNum,int portsList[],char *pbindIp,int pseqNum)
{
	int i=0;
	bzero( (void *)&servaddr, sizeof(servaddr) );
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(atoi(port));
	servaddr.sin_addr.s_addr = inet_addr(pBindHost);
	N_port = portsNum;
	if (sizeof(ports)/sizeof(int) < N_port){
		printf("zlj/add port buf not enough space, buf_port:%d,N_port:%d\n",sizeof(ports)/sizeof(int),N_port);
	}
	for(i=0;i<N_port;i++)
	{
		ports[i] = portsList[i];
	}    
	strncpy(bindIp,pbindIp,sizeof(bindIp));
	seqNum = pseqNum;
	strncpy(server_ip,pBindHost,sizeof(server_ip));
	return true;
}


void PH_Interface::Entry()
{
	qtss_printf(" PH_Interface run\n");
	fdevent_revents *revents = fdevent_revents_init();
	int poll_errno;
	int n;

	QTSS_TimeVal prevTs = QTSS_Milliseconds(), curTs;

	while(IsStopRequested() == false)
	{        
		n = fdevent_poll(ev, this->polltimeout);         
		poll_errno = errno;

		if (n > 0) 
		{
			size_t i;
			fdevent_get_revents(ev, n, revents);           
			for (i = 0; i < revents->used; i++) 
			{
				fdevent_revent *revent = revents->ptr[i];
				handler_t r;      

				switch (r = (*(revent->handler))(this, revent->context, revent->revents)) 
				{
				case HANDLER_FINISHED:
				case HANDLER_GO_ON:
				case HANDLER_WAIT_FOR_EVENT:
					break;
				case HANDLER_ERROR:
					/* should never happen */
					Assert(false);
					break;
				default:
					RLogRequest(INFO_ARTS_MODULE, 0, "event loop returned: %d", (int) r);
					break;
				}
			}
		} 
		else if (n < 0 && poll_errno != EINTR) 
		{
			RLogRequest(INFO_ARTS_MODULE, 0, "event loop: fdevent_poll failed: %s", strerror(poll_errno)); 

		}


		curTs = QTSS_Milliseconds();
		if(curTs >= prevTs + this->polltimeout ) 
		{
			Trigger();
			prevTs = curTs;
		}      

	}
	fdevent_revents_free(revents);     
}


handler_t  balance_conn( struct sockaddr_in*addr,int *pfd)
{
	int fd =-1;
	if (-1 == (fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP))) {
		switch (errno) {
		case EMFILE:
			return HANDLER_WAIT_FOR_FD;
		default:
			return HANDLER_ERROR;
		}
	}

	(*pfd) = fd;
	fcntl(fd, F_SETFL, O_NONBLOCK | O_RDWR);
	if (-1 == connect(fd, (struct sockaddr *)addr, sizeof(struct sockaddr) )) {
		switch(errno) {
		case EINPROGRESS:
		case EALREADY:
		case EINTR:
			return HANDLER_WAIT_FOR_EVENT;
		default:
			close(fd);
			return HANDLER_ERROR;
		}
	}

	return HANDLER_GO_ON;
}

handler_t Balance_recv_handle(void *s,void *ctx,int revents)
{
	PH_Interface *sPHInterface = (PH_Interface *)s;
	// res_data_t *context = (res_data_t*)ctx;
	qtss_printf("Balance_recv_handle\n");
	int ret =0; 
	char buf[10] = {0};
	int iValue = 0;
	if (revents & FDEVENT_IN)
	{
		ret = recv(sPHInterface->sock->fd,buf,4,0); 
		if ((ret <=0) || (revents & FDEVENT_HUP) || (revents & FDEVENT_ERR)){
			std::cout <<"connet to balance failed" <<std::endl;
			std::cout <<"zlj/add balance recv 4 byte return -1 connet to balance failed" <<std::endl;
			perror("ABC");
			fdevent_event_del(sPHInterface->ev, sPHInterface->sock);            
			fdevent_unregister(sPHInterface->ev, sPHInterface->sock);
			sPHInterface->conn_statu = DISCONN;
			sPHInterface->fresh_server_addr();
			return HANDLER_GO_ON;
		}else if (4 != ret){
			printf("zlj/add---- recv 4 byte error\n");
			return HANDLER_GO_ON;
		}   
		ret = 0;
		memcpy((char *)&iValue,buf,4);

		memset(sPHInterface->recvBuf,0,MAX_BUFFER_LEN);
		ret = recv(sPHInterface->sock->fd,sPHInterface->recvBuf,iValue,0);
		if(ret >0)
		{
			Json::Reader reader;
			Json::Value root;

			std::string flag;
			std::string url;
			std::string DeviceID;
			std::string port_t; //12.25.duan
			char tmp[64]={0};	//12.25.duan
			if(reader.parse(sPHInterface->recvBuf, root))
			{
				flag = root["flag"].asString();
				url = root["url"].asString();
				DeviceID=root["DeviceId"].asString();
			}
			fprintf(stdout,"recv %s\n",sPHInterface->recvBuf);
			fprintf(stdout,"flag:%s,url:%s,DeviceID:%s\n",flag.c_str(),url.c_str(),DeviceID.c_str());


			if(strcmp(flag.c_str(), "RecvStream") == 0)
			{
				res_data_t  *context = NULL;
				if(strstr(url.c_str(),"rtsp") || strstr(url.c_str(),"udp") || strstr(url.c_str(),"hikplat") || strstr(url.c_str(),"rtmp") ||  strstr(url.c_str(),"windows") ||  strstr(url.c_str(),"8200") || strstr(url.c_str(),"sip") || strstr(url.c_str(),"http"))
				{	               
					context = get_node(res_data_list,url.c_str(),NULL);	 
				}else if (strstr(url.c_str(),"nvr"))
				{	                
					context = get_node(res_data_list,NULL,DeviceID.c_str());
				} 
				fprintf(stdout,"context:%x\n",context);
				if(context == NULL)
				{
					return HANDLER_GO_ON;
				}
				//qtss_printf("----------------------------------1\n");//zlj 12.25.duan
				strcpy(context->DeviceID,DeviceID.c_str());
				strcpy(context->ip,root["ip"].asString().c_str());
				//context->port = root["port"].asInt(); 
				//port_t = root["DeviceId"].asString();
					
				context->port = 40000 + atoi((root["DeviceId"].asString().c_str())) * 2; //为双流的监控适配的，udp起始端口为40000，端口固定 2016.12.25 duan
				//qtss_printf("------shuangliu port:%d\n",context->port); //zlj 12.25daun
				context->statu = RECVSTREAMRESP;	           
				fprintf(stdout,"ID:%s,ip:%s,port:%d,statu:%d\n",context->DeviceID,context->ip,context->port,context->statu);
			}else
			{
				std::cout<<"recv flag"<<flag<<std::endl;
			}
		}       

	}
	if((ret <=0) || (revents & FDEVENT_HUP) || (revents & FDEVENT_ERR))
	{
		std::cout <<"connet to balance failed" <<std::endl;
		fdevent_event_del(sPHInterface->ev, sPHInterface->sock);        
		fdevent_unregister(sPHInterface->ev, sPHInterface->sock);
		sPHInterface->conn_statu = DISCONN;
		sPHInterface->fresh_server_addr();
	}
	return HANDLER_GO_ON;
}

handler_t Balance_send_handle(void *s, void *ctx, int revents)
{
	PH_Interface *sPHInterface = (PH_Interface *)s;
	int ret =0;
	qtss_printf("Balance_send_handle,revents:%d,sPHInterface->send_len:%d\n",revents,sPHInterface->send_len);

	if (revents & FDEVENT_OUT )
	{

		RegisterAndsendPort(sPHInterface);        

		if( sPHInterface->send_len >0)
		{
			ret = send(sPHInterface->sock->fd, sPHInterface->sendPtr, sPHInterface->send_len, MSG_NOSIGNAL);
			if(ret == sPHInterface->send_len)
			{
				std::cout <<"send success,buf:"<<sPHInterface->sendPtr<<std::endl;  
				sPHInterface->send_len =0;
				sPHInterface->sendPtr = sPHInterface->sendBuf;

				//fdevent_event_del(sPHInterface->ev, sPHInterface->sock);        
				//fdevent_unregister(sPHInterface->ev, sPHInterface->sock);
				fdevent_register(sPHInterface->ev, sPHInterface->sock, Balance_recv_handle,NULL);
				fdevent_event_add(sPHInterface->ev, sPHInterface->sock, FDEVENT_IN | FDEVENT_HUP);    
				sPHInterface->conn_statu = CONNTED;                           
			}           
			else if(ret >0 && ret <sPHInterface->send_len)
			{
				sPHInterface->send_len -= ret;
				sPHInterface->sendPtr = sPHInterface->sendBuf + ret;
			}
		}     
	}

	if((ret <=0) || (revents & FDEVENT_HUP) || (revents & FDEVENT_ERR))
	{
		std::cout <<"connet to balance failed" <<std::endl;
		fdevent_event_del(sPHInterface->ev, sPHInterface->sock);        
		fdevent_unregister(sPHInterface->ev, sPHInterface->sock);
		sPHInterface->conn_statu = DISCONN;        
		sPHInterface->fresh_server_addr();
	}

	return HANDLER_GO_ON;
}


void PH_Interface:: fresh_server_addr()
{
	if(g_balanceNum <=1)
		return ;
	int i=0;
	char host[32]={'\0'};

	char *p_port = NULL;
	int port = 0;
	for(i=0;i<g_balanceNum;i++)
	{
		char *IpAddr = g_balanceHost[i];
		if(strstr(IpAddr,server_ip))
		{
			continue;
		}
		p_port = strstr(IpAddr,":");
		if(p_port)
		{
			memcpy(host,IpAddr,p_port -IpAddr );
			p_port += 1;
			port = atoi(p_port);
		}        
	}
	if(strlen(host) && port >0)
	{
		fprintf(stdout,"new host:%s,port:%d\n",host,port);
		servaddr.sin_addr.s_addr = inet_addr(host);
		servaddr.sin_port = htons(port);
		strcpy(server_ip,host);
		fresh_server = true;
	}
}



void PH_Interface::Trigger()
{
	switch(conn_statu)
	{
	case DISCONN:
		UnInit();
		Init();
		switch (balance_conn(&servaddr,&sock->fd))
		{
		case HANDLER_WAIT_FOR_FD:
			std::cout<<"HANDLER_WAIT_FOR_FD"<<std::endl;
			break;
		case HANDLER_WAIT_FOR_EVENT:  
			std::cout<<"***connecting,fdevent_register,sock->fd:"<<sock->fd <<std::endl;
			fdevent_register(ev, sock, Balance_send_handle,NULL);
			fdevent_event_add(ev, sock, FDEVENT_OUT | FDEVENT_HUP); 


			break; 
		case HANDLER_GO_ON: 
			std::cout<<"connection established"<<std::endl; 
			break;
		default:
			std::cout<<"connection error "<<std::endl;
			fresh_server_addr();
			break;
		}
		break;
	default:
		break;    
	} 
}


int insert_node(res_data_t **list,res_data_t **last_node,res_data_t *node)
{


	if(node != NULL)
	{
		if((*list) == NULL)
		{
			(*list) = node;
			(*list)->next = NULL;
			(*last_node)= node;
		}else 
		{   
			(*last_node)->next = node;
			(*last_node) = node;
			(*last_node)->next = NULL;
		} 

		return 1;          
	}else
		return 0;     
}

res_data_t *get_node(res_data_t *list,const char *url,const char *DeviceID)
{
	res_data_t *p = list;
	while(p)
	{
		if(url && strlen(p->url) && strlen(url))
		{
			if(strcmp(p->url,url) ==0)
			{
				return p;
			}
		}else if(DeviceID && strlen(DeviceID) && strlen(p->DeviceID))
		{
			if(strcmp(p->DeviceID,DeviceID) == 0)
			{
				return p;
			}
		}        

		p = (res_data_t*)p->next;
	}

	return NULL;    
}


int del_node(res_data_t **list,res_data_t **last_node,char *DeviceID,char *url)
{
	res_data_t *p = (*list);
	res_data_t *prev = NULL;
	while(p)
	{
		if( (url && strlen(p->url) && strlen(url) && (strcmp(p->url,url) == 0) ) || 

				DeviceID && strlen(DeviceID) && strlen(p->DeviceID)&& (strcmp(p->DeviceID,DeviceID) == 0)      
		  )
		{
			fprintf(stdout,"get node,p:%x,prev:%x\n",p,prev);
			break;
		}

		prev =p;
		p = (res_data_t *)p->next;
	}

	if(prev == NULL && p!= NULL)
	{
		(*list)=(res_data_t*) p->next;
		if((*last_node) == p)
		{
			(*last_node) = (res_data_t*)p->next;
		}
	}else if(prev!= NULL && p!= NULL)
	{
		prev->next = p->next;
		if((*last_node) == p)
			(*last_node) = (res_data_t*)p->next;
	}

	free(p);    
}


void RegisterAndsendPort(PH_Interface *obj)
{
	if(obj == NULL)
		return ;

	char temp[MAX_BUFFER_LEN]={'\0'};
	char *p =temp;
	Json::Value root;
	Json::FastWriter fast_writer;
	root["flag"] = "register";
	root["DeviceType"] = 1;
	root["SerialNumber"] = obj->GetSeqNum();
	int iStrLen = strlen(fast_writer.write(root).c_str());
	memcpy(temp,&iStrLen,4);
	memcpy(temp+4,fast_writer.write(root).c_str(),iStrLen);
	obj->send_len += iStrLen+4;
	p += (iStrLen+4);

	Json::Value root1;
	Json::FastWriter fast_writer1;	

	root1["flag"] = "SendPort";
	char port[32];
	root1["ip"] = obj->GetBindIp();
	root1["PortCount"] = obj->GetPortNum();
	int *PortArr = obj->GetPortsList();
	for(int i = 0; i < obj->GetPortNum(); i++)
	{
		memset(port, 0, 32);
		sprintf(port, "port%d", i+1);
		root1[port] = PortArr[i];
		printf("send port %d\n", PortArr[i]);
	}


	iStrLen = strlen(fast_writer1.write(root1).c_str());
	qtss_printf("----zlj port josn strlen: %d\n",iStrLen);
	memcpy(p,&iStrLen,4);
	memcpy(p+4,fast_writer1.write(root1).c_str(),iStrLen);
	obj->send_len += (iStrLen+4);
	memcpy(obj->sendBuf,temp,obj->send_len);
}


//int  stopStream(PH_Interface *obj,char *DeviceID)
int  stopStream(PH_Interface *obj,char *DeviceID,char *StopFlag)
{

	char temp[URL_LEN]={'\0'};
	int len =0;
	Json::Value root;
	Json::FastWriter fter;
	if (0 == strcmp(StopFlag,"stopwin")){
		root["flag"] = "stopwin";
	}else if (0 == strcmp(StopFlag,"stop8200")){
		root["flag"] = "stop8200";
	}else{
		root["flag"] = "StopStream";
	}
	root["DeviceId"] = DeviceID;
	int iStrLen = strlen(fter.write(root).c_str());
	memcpy(temp,&iStrLen,4);
	memcpy(temp+4,fter.write(root).c_str(),iStrLen);
	len += iStrLen +4;

	return send(obj->sock->fd, temp, len, MSG_NOSIGNAL);
}

void get_param(char *url,char *host,char*port,char*channel)
{
	char *p = NULL;
	char *q = NULL;
	if(url)
	{
		p = strstr(url,"hikplat://");
		if(p)
		{
			p = p+ strlen("hikplat://");
			char delims[] = ":";
			char *result = NULL;
			result = strtok( p, delims );
			int i=0;
			while( result != NULL ) {	
				if(i==0)
					memcpy(host,result,strlen(result));
				else if(i==1)
					memcpy(port,result,strlen(result));
				else if (i==2)
					memcpy(channel,result,strlen(result));

				result = strtok( NULL, delims );
				i++;
			}          
		}
	}
}


int sendStream(PH_Interface *obj,char *url,char *user,char *pwd,char *DeviceID,int module)
{
	Json::Value root;
	Json::FastWriter fter;
	char temp[URL_LEN]={'\0'};
	int len =0;
	printf("send url:%s\n",url);
	if(url != NULL && strlen(url) >0 && (strstr(url,"rtsp") || strstr(url,"rtmp") ||  strstr(url,"http")))
	{
		root["flag"]="rtsp";
		root["url"]=url;
		root["UserName"]=user;
		root["passwd"]= pwd;      
	}else if(url != NULL && strlen(url) >0 && strstr(url,"sip")){
		root["flag"]="sip";                                                                                                                                                                                  
		root["url"]=url;
	}else if(url != NULL && strlen(url) >0 && strstr(url,"8200")){
		root["flag"]="8200";                                                                                                                                                                                  
		root["channelid"]=url;
	}else if(url != NULL && strlen(url) >0 && strstr(url,"windows")){
		root["flag"]="windows";                                                                                                                                                                                  
		root["channelid"]=url;
	}else if(url!=NULL && strlen(url) >0 && (strstr(url,"NVR") || strstr(url,"nvr")))
	{
		root["flag"] = "nvr";	
		root["DeviceId"] = "196609";
	}else if(url!=NULL && strlen(url) >0 && (strstr(url,"WINDOWS") || strstr(url,"windows")))
	{
		root["flag"] = "windows";	
		root["DeviceId"] = DeviceID;
	}else if(url!=NULL && strlen(url)>0 && strstr(url,"hikplat"))
	{
		char host[32]={'\0'};
		char port[32]={'\0'};
		char channel[32]={'\0'};

		get_param(url,host,port,channel);
		printf("host:%s,port:%s,channel:%s,url:%s\n",host,port,channel,url);
		root["flag"]="hikplat";
		root["host"]=host;
		root["UserName"]=user;
		root["passwd"]=pwd;
		root["port"]=atoi(port);
		root["channel"]=atoi(channel);

	}  
	else
	{
		root["flag"] = "StartStream";	
		root["DeviceId"] = DeviceID;
	}

	if (!(strstr(url,"sip") || strstr(url,"ivms8700") || strstr(url,"8200"))){   
		root["module"]=module;                                                                                                                                                                                   
	} 
	int iStrLen = strlen(fter.write(root).c_str());

	memcpy(temp,&iStrLen,4);
	memcpy(temp+4,fter.write(root).c_str(),iStrLen);
	len += (iStrLen+4);	

	return send(obj->sock->fd, temp, len, MSG_NOSIGNAL);
}

