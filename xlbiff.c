static char rcsid[]= "$Id: xlbiff.c,v 1.72 1994/04/04 23:35:25 esm Exp $";
/*\
|* xlbiff  --  X Literate Biff
|*
|* DESCRIPTION:
|*
|* 	xlbiff is yet another biff utility.  It lurks around, polling
|*	a mail file until its size changes.  When this happens, it pops
|*	up a window containing a `scan' of the contents of the mailbox.
|*	Xlbiff is modeled after xconsole; it remains invisible at non-
|*	useful times, eg, when no mail is present.  See README for details.
|*
|*	Author:		Eduardo Santiago Munoz,  esm@auspex.com
|* 	Created:	20 August 1991
|*	Last Updated:	24 September 1992
|*
|*	Copyright 1991, 1992 Eduardo Santiago Munoz
|*	Portions copyright 1991 Digital Equipment Corporation
|*
|*	Permission to use, copy, modify, distribute, and sell this software
|*	and its documentation for any purpose is hereby granted without
|*	fee, provided that the above copyright notice appear in all copies
|*	and that both that copyright notice and this permission notice
|*	appear in supporting documentation.  The author makes no repre-
|*	sentation about the suitability of this software for any purpose.
|*	It is provided "as is" without express or implied warranty.
|*
\*/

#include "patchlevel.h"

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>
#include <X11/Xaw/Command.h>
#include <X11/Xos.h>

#include <stdio.h>
#include <sys/stat.h>
#include <pwd.h>
#include <errno.h>

/*
** This grody gunk stolen outright from mit/lib/Xaw/Mailbox.h
*/
#ifndef X_NOT_POSIX
#ifdef _POSIX_SOURCE
# include <sys/wait.h>
#else
#define _POSIX_SOURCE
# include <sys/wait.h>
#undef _POSIX_SOURCE
#endif
# define waitCode(w)    WEXITSTATUS(w)
# define waitSig(w)     WIFSIGNALED(w)
typedef int             waitType;
# define INTWAITTYPE
#else /* ! X_NOT_POSIX */
#if	defined(SYSV) || defined(SVR4)
# define waitCode(w)    (((w) >> 8) & 0x7f)
# define waitSig(w)     ((w) & 0xff)
typedef int             waitType;
# define INTWAITTYPE
#else
# include       <sys/wait.h>
# define waitCode(w)    ((w).w_T.w_Retcode)
# define waitSig(w)     ((w).w_T.w_Termsig)
typedef union wait      waitType;
#endif /* SYSV else */
#endif /* ! X_NOT_POSIX else */


#ifdef	NEED_STRERROR
char	*strerror();
#endif

/*
** if compiled with -DDEBUG *and* run with debugging on, this does lots
** of useless/useful printfs.
*/
#ifdef	DEBUG
#define	DP(x)	if (lbiff_data.debug) printf x
#else
#define DP(x)
#endif

/*
** This defines the file we need to monitor.  If not defined explicitly
** on the command line, we pick this default.
*/
#ifndef	MAILPATH
#define	MAILPATH	"/usr/spool/mail/%s"
#endif

/*****************************************************************************\
**                                prototypes                                 **
\*****************************************************************************/
char		*doScan();
void		Popdown(),Popup();
void		Usage();
extern char	*getlogin();

#ifdef	FUNCPROTO
void	Shrink(Widget, caddr_t, XEvent*, Boolean*);
void	handler(XtPointer,XtIntervalId*);
void	initStaticData(int*,int*,int*);
void	Exit(Widget, XEvent*, String*, Cardinal*);
void	Mailer(Widget, XEvent*, String*, Cardinal*);
void	lbiffUnrealize(), lbiffRealize(char*);
void	getDimensions(char*,Dimension*,Dimension*);
void	toggle_key_led(int);
void	ErrExit(Boolean,char*);
Bool	CheckEvent(Display*,XEvent*,caddr_t);
#else
void	Shrink();
void	handler();
void	initStaticData();
void	Exit();
void	Mailer();
void	lbiffUnrealize(), lbiffRealize();
void	getDimensions();
void	toggle_key_led();
void	ErrExit();
Bool	CheckEvent();
#endif

/*****************************************************************************\
**                                 globals				     **
\*****************************************************************************/
extern int errno;

Widget	topLevel,textBox;		/* my widgets			*/
XtAppContext app_context;		/* application context		*/
Boolean	visible;			/* is window visible?		*/
Boolean hasdata;			/* Something is to be displayed */
char	*default_file;			/* default filename		*/
char	*progname;			/* my program name		*/
long	acknowledge_time = 0;		/* time window was acknowledged	*/
long	popup_time = 0;			/* time window was popped up	*/

static Atom wm_delete_window;		/* for handling WM_DELETE	*/

typedef struct {
    Boolean	debug;			/* print out useful stuff 	*/
    char	*file;			/* file to monitor size of 	*/
    char	*checkCmd;		/* command to run for check     */
    char	*cmd;			/* command to run for output	*/
    char	*mailerCmd;		/* command to read mail		*/
    int		update;			/* update interval, in seconds	*/
    int		fade;			/* popdown interval, in seconds */
    int		columns;		/* number of columns across	*/
    int		rows;			/* max# of lines in display	*/
    int		volume;			/* bell volume, 0-100 percent	*/
    Boolean	bottom;			/* Put window at window bottom  */
    Boolean	resetSaver;		/* reset screensaver on popup   */
    long	refresh;		/* seconds before reposting msg	*/
    int		led;			/* led number to light up	*/
    Boolean	ledPopdown;		/* turn off LED on popdown?	*/
    char	*sound;			/* Sound file to use		*/
} AppData, *AppDataPtr;
AppData		lbiff_data;

#define offset(field)	XtOffset(AppDataPtr,field)

static XtResource	xlbiff_resources[] = {
    { "debug", "Debug", XtRBoolean, sizeof(Boolean),
	offset(debug), XtRImmediate, False},
    { "file", "File", XtRString, sizeof(String),
	offset(file), XtRString, NULL},
    { "checkCommand", "CheckCommand", XtRString, sizeof(String),
	offset(checkCmd), XtRString, NULL},
    { "scanCommand", "ScanCommand", XtRString, sizeof(String),
	offset(cmd), XtRString, "scan -file %s -width %d" },
    { "mailerCommand", "MailerCommand", XtRString, sizeof(String),
	offset(mailerCmd), XtRString, NULL },
    { "update", "Interval", XtRInt, sizeof(int),
	offset(update), XtRImmediate, (XtPointer)15},
    { "fade", "Fade", XtRInt, sizeof(int),
	offset(fade), XtRImmediate, (XtPointer)0},
    { "columns", "Columns", XtRInt, sizeof(int),
	offset(columns), XtRImmediate, (XtPointer)80},
    { "rows", "Rows", XtRInt, sizeof(int),
	offset(rows), XtRImmediate, (XtPointer)20},
    { "sound", "Sound", XtRString, sizeof(String),
	offset(sound), XtRString, "" },
    { "volume", "Volume", XtRInt, sizeof(int),
	offset(volume), XtRImmediate, (XtPointer)100},
    { "bottom", "Bottom", XtRBoolean, sizeof(Boolean),
	offset(bottom), XtRImmediate, False},
    { "resetSaver", "ResetSaver", XtRBoolean, sizeof(Boolean),
	offset(resetSaver), XtRImmediate, False},
    { "refresh", "Refresh", XtRInt, sizeof(int),
	offset(refresh), XtRImmediate, (XtPointer)1800},
    { "led", "Led", XtRInt, sizeof(int),
	offset(led), XtRImmediate, (XtPointer)0},
    { "ledPopdown", "LedPopdown", XtRBoolean, sizeof(Boolean),
	offset(ledPopdown), XtRImmediate, False}
};

static XrmOptionDescRec	optionDescList[] = {
    { "-bottom",      ".bottom",      XrmoptionNoArg,	(caddr_t) "true"},
    { "+bottom",      ".bottom",      XrmoptionNoArg,	(caddr_t) "false"},
    { "-debug",       ".debug",	      XrmoptionNoArg,	(caddr_t) "true"},
    { "-file",	      ".file",        XrmoptionSepArg,	(caddr_t) NULL},
    { "-rows",        ".rows",	      XrmoptionSepArg,	(caddr_t) NULL},
    { "-columns",     ".columns",     XrmoptionSepArg,	(caddr_t) NULL},
    { "-update",      ".update",      XrmoptionSepArg,	(caddr_t) NULL},
    { "-fade",	      ".fade",        XrmoptionSepArg,	(caddr_t) NULL},
    { "-volume",      ".volume",      XrmoptionSepArg,	(caddr_t) NULL},
    { "-resetSaver",  ".resetSaver",  XrmoptionNoArg,	(caddr_t) "true"},
    { "+resetSaver",  ".resetSaver",  XrmoptionNoArg,	(caddr_t) "false"},
    { "-refresh",     ".refresh",     XrmoptionSepArg,	(caddr_t) NULL},
    { "-led",         ".led",         XrmoptionSepArg,	(caddr_t) NULL},
    { "-ledPopdown",  ".ledPopdown",  XrmoptionNoArg,	(caddr_t) "true"},
    { "+ledPopdown",  ".ledPopdown",  XrmoptionNoArg,	(caddr_t) "false"},
    { "-sound",	      ".sound",       XrmoptionSepArg,  (caddr_t) NULL},
    { "-scanCommand", ".scanCommand", XrmoptionSepArg,	(caddr_t) NULL},
    { "-mailerCommand",".mailerCommand",XrmoptionSepArg,(caddr_t) NULL},
    { "-checkCommand",".checkCommand",XrmoptionSepArg,  (caddr_t) NULL}
};

static char *fallback_resources[] = {
    "*Font:		-*-clean-bold-r-normal--13-130-75-75-c-80-iso8859-1",
    "*Geometry:		+0-0",
    NULL
};

static XtActionsRec lbiff_actions[] = {
    {"exit",Exit},
    {"mailer",Mailer},
    {"popdown",Popdown}
};


/*****************************************************************************\
**                                  code                                     **
\*****************************************************************************/

/**********\
|*  main  *|
\**********/
#ifdef	FUNCPROTO
main( int argc, char *argv[] )
#else
main(argc, argv)
    int argc;
    char *argv[];
#endif
{
    progname = argv[0];

    topLevel = XtVaAppInitialize(&app_context,
				 "XLbiff",
				 optionDescList, XtNumber(optionDescList),
				 &argc, argv,
				 fallback_resources,
				 XtNallowShellResize, True,
				 NULL);

    XtGetApplicationResources(topLevel, &lbiff_data,
			      xlbiff_resources, XtNumber(xlbiff_resources),
			      (ArgList)NULL,0);

#ifndef	DEBUG
    if (lbiff_data.debug)
      fprintf(stderr,"%s: DEBUG support not compiled in, sorry\n",progname);
#endif

    /*
    ** Check command line arguments
    */
    if (argc > 1) {
	if (!strncmp(argv[1],"-version",strlen(argv[1]))) {
	    fprintf(stderr,
#if	TESTLEVEL != 0
		    "%s version %d.%d.%d\n",
#else
		    "%s version %d.%d\n",
#endif
		    progname,  VERSION,PATCHLEVEL,TESTLEVEL);
	    exit(0);
	} else if (!strncmp(argv[1],"-help",strlen(argv[1]))) {
	    Usage();
	} else if (argv[1][0] != '-') {
	    lbiff_data.file = argv[1];
	} else {
	    fprintf(stderr,
		    "%s: no such option \"%s\", type '%s -help' for help\n",
		    progname,argv[1],progname);
	    exit(1);
	}
    }

    /*
    ** If no data file was explicitly given, make our best guess
    */
    if (lbiff_data.file == NULL) {
	char *username = getlogin();

	if (username == NULL || username[0] == '\0') {
	    struct passwd  *pwd = getpwuid(getuid());
	    
	    if (pwd == NULL) {
		fprintf(stderr, "%s: cannot get username\n", progname);
		exit(1);
	    }
	    username = pwd->pw_name;
	}

	default_file = (char*)malloc(strlen(MAILPATH) + strlen(username));
	if (default_file == NULL)
	  ErrExit(True,"default_file malloc()");

	sprintf(default_file,MAILPATH,username);
	lbiff_data.file = default_file;
    }
    DP(("file= %s\n",lbiff_data.file));


    /*
    ** Fix DISPLAY environment variable, might be needed by subprocesses
    */
    {
      char *envstr = (char*)malloc(strlen("DISPLAY=") + 1
			    + strlen(XDisplayString(XtDisplay(topLevel))));

      sprintf(envstr, "DISPLAY=%s", XDisplayString(XtDisplay(topLevel)));
      putenv(envstr);
    }

    textBox = XtVaCreateManagedWidget("text",
				      commandWidgetClass,
				      topLevel,
				      NULL);

    XtAddCallback(textBox, XtNcallback, Popdown, textBox);
    XtAppAddActions(app_context, lbiff_actions, XtNumber(lbiff_actions));
    XtAddEventHandler(topLevel, StructureNotifyMask, False,
		      (XtEventHandler)Shrink, (caddr_t)NULL);

    XtOverrideTranslations(topLevel,
	XtParseTranslationTable ("<Message>WM_PROTOCOLS: exit()"));

    wm_delete_window = XInternAtom (XtDisplay(topLevel), "WM_DELETE_WINDOW",
	    			    False);

    toggle_key_led(False);

    /*
    ** check to see if there's something to do, pop up window if necessary,
    ** and set up alarm to wake us up again every so often.
    */
    handler(NULL,NULL);

    /*
    ** main program loop  --  mostly just loops forever waiting for events
    **
    ** note that we will continually be interrupted by the timeout code
    */
    XtAppMainLoop(app_context);
}


/***********\
|*  Usage  *|  displays usage message
\***********/
void
Usage()
{
    static char *help_message[] = {
"where options include:",
"    -version                           display xlbiff version number",
"    -display host:dpy                  X server to contact",
"    -geometry +x+y                     x,y coords of window",
"    -rows height                       height of window, in lines",
"    -columns width                     width of window, in characters",
"    -file file                         file to watch",
"    -update seconds                    how often to check for mail",
"    -fade seconds			lifetime of unmodified window",
"    -volume percentage                 how loud to ring the bell",
"    -bg color                          background color",
"    -fg color                          foreground color",
"    -refresh seconds                   seconds before re-posting window",
"    -led ledNum                        keyboard LED to light up",
"    -ledPopdown                        turn off LED when popped down",
"    -scanCommand command               command to interpret and display",
"    -checkCommand command              command used to check for change",
"    -mailerCommand command             command used to read mail",
NULL};
    char **s;

    printf("usage:\t%s  [-options ...]  [file to watch]\n", progname);
    for (s= help_message; *s; s++)
      printf("%s\n", *s);
    printf("\n");
    exit(1);
}


/**********\
|*  Exit  *|  called via callback, exits the program
\**********/
void
#ifdef	FUNCPROTO
Exit(Widget w, XEvent *event, String *params, Cardinal *num_params)
#else
Exit(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
#endif
{
    DP(("++Exit()\n"));

    if (event->type == ClientMessage) {
	if (event->xclient.data.l[0] != wm_delete_window) {
	    DP(("received client message that was not delete_window\n"));
	    XBell (XtDisplay(w), 0);
	    return;
	} else
	    DP(("exiting after receiving a wm_delete_window message\n"));
    }

    toggle_key_led(False);

    XCloseDisplay(XtDisplay(w));

    exit(0);
}


/************\
|*  Mailer  *|  called via callback, starts a mailer
\************/
void
#ifdef	FUNCPROTO
Mailer(Widget w, XEvent *event, String *params, Cardinal *num_params)
#else
Mailer(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
#endif
{
    DP(("++Mailer()\n"));

    if (lbiff_data.mailerCmd != NULL && lbiff_data.mailerCmd[0] != '\0') {
	Popdown();
	system(lbiff_data.mailerCmd);
	Popup();
	checksize();
    }
}


/***************\
|*  checksize  *|  checks mail file to see if new mail is present
|***************
|*	This routine stat's the mail spool file and compares its size
|*	with the previously obtained result.  If the size has become 
|*	zero, it pops down the window.  If nonzero, it calls a routine
|*	to execute the scanCommand.  If the result of this is non-null,
|*	it pops up a window showing it (note that users of Berkeley
|*	mail may have non-empty mail files with all old mail).
\*/
checksize()
{
    static int mailsize = 0;
    struct stat mailstat;
    int pop_window = False;
    struct timeval tp;
    struct timezone tzp;

    DP(("++checksize()..."));

    /*
    ** If user has specified a command to use to check the file, invoke
    ** it with lbiff_data.file and "previous" as arguments, where "previous"
    ** is the output of the script the last time it was run (or zero,
    ** the first time we call it).  This is useful as a way of keeping
    ** state for the checkCommand; in this manner it knows, if the
    ** spool file size is nonzero, whether it has grown since the last
    ** time we called it.
    */
    if (lbiff_data.checkCmd != NULL && lbiff_data.checkCmd[0] != '\0') {
	FILE        *p;
	waitType     status;
	char	     outbuf[80];
	static char *cmd_buf;
	static int   previous;

	if (cmd_buf == NULL) {
	    cmd_buf = (char*)malloc(strlen(lbiff_data.checkCmd) +
				    strlen(lbiff_data.file) + 10);
	    if (cmd_buf == NULL)
	      ErrExit(True,"scan command buffer malloc()");
	}

	sprintf(cmd_buf, lbiff_data.checkCmd, lbiff_data.file, previous);
	DP(("++checkCommand= %s\n",cmd_buf));

	if ((p= popen(cmd_buf,"r")) == NULL)
	  ErrExit(True,"popen(checkCommand)");
	if (fread(outbuf,1,sizeof outbuf,p) < 0)
	  ErrExit(True,"fread(checkCommand)");
	previous = atol(outbuf);
	DP(("checkCommand returns %d\n",previous));

#ifdef	INTWAITTYPE
	status = 	  pclose(p);
#else
	status.w_status = pclose(p);
#endif
	switch (waitCode(status)) {
	case 0:					/* 0: new data */
	    mailstat.st_size = mailsize + 1;
	    break;
        case 2:					/* 2: no data (clear) */
	    mailstat.st_size = 0;
	    break;
	default:				/* 1: same as before */
	    mailstat.st_size = mailsize;
	}
    } else {	/* no checkCmd, just stat the mailfile and check size */
	/*
	 ** Do the stat to get the mail file size.  If it fails for any reason,
	 ** ignore the failure and assume the file is size 0.  Failures I can
	 ** think of are that should be ignored are:
	 **
	 **		+ nonexistent file.  Some Berkeley-style mailers delete
	 **		  the spool file when they're done with it.
	 **		+ NFS stale filehandle.  Yuk.  This one happens if
	 ** 		  your mail spool file is on an NFS-mounted directory
	 **		  _and_ your update interval is too low _and_ you use
	 **		  a Berkeleyish mailer.  Yuk.
	 **
	 ** Doubtless there are errors we should complain about, but this 
	 ** would get too ugly.
	 */
	if (stat(lbiff_data.file,&mailstat) != 0) {
	    DP(("stat() failed, errno=%d.  Assuming filesize=0!\n",errno));
	    mailstat.st_size = 0;
	}
    }

    /*
    ** If it's changed size, take appropriate action.
    */
    if (mailstat.st_size != mailsize) {
	DP(("changed size: %d -> %d\n",mailsize,mailstat.st_size));
	mailsize = mailstat.st_size;
	pop_window = True;
    } else if (!visible && lbiff_data.refresh && mailsize != 0) {
	/*
	** If window has been popped down, check if it's time to refresh
	*/
	if (gettimeofday(&tp,&tzp) != 0) {
	    ErrExit(True,"gettimeofday() in checksize()");
	} else {
	    if ((tp.tv_sec - acknowledge_time) > lbiff_data.refresh) {
		DP(("reposting window, repost time reached\n"));
		pop_window = True;
	    }
	}
    } else if (visible && (mailstat.st_size = mailsize)) {
	/*
	** window is visible--see if fade time has been reached, and
	** if so, popdown window
	** if fade is zero, do not pop down
	*/
	if (gettimeofday(&tp,&tzp) != 0) {
	    ErrExit(True,"gettimeofday() in checksize()");
	} else if (lbiff_data.fade > 0) {
	    if ((tp.tv_sec - popup_time) > lbiff_data.fade) lbiffUnrealize();
	}
    }


    if (pop_window) {
	if (mailsize == 0) {
	    hasdata = False;
	    toggle_key_led(False);
	    lbiffUnrealize();
	} else {				/* something was added? */
	    char *s = doScan();

	    if (strlen(s) != 0)	{		/* is there anything new? */
		if (hasdata) /* ESM && isvisible? ESM */
		  lbiffUnrealize();		/* pop down if it's up    */
		hasdata = True;
		toggle_key_led(True);
		lbiffRealize(s);		/* pop back up */
	    }
	}
    } else {
	DP(("no change\n"));
    }
}


/*************\
|*  handler  *|  Checks mail file and reschedules itself to do so again
\*************/
void
#ifdef	FUNCPROTO
handler( XtPointer closure, XtIntervalId *id )
#else
/* ARGSUSED */
handler( closure, id )
     XtPointer 	   closure;
     XtIntervalId  *id;
#endif
{
    checksize();
    XtAppAddTimeOut(app_context,lbiff_data.update * 1000, handler, NULL);
}


/************\
|*  doScan  *|  invoke MH ``scan'' command to examine mail messages
|************
|*	This routine looks at the mail file and parses the contents. It
|*	does this by invoking scan(1) or some other user-defined function.
\*/
char *
doScan()
{
    static char	*cmd_buf;
    static char *buf = NULL;
    static int	bufsize;
    static char scan_fail_msg[] = "\n---->>>> scanCommand failed <<<<<----\n";
    FILE 	*p;
    size_t	size;
    waitType	status;

    DP(("++doScan()\n"));

    /*
    ** Initialise display buffer to #rows * #cols
    ** Initialise command string
    */
    if (buf == NULL) {
	/* +1 for the newline */
	bufsize = (lbiff_data.columns + 1) * lbiff_data.rows;

	buf = (char*)malloc(bufsize + sizeof(scan_fail_msg) + 1);
	if (buf == NULL)
	  ErrExit(True,"text buffer malloc()");

	DP(("---size= %dx%d\n", lbiff_data.rows, lbiff_data.columns));

	cmd_buf = (char*)malloc(strlen(lbiff_data.cmd) +
				strlen(lbiff_data.file) + 10);
	if (cmd_buf == NULL)
	  ErrExit(True,"command buffer malloc()");

	sprintf(cmd_buf,lbiff_data.cmd, lbiff_data.file, lbiff_data.columns);
	DP(("---cmd= %s\n",cmd_buf));
    }

    /*
    ** Execute the command, read the results, then set the contents of window.
    ** If there is data remaining in the pipe, read it in (and throw it away)
    ** so our exit status is correct (eg, not "Broken pipe").
    */
    if ((p= popen(cmd_buf,"r")) == NULL)
      ErrExit(True,"popen");
    if ((size= fread(buf,1,bufsize,p)) < 0)
      ErrExit(True,"fread");
    if (size == bufsize) {
	char junkbuf[100];
	while (fread(junkbuf, 1, 100, p) > 0)
	  ;	/* Keep reading until no more left */
    }

#ifdef	INTWAITTYPE
    status = 		pclose(p);
#else
    status.w_status =	pclose(p);
#endif
    if (waitCode(status) != 0) {
	strcpy(buf+size, scan_fail_msg);
	size += strlen(scan_fail_msg);
    }

    buf[size] = '\0';				/* null-terminate it! */

    DP(("scanned:\n%s\n",buf));
    return buf;
}


/****************\
|*  CheckEvent  *|  
\****************/
Bool
#ifdef	FUNCPROTO
CheckEvent( Display *d, XEvent *e, caddr_t arg )
#else	/* ~FUNCPROTO */
CheckEvent( d, e, arg )
     Display *d;
     XEvent  *e;
     caddr_t arg;
#endif	/* FUNCPROTO */
{
    if (e->type == MapNotify || e->type == UnmapNotify)
      if (e->xmap.window == (Window)arg)
	return True;

    return False;
}

static XEvent lastEvent;

/* ARGSUSED */
/*
** Handler for map/unmap events.  Copied from xconsole.
** When unmap and map events occur consecutively, eg when resetting a
** window manager, this makes sure that only the last such event takes place.
*/
/************\
|*  Shrink  *|  get StructureNotify events, popdown if iconified
\************/
void
#ifdef	FUNCPROTO
Shrink( Widget w, caddr_t data, XEvent *e, Boolean *b )
#else
Shrink(w, data, e, b)
    Widget w;
    caddr_t data;
    XEvent *e;
    Boolean *b;
#endif
{
    DP(("++Shrink()\n"));
    if (e->type == MapNotify || e->type == UnmapNotify) {
	int    event_seen = 0;
	Window win = e->xmap.window;

#ifdef	USE_BCOPY
	bcopy((char*)e,(char*)&lastEvent,sizeof(XEvent));
#else
	memcpy((char*)&lastEvent,(char*)e,sizeof(XEvent));
#endif

	XSync(XtDisplay(w),False);

	while(XCheckIfEvent(XtDisplay(w),&lastEvent,CheckEvent,(caddr_t)win))
	  event_seen = 1;

	if (!event_seen)
	  return;

	if (lastEvent.type == UnmapNotify && visible)
	  Popdown();
	else if (lastEvent.type == MapNotify && hasdata)
	  Popup();
    }
}


/*
 ** These here routines (Popdown/Popup) bring the main window up or down.
 ** They are pretty simple except for the issue with *bottom...
 ** If running with *bottom, things are more complicated.  You can't
 ** just map/unmap(), because since the window has already been placed
 ** at the bottom (when realized) any lines that get added to it when
 ** more mail comes in will just drop off the edge of the screen.
 ** Thus when *bottom is true we need to realize() the window anew
 ** each time something changes in it.
 */
/*************\
|*  Popdown  *|  kill window
\*************/
void
Popdown()
{
    struct timeval tp;
    struct timezone tzp;

    DP(("++Popdown()\n"));

    if (visible) {
	XtPopdown(topLevel);
	XSync(XtDisplay(topLevel), False);
    }
    visible = False;

    /*
    ** Remember when we were popped down so we can refresh later
    */
    if (gettimeofday(&tp,&tzp) != 0)
      ErrExit(True,"gettimeofday() in lbiffUnrealize()");

    acknowledge_time = tp.tv_sec;

    if (lbiff_data.ledPopdown)		/* Turn off LED if so requested */
      toggle_key_led(False);
}


void
Popup()
{
    struct timeval tp;
    struct timezone tzp;

    DP(("++Popup()\n"));

    /*
    ** Remember when we were popped up so we can fade later
    */
    if (gettimeofday(&tp,&tzp) != 0)
      ErrExit(True,"gettimeofday() in Popup()");
    popup_time = tp.tv_sec;

    if (hasdata && !visible) {
	XtPopup(topLevel, XtGrabNone);
	XSync(XtDisplay(topLevel), False);
    }
    visible = True;
}


/********************\
|*  lbiffUnrealize  *|  kill window
\********************/
void
lbiffUnrealize()
{
    DP(("++lbiffUnrealize()\n"));
    if (lbiff_data.bottom)
      XtUnrealizeWidget(topLevel);
    else
      Popdown();

    visible = False;
}


/******************\
|*  lbiffRealize  *|  reformat window, set the text and bring window up
\******************/
void
#ifdef	FUNCPROTO
lbiffRealize( char *s )
#else
lbiffRealize( s )
     char *s;
#endif
{
    Arg		args[4];
    int		n;
    static int	first_time = 1;

    DP(("++lbiffRealize()\n"));

    /*
    ** Set the contents of the window
    */
    n = 0;
    XtSetArg(args[n],XtNlabel,s); n++;
    XtSetValues(textBox, args, n);

    /*
    ** If running with *bottom, we need to tell the widget what size it
    ** is before realize()ing it.  This is so the WM can position it
    ** properly at the bottom of the screen.
    */
    if (lbiff_data.bottom) {
	Dimension width,height;

	getDimensions(s,&width,&height);

	n = 0;
 	XtSetArg(args[n], XtNwidth, width); n++;
	XtSetArg(args[n], XtNheight, height); n++;
	XtSetArg(args[n], XtNy, -1); n++;

	XtSetValues(topLevel, args, n);
	XtRealizeWidget(topLevel);
    }
    Popup();

    if (first_time) {
	/* first time through this code */
	(void) XSetWMProtocols (XtDisplay(topLevel), XtWindow(topLevel),
				&wm_delete_window, 1);
	first_time = 0;
    }


    if (lbiff_data.sound[0] == '\0') {
	/*
	** No, the following is not a typo, nor is it redundant code.
	** Apparently there is one X terminal that beeps whenever XBell()
	** is called, even with volume zero.
	*/
	if (lbiff_data.volume > 0) {
	    XBell(XtDisplay(topLevel),lbiff_data.volume - 100);
	    DP(("---sound= %s\n","XBell default"));
	}
    }
    else {
	static char	*sound_buf;

	/*
	** Initialise sound string
	*/
	if (sound_buf == NULL) {
	    sound_buf = (char*)malloc(strlen(lbiff_data.sound) + 10);
	    if (sound_buf == NULL)
	      ErrExit(True,"sound_buf malloc()");

	    sprintf(sound_buf,lbiff_data.sound, lbiff_data.volume);
	    DP(("---sound= %s\n",sound_buf));
	}
	system(sound_buf);
    }

    if (lbiff_data.resetSaver)
      XResetScreenSaver(XtDisplay(topLevel));

    visible = True;
}


/*******************\
|*  getDimensions  *|  get width x height of text string
\*******************/
void
#ifdef	FUNCPROTO
getDimensions( char *s, Dimension *width, Dimension *height )
#else
getDimensions(s,width,height)
     char *s;
     Dimension *width, *height;
#endif
{
    Dimension	tmp_width;
    int		i,
                len = strlen(s);
    static int	fontWidth, fontHeight;
    static int	borderWidth = -1;

    tmp_width = *width = *height = 1;

    if (borderWidth == -1)
      initStaticData(&borderWidth,&fontHeight,&fontWidth);

    /*
    ** count rows and columns
    */
    for (i=0; i < len-1; i++) {
	if (s[i] == '\n') {			/* new line: clear width */
	    ++*height;
	    tmp_width = 0;
	} else {
	    ++tmp_width;
	    if (tmp_width > *width)		/* monitor highest width */
	      *width = tmp_width;
	}
    }

    if (*height > lbiff_data.rows)		/* cut to fit max wid/hgt */
      *height = lbiff_data.rows;
    if (*width > lbiff_data.columns)
      *width = lbiff_data.columns;

    DP(("geom= %dx%d chars (%dx%d pixels)\n",*width,*height,
	                                     *width*fontWidth,
	                                     *height*fontHeight));

    *width  *= fontWidth;  *width  += 6;	/* convert to pixels 	  */
    *height *= fontHeight; *height += 4;	/* and add a little fudge */
}


/********************\
|*  initStaticData  *|  initializes font size & borderWidth
\********************/
void
#ifdef	FUNCPROTO
initStaticData( int *bw, int *fontH, int *fontW )
#else
initStaticData(bw, fontH, fontW)
    int *bw, *fontH, *fontW;
#endif
{
    Arg		args[2];
    XFontStruct *fs;
    int tmp;

    DP(("++initStaticData..."));
    XtSetArg(args[0],XtNfont,&fs);
    XtSetArg(args[1],XtNborderWidth,&tmp);
    XtGetValues(textBox, args, 2);
    if (fs == NULL)
      ErrExit(False,"unknown font");

    *bw	   = tmp;
    *fontW = fs->max_bounds.width;
    *fontH = fs->max_bounds.ascent + fs->max_bounds.descent;

    DP(("font= %dx%d,  borderWidth= %d\n",*fontH,*fontW,*bw));
}


/********************\
|*  toggle_key_led  *|  toggle a keyboard LED on and off
\********************/
void
#ifdef	FUNCPROTO
toggle_key_led(int flag)
#else
toggle_key_led(flag)
int	flag;
#endif
{
    XKeyboardControl	keyboard;

    if (lbiff_data.led == 0)		/* return if no led action desired */
      return;

    DP(("++toggle_key_led(%d,%s)\n", lbiff_data.led, flag ? "True" : "False"));

    if (flag)
	keyboard.led_mode = LedModeOn;
    else
	keyboard.led_mode = LedModeOff;

    keyboard.led = lbiff_data.led;

    XChangeKeyboardControl(XtDisplay(topLevel), KBLed | KBLedMode, &keyboard);
}


/*************\
|*  ErrExit  *|  print out error message, clean up and exit
|*************
|* ErrExit prints out a given error message to stderr.  If <errno_valid>
|* is True, it calls strerror(errno) to get the descriptive text for the
|* indicated error.  It then clears the LEDs and exits.
|*
|* It is the intention that someday this will bring up a popup window.
\*/
void
#ifdef	FUNCPROTO
ErrExit(Boolean errno_valid, char *s)
#else
ErrExit(errno_valid, s)
     Boolean errno_valid;
     char    *s;
#endif
{
    if (errno_valid)
      fprintf(stderr,"%s: %s: %s\n", progname, s, strerror(errno));
    else
      fprintf(stderr,"%s: %s\n", progname, s);

    toggle_key_led(False);

    XCloseDisplay(XtDisplay(topLevel));

    exit(1);
}


#ifdef	NEED_STRERROR
/**************\
|*  strerror  *|  return descriptive message text for given errno
\**************/
char *
#ifdef	FUNCPROTO
strerror(int err)
#else	/* ~FUNCPROTO */
strerror(err)
     int err;
#endif	/* FUNCPROTO */
{
    static char unknown[30];
    extern int	sys_nerr;
    extern char *sys_errlist[];

    if (err >= 0 && err < sys_nerr)
      return sys_errlist[err];

    sprintf(unknown,"Unknown error %d", err);
    return unknown;
}
#endif	/* NEED_STRERROR */
