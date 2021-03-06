// BsdSocketClient.cpp
// Created by Robin Rowe on 11/27/2015
// Copyright (c) 2015 Robin.Rowe@CinePaint.org
// License open source MIT

#include "BsdSocketClient.h"
#include "Logger.h"

namespace portable 
{

bool BsdSocketClient::Open(const char* serverName,int serverPort,bool isReuseSocket)
{	puts("libunistd 1.1 " __DATE__ " " __TIME__);
	if(!serverName || !*serverName || !serverPort)
	{	errorMsg.Set("No server to open specified");
		return false;
	}
	socketfd=OpenSocket();
	if(socketfd == -1)			
	{	puts("OpenSocket failed");
		errorMsg.GetLastError();
		return false;
	}
	memset((char *) &server_sockaddr, 0, sizeof(server_sockaddr));
	server_sockaddr.sin_family = AF_INET;
	server_sockaddr.sin_port = htons((u_short) serverPort);  
//		server_sockaddr.sin_addr.S_un.S_addr = inet_addr(serverName);
	std::string hostname;
	if(!GetIp(serverName,hostname))
	{	hostname = serverName;
	}
	if(1!=inet_pton(AF_INET,hostname.c_str(),&server_sockaddr.sin_addr))
	{	puts("inet_pton failed");
		errorMsg.GetLastError();
		return false;
	}
	if(isReuseSocket)
	{	const bool ok = SetReuse(socketfd);
		if(!ok)
		{	puts("Can't reuse socket");
	}	}
	const int ok = connect(socketfd, (struct sockaddr*)&server_sockaddr, sizeof(server_sockaddr));
	if(ok<0) 
	{	puts("connect failed");
		errorMsg.GetLastError();
		isGo=false;
		return false;
	}
	SetReuse(socketfd);
	Start();
	return true;
}

void BsdSocketClient::Run()
{	isGo = true;
	std::unique_ptr<char[]> buffer(new char[bufsize]);
	PacketReader packet(buffer.get(),bufsize);
	unsigned totalBytes = 0;
	while(isGo)
	{	const int recvBytes = RecvFrom(buffer.get(),bufsize-totalBytes,totalBytes);
		if(recvBytes<=0)
		{	printf("ERROR: socket received %i bytes (closed)\n",recvBytes);
			Stop();
			continue;
		}
		totalBytes += recvBytes;
		const unsigned consumedBytes = OnPacket(totalBytes,packet);
		if(!consumedBytes)
		{	continue;
		}
		if(consumedBytes==totalBytes)
		{	totalBytes = 0;
		}
		else
		{	const unsigned remainingBytes = totalBytes - consumedBytes;
			// printf("memmove(buffer,buffer+%u,%u)\n",consumedBytes,remainingBytes);
			memmove(buffer.get(),buffer.get()+consumedBytes,remainingBytes);
			totalBytes = remainingBytes;
		}
	}	
	OnStop();
}

unsigned BsdSocketClient::OnPacket(unsigned bytes,portable::PacketReader& packet)
{	
#if 0
	printf("bytes: %i packetSize: %i\n",bytes,packetSize);
#endif
	//LogMsg("Receive packet");
	while(bytes)
	{	if(!packet.ReadPacketHeader(bytes))
		{	stats.fragments++;
			return bytes;
		}
#if 1
		if(!stats.GetLast())
#endif
		{	printf("reading packet #%u\n",packet.header.packetId);
		}
		if(!packet.header.packetSize)
		{	LogError("invalid packet");
			return bytes;
		}
		if(0==packet.header.packetId)
		{	LogMsg("Reading header");
			if(!ReadHeader(packet))
			{	//stats.Print(packet.header.packetId,bytes,packet.header.packetSize, capacity);
				SocketReset("Packet header corrupted",packet);
				return 0;
		}	}
		else
		{//	LogMsg("Reading frame");
			ReadFrame(packet,packet.header.packetId);
		}
		stats.Transmit(packet.header.packetId);
#if 0
		const unsigned readOffset=packet.GetReadOffset();
		if(readOffset!=packetSize || bytes<packetSize)
		{	std::string s("readOffset/packetSize = ");
			s+=std::to_string(readOffset);
			s+="/";
			s+=std::to_string(packetSize);
			puts(s.c_str());
		}
#endif
		bytes-=packet.header.packetSize;
		if(!bytes)
		{	return 0;
		}
		stats.pipelined++;
		packet.NextInPipeline();
#if 0
		std::string msg("Pipelining #");
		msg+=std::to_string(packetId);
		msg+=": ";
		msg+=std::to_string(bytes);
		msg+=" of ";
		msg+=std::to_string(packetSize);
		puts(msg.c_str());
#endif
		if(bytes<=sizeof(unsigned))
		{//	stats.Print(packetId,bytes,packetSize, capacity);
			SocketReset("Packet receive underflow bytes",packet);
			return bytes;
		}
//		packet>>packetSize;
//		packetSize=packet.GetPacketSize(bytes);
#if 0
		msg="Pipelined packetSize = ";
		msg+=std::to_string(packetSize);
		puts(msg.c_str());
#endif
	}
	return 0;
}

void BsdSocketClient::SocketReset(const char* msg,portable::PacketReader& packet)
{	(void)msg;
    packet.Dump();
	BsdSocket bsdSocket(socketfd);
	bsdSocket.Close();
	socketfd = 0;
	Stop();
}

}
