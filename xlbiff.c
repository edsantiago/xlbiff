#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <setjmp.h>

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>

#include <X11/Xaw/Command.h>


#ifdef	DEBUG
#undef	DEBUG
#define	DEBUG(x)	printf x
#else
#define DEBUG(x)
#endif


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
    DEBUG(("++Quit()\n"));
    popdown();
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


/*\
|*  checksize  --  checks mail file to see if size has changed
\*/
checksize()
{
    static int mailsize = 0;
    struct stat mailstat;

    DEBUG(("++checksize()..."));
    stat("/usr/spool/mail/santiago",&mailstat);
    if (mailstat.st_size != mailsize) {
	DEBUG(("changed size: %d\n",mailstat.st_size));
	mailsize = mailstat.st_size;
	if (mailsize == 0) {
	    popdown();
	} else {
	    if (visible)
	      popdown();
	    doscan();
	    popup();
	}
    } else {
	DEBUG(("ok\n"));
    }
}


/*\
|*  handler  --  handles SIGALRM, checks mail file, and resets alarm
\*/
void
handler()
{
    checksize();

    alarm(10);
    longjmp(myjumpbuf);
}


/*\
|*  doscan  --  invoke MH ``scan'' command to examine mail messages
|*
|*	This routine looks at the mail file to see if it 
\*/
int
doscan()
{
    Arg 	args[1];
    char 	cmd[200];
    char 	buf[1024];
    FILE 	*p;
    size_t	size;

    DEBUG(("++doscan()\n"));
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

    DEBUG(("scanned: %s\n",buf));

    XtSetArg(args[0],XtNlabel,buf);
    XtSetValues(goodbye,args,1);
}




/*\
|*  popdown  --  kill window
\*/
popdown()
{
    DEBUG(("++popdown()..."));
    XtUnmapWidget(topLevel);
    XtUnrealizeWidget(topLevel);
    DEBUG(("..done\n"));
    visible = 0;
}

/*\
|*  popup  --  bring window up
\*/
popup()
{
    DEBUG(("++popup()\n"));
    XBell(XtDisplay(topLevel),0);
    XtRealizeWidget(topLevel);
    visible = 1;
}
