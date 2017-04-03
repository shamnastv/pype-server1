#include<iostream>
#include<sys/types.h>
#include<sys/socket.h>
#include<stdlib.h>
#include<arpa/inet.h>
#include<string.h>
#include<unistd.h>
#include<netdb.h>
#include<semaphore.h>
#include<pthread.h>
#include<signal.h>
#include<map>
#include<list>
#include <utility> 
#include<random>

#define MAX 50
#define MAX_PEERS 2
#define INIT "a"
#define POLL "b"
#define GETCON "c"
#define GETMYADDR "d"
#define NOTHING "e"
#define NEXT "f"
using namespace std;

struct Threadargs{
  int sockfd;
  sockaddr_in addr;
};
typedef struct Threadargs Targs;

struct Netaddress{
  char ip[INET_ADDRSTRLEN];
  int port;
};
typedef struct Netaddress Netaddr;

int peersn=0;
int peerstop=0;
sockaddr_in peers[MAX_PEERS];

list < pair<sockaddr_in,list<sockaddr_in> > > duties;

void perror(string s)
{
  cout<<s<<endl;
  exit(0);
}

void init(Targs* targs)
{
  int sockfd=targs->sockfd;
  char buffer[MAX];
  int n;
  bzero(buffer,MAX);
  Netaddr netaddr;
  strcpy(netaddr.ip,inet_ntoa(targs->addr.sin_addr));
  netaddr.port=ntohs(targs->addr.sin_port);
  if((n=write(sockfd,(char*)&netaddr,sizeof(Netaddr)))<0)
    perror("error while writing");
  if((n=read(sockfd,buffer,MAX))<0)
    perror("error while reading");
  buffer[n]='\0';
  if(strcmp(buffer,NEXT)==0)
    {
      if(peersn>0)
	{
	  int i=rand()%peersn;
	  
	  list<pair < sockaddr_in,list < sockaddr_in > > >::iterator it1;
	  for(it1=duties.begin();it1!=duties.end();++it1)
	    {
	      if((it1->first).sin_port==peers[i].sin_port&&(it1->first).sin_addr.s_addr==peers[i].sin_addr.s_addr)
		break;
	    }
	  if(it1!=duties.end())
	    (it1->second).push_back(targs->addr);
	  else
	    {
	      list <sockaddr_in> l;
	      l.push_back(targs->addr);
	      duties.push_back(make_pair(peers[i],l));
	    }
	  strcpy(netaddr.ip,inet_ntoa(peers[i].sin_addr));
	  netaddr.port=ntohs(peers[i].sin_port);
	  if((n=write(sockfd,(char*)&netaddr,sizeof(Netaddr)))<0)
	    perror("error while writing");
	}
      else
	{
	  strcpy(buffer,NOTHING);
	  if((n=write(sockfd,buffer,strlen(buffer)))<0)
	    perror("error while writing");
	}
    }
  peers[peerstop]=targs->addr;
  peerstop=(peerstop+1)%MAX_PEERS;
  if(peersn<MAX_PEERS)
    peersn++;
}
void poll(Targs* targs)
{
  char buffer[MAX];
  int n;
  Netaddr netaddr;
  int sockfd=targs->sockfd;
  list<pair < sockaddr_in,list < sockaddr_in > > >::iterator it1;
  for(it1=duties.begin();it1!=duties.end();++it1)
  {
    if((it1->first).sin_port==(targs->addr).sin_port&&(it1->first).sin_addr.s_addr==(targs->addr).sin_addr.s_addr)
      break;
  }
  
  if(it1 == duties.end())
    {
      strcpy(buffer,NOTHING);
      if((n=write(sockfd,buffer,strlen(buffer)))<0)
	perror("error while writing");
    }
  else
    {
      for(list< sockaddr_in > ::iterator it2 = (it1->second).begin();it2!=(it1->second).end();++it2)
	{
	  strcpy(netaddr.ip,inet_ntoa(it2->sin_addr));
	  netaddr.port=ntohs(it2->sin_port);
	  if((n=write(sockfd,(char*)&netaddr,sizeof(Netaddr)))<0)
	    perror("error while writing");
	  if((n=read(sockfd,buffer,MAX))<0)
	    perror("error while reading");
	  buffer[n]='\0';
	  if(strcmp(buffer,NEXT)!=0)
	    perror("buffer is not NEXT");
     	}
      duties.erase(it1);
      strcpy(buffer,NOTHING);
      if((n=write(sockfd,buffer,strlen(buffer)))<0)
	perror("error while writing");
	}
}
void getcon(Targs* targs)
{
  char buffer[MAX];
  int n;
  Netaddr netaddr;
  int sockfd=targs->sockfd;
  sockaddr_in sock;
  strcpy(buffer,NEXT);
  if((n=write(sockfd,buffer,strlen(buffer)))<0)
    perror("error while writing");
  if((n=read(sockfd,(char*)&netaddr,sizeof(Netaddr)))<0)
    perror("error while reading");
  inet_aton(netaddr.ip,&(sock.sin_addr));
  sock.sin_port=htons(netaddr.port);

  list<pair < sockaddr_in,list < sockaddr_in > > >::iterator it1;
  for(it1=duties.begin();it1!=duties.end();++it1)
  {
    if((it1->first).sin_port==sock.sin_port&&(it1->first).sin_addr.s_addr==sock.sin_addr.s_addr)
      break;
  }
  if(it1!=duties.end())
    (it1->second).push_back(targs->addr);
  else
    {
      list <sockaddr_in> l;
      l.push_back(targs->addr);
      duties.push_back(make_pair(sock,l));
    }
}
void noidea(Targs* targs)
{
  char buffer[MAX];
  int n;
}

void *threadmain(void *targ)
{
  Targs *targs=(Targs*) targ;
  pthread_detach(pthread_self());
  int sockfd=targs->sockfd;
  char buffer[MAX];
  int n;
  while(1)
    {
      bzero(buffer,MAX);

      if((n=read(sockfd,buffer,MAX))<0)
	perror("error while reading");
      if(n==0)
	break;
      buffer[n]='\0';
      if(strcmp(buffer,INIT)==0)
	init(targs);
      if(strcmp(buffer,POLL)==0)
	poll(targs);
      else if(strcmp(buffer,GETCON)==0)
	getcon(targs);
      else
	noidea(targs);
    }
  close(sockfd);
}
int main(int argc,char* argv[])
{
  int sockfd,newsockfd;
  int port;
  socklen_t clielen;
  sockaddr_in serv_addr,clien_addr;
  hostent* server;
  Targs *targs;
  if(argc<2)
    {
      cout<<"Usage : ./name port"<<endl;
      exit(0);
    }
  port=atoi(argv[1]);
  if((sockfd=socket(AF_INET,SOCK_STREAM,0))<0)
    perror("error in socket creation");
  bzero((char*)&serv_addr,sizeof(serv_addr));
  serv_addr.sin_family=AF_INET;
  serv_addr.sin_port=htons(port);
  serv_addr.sin_addr.s_addr=INADDR_ANY;
  if(bind(sockfd,(sockaddr *) &serv_addr,sizeof(serv_addr))<0)
    perror("bining error");
  clielen=sizeof(clien_addr);
  listen(sockfd,5);
  pthread_t tid;
  while(1)
    {
      if((newsockfd=accept(sockfd,(sockaddr *)&clien_addr,&clielen))<0)
	perror("error on accept");
      targs=(Targs *)malloc(sizeof(Targs));
      targs->sockfd=newsockfd;
      targs->addr=(sockaddr_in)clien_addr;
      if(pthread_create(&tid,NULL,threadmain,(void *)targs)!=0)
	perror("error creating thread");
    }
  close(sockfd);
  return 0;
}
