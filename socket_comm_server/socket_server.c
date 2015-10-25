/****************************************************************************
 *
 * MODULE:             socket_server.c
 *
 * COMPONENT:          Utils interface
 *
 * REVISION:           $Revision:  0$
 *
 * DATED:              $Date: 2015-10-21 15:13:17 +0100 (Thu, 21 Oct 2015 $
 *
 * AUTHOR:             PCT
 *
 ****************************************************************************
 *
 * Copyright panchangtao@gmail.com B.V. 2015. All rights reserved
 *
 ***************************************************************************/

/****************************************************************************/
/***        Include files                                                 ***/
/****************************************************************************/
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/epoll.h> 

#include "socket_server.h"
/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/
#define DBG_SOCK 1 
/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/
static void *SocketServerHandleThread(void *arg);
/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/
static tsSocketServer sSocketServer;

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/
teSocketStatus SocketServerInit(int iPort, char *psNetAddress)
{
    BLUE_vPrintf(DBG_SOCK, "SocketServerInit\n");

    memset(&sSocketServer, 0, sizeof(sSocketServer));
    signal(SIGPIPE, SIG_IGN);//ingnore signal interference
    
	struct sockaddr_in server_addr;  
	server_addr.sin_family = AF_INET;  
    if(NULL != psNetAddress)
    {
        server_addr.sin_addr.s_addr = inet_addr(psNetAddress);  /*just receive one address*/
    }
    else
    {
        server_addr.sin_addr.s_addr = htonl(INADDR_ANY);        /*receive any address*/
    }
	server_addr.sin_port = htons(iPort);

    if(-1 == (sSocketServer.iSocketFd = socket(AF_INET, SOCK_STREAM, 0)))
    {
        ERR_vPrintf(T_TRUE, "socket create error %s\n", strerror(errno));
        return E_SOCK_ERROR_CREATESOCK;
    }

    int on = 1;  /*SO_REUSEADDR port can used twice by program */
    if((setsockopt(sSocketServer.iSocketFd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)))<0) 
    {  
        ERR_vPrintf(T_TRUE,"setsockopt failed, %s\n", strerror(errno));  
        return E_SOCK_ERROR_SETSOCK;
    }  

    if(-1 == bind(sSocketServer.iSocketFd, (struct sockaddr*)&server_addr, sizeof(server_addr)))
    {
        ERR_vPrintf(T_TRUE,"bind socket failed, %s\n", strerror(errno));  
        return E_SOCK_ERROR_BIND;
    }

    if(-1 == listen(sSocketServer.iSocketFd, SOCKET_LISTEN_NUM))
    {
        ERR_vPrintf(T_TRUE,"listen socket failed, %s\n", strerror(errno));  
        return E_SOCK_ERROR_LISTEN;
    }

    //start accept thread
    BLUE_vPrintf(DBG_SOCK, "pthread_create\n");
    if(0 != pthread_create(&sSocketServer.pthSocketServer, NULL, SocketServerHandleThread, NULL))
    {
        ERR_vPrintf(T_TRUE,"pthread_create failed, %s\n", strerror(errno));  
        return E_SOCK_ERROR_PTHREAD_CREATE;
    }
    return E_SOCK_OK;
}

teSocketStatus SocketServerFinished()
{
    BLUE_vPrintf(DBG_SOCK, "Waiting SocketServerFinished...\n");
    
    sSocketServer.eState = E_THREAD_STOPPED;
    pthread_kill(sSocketServer.pthSocketServer, THREAD_SIGNAL);
    void *psThread_Result;
    if(0 != pthread_join(sSocketServer.pthSocketServer, &psThread_Result))
    {
        ERR_vPrintf(T_TRUE,"phread_join socket failed, %s\n", strerror(errno));  
        return E_SOCK_ERROR_JOIN;
    }

    BLUE_vPrintf(DBG_SOCK, " SocketServerFinished %s\n", (char*)psThread_Result);

    return E_SOCK_OK;
}
/****************************************************************************/
/***        Local    Functions                                            ***/
/****************************************************************************/
static void ThreadSignalHandler(int sig)
{
    BLUE_vPrintf(DBG_SOCK, "ThreadSignalHandler Used To Interrupt System Call\n");
}

static void *SocketServerHandleThread(void *arg)
{
    BLUE_vPrintf(DBG_SOCK, "SocketServerHandleThread\n");
    sSocketServer.eState = E_THREAD_RUNNING;
    signal(THREAD_SIGNAL, ThreadSignalHandler);

    int i = 0;
    tsSocketClient sSocketClient[SOCKET_LISTEN_NUM];
    memset(&sSocketClient, 0, sizeof(sSocketClient));
    for(i = 0; i < SOCKET_LISTEN_NUM; i++)
    {
        sSocketClient[i].iSocketFd = -1;
    }

    int iEpollFd = epoll_create(65535);
    if(-1 == iEpollFd)
    {
        ERR_vPrintf(T_TRUE,"epoll_create failed, %s\n", strerror(errno));  
        pthread_exit("epoll_create failed");
    }
    struct epoll_event EpollEvevt, EpollEventList[EPOLL_EVENT_NUM];
    EpollEvevt.data.fd = sSocketServer.iSocketFd;
    EpollEvevt.events = EPOLLIN;  /*read*/
    if(-1 == epoll_ctl (iEpollFd, EPOLL_CTL_ADD, sSocketServer.iSocketFd, &EpollEvevt))
    {
        ERR_vPrintf(T_TRUE,"epoll_create failed, %s\n", strerror(errno));  
        pthread_exit("epoll_create failed");
    }
        
    while(sSocketServer.eState)
    {
        DBG_vPrintf(DBG_SOCK, "\n++++++++++++++Waiting for iEpollFd Changed\n");
        int iEpollResult = epoll_wait(iEpollFd,EpollEventList,EPOLL_EVENT_NUM,-1);

        switch (iEpollResult)
        {
            case (E_EPOLL_ERROR):
            {
                ERR_vPrintf(T_TRUE,"epoll_wait failed, %s\n", strerror(errno));  
                pthread_exit("epoll_wait failed");
            }
            break;
            case (E_EPOLL_TIMEOUT):
                ERR_vPrintf(T_TRUE,"epoll_wait E_EPOLL_TIMEOUT\n");  
            break;
            default:
            {
                DBG_vPrintf(DBG_SOCK, "Epoll_wait Find %d Changed\n", iEpollResult);
                int n = 0;
                for(i = 0; i < iEpollResult; i++)
                {
                    if((EpollEventList[n].events & EPOLLERR) || (EpollEventList[n].events & EPOLLHUP))
                    {
                        ERR_vPrintf(T_TRUE,"The Fd Occured an Error, %s\n", strerror(errno));  
                        continue;
                    }
                    else if(EpollEventList[n].data.fd == sSocketServer.iSocketFd)    /*Server accept event*/
                    {
                        DBG_vPrintf(DBG_SOCK, "sSocketServer.iSocketFd Changed\n");
                        if(sSocketServer.u8NumConnClient >= (SOCKET_LISTEN_NUM - 1))
                        {
                            ERR_vPrintf(T_TRUE, "The Number of client is 10, do not allowed connect, will not wait server fd\n");
                            EpollEvevt.data.fd = sSocketServer.iSocketFd;
                            EpollEvevt.events = EPOLLIN;  /*read ,Ede-Triggered, close*/
                            if(-1 == epoll_ctl (iEpollFd, EPOLL_CTL_DEL, sSocketServer.iSocketFd, &EpollEvevt))
                            {
                                ERR_vPrintf(T_TRUE,"epoll_ctl failed, %s\n", strerror(errno));                                         
                                pthread_exit("epoll_ctl failed");
                            }
                            continue;
                            sleep(1);
                        }
                        
                        for(i = 0; i < SOCKET_LISTEN_NUM; i++)
                        {
                            if(-1 == sSocketClient[i].iSocketFd)//not used
                            {
                                sSocketClient[i].iSocketLen = sizeof(sSocketClient[i].addrclient);
                                sSocketClient[i].iSocketFd = accept(sSocketServer.iSocketFd,
                                    (struct sockaddr*)&sSocketClient[i].addrclient, (socklen_t *)&sSocketClient[i].iSocketLen);
                                if(-1 == sSocketClient[i].iSocketFd)
                                {
                                    ERR_vPrintf(T_TRUE, "socket accept error %s\n", strerror(errno));
                                }
                                else
                                {
                                    YELLOW_vPrintf(DBG_SOCK, "A client already connected, [%d]-[%d]\n", i, sSocketClient[i].iSocketFd);
                                    sSocketServer.u8NumConnClient++;
                                    EpollEvevt.data.fd = sSocketClient[i].iSocketFd;
                                    EpollEvevt.events = EPOLLIN | EPOLLET;  /*read ,Ede-Triggered, close*/
                                    if(-1 == epoll_ctl (iEpollFd, EPOLL_CTL_ADD, sSocketClient[i].iSocketFd, &EpollEvevt))
                                    {
                                        ERR_vPrintf(T_TRUE,"epoll_ctl failed, %s\n", strerror(errno));                                         
                                        pthread_exit("epoll_ctl failed");
                                    }
                                    DBG_vPrintf(DBG_SOCK, "Client[%d] Already Add Epoll_wait Fd\n", i);
                                    break;  /*jump out for, otherwise will go accept again*/
                                }
                        
                            }
                        }/*for*/
                    }
                    else/*Client recive event or disconnect event*/
                    {
                        for(i = 0; i < SOCKET_LISTEN_NUM; i++)
                        {
                            if(-1 == sSocketClient[i].iSocketFd)
                            {
                                continue;
                            }
                            else if(EpollEventList[n].data.fd == sSocketClient[i].iSocketFd)
                            {
                                BLUE_vPrintf(DBG_SOCK, "Socket Client[%d] Begin Recv Data...\n", i);
                                sSocketClient[i].iSocketLen = recv(sSocketClient[i].iSocketFd, 
                                    sSocketClient[i].csClientData, sizeof(sSocketClient[i].csClientData), 0);
                                if(-1 == sSocketClient[i].iSocketLen)
                                {
                                    ERR_vPrintf(T_TRUE, "socket recv error %s\n", strerror(errno));
                                }
                                else if(0 == sSocketClient[i].iSocketLen)   /*disconnect*/
                                {
                                    ERR_vPrintf(T_TRUE, "The Client[%d] is disconnect, Closet It\n", i);
                                    
                                    EpollEvevt.data.fd = sSocketClient[i].iSocketFd;
                                    EpollEvevt.events = EPOLLIN | EPOLLET | EPOLLRDHUP;  /*read ,Ede-Triggered, close*/
                                    if(-1 == epoll_ctl (iEpollFd, EPOLL_CTL_DEL, sSocketClient[i].iSocketFd, &EpollEvevt))
                                    {
                                        ERR_vPrintf(T_TRUE,"epoll_ctl failed, %s\n", strerror(errno));                                         
                                        pthread_exit("epoll_ctl failed");
                                    }
                                    
                                    close(sSocketClient[i].iSocketFd);
                                    sSocketClient[i].iSocketFd = -1;
                                    sSocketServer.u8NumConnClient --;
                                    if(sSocketServer.u8NumConnClient < SOCKET_LISTEN_NUM)
                                    {
                                        EpollEvevt.data.fd = sSocketServer.iSocketFd;
                                        EpollEvevt.events = EPOLLIN;  /*read*/
                                        if(-1 == epoll_ctl (iEpollFd, EPOLL_CTL_ADD, sSocketServer.iSocketFd, &EpollEvevt))
                                        {
                                            ERR_vPrintf(T_TRUE,"epoll_create failed, %s\n", strerror(errno));  
                                            pthread_exit("epoll_create failed");
                                        }
                                    }
                                }
                                else    /*recv event*/
                                {
                                    YELLOW_vPrintf(DBG_SOCK, "Recv Data is [%d]--- %s\n", i, sSocketClient[i].csClientData);
                                }
                                break;
                            }
                        }/*for*/
                    }
                }
            }
            break;
        }



        
        sleep(0);
    }
    close(iEpollFd);
    close(sSocketServer.iSocketFd);
    DBG_vPrintf(DBG_SOCK, "Exit SocketServerHandleThread\n");
    pthread_exit("Get Killed Signal");
}

