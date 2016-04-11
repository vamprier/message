////////////////////////////////////////////////////////////
//公司名称：西安影子科技有限公司
//产品名称：思扣快信
//版 本 号：1.0.0.0
//文 件 名：stunServer.cpp
//开发人员：赵娟
//日    期：2015-10-23
//更新人员：
//更新日期：
//文件说明：MessageServer服务器端
////////////////////////////////////////////////////////////

#include "typedef.h"
#include "logfile.h"
#include <iostream>
#include <algorithm>
#include <map>
#include <vector>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

using namespace std;

#define MAG_NUMBER 100000

typedef struct 
 {
	RequestMessagePackage msg;
 	time_t msgTime;
 	unsigned int ip;
 	unsigned short port;
 }MessageContent;

 map<unsigned int,int> unAvailabeAddress;//不可用地址
 map<unsigned int,int> AvailabeAddress;//可用地址
 MessageContent messageVector[MAG_NUMBER];//消息数组
 vector<pair<unsigned int,unsigned short> > ipPortVec;//存储ip和port对
 Logger Log;//日志类对象
 char writeChar[MSG_TOTAL_LENGTH];//
 RequestMessagePackage recvmsp;//
 size_t totalRecvNumber = 0;//接收的数据总数
 size_t totalSendNumber = 0;//发送的数据总数



 //===========================================
 //ChangeMessage函数说明
 //函数功能：交换包信息
 //参数：    mspNeedChangeLeft：需要交换信息的数据包
 //		    mspNeedChangeRight：需要交换信息的数据包
 //函数返回： 成功或者失败
 //===========================================
void
ChangeMessage( RequestMessagePackage* mspNeedChangeLeft, RequestMessagePackage* mspNeedChangeRight)
{
	mspNeedChangeLeft->messageContent.remoteAddr.ip = mspNeedChangeRight->messageContent.localAddr.ip;
	mspNeedChangeLeft->messageContent.remoteAddr.port = mspNeedChangeRight->messageContent.localAddr.port;
	mspNeedChangeLeft->messageContent.remoteNatAddr.ip = mspNeedChangeRight->messageContent.localNatAddr.ip;
	mspNeedChangeLeft->messageContent.remoteNatAddr.port = mspNeedChangeRight->messageContent.localNatAddr.port;
    //交换信息
	mspNeedChangeRight->messageContent.remoteAddr.ip = mspNeedChangeLeft->messageContent.localAddr.ip;
	mspNeedChangeRight->messageContent.remoteAddr.port = mspNeedChangeLeft->messageContent.localAddr.port;
	mspNeedChangeRight->messageContent.remoteNatAddr.ip = mspNeedChangeLeft->messageContent.localNatAddr.ip;
	mspNeedChangeRight->messageContent.remoteNatAddr.port = mspNeedChangeLeft->messageContent.localNatAddr.port;
}

//===========================================
//WaitMessage函数说明
//函数功能：主消息循环，接收并交换信息
//参数：    localPortSocket：socket连接
//		   timeOut：超时时间
//函数返回： 无
//===========================================
void 
WaitMessage( Socket localPortSocket, double timeOut)
{
    //主消息循环
	while (1)
	{	
		int mspSize = sizeof( RequestMessagePackage);
		NetAddr from;
		memset( &recvmsp, 0x00, mspSize);
		bool isok =  getMessage( localPortSocket, (char*)(&recvmsp), &mspSize, &from.ip, &from.port);
		//获得消息接收时间
		time_t startTime = GetTime();
		//消息接收成功
		if ( isok)
		{
			//记录收到包的总数
			++totalRecvNumber;
			//收到地址交换请求数据包后，判断包头、包尾格式,解析配对信息，在队列中查找是否存在同样配对的请求信息
			if( CheckRequestMessage(&recvmsp) && recvmsp.dataType == EXCHANG_TYPE)
			{
				//如果接收到信息的ip和port已经存在的话，不做处理直接进入下次循环
				vector<pair<unsigned int,unsigned short> >::iterator vit = find(ipPortVec.begin(),ipPortVec.end(),make_pair(from.ip, from.port));
				if( vit != ipPortVec.end())
				{
					continue;
				}
				else
				{
					ipPortVec.push_back(make_pair(from.ip, from.port));
				}
				memset(writeChar,0x00,MSG_TOTAL_LENGTH);
				sprintf(writeChar,"receive from : [ %d ] [%d ]\n",from.ip,from.port);
				Log.Log(writeChar);
				cout<<recvmsp.messageContent.pairingFlag<<endl;
				//在不可用地址中查找配对码
				map<unsigned int,int>::iterator it = unAvailabeAddress.find( recvmsp.messageContent.pairingFlag);
				//找到同样配对信息，将双方信息打包后发送给双方，并且在消息队列中删除找到的配对信息
				if ( it != unAvailabeAddress.end())
				{
					cout<<"find same pairing message from list\n";
					int index = it->second;
					//找到配对信息后，从不可用map中将该地址删除
					unAvailabeAddress.erase(it++);
					//可用地址中加入该地址
					AvailabeAddress.insert(make_pair( messageVector[index].msg.messageContent.pairingFlag, index));
					//交换信息
					ChangeMessage(&recvmsp,&( messageVector[index].msg));
					usleep(1);
					bool issendA = sendMessage(localPortSocket, (char*)(&recvmsp), mspSize, from.ip, from.port);
					memset(writeChar,0x00,MSG_TOTAL_LENGTH);
					sprintf(writeChar,"sendMessage to [ %d ] [%d ] is %d\n",from.ip,from.port,issendA);
					Log.Log(writeChar);
					usleep(1);
					bool issendB = sendMessage(localPortSocket, (char*)(&messageVector[index]), mspSize, messageVector[index].ip, messageVector[index].port);
					memset(writeChar,0x00,MSG_TOTAL_LENGTH);
					sprintf(writeChar,"sendMessage to [ %d ] [%d ] is %d\n",
							messageVector[index].ip,messageVector[index].port,issendB);
					Log.Log(writeChar);
					//ipPortVec中删除配对成功的信息
					vit = find(ipPortVec.begin(),ipPortVec.end(),make_pair(from.ip, from.port));
					ipPortVec.erase(vit++);
					vit = find(ipPortVec.begin(),ipPortVec.end(),make_pair(messageVector[index].ip, messageVector[index].port));
					ipPortVec.erase(vit++);
					totalSendNumber +=2;
				}//end if
				else //未找到配对信息
				{
					//如果消息队列的数量没有到达上限
					if ( AvailabeAddress.size() != 0)
					{
						//收到的消息加入到消息队列后，不可用地址的map增加1,可用地址的map减1
						map<unsigned int,int>::iterator it = AvailabeAddress.begin();
						int index = it->second;
						messageVector[index].msg = recvmsp;
						messageVector[index].msgTime = startTime;
						messageVector[index].ip = from.ip;
						messageVector[index].port = from.port;
						unAvailabeAddress.insert( make_pair(recvmsp.messageContent.pairingFlag, index));
						AvailabeAddress.erase( it++);
						cout<<"add message to list\n";
					}
					else
					{
						Log.Log("message list has reached the limit\n");
					}
				}
			}//end if
		}//end if
		else //接收消息失败
		{
			Log.Log("receive message false\n");
			time_t endTime = GetTime();
			for ( map<unsigned int,int>::iterator it = unAvailabeAddress.begin();it != unAvailabeAddress.end();)
			{
				int index = it->second;
				double diff = difftime( endTime, messageVector[index].msgTime);
				if ( diff >= timeOut)
				{
					Log.Log("time out\n");
					int index = it->second;
					unAvailabeAddress.erase(it++);
					AvailabeAddress.insert(make_pair( messageVector[index].msg.messageContent.pairingFlag, index));
				}
				else
				{
					++it;
				}
			}//end for		
		}//end else
	}//end while
}

//===========================================
//StartServer函数说明
//函数功能：***
//参数：    ***
//			***
//			***
//函数返回：***
//===========================================
bool 
StartServer( unsigned short localPort, double timeOut)
{
	//UDP服务器打开消息服务端口号进行监听
	Socket localPortSocket;
	localPortSocket = openPort( localPort, 0);
	bool isok = !( localPortSocket == INVALID_SOCKET);
    if (isok)
    {
    	//清空内存
		memset( messageVector, 0, sizeof(MessageContent)*MAG_NUMBER);
		//可用地址队列赋值
		for ( int i=0; i<MAG_NUMBER; i++)
		{
			AvailabeAddress.insert( make_pair(i,i));
		}
		try
		{
			char time[256];
			GetDate(&time[0]);
			char filename[512];
			sprintf(filename,"%d %s.log",localPort,time);
			Log.CreateFile(filename);
			WaitMessage( localPortSocket, timeOut);
		}
		catch (exception* e)
		{
			Log.CreateFile("WaitMessage false\n");
			isok = false;
		}
    }
    if ( localPortSocket != INVALID_SOCKET)
    {
    	CloseSocket( localPortSocket);
    }	
	return isok;
}

static char Usage[] =
	"Usage: MessageServer [options]\n"
	"-p  server port start number(like 10030)\n"
	"-t  timeout(like 4000)\n";

int main(int argc, char** argv)
{
	//解析参数
	if (argc == 1)
	{
		printf("%s",Usage);
		return -1;
	}
	u_16 port = 0;
	double timeout = 0;
	for (int arg = 1; arg < argc; arg++)
	{
		if ( !strcmp(argv[arg],"-p"))
		{
			arg++;
			if ( argc <= arg )
			{
				printf("%s",Usage);
				return -1;
			}
			port = strtol(argv[arg],NULL,10);
		}
		if(!strcmp(argv[arg],"-t"))
		{
			arg++;
			if ( argc <= arg )
			{
				printf("%s",Usage);
				return -1;
			}
			timeout = strtod(argv[2], NULL);
		}
	}
	StartServer(port,timeout);
	return 0;
}
