char *read_header(int sock,int *status)
{
	char *buf;
	int x,state;
	buf=malloc(8192);
	if(buf==NULL)
	{
		*status=2; // server error
		return NULL;
	}
	x=0;
	state=0;
	while(1)
	{
		if(sock_read(sock,buf+x,1)!=1)
		{
			free(buf);
			*status=3;
			return NULL;
		}
		if(buf[x]=='\r')
		{
			++state;
		}
		else if(buf[x]=='\n')
		{
			if(state==2)
			{
				buf[x+1]=0;
				return buf;
			}
		}
		else
		{
			state=0;
		}
		++x;
		if(x==8191)
		{
			free(buf);
			*status=1; // client error
			return NULL;
		}
	}
}
void send_page_302(int sock)
{
	char *msg;
	msg="HTTP/1.1 302 Redirect\r\n\
Connection: close\r\n\
Location: /\r\n\
Content-Length: 0\r\n\
\r\n";
	sock_write(sock,msg,strlen(msg));
}
void send_page_400(int sock)
{
	char *msg;
	msg="HTTP/1.1 400 Bad Request\r\n\
Connection: close\r\n\
Content-Type: text/html\r\n\
Content-Length: 94\r\n\
\r\n\
<html><head><title>400 Bad Request</title></head><body><h1>400 Bad Request</h1></body></html>\n";
	sock_write(sock,msg,strlen(msg));
}
void send_page_404(int sock)
{
	char *msg;
	msg="HTTP/1.1 404 Not Found\r\n\
Connection: close\r\n\
Content-Type: text/html\r\n\
Content-Length: 90\r\n\
\r\n\
<html><head><title>404 Not Found</title></head><body><h1>404 Not Found</h1></body></html>\n";
	sock_write(sock,msg,strlen(msg));
}
void send_page_500(int sock)
{
	char *msg;
	msg="HTTP/1.1 500 Internal Server Error\r\n\
Connection: close\r\n\
Content-Type: text/html\r\n\
Content-Length: 114\r\n\
\r\n\
<html><head><title>500 Internal Server Error</title></head><body><h1>500 Internal Server Error</h1></body></html>\n";
	sock_write(sock,msg,strlen(msg));
}
int is_valid_char(char c)
{
	if(c>='A'&&c<='Z'||c>='a'&&c<='z'||c>='0'&&c<='9'||c=='.'||c=='/'||c=='-'||c=='_')
	{
		return 1;
	}
	return 0;
}
int is_valid_name(char *str)
{
	char c;
	while(c=*str)
	{
		if(!is_valid_char(c))
		{
			return 0;
		}
		++str;
	}
	return 1;
}
char *get_real_path(char *path,int pathlen,int *status)
{
	char *real_path,*p;
	int x,x1,i;
	char name[256];
	if(pathlen<=0)
	{
		*status=1;
		return NULL;
	}
	real_path=malloc(pathlen+strlen(server_root)+1040);
	if(real_path==NULL)
	{
		*status=2;
		return NULL;
	}
	strcpy(real_path,server_root);
	strcat(real_path,"/");
	p=real_path+strlen(real_path);
	x=0;
	while(x<pathlen)
	{
		while(x<pathlen&&path[x]=='/')
		{
			++x;
		}
		x1=0;
		while(x<pathlen&&path[x]&&path[x]!='/')
		{
			name[x1]=path[x];
			++x;
			++x1;
			if(x1==256)
			{
				free(real_path);
				*status=1;
				return NULL;
			}
		}
		name[x1]=0;
		if(x1)
		{
			if(!strcmp(name,".."))
			{
				i=strlen(p);
				while(i&&p[i-1]!='/')
				{
					--i;
				}
				p[i]=0;
			}
			else if(strcmp(name,"."))
			{
				if(*p)
				{
					strcat(p,"/");
				}
				strcat(p,name);
			}
		}
	}
	return real_path;
}
struct file_entry
{
	struct file_entry *next;
	char name[768];
	long isdir;
};
void release_file_entries(struct file_entry *entries)
{
	struct file_entry *p;
	while(p=entries)
	{
		entries=p->next;
		free(p);
	}
}
void handle_get(int sock,char *header)
{
	int i,status;
	char c;
	char *real_path;
	char *rpath;
	char buf[4096];
	long size;
	int fd,dirfd;
	struct stat st;
	struct DIR db;
	struct dirent *dir;
	struct file_entry *head,*node,*p,*pp;
	i=4;
	while(c=header[i])
	{
		if(!is_valid_char(c))
		{
			if(i==4||strncmp(header+i," HTTP/1.",8))
			{
				send_page_404(sock);
				return;
			}
			break;
		}
		++i;
	}
	real_path=get_real_path(header+4,i-4,&status);
	if(real_path==NULL)
	{
		if(status==1)
		{
			send_page_404(sock);
		}
		else
		{
			send_page_500(sock);
		}
		return;
	}
	rpath=real_path+strlen(server_root);
	if(lstat(real_path,&st))
	{
		free(real_path);
		send_page_404(sock);
		return;
	}
	if((st.mode&0170000)==STAT_DIR)
	{
//<html><head><title>[LocalShare] ...</title></head>\n
//<body><h1>[LocalShare] ...</h1>\n
//<form action="/$PUTF" enctype="multipart/form-data" method="post">\n
//Upload To <input type="text" name="FP"/><br/><input type="file" name="F" multiple/><br/><input type="submit" name="SM" value="Upload"/><br/></form>\n
//<p><a href=".../..">PARENT</a></p>\n
//...
//</body></html>\n

//<p>[FILE] <a href=".../file">file</a></p>\n
//<p>[DIR ] <a href=".../dir">dir</a></p>\n
		head=NULL;
		size=48+29+67+148+32+15+3*strlen(rpath);
		if(rpath[1]==0)
		{
			--size;
		}
		dirfd=open(real_path,0,0);
		if(dirfd<0)
		{
			free(real_path);
			send_page_500(sock);
			return;
		}
		dir_init(dirfd,&db);
		while(dir=readdir(&db))
		{
			if(fstatat(dirfd,dir->name,&st,AT_SYMLINK_NOFOLLOW)==0&&((st.mode&0170000)==STAT_DIR||(st.mode&0170000)==STAT_REG))
			{
				if(strcmp(dir->name,".")&&strcmp(dir->name,"..")&&is_valid_name(dir->name))
				{
					node=malloc(sizeof(*node));
					if(node==NULL)
					{
						release_file_entries(head);
						close(dirfd);
						free(real_path);
						send_page_500(sock);
						return;
					}
					strcpy(node->name,dir->name);
					node->isdir=0;
					if((st.mode&0170000)==STAT_DIR)
					{
						node->isdir=1;
					}
					p=head;
					pp=NULL;
					while(p)
					{
						if(strcmp(p->name,dir->name)>0)
						{
							break;
						}
						pp=p;
						p=p->next;
					}
					if(pp)
					{
						pp->next=node;
					}
					else
					{
						head=node;
					}
					node->next=p;
					size+=31+2*strlen(dir->name)+strlen(rpath);
					if(rpath[1]==0)
					{
						--size;
					}
				}
			}
		}
		close(dirfd);
		strcpy(buf,"HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\nContent-Length: ");
		sprinti(buf,size,1);
		strcat(buf,"\r\n\r\n");
		sock_write(sock,buf,strlen(buf));
		strcpy(buf,"<html><head><title>[LocalShare] ");
		sock_write(sock,buf,strlen(buf));
		sock_write(sock,rpath,strlen(rpath));
		strcpy(buf,"</title></head>\n<body><h1>[LocalShare] ");
		sock_write(sock,buf,strlen(buf));
		sock_write(sock,rpath,strlen(rpath));
		strcpy(buf,"</h1>\n<form action=\"/$PUTF\" enctype=\"multipart/form-data\" method=\"post\">\n\
Upload To <input type=\"text\" name=\"FP\"/><br/>\
<input type=\"file\" name=\"F\" multiple/><br/>\
<input type=\"submit\" name=\"SM\" value=\"Upload\"/><br/></form>\n\
<p><a href=\"");
		sock_write(sock,buf,strlen(buf));
		sock_write(sock,rpath,strlen(rpath));
		if(rpath[1])
		{
			sock_write(sock,"/",1);
		}
		strcpy(buf,"..\">PARENT</a></p>\n");
		sock_write(sock,buf,strlen(buf));
		p=head;
		while(p)
		{
			if(p->isdir)
			{
				strcpy(buf,"<p>[DIR ] <a href=\"");
			}
			else
			{
				strcpy(buf,"<p>[FILE] <a href=\"");
			}
			sock_write(sock,buf,strlen(buf));
			sock_write(sock,rpath,strlen(rpath));
			if(rpath[1])
			{
				sock_write(sock,"/",1);
			}
			sock_write(sock,p->name,strlen(p->name));
			sock_write(sock,"\">",2);
			sock_write(sock,p->name,strlen(p->name));
			strcpy(buf,"</a></p>\n");
			sock_write(sock,buf,strlen(buf));
			p=p->next;
		}
		strcpy(buf,"</body></html>\n");
		sock_write(sock,buf,strlen(buf));
		release_file_entries(head);
	}
	else if((st.mode&0170000)==STAT_REG)
	{
		if((fd=open(real_path,0,0))>=0)
		{
			size=lseek(fd,0,2);
			if(size<0)
			{
				close(fd);
				free(real_path);
				send_page_500(sock);
				return;
			}
			lseek(fd,0,0);
			strcpy(buf,"HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nConnection: close\r\nContent-Length: ");
			sprinti(buf,size,1);
			strcat(buf,"\r\n\r\n");
			sock_write(sock,buf,strlen(buf));
			while(size>0)
			{
				i=read(fd,buf,4096);
				if(i<=0)
				{
					memset(buf,0,4096);
					i=4096;
				}
				if(i>size)
				{
					i=size;
				}
				if(sock_write(sock,buf,i)==0)
				{
					break;
				}
				size-=i;
			}
			close(fd);
		}
		else
		{
			send_page_500(sock);
		}
	}
	else
	{
		send_page_404(sock);
	}
	free(real_path);
}
int mem_match(void *data,int datalen,void *target,int targetlen)
{
	int i;
	i=0;
	while(i<=datalen-targetlen)
	{
		if(((char *)data)[i]==*(char *)target)
		{
			if(!memcmp((char *)data+i,target,targetlen))
			{
				return i;
			}
		}
		++i;
	}
	return -1;
}
char *get_tmp_file(void)
{
	char *name;
	char buf[32];
	int i;
	unsigned int val;
	buf[0]='#';
	buf[31]=0;
	name=malloc(strlen(server_root)+48);
	if(name==NULL)
	{
		return NULL;
	}
	strcpy(name,server_root);
	strcat(name,"/");
	i=1;
	while(i<31)
	{
		getrandom(&val,4,1);
		val%=62;
		if(val<10)
		{
			val+='0';
		}
		else if(val<36)
		{
			val+='A'-10;
		}
		else
		{
			val+='a'-36;
		}
		buf[i]=val;
		++i;
	}
	strcat(name,buf);
	return name;
}

void handle_post(void *sock,char *header)
{
	char val;
	char *bound;
	char *real_path;
	char *tmp_file;
	char buf[8192];
	int i,bound_len,status,stage,size,read_size,ret,path_len;
	int fd;
	if(strncmp(header+5,"/$PUTF HTTP/1.",14))
	{
		send_page_400(sock);
		return;
	}
	bound=header+18;
	while(*bound)
	{
		if(*bound=='\r'&&!strncmp(bound,"\r\nContent-Type: multipart/form-data; boundary=",46))
		{
			bound+=46;
			break;
		}
		++bound;
	}
	if(!*bound)
	{
		send_page_400(sock);
		return;
	}
	bound_len=0;
	while(bound[bound_len]&&bound[bound_len]!='\r')
	{
		++bound_len;
	}
	if(bound_len<8||bound_len>2048)
	{
		send_page_400(sock);
		return;
	}
	tmp_file=get_tmp_file();
	if(tmp_file==NULL)
	{
		send_page_500(sock);
		return;
	}
	stage=0;
	size=0;
	read_size=0;
	send_page_302(sock);
	while(1)
	{
		memmove(buf,buf+read_size,size-read_size);
		size-=read_size;
		read_size=0;
		do
		{
			ret=sock_read(sock,buf+size,8192-size);
			if(ret<0)
			{
				ret=0;
			}
			size+=ret;
			if(mem_match(buf+read_size,size-read_size,bound,bound_len)>=0)
			{
				break;
			}
		}
		while(size<8192&&ret);
		ret=mem_match(buf+read_size,size-read_size,bound,bound_len);
		if(ret<0)
		{
			if(stage!=2||size<read_size+4+bound_len)
			{
				if(stage==1||stage==2)
				{
					free(real_path);
				}
				free(tmp_file);
				send_page_400(sock);
				return;
			}
			i=write(fd,buf+read_size,size-read_size-4-bound_len);
			if(i!=size-read_size-4-bound_len)
			{
				free(real_path);
				free(tmp_file);
				send_page_500(sock);
				return;
			}
			read_size+=size-read_size-4-bound_len;
		}
		else
		{
			if(stage==0)
			{
				if(ret!=2)
				{
					free(tmp_file);
					send_page_400(sock);
					return;
				}
				ret+=bound_len+2;
				if(memcmp(buf+read_size+ret,"Content-Disposition: form-data; ",32))
				{
					free(tmp_file);
					send_page_400(sock);
					return;
				}
				ret+=32;
				while(1)
				{
					if(read_size+ret+9>=size||buf[read_size+ret]=='\n')
					{
						free(tmp_file);
						send_page_400(sock);
						return;
					}
					if(buf[read_size+ret]=='n')
					{
						if(!memcmp(buf+read_size+ret,"name=\"",6))
						{
							if(!memcmp(buf+read_size+ret+6,"FP\"",3))
							{
								break;
							}
							else
							{
								free(tmp_file);
								send_page_400(sock);
								return;
							}
						}
					}
					++ret;
				}
				while(1)
				{
					if(read_size+ret+3>=size)
					{
						free(tmp_file);
						send_page_400(sock);
						return;
					}
					if(buf[read_size+ret]=='\n')
					{
						if(!memcmp(buf+read_size+ret,"\n\r\n",3))
						{
							break;
						}
					}
					++ret;
				}
				ret+=3;
				i=ret;
				while(read_size+ret<size&&buf[read_size+ret]!='\r')
				{
					++ret;
					if(i+1024<ret)
					{
						free(tmp_file);
						send_page_400(sock);
						return;
					}
				}
				if(read_size+ret+2>=size)
				{
					free(tmp_file);
					send_page_400(sock);
					return;
				}
				real_path=get_real_path(buf+read_size+i,ret-i,&status);
				if(real_path==NULL)
				{
					free(tmp_file);
					if(status==2)
					{
						send_page_500(sock);
					}
					else
					{
						send_page_400(sock);
					}
					return;
				}
				mkdir(real_path,0755);
				path_len=strlen(real_path);
				ret+=2;
				read_size+=ret;
				stage=1;
			}
			else if(stage==1)
			{
				if(ret!=2)
				{
					free(real_path);
					free(tmp_file);
					send_page_400(sock);
					return;
				}
				ret+=bound_len+2;
				if(memcmp(buf+read_size+ret,"Content-Disposition: form-data; ",32))
				{
					free(real_path);
					free(tmp_file);
					send_page_400(sock);
					return;
				}
				ret+=32;
				i=ret;
				while(1)
				{
					if(read_size+ret+8>=size||buf[read_size+ret]=='\n')
					{
						free(real_path);
						free(tmp_file);
						send_page_400(sock);
						return;
					}
					if(buf[read_size+ret]=='n')
					{
						if(!memcmp(buf+read_size+ret,"name=\"",6))
						{
							if(!memcmp(buf+read_size+ret+6,"F\"",2))
							{
								break;
							}
							else
							{
								free(real_path);
								free(tmp_file);
								return;
							}
						}
					}
					++ret;
				}
				ret=i;
				while(1)
				{
					if(read_size+ret+10>=size||buf[read_size+ret]=='\n')
					{
						free(real_path);
						free(tmp_file);
						send_page_400(sock);
						return;
					}
					if(buf[read_size+ret]=='f')
					{
						if(!memcmp(buf+read_size+ret,"filename=\"",10))
						{
							ret+=10;
							i=ret;
							while(read_size+ret<size&&buf[read_size+ret]!='\"')
							{
								if(buf[read_size+ret]=='/')
								{
									free(real_path);
									free(tmp_file);
									send_page_400(sock);
									return;
								}
								++ret;
							}
							if(read_size+ret>=size||ret-i>=256)
							{
								free(real_path);
								free(tmp_file);
								send_page_400(sock);
								return;	
							}
							buf[read_size+ret]=0;
							++ret;
							real_path[path_len]='/';
							real_path[path_len+1]=0;
							strcat(real_path,buf+read_size+i);
							break;
						}
					}
					++ret;
				}
				while(1)
				{
					if(read_size+ret+3>=size)
					{
						free(tmp_file);
						send_page_400(sock);
						return;
					}
					if(buf[read_size+ret]=='\n')
					{
						if(!memcmp(buf+read_size+ret,"\n\r\n",3))
						{
							break;
						}
					}
					++ret;
				}
				ret+=3;
				read_size+=ret;
				fd=open(tmp_file,578,0644);
				if(fd<0)
				{
					free(real_path);
					free(tmp_file);
					send_page_500(sock);
					return;
				}
				stage=2;
			}
			else if(stage==2)
			{
				i=write(fd,buf+read_size,ret-4);
				close(fd);
				if(i!=ret-4)
				{
					free(real_path);
					free(tmp_file);
					send_page_500(sock);
					return;
				}
				rename(tmp_file,real_path);
				stage=1;
				read_size+=ret-2;
			}
		}
	}
}
int T_service(void *arg)
{
	int status;
	char *header;
	int sock;
	sock=(long)arg;
	header=read_header(sock,&status);
	if(header==NULL)
	{
		if(status==1)
		{
			send_page_400(sock);
		}
		else if(status==2)
		{
			send_page_500(sock);
		}
		if(status!=3)
		{
			sock_clean(sock);
		}
		close(sock);
		return 0;
	}
	if(!strncmp(header,"GET ",4))
	{
		handle_get(sock,header);
	}
	else if(!strncmp(header,"POST ",5))
	{
		handle_post(sock,header);
	}
	else
	{
		send_page_400(sock);
	}
	free(header);
	sock_clean(sock);
	close(sock);
	return 0;
}
