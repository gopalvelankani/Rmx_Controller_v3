#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <jsoncpp/json/json.h>
#define SOCKET_INVALID(s) (s<0)
#define SOCKET_TYPE int
#define STX1 0x4D
#define STX2 0x56
#define STX3 0x44
#define UDPNUMBER 0x01
#define UNUSED 0x01
#define CHIPSELECT 0x02
#define READMODE 0x01
#define WRITEMODE 0xF0
#define CMD_GET_UDPCHANNELS 0x34
#define UDP_PORT_SOURCE 0x28
#define UDP_PORT_DESTINATION 0x2C
#define UDP_CHANNEL_NUMBER 0x30
#define IGMP_CHANNEL_NUMBER 0x20
#define CMD_GET_VERSION_OF_CORE 0x08




#define CMD_SET_DESTINATION_IP 0x24
#define CMD_SET_SUBNET_MASK 0x3C
#define CMD_SET_IP_GATEWAY 0x38
#define CMD_GET_DHCP_IP_GATEWAY 0x40
#define CMD_GET_DHCP_IP_MASK 0x44
#define CMD_SET_IP_MULTICAST 0x1C
#define CMD_GET_FPGA_IP 0x14
#define CMD_SET_IP_DHCP 0x18
#define CMD_SET_FPGA_MAC_LSB 0x0C
#define CMD_SET_FPGA_MAC_MSB 0x10
#define ETX 0x03
#define BUFFER_LENGTH  1024
#define UDP_PORT_SOURCE 0x28
#define UDP_PORT_DESTINATION 0x2C


#define ETX 0x03
#define BUFFER_LENGTH  1024

#define MMSB_32_8(x)		(x&0xFF000000)>>24
#define MLSB_32_8(x)		0xFF&((x&0x00FF0000)>>16)
#define LMSB_32_8(x)		0xFF&((x&0x0000FF00)>>8)
#define LLSB_32_8(x)		0xFF&((x&0x000000FF))

class Udpipstack{


   	public: void echo(char c){
   	 	std::cout<<c<<std::endl;
   	}
   	struct sockaddr_in serverAddress,
        clientAddress;
	unsigned short Port = 62000;
	unsigned short port = 62000;
    unsigned char msgBuf[20];
	//unsigned char RxBuffer[15]={0};
	    unsigned short len=0;
		unsigned char MAJOR;
		unsigned char MINOR;
		unsigned char INPUT;
		unsigned char OUTPUT;
		unsigned short FIFOSIZE;
		unsigned char PRESENCE_SFN;
	double clk;
	
	char hexmap[16] = {'0', '1', '2', '3', '4', '5', '6', '7',
                           '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
	
	int callCommand(int cmd,unsigned char* RxBuffer,int rx_len,int msgBLen,Json::Value json,int readWriteMode)
	{
		std::cout << "gimme something: ";
		// RxBuffer1[9] = (char)'a';
		unsigned char msgBuf[msgBLen];
		unsigned short len=0;
		 struct timeval tv;

		tv.tv_sec = 1;  /* 1 Secs Timeout */
		tv.tv_usec = 0;
		SOCKET_TYPE socketHandle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP/* or 0? */);
		setsockopt(socketHandle, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,sizeof(struct timeval));
	    if (SOCKET_INVALID(socketHandle))
	    {
	        std::cerr << "could not make socket\n";
	        return 0;
	    }
	    memset(&serverAddress, 0, sizeof(serverAddress));
	    memset(&clientAddress, 0, sizeof(clientAddress));
	    serverAddress.sin_family = AF_INET;
	    serverAddress.sin_port = htons(Port);
	    serverAddress.sin_addr.s_addr = inet_addr("192.168.1.20");

	    clientAddress.sin_family = AF_INET;
		clientAddress.sin_addr.s_addr = htonl(INADDR_ANY);
	    clientAddress.sin_port = htons(port);
	   
		if(bind(socketHandle,(struct sockaddr*)&clientAddress,sizeof(clientAddress))<0){
			std ::cout << "bind failed";
		}
		std::cout << "gimme something: ";
		std::cout << sizeof(RxBuffer);
		len = 0;
	    msgBuf[0] = STX1;
	    msgBuf[1] = STX2;
		msgBuf[2] = STX3;
	    msgBuf[3] = UDPNUMBER;
	    msgBuf[4] = UNUSED;
	    msgBuf[5] = CHIPSELECT;
		unsigned short uProg;
	
	    switch(cmd){
	    	case 8:
			    if(readWriteMode==0){
					msgBuf[6] = READMODE;
				    msgBuf[7] = CMD_GET_VERSION_OF_CORE;
				    msgBuf[8] = 0x00;//dont care
				    msgBuf[9] = 0x00;//dont care
				    msgBuf[10]= 0x00;//dont care
				    msgBuf[11]= 0x00;//dont care
				}else{
					unsigned short address=(unsigned short)std::stoi(json["address"].asString());
					msgBuf[6] = WRITEMODE;
				    msgBuf[7] = CMD_GET_VERSION_OF_CORE;
				    msgBuf[8] = 0x00;//dont care
				    msgBuf[9] = 0x00;//dont care
				    msgBuf[10]= (unsigned char)(address >> 8);//dont care
				    msgBuf[11]= (unsigned char)address;//dont care
				}
				break;
			case 16:
				if(readWriteMode==0){
					msgBuf[6] = READMODE;
				    msgBuf[7] = CMD_SET_FPGA_MAC_MSB;
				    msgBuf[8] = 0x00;//dont care
				    msgBuf[9] = 0x00;//dont care 
				    msgBuf[10]= 0x00;//dont care
				    msgBuf[11]= 0x00;//dont care	
				}else{
					string tmp2=json["mac_msb"].asString();
					msgBuf[6] = WRITEMODE;
				    msgBuf[7] = CMD_SET_FPGA_MAC_MSB;
				    msgBuf[8] = 0x00;  
					msgBuf[9] = 0x00; 
					int j=10;
				    for (int i = 0; i < 4; i=i+2)
				    {
				        std::string byte=tmp2.substr(i, 2);
				        std::cout<<byte<<std::endl;
						msgBuf[j] = (unsigned char) (int)strtol(byte.c_str(), NULL, 16); 
						std::cout<<j<<std::endl;
						j++;
				    }
				}
				break;
			case 12:
				if(readWriteMode==0){
					msgBuf[6] = READMODE;
				    msgBuf[7] = CMD_SET_FPGA_MAC_LSB;
				    msgBuf[8] = 0x00;//dont care
				    msgBuf[9] = 0x00;//dont care 
				    msgBuf[10]= 0x00;//dont care
				    msgBuf[11]= 0x00;//dont care	

				}else{
					std::string tmp2=json["mac_lsb"].asString();
					msgBuf[6] = WRITEMODE;
				    msgBuf[7] = CMD_SET_FPGA_MAC_LSB;
				    int j=8;
				    for (int i = 0; i < 8; i=i+2)
				    {
				        std::string byte=tmp2.substr(i, 2);
				        std::cout<<byte<<std::endl;
						msgBuf[j] = (unsigned char) (int)strtol(byte.c_str(), NULL, 16); 
						std::cout<<j<<std::endl;
						j++;
				    }
				}
				break;
			case 14:
					msgBuf[6] = READMODE;
				    msgBuf[7] = CMD_GET_FPGA_IP;
				    msgBuf[8] = 0x00;//dont care
				    msgBuf[9] = 0x00;//dont care
				    msgBuf[10]= 0x00;//dont care
				    msgBuf[11]= 0x00;//dont care	
				break;
			case 18:
				if(readWriteMode==0){
					msgBuf[6] = READMODE;
				    msgBuf[7] = CMD_SET_IP_DHCP;
				    msgBuf[8] = 0x00;//dont care
				    msgBuf[9] = 0x00;//dont care 
				    msgBuf[10]= 0x00;//dont care
				    msgBuf[11]= 0x00;//dont care	

				}else{
					string ip_address=json["ip_address"].asString();
					std::stringstream s(ip_address);
					int a,b,c,d; //to store the 4 ints
					char ch; //to temporarily store the '.'
					s >> a >> ch >> b >> ch >> c >> ch >> d;
					std::cout << a << "  " << b << "  " << c << "  "<< d;
					
					// unsigned int Data=(unsigned int)std::stoi(tmp2);
					msgBuf[6] = WRITEMODE;
				    msgBuf[7] = CMD_SET_IP_DHCP;
				    msgBuf[8] = a;  
					msgBuf[9] = b; 
					msgBuf[10] = c; 
					msgBuf[11] = d;
				}
				break;
			case 24:
				if(readWriteMode==0){
					msgBuf[6] = READMODE;
				    msgBuf[7] = CMD_SET_DESTINATION_IP;
				    msgBuf[8] = 0x00;//dont care
				    msgBuf[9] = 0x00;//dont care 
				    msgBuf[10]= 0x00;//dont care
				    msgBuf[11]= 0x00;//dont care	
				}else{
					string ip_address=json["ip_address"].asString();
					std::stringstream s(ip_address);
					unsigned int a,b,c,d; //to store the 4 ints
					unsigned char ch; //to temporarily store the '.'
					s >> a >> ch >> b >> ch >> c >> ch >> d;
					std::cout << a << "  " << b << "  " << c << "  "<< d;
					msgBuf[6] = WRITEMODE;
				    msgBuf[7] = CMD_SET_DESTINATION_IP;
				    msgBuf[8] = a;  
					msgBuf[9] = b; 
					msgBuf[10] = c; 
					msgBuf[11] = d;
				}
				break;
			case 28:
				if(readWriteMode==0){
					msgBuf[6] = READMODE;
				    msgBuf[7] = UDP_PORT_SOURCE;
				    msgBuf[8] = 0x00;//dont care
				    msgBuf[9] = 0x00;//dont care
				    msgBuf[10]= 0x00;//dont care
				    msgBuf[11]= 0x00;//dont care
				}else{
					unsigned short address=(unsigned short)std::stoi(json["address"].asString());
					msgBuf[6] = WRITEMODE;
				    msgBuf[7] = UDP_PORT_SOURCE;
				    msgBuf[8] = 0x00;//dont care
				    msgBuf[9] = 0x00;//dont care
				    msgBuf[10]= (unsigned char)(address >> 8);//dont care
				    msgBuf[11]= (unsigned char)address;//dont care
				}
				break;
			case 29:
				if(readWriteMode==0){
					msgBuf[6] = READMODE;
				    msgBuf[7] = CMD_SET_IP_MULTICAST;
				    msgBuf[8] = 0x00;//dont care
				    msgBuf[9] = 0x00;//dont care 
				    msgBuf[10]= 0x00;//dont care
				    msgBuf[11]= 0x00;//dont care	
				}else{
					string ip_address=json["ip_address"].asString();
					std::stringstream s(ip_address);
					int a,b,c,d; //to store the 4 ints
					char ch; //to temporarily store the '.'
					s >> a >> ch >> b >> ch >> c >> ch >> d;
					std::cout << a << "  " << b << "  " << c << "  "<< d;
					msgBuf[6] = WRITEMODE;
				    msgBuf[7] = CMD_SET_IP_MULTICAST;
				    msgBuf[8] = a;  
					msgBuf[9] = b; 
					msgBuf[10] = c; 
					msgBuf[11] = d;
				}
				break;
			case 32:
				if(readWriteMode==0){
					msgBuf[6] = READMODE;
				    msgBuf[7] = IGMP_CHANNEL_NUMBER;
				    msgBuf[8] = 0x00;//dont care
				    msgBuf[9] = 0x00;//dont care
				    msgBuf[10]= 0x00;//dont care
				    msgBuf[11]= 0x00;//dont care
				}else{
					unsigned short channel=(unsigned short)std::stoi(json["ch_number"].asString());
					msgBuf[6] = WRITEMODE;
				    msgBuf[7] = IGMP_CHANNEL_NUMBER;
				    msgBuf[8] = 0x00;//dont care
				    msgBuf[9] = 0x00;//dont care
				    msgBuf[10]= 0x00;//dont care
				    msgBuf[11]= (unsigned char)channel;//dont care
				}
				break;
				
			case 34:
				msgBuf[6] = READMODE;
			    msgBuf[7] = CMD_GET_UDPCHANNELS;
			    msgBuf[8] = 0x00;//dont care
			    msgBuf[9] = 0x00;//dont care
			    msgBuf[10]= 0x00;//dont care
			    msgBuf[11]= 0x00;//dont care	
				break;

			case 38:
				if(readWriteMode==0){
					msgBuf[6] = READMODE;
				    msgBuf[7] = CMD_SET_IP_GATEWAY;
				    msgBuf[8] = 0x00;//dont care
				    msgBuf[9] = 0x00;//dont care 
				    msgBuf[10]= 0x00;//dont care
				    msgBuf[11]= 0x00;//dont care	
				}else{
					string ip_address=json["ip_address"].asString();
					std::stringstream s(ip_address);
					int a,b,c,d; //to store the 4 ints
					char ch; //to temporarily store the '.'
					s >> a >> ch >> b >> ch >> c >> ch >> d;
					std::cout << a << "  " << b << "  " << c << "  "<< d;
					msgBuf[6] = WRITEMODE;
				    msgBuf[7] = CMD_SET_IP_GATEWAY;
				    msgBuf[8] = a;  
					msgBuf[9] = b; 
					msgBuf[10] = c; 
					msgBuf[11] = d;
				}
				break;
			case 40:
				msgBuf[6] = READMODE;
			    msgBuf[7] = CMD_GET_DHCP_IP_GATEWAY;
			    msgBuf[8] = 0x00;//dont care
			    msgBuf[9] = 0x00;//dont care
			    msgBuf[10]= 0x00;//dont care
			    msgBuf[11]= 0x00;//dont care	
				break;
			case 44:
				if(readWriteMode==0){
					msgBuf[6] = READMODE;
				    msgBuf[7] = UDP_PORT_DESTINATION;
				    msgBuf[8] = 0x00;//dont care
				    msgBuf[9] = 0x00;//dont care
				    msgBuf[10]= 0x00;//dont care
				    msgBuf[11]= 0x00;//dont care
				}else{
					unsigned short address=(unsigned short)std::stoi(json["address"].asString());
					msgBuf[6] = WRITEMODE;
				    msgBuf[7] = UDP_PORT_DESTINATION;
				    msgBuf[8] = 0x00;//dont care
				    msgBuf[9] = 0x00;//dont care
				    msgBuf[10]= (unsigned char)(address >> 8);//dont care
				    msgBuf[11]= (unsigned char)address;//dont care
				}
				break;
			case 45:
				if(readWriteMode==0){
					msgBuf[6] = READMODE;
				    msgBuf[7] = UDP_PORT_DESTINATION;
				    msgBuf[8] = 0x00;//dont care
				    msgBuf[9] = 0x00;//dont care
				    msgBuf[10]= 0x00;//dont care
				    msgBuf[11]= 0x00;//dont care
				}else{
					unsigned short address=(unsigned short)std::stoi(json["address"].asString());
					msgBuf[6] = WRITEMODE;
				    msgBuf[7] = UDP_PORT_DESTINATION;
				    msgBuf[8] = 0x00;//dont care
				    msgBuf[9] = 0x00;//dont care
				    msgBuf[10]= (unsigned char)(address >> 8);//dont care
				    msgBuf[11]= (unsigned char)address;//dont care
				}
				break;
			case 48:
				if(readWriteMode==0){
					msgBuf[6] = READMODE;
				    msgBuf[7] = UDP_CHANNEL_NUMBER;
				    msgBuf[8] = 0x00;//dont care
				    msgBuf[9] = 0x00;//dont care
				    msgBuf[10]= 0x00;//dont care
				    msgBuf[11]= 0x00;//dont care
				}else{
					unsigned short ch_number=(unsigned short)std::stoi(json["ch_number"].asString());
					msgBuf[6] = WRITEMODE;
				    msgBuf[7] = UDP_CHANNEL_NUMBER;
				    msgBuf[8] = 0x00;//dont care
				    msgBuf[9] = 0x00;//dont care
				    msgBuf[10]= 0x00;//dont care
				    msgBuf[11]= (unsigned char)ch_number;//dont care
				}
				break;
			
			case 52:

				msgBuf[6] = READMODE;
			    msgBuf[7] = CMD_GET_UDPCHANNELS;
			    msgBuf[8] = 0x00;//dont care
			    msgBuf[9] = 0x00;//dont care
			    msgBuf[10]= 0x00;//dont care
			    msgBuf[11]= 0x00;//dont care	
				break;
			case 60:
				if(readWriteMode==0){
					msgBuf[6] = READMODE;
				    msgBuf[7] = CMD_SET_SUBNET_MASK;
				    msgBuf[8] = 0x00;//dont care
				    msgBuf[9] = 0x00;//dont care 
				    msgBuf[10]= 0x00;//dont care
				    msgBuf[11]= 0x00;//dont care	
				}else{
					string ip_address=json["ip_address"].asString();
					std::stringstream s(ip_address);
					unsigned int a,b,c,d; //to store the 4 ints
					unsigned char ch; //to temporarily store the '.'
					s >> a >> ch >> b >> ch >> c >> ch >> d;
					std::cout << a << "  " << b << "  " << c << "  "<< d;
					// unsigned int Data=(unsigned int)std::stoi(tmp2);
					msgBuf[6] = WRITEMODE;
				    msgBuf[7] = CMD_SET_SUBNET_MASK;
				    msgBuf[8] = a;  
					msgBuf[9] = b; 
					msgBuf[10] = c; 
					msgBuf[11] = d;
				
				}
				break;
			case 90:
				unsigned int Address=(unsigned int)std::stoi(json["address"].asString());
	    		unsigned char CMP_ID=(unsigned char)std::stoi(json["cs"].asString());
	    		unsigned char data=(unsigned char)std::stoi(json["data"].asString());
	    		msgBuf[3] = UNUSED;
			    msgBuf[4] = (0xff00&CMP_ID)>>8;
			    msgBuf[5] = (0x00ff&CMP_ID)>>0;
				if(readWriteMode==0){
					msgBuf[6] = READMODE;
				    msgBuf[7] = Address;
				    msgBuf[8] = 0x00;//dont care
				    msgBuf[9] = 0x00;//dont care 
				    msgBuf[10]= 0x00;//dont care
				    msgBuf[11]= 0x00;//dont care	
				}else{
					msgBuf[6] = WRITEMODE;
				    msgBuf[7] = Address;
				    msgBuf[8] = MMSB_32_8(data);  
					msgBuf[9] = MLSB_32_8(data); 
					msgBuf[10]= LMSB_32_8(data); 
					msgBuf[11]= LLSB_32_8(data);
				
				}
				break;
			

	    }
		len += 12;
		int x=0;
		int count;		
		while (x == 0)
		{ 	
					int n = sendto(socketHandle,msgBuf, len, 0,(struct sockaddr*)&serverAddress, sizeof(serverAddress));
					std::cout << "sent "<<n<<" bytes of the msg\n";
			
				
					int serv_addr_size = sizeof(clientAddress);
					count=recvfrom(socketHandle,RxBuffer,rx_len, 0, (struct sockaddr*)&clientAddress,(socklen_t*) &serv_addr_size);

					std::cout << "sent "<<n<<" bytes of the msg\n";
					std ::cout << count<<"\n";

					// std ::cout << std::to_string(RxBuffer[0])+" -- "+std::to_string(RxBuffer[3]);
					for(int i=0;i<12;i++){
						std ::cout << hexStr(&RxBuffer[i],1)<<"\n" ;
						
						// std::cout << string2long(hexStr(&RxBuffer[i],1), 16) << std::endl;
					}
					std::cout << "done\n";
					x=1;
		}
		
		close(socketHandle);
	    
		
	    return count;
	}
	
std::string hexStr(unsigned char *data, int len)
{
  std::string s(len * 2, ' ');
  for (int i = 0; i < len; ++i) {
    s[2 * i]     = hexmap[(data[i] & 0xF0) >> 4];
    s[2 * i + 1] = hexmap[data[i] & 0x0F];
  }
  return s;
}

long string2long(const std::string &str, int base = 10)
{
	long result = 0;

	if (base < 0 || base > 36)
		return result;

	for (size_t i = 0, mx = str.size(); i < mx; ++i)
	{
		char ch = tolower(str[i]);
		int num = 0;

		if ('0' <= ch && ch <= '9')
			num = (ch - '0');
		else if ('a' <= ch && ch <= 'z')
			num = (ch - 'a') + 10;
		else
			break;

		if (num > base)
			break;

		result = result*base + num;
	}

	return result;
}

};
