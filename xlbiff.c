#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <setjmp.h>

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>

#include <X11/Xaw/Command.h>

/*
** globals
*/
Widget	topLevel,goodbye;
jmp_buf	myjumpbuf;
int	visible;

/*
** prototypes
*/
void	handler();


void Quit(Widget w, XtPointer client_data, XtPointer call_data)
{
    printf("nice knowing you\n");
    XtUnmapWidget(topLevel);
    XtUnrealizeWidget(topLevel);
    visible = 0;
    longjmp(myjumpbuf);
}

main( int argc, char *argv[] )
{
    XtAppContext app_context;

    topLevel = XtVaAppInitialize(&app_context,
				 "XGoodbye",
				 NULL, 0,
				 &argc, argv,
				 NULL,
				 NULL);

    goodbye = XtVaCreateManagedWidget("goodbye",
				      commandWidgetClass,
				      topLevel,
				      NULL);

    XtAddCallback(goodbye,XtNcallback, Quit, goodbye );

    checksize();
    signal(SIGALRM,handler);
    alarm(10);

    setjmp(myjumpbuf);
    if (visible) {
	XtAppMainLoop(app_context);
    }

    while (1) sleep(1000);
}


checksize()
{
    static int mailsize = 0;
    struct stat mailstat;

    stat("/usr/spool/mail/santiago",&mailstat);
    if (mailstat.st_size != mailsize) {
	printf("changed size: %d\n",mailstat.st_size);
	mailsize = mailstat.st_size;
	if (mailsize == 0) {
	    visible = 0;
	    /* vanish the window */
	} else {
	    doscan();
	    visible = 1;
	    XBell(XtDisplay(topLevel),0);
	    XtRealizeWidget(topLevel);
	}
    }
}



void
handler()
{
    checksize();

    alarm(10);
    longjmp(myjumpbuf);
}


int
doscan()
{
    Arg 	args[1];
    char 	cmd[200];
    char 	buf[1024];
    FILE 	*p;
    size_t	size;

    sprintf(cmd,"scan -file %s -form %s -width %d",
	    "/usr/spool/mail/santiago",
	    "xmsg.form",
	    120);

    if ((p= popen(cmd,"r")) == NULL) {
	perror("popen");
	exit(1);
    }

    if ((size= fread(buf,1,sizeof buf,p)) < 0) {
	perror("fread");
	exit(1);
    }

    pclose(p);

    XtSetArg(args[0],XtNlabel,buf);
    XtSetValues(goodbye,args,1);
}

