#ifndef _SOCKET_C_
#define _SOCKET_C_
#define AF_UNIX 1
#define AF_INET 2
#define AF_INET6 10
#define AF_NETLINK 16
#define AF_PACKET 17
#define AF_ALG 38

#define SOCK_STREAM 1
#define SOCK_DGRAM 2
#define SOCK_RAW 3
#define SOCK_SEQPACKET 5
#define SOCK_CLOEXEC 02000000
#define SOCK_NONBLOCK 04000

#define IPPROTO_TCP 6

struct sockaddr_un
{
	unsigned short int family;
	char sun_path[108];
};
struct sockaddr_in
{
	unsigned short int sin_family;
	unsigned short int sin_port;
	unsigned int sin_addr;
	char pad[8];
};

#define htons(n) ((unsigned short)(n)<<8|(unsigned short)(n)>>8)

#define MSG_DONTWAIT 0x40

#endif
