/****************************************************************************
 *
 * MODULE:             main.c
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
#include <stdio.h>
#include "utils.h"
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include "socket_client.h"

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/
#define DBG_MAIN 1
/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/
static void SignalHandler();

/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/
const char *psVersion = "Version 0.1";
/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/
int main (int argc, char *argv[])
{
    DBG_vPrintf(DBG_MAIN, "This is a socket comminution client program\n");

    signal(SIGTERM, SignalHandler);
    signal(SIGINT,  SignalHandler);

    teSocketStatus eSocketStatus;
    eSocketStatus = SocketClientInit(7788, "127.0.0.1");
    if(E_SOCK_OK != eSocketStatus)
    {
        ERR_vPrintf(T_TRUE, "SocketClientInit Error %d\n", eSocketStatus);
        return -1;
    }
    while(1)
    {
		SocketSendMessage("Test Message\n", sizeof("Test Message\n"));
        sleep(3);
    }
    SocketClientFinished();
    return 0;
}

static void SignalHandler()
{
    PURPLE_vPrintf(DBG_MAIN, "Receive a Terminal signal, Exit This Program\n");
    exit(0);
}
