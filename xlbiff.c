/*\
|* xlbiff  --  X Literate Biff
|*
|* This application does mumble
\*/


#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <setjmp.h>
#include <pwd.h>
#include <unistd.h>

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>

#include <X11/Xaw/Command.h>


#ifdef	DEBUG
#undef	DEBUG
#define	DEBUG(x)	printf x
#else
#define DEBUG(x)
#endif


#if	1
#define	MAILPATH	"/usr/spool/mail/%s"
#else
#define	MAILPATH	"/udir/%s/.mailbox"
#endif


/*****************************************************************************\
** globals								     *|
\*****************************************************************************/
Widget	topLevel,goodbye;
jmp_buf	myjumpbuf;
int	visible;
char	default_file[80];

typedef struct {
    Boolean	debug;
    char	*file;
    char	*cmd;
    int		timeout;
} AppData, *AppDataPtr;

AppData		lbiff_data;

#define offset(field)	XtOffset(AppDataPtr,field)

static XtResource	xlbiff_resources[] = {
    { "debug", "Debug", XtRBoolean, sizeof(Boolean),
	offset(debug), XtRString, "false"},
    { "file", "File", XtRString, sizeof(String),
	offset(file), XtRString, NULL},
    { "timeout", "Timeout", XtRString, sizeof(String),
	offset(timeout), XtRString, "5"},
    { "command", "Command", XtRString, sizeof(String),
	offset(cmd), XtRString, "scan" }
};

static XrmOptionDescRec	optionDescList[] = {
    { "-file",	"*file",	XrmoptionSepArg,	(caddr_t) NULL}
};

static char *fallback_resources[] = {
    "XLbiff*font:	-*-clean-bold-r-normal--13-130-75-75-c-80-iso8859-1",
    NULL
  };
/*
** prototypes
*/
void	handler();


/*\
|*  Quit  --  callback for buttonpress anywhere in text window
\*/
void
Quit(Widget w, XtPointer client_data, XtPointer call_data)
{
    DEBUG(("++Quit()\n"));
    popdown();
    longjmp(myjumpbuf,1);
}


/*\
|*  main
\*/
main( int argc, char *argv[] )
{
    char *username;
    XtAppContext app_context;

    /*
    ** Get user name, in case no explicit path is given
    */
    if ((username= getlogin()) == NULL) {
	struct passwd *pwd= getpwuid(getuid());

	if (pwd == NULL) {
	    fprintf(stderr,"%s: cannot get username\n",argv[0]);
	    exit(1);
	}
	username = pwd->pw_name;
    }

    /*
    ** Do all the X stuff
    */
    topLevel = XtVaAppInitialize(&app_context,
				 "XLbiff",
				 optionDescList, XtNumber(optionDescList),
				 &argc, argv,
				 fallback_resources,
				 NULL);

    goodbye = XtVaCreateManagedWidget("goodbye",
				      commandWidgetClass,
				      topLevel,
				      NULL);

    XtGetApplicationResources(topLevel, &lbiff_data,
			      xlbiff_resources, XtNumber(xlbiff_resources),
			      (ArgList)NULL,0);


    XtAddCallback(goodbye,XtNcallback, Quit, goodbye );

    /*
    ** If no data file was explicitly given, make our best guess
    */
    if (lbiff_data.file == NULL) {
	sprintf(default_file,MAILPATH,username);
	lbiff_data.file = default_file;
    }

    /*
    ** check to see if there's something to do, pop up window if necessary,
    ** and set up alarm to wake us up again every so often.
    */
    checksize();
    signal(SIGALRM,handler);
    alarm(10);

    /*
    ** main program loop  --  mostly just loops forever waiting for events
    **
    ** note that we will continually be interrupted by the timeout code
    */
    setjmp(myjumpbuf);
    if (visible) {
	while (1) {
	    XEvent ev;

	    XtAppNextEvent(app_context,&ev);
	    DEBUG(("got event\n"));
	    XtDispatchEvent(&ev);
	}
/*	XtAppMainLoop(app_context);*/
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
    stat(lbiff_data.file,&mailstat);
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
    longjmp(myjumpbuf,1);
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
    char	cmd_buf[200];
    char 	buf[1024];
    FILE 	*p;
    size_t	size;

    DEBUG(("++doscan()\n"));

    sprintf(cmd_buf,lbiff_data.cmd,lbiff_data.file);
    if ((p= popen(cmd_buf,"r")) == NULL) {
	perror("popen");
	exit(1);
    }

    if ((size= fread(buf,1,sizeof buf,p)) < 0) {
	perror("fread");
	exit(1);
    }

    pclose(p);
    buf[size] = '\0';

    DEBUG(("scanned: %s\n",buf));

    XtSetArg(args[0],XtNlabel,buf);
    XtSetValues(goodbye,args,1);
}


/*****************************\
|*  popdown  --  kill window *|
\*****************************/
popdown()
{
    DEBUG(("++popdown()..."));
    XtUnmapWidget(topLevel);
    XtUnrealizeWidget(topLevel);
    DEBUG(("..done\n"));
    visible = 0;
}


/*******************************\
|*  popup  --  bring window up *|
\*******************************/
popup()
{
    DEBUG(("++popup()\n"));
    XBell(XtDisplay(topLevel),0);
    XtRealizeWidget(topLevel);
    visible = 1;
}
