static char rcsid[]= "$Id: xlbiff.c,v 1.42 1991/10/15 03:52:37 santiago Exp $";
/*\
|* xlbiff  --  X Literate Biff
|*
|* by Eduardo Santiago Munoz
|*
|*	Copyright (c) 1991 Digital Equipment Corporation
|*
|* Permission to use, copy, modify, distribute, and sell this software and its
|* documentation for any purpose is hereby granted without fee, provided that
|* the above copyright notice appear in all copies and that both that
|* copyright notice and this permission notice appear in supporting
|* documentation, and that the name of Digital Equipment Corporation not be 
|* used in advertising or publicity pertaining to distribution of the 
|* software without specific, written prior permission.  Digital Equipment
|* Corporation makes no representations about the suitability of this software 
|* for any purpose.  It is provided "as is" without express or implied 
|* warranty.
|*
|* DIGITAL EQUIPMENT CORPORATION  DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS 
|* SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
|* IN NO EVENT SHALL DIGITAL EQUIPMENT CORPORATION BE LIABLE FOR ANY SPECIAL,
|* INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM 
|* LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE 
|* OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE 
|* OR PERFORMANCE OF THIS SOFTWARE.
|*
|* DESCRIPTION:
|*
|* 	Now that that's out of the way -- xlbiff is yet another biff
|*	utility.  It lurks around, polling a mail file until its size
|*	changes.  When this happens, it pops up a window containing
|*	a `scan' of the contents of the mailbox.  Xlbiff is modeled
|*	after xconsole; it remains invisible at non-useful times,
|*	eg, when no mail is present.  See README for details.
|*
|*	Author:		Eduardo Santiago Munoz,  santiago@pa.dec.com
|* 	Created:	20 August 1991
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
void		Popdown();
void		Usage();
extern char	*getlogin();

#ifdef	FUNCPROTO
void	Shrink(Widget, caddr_t, XEvent*, Boolean*);
void	handler(XtPointer,XtIntervalId*);
void	initStaticData(int*,int*,int*);
void	Exit(Widget, XEvent*, String*, Cardinal*);
void	Popup(char*);
void	getDimensions(char*,Dimension*,Dimension*);
void	toggle_key_led(int);
void	ErrExit(char*,Boolean);
#else
void	Shrink();
void	handler();
void	initStaticData();
void	Exit();
void	Popup();
void	getDimensions();
void	toggle_key_led();
void	ErrExit();
#endif

/*****************************************************************************\
**                                 globals				     **
\*****************************************************************************/
extern int errno;

Widget	topLevel,textBox;		/* my widgets			*/
XtAppContext app_context;		/* application context		*/
Boolean	visible;			/* is window visible?		*/
char	*default_file;			/* default filename		*/
char	*progname;			/* my program name		*/
long	acknowledge_time = 0;		/* time window was acknowledged	*/

static Atom wm_delete_window;		/* for handling WM_DELETE	*/

typedef struct {
    Boolean	debug;			/* print out useful stuff 	*/
    char	*file;			/* file to monitor size of 	*/
    char	*cmd;			/* command to execute		*/
    int		update;			/* update interval, in seconds	*/
    int		columns;		/* number of columns across	*/
    int		rows;			/* max# of lines in display	*/
    int		volume;			/* bell volume, 0-100 percent	*/
    Boolean	bottom;			/* Put window at window bottom  */
    Boolean	resetSaver;		/* reset screensaver on popup   */
    long	refresh;		/* seconds before reposting msg	*/
    int		led;			/* led number to light up	*/
} AppData, *AppDataPtr;
AppData		lbiff_data;

#define offset(field)	XtOffset(AppDataPtr,field)

static XtResource	xlbiff_resources[] = {
    { "debug", "Debug", XtRBoolean, sizeof(Boolean),
	offset(debug), XtRString, "false"},
    { "file", "File", XtRString, sizeof(String),
	offset(file), XtRString, NULL},
    { "scanCommand", "ScanCommand", XtRString, sizeof(String),
	offset(cmd), XtRString, "scan -file %s -width %d" },
    { "update", "Interval", XtRInt, sizeof(int),
	offset(update), XtRString, "15"},
    { "columns", "Columns", XtRInt, sizeof(int),
	offset(columns), XtRString, "80"},
    { "rows", "Rows", XtRInt, sizeof(int),
	offset(rows), XtRString, "20"},
    { "volume", "Volume", XtRInt, sizeof(int),
	offset(volume), XtRString, "100"},
    { "bottom", "Bottom", XtRBoolean, sizeof(Boolean),
	offset(bottom), XtRString, "false"},
    { "resetSaver", "ResetSaver", XtRBoolean, sizeof(Boolean),
	offset(resetSaver), XtRString, "false"},
    { "refresh", "Refresh", XtRInt, sizeof(int),
	offset(refresh), XtRString, "1800"},
    { "led", "Led", XtRInt, sizeof(int),
	offset(led), XtRString, "0"}
};

static XrmOptionDescRec	optionDescList[] = {
    { "-bottom",      ".bottom",      XrmoptionNoArg,	(caddr_t) "true"},
    { "+bottom",      ".bottom",      XrmoptionNoArg,	(caddr_t) "false"},
    { "-debug",       ".debug",	      XrmoptionNoArg,	(caddr_t) "true"},
    { "-file",	      ".file",        XrmoptionSepArg,	(caddr_t) NULL},
    { "-rows",        ".rows",	      XrmoptionSepArg,	(caddr_t) NULL},
    { "-columns",     ".columns",     XrmoptionSepArg,	(caddr_t) NULL},
    { "-update",      ".update",      XrmoptionSepArg,	(caddr_t) NULL},
    { "-volume",      ".volume",      XrmoptionSepArg,	(caddr_t) NULL},
    { "-resetSaver",  ".resetSaver",  XrmoptionNoArg,	(caddr_t) "true"},
    { "+resetSaver",  ".resetSaver",  XrmoptionNoArg,	(caddr_t) "false"},
    { "-refresh",     ".refresh",     XrmoptionSepArg,	(caddr_t) NULL},
    { "-led",         ".led",         XrmoptionSepArg,	(caddr_t) NULL}
};

static char *fallback_resources[] = {
    "*Font:		-*-clean-bold-r-normal--13-130-75-75-c-80-iso8859-1",
    "*Geometry:		+0-0",
    NULL
};

static XtActionsRec lbiff_actions[] = {
    {"exit",Exit},
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
	if (!strncmp(argv[1],"-v",2)) {
	    fprintf(stderr,"%s version %d, patchlevel %d\n",
		            progname,  VERSION,       PATCHLEVEL);
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
	  ErrExit("default_file malloc()",True);

	sprintf(default_file,MAILPATH,username);
	lbiff_data.file = default_file;
    }
    DP(("file= %s\n",lbiff_data.file));

    textBox = XtVaCreateManagedWidget("text",
				      commandWidgetClass,
				      topLevel,
				      NULL);

    XtAddCallback(textBox,XtNcallback, Popdown, textBox);
    XtAppAddActions(app_context,lbiff_actions,XtNumber(lbiff_actions));
    XtAddEventHandler(topLevel,StructureNotifyMask,False,
		      (XtEventHandler)Shrink,(caddr_t)NULL);

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
"    -display host:dpy                  X server to contact",
"    -geometry +x+y                     x,y coords of window",
"    -rows height                       height of window, in lines",
"    -columns width                     width of window, in characters",
"    -file file                         file to watch",
"    -update seconds                    how often to check for mail",
"    -volume percentage                 how loud to ring the bell",
"    -bg color                          background color",
"    -fg color                          foreground color",
"    -refresh seconds                   seconds before re-posting window",
"    -led ledNum                        keyboard LED to light up",
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
    DP(("++exit()\n"));

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

    /*
    ** If it's changed size, take appropriate action.
    */
    if (mailstat.st_size != mailsize) {
	DP(("changed size: %d -> %d\n",mailsize,mailstat.st_size));
	mailsize = mailstat.st_size;
	pop_window = True;
	if (mailsize == 0)
	  toggle_key_led(False);
	else
	  toggle_key_led(True);
    } else if (!visible && lbiff_data.refresh && mailsize != 0) {
	/*
	** If window has been popped down, check if it's time to refresh
	*/
	if (gettimeofday(&tp,&tzp) != 0) {
	    ErrExit("gettimeofday(), in checksize()",True);
	} else {
	    if ((tp.tv_sec - acknowledge_time) > lbiff_data.refresh) {
		DP(("reposting window, repost time reached\n"));
		pop_window = True;
	    }
	}
    }

    if (pop_window) {
	if (mailsize == 0) {
	    if (visible)
		Popdown();
	} else {				/* something was added? */
	    char *s = doScan();

	    if (strlen(s) != 0)	{		/* is there anything new? */
		if (visible && lbiff_data.bottom)  /* popdown if at bottom */
		  Popdown();
		Popup(s);			/* pop back up */
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
    FILE 	*p;
    size_t	size;
    int		status;

    DP(("++doScan()\n"));

    /*
    ** Initialise display buffer to #rows * #cols
    ** Initialise command string
    */
    if (buf == NULL) {
	bufsize = lbiff_data.columns * lbiff_data.rows;

	buf = (char*)malloc(bufsize);
	if (buf == NULL)
	  ErrExit("text buffer malloc()",True);

	DP(("---size= %dx%d\n", lbiff_data.rows, lbiff_data.columns));

	cmd_buf = (char*)malloc(strlen(lbiff_data.cmd) +
				strlen(lbiff_data.file) + 10);
	if (cmd_buf == NULL)
	  ErrExit("command buffer malloc()",True);

	sprintf(cmd_buf,lbiff_data.cmd, lbiff_data.file, lbiff_data.columns);
	DP(("---cmd= %s\n",cmd_buf));
    }

    /*
    ** execute the command, read the results, then set the contents of window
    */
    if ((p= popen(cmd_buf,"r")) == NULL)
      ErrExit("popen",True);
    if ((size= fread(buf,1,bufsize,p)) < 0)
      ErrExit("fread",True);
    if ((status= pclose(p)) != 0)
      ErrExit("scanCommand failed",False);

    buf[size] = '\0';				/* null-terminate it! */

    DP(("scanned:\n%s\n",buf));
    return buf;
}


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
    if (e->type == UnmapNotify && visible)
      Popdown();
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
    if (lbiff_data.bottom) {
	XtUnrealizeWidget(topLevel);
    } else {
	XtPopdown(topLevel);
    }

    /*
    ** Remember when we were popped down so we can refresh later
    */
    if (gettimeofday(&tp,&tzp) != 0)
      ErrExit("gettimeofday() in Popdown()",True);

    acknowledge_time = tp.tv_sec;

    visible = False;
}


/***********\
|*  Popup  *|  reformat window, set the text and bring window up
\***********/
void
#ifdef	FUNCPROTO
Popup( char *s )
#else
Popup( s )
     char *s;
#endif
{
    Arg		args[4];
    int		n;

    DP(("++Popup()\n"));

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
    } else {
	XtPopup(topLevel, XtGrabNone);
    }

    if (acknowledge_time == 0) {
	/* first time through this code */
	(void) XSetWMProtocols (XtDisplay(topLevel), XtWindow(topLevel),
				&wm_delete_window, 1);
    }


    XBell(XtDisplay(topLevel),lbiff_data.volume - 100);

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
      ErrExit("unknown font",False);

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

    DP(("++toggle_key_led(%d,%s)\n",flag ? "True" : "False", lbiff_data.led));

    if (flag)
	keyboard.led_mode = LedModeOn;
    else
	keyboard.led_mode = LedModeOff;

    keyboard.led = lbiff_data.led;

    DP(("will toggle key led = %d\n",lbiff_data.led));

    XChangeKeyboardControl(XtDisplay(topLevel), KBLed | KBLedMode, &keyboard);
    XSync(XtDisplay(topLevel),False);
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
ErrExit(char *s, Boolean errno_valid)
#else
ErrExit(s, errno_valid)
     char    *s;
     Boolean errno_valid;
#endif
{
    if (errno_valid)
      fprintf(stderr,"%s: %s: %s\n", progname, s, strerror(errno));
    else
      fprintf(stderr,"%s: %s\n", progname, s);

    toggle_key_led(False);
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
    static char unknown[20];
    extern char *sys_errlist[];

    if (err >= 0 && err < sys_nerr)
      return sys_errlist[err];

    sprintf(unknown,"Unknown error %d", err);
    return unknown;
}
#endif	/* NEED_STRERROR */
