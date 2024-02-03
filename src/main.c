#include "../include/malloc.c"
#include "../include/mem.c"
#include "../include/iformat.c"
#include "../include/lwp.c"
#include "../include/dirent.c"
#include "../include/signal.c"
#include "../include/socket.c"
#include "../include/poll.c"
#include "../include/stat.c"
char server_root[16384];
int server_sock;
void fatal(char *str)
{
	write(1,"Error: ",7);
	write(1,str,strlen(str));
	write(1,"\n",1);
	exit(1);
}
void buf_gets(char *buf,int len)
{
	int l;
	l=read(0,buf,len);
	if(l>0)
	{
		buf[l-1]=0;
	}
	else
	{
		buf[0]=0;
	}
}
int string_to_addr(char *str,struct sockaddr_in *addr)
{
	char *p,c;
	long val;
	int digits,state;
	p=str;
	digits=0;
	state=1;
	while(c=*p)
	{
		if(c>='0'&&c<='9')
		{
			++digits;
			if(digits>5)
			{
				return 1;
			}
			state=0;
		}
		else if(c=='.'||c==':')
		{
			digits=0;
			if(state)
			{
				return 1;
			}
			state=1;
		}
		else
		{
			return 1;
		}
		++p;
	}
	p=sinputi(str,&val);
	if(*p!='.'||val>255)
	{
		return 1;
	}
	addr->sin_addr|=val;
	p=sinputi(p+1,&val);
	if(*p!='.'||val>255)
	{
		return 1;
	}
	addr->sin_addr|=val<<8;
	p=sinputi(p+1,&val);
	if(*p!='.'||val>255)
	{
		return 1;
	}
	addr->sin_addr|=val<<16;
	p=sinputi(p+1,&val);
	if(*p!=':'||val>255)
	{
		return 1;
	}
	addr->sin_addr|=val<<24;
	p=sinputi(p+1,&val);
	if(*p||val>65535)
	{
		return 1;
	}
	addr->sin_port=htons(val);
	addr->sin_family=AF_INET;
	return 0;
}
int sock_read(int sock,void *buf,int size)
{
	struct pollfd pfd;
	int ret;
	pfd.fd=sock;
	pfd.events=POLLIN;
	pfd.revents=0;
	if(poll(&pfd,1,15000)==1&&pfd.revents&POLLIN)
	{
		ret=read(sock,buf,size);
		if(ret<0)
		{
			return 0;
		}
		return ret;
	}
	return 0;
}
int sock_write(int sock,void *buf,int size)
{
	struct pollfd pfd;
	int ret;
	pfd.fd=sock;
	pfd.events=POLLOUT;
	pfd.revents=0;
	if(poll(&pfd,1,15000)==1&&pfd.revents&POLLOUT)
	{
		ret=write(sock,buf,size);
		if(ret<0)
		{
			return 0;
		}
		return ret;
	}
	return 0;
}
void sock_clean(int sock)
{
	char buf[1024];
	while(sock_read(sock,buf,1024)!=0);
}

#include "server.c"
void server_run(void)
{
	long csock;
	while(1)
	{
		csock=accept(server_sock,NULL,NULL);
		if(csock>=0)
		{
			if(!valid(create_lwp(32768,T_service,(void *)csock)))
			{
				close(csock);
			}
		}
	}
}
void server_init(void)
{
	char addr_string[32];
	static struct sockaddr_in addr;
	struct stat st;
	write(1,"Server root directory path: ",28);
	buf_gets(server_root,16382);
	if(stat(server_root,&st)||(st.mode&0170000)!=STAT_DIR)
	{
		fatal("Cannot open directory");
	}
	write(1,"Server address (format: 127.0.0.1:80): ",39);
	buf_gets(addr_string,32);
	if(string_to_addr(addr_string,&addr))
	{
		fatal("Invalid address");
	}
	server_sock=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	if(server_sock<0)
	{
		fatal("Failed to initialize the server");
	}
	if(bind(server_sock,&addr,sizeof(addr)))
	{
		close(server_sock);
		fatal("Failed to initialize the server");
	}
	if(listen(server_sock,128))
	{
		close(server_sock);
		fatal("Failed to initialize the server");
	}
	write(1,"Now you can access your files through http://",45);
	write(1,addr_string,strlen(addr_string));
	write(1,"\n",1);
}

int main(void)
{
	server_init();
	server_run();
}
