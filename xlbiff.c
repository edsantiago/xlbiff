static char rcsid[]= "$Id: xlbiff.c,v 1.26 1991/09/27 23:36:16 santiago Exp $";
/*\
|* xlbiff  --  X Literate Biff
|*
|* LEGALESE GARBAGE:
|*
|*	Copyright (c) 1991 by Eduardo Santiago Munoz
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
void	handler();
char	*doScan();
void	Usage();
void	Exit();
void	Popdown(), Popup();

#ifdef	FUNCPROTO
void	Shrink(Widget, caddr_t, XEvent*);
void	initStaticData(int*,int*,int*);
void	setXbuf(char*);
#else
void	Shrink();
void	initStaticData();
void	setXbuf();
#endif

/*****************************************************************************\
**                                 globals				     **
\*****************************************************************************/
extern int errno;

Widget	topLevel,textBox;		/* my widgets			*/
XtAppContext app_context;		/* application context		*/
int	visible;			/* is window visible?		*/
char	*default_file;			/* default filename		*/
char	*progname;			/* my program name		*/

typedef struct {
    Boolean	debug;			/* print out useful stuff 	*/
    char	*file;			/* file to monitor size of 	*/
    char	*cmd;			/* command to execute		*/
    int		update;			/* update interval, in seconds	*/
    int		width;			/* number of columns across	*/
    int		maxRows;		/* max# of lines in display	*/
    int		volume;			/* bell volume, 0-100 percent	*/
    Boolean	bottom;			/* Put window at window bottom  */
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
    { "width", "Width", XtRInt, sizeof(int),
	offset(width), XtRString, "80"},
    { "maxRows", "Height", XtRInt, sizeof(int),
	offset(maxRows), XtRString, "20"},
    { "volume", "Volume", XtRInt, sizeof(int),
	offset(volume), XtRString, "100"},
    { "bottom", "Boolean", XtRBoolean, sizeof(Boolean),
	offset(bottom), XtRString, "false"}
};

static XrmOptionDescRec	optionDescList[] = {
    { "-bottom","*bottom",	XrmoptionNoArg,		(caddr_t) "true"},
    { "-debug", "*debug",	XrmoptionNoArg,		(caddr_t) "true"},
    { "-file",	"*file",	XrmoptionSepArg,	(caddr_t) NULL},
    { "-update","*update",	XrmoptionSepArg,	(caddr_t) NULL},
    { "-volume","*volume",	XrmoptionSepArg,	(caddr_t) NULL},
    { "-width", "*width",	XrmoptionSepArg,	(caddr_t) NULL}
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
    char *username;
    struct passwd  *pwd;

    progname = argv[0];
    /*
    ** Get user name, in case no explicit path is given
    */
    pwd = getpwuid(getuid());
    if (pwd != NULL) {
	username = pwd->pw_name;
    } else {
	username = getlogin();
	if (username == NULL) {
	    fprintf(stderr,"%s: cannot get username\n",progname);
	    exit(1);
	}
    }

    topLevel = XtVaAppInitialize(&app_context,
				 "XLbiff",
				 optionDescList, XtNumber(optionDescList),
				 &argc, argv,
				 fallback_resources,
				 XtNallowShellResize, TRUE,
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
	default_file = (char*)malloc(strlen(MAILPATH) + strlen(username));
	if (default_file == NULL) {
	    fprintf(stderr,"%s: ", progname);
	    perror("default_file malloc()");
	    exit(1);
	}

	sprintf(default_file,MAILPATH,username);
	lbiff_data.file = default_file;
    }
    DP(("username= %s\tfile= %s\n",username,lbiff_data.file));

    textBox = XtVaCreateManagedWidget("text",
				      commandWidgetClass,
				      topLevel,
				      NULL);

    XtAddCallback(textBox,XtNcallback, Popdown, textBox);
    XtAppAddActions(app_context,lbiff_actions,XtNumber(lbiff_actions));
    XtAddEventHandler(topLevel,StructureNotifyMask,False,Shrink,(caddr_t)NULL);

    if (!lbiff_data.bottom) {
	XtSetMappedWhenManaged(topLevel, FALSE);
	XtRealizeWidget(topLevel);
    }

    /*
    ** check to see if there's something to do, pop up window if necessary,
    ** and set up alarm to wake us up again every so often.
    */
    handler();

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
"    -width width                       width of window, in characters",
"    -file file                         file to watch",
"    -update seconds                    how often to check for mail",
"    -volume percentage                 how loud to ring the bell",
"    -bg color                          background color",
"    -fg color                          foreground color",
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
Exit()
{
    DP(("++exit()\n"));
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

    DP(("++checksize()..."));
    stat(lbiff_data.file,&mailstat);
    if (mailstat.st_size != mailsize) {
	DP(("changed size: %d -> %d\n",mailsize,mailstat.st_size));
	mailsize = mailstat.st_size;		/* remember the new size */
	if (mailsize == 0) {			/* mail file got inc'ed  */
	    if (visible)
	      Popdown();
	} else {				/* something was added? */
	    char *s = doScan();

	    if (strlen(s) != 0)	{		/* is there anything new? */
		setXbuf(s);
		if (visible && lbiff_data.bottom)  /* popdown if at bottom */
		  Popdown();
		Popup();			/* pop back up */
	    }
	}
    } else {
	DP(("ok\n"));
    }
}


/*************\
|*  handler  *|  handles SIGALRM, checks mail file, and resets alarm
\*************/
void
handler()
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
	bufsize = lbiff_data.width * lbiff_data.maxRows;

	buf = (char*)malloc(bufsize);
	if (buf == NULL) {
	    fprintf(stderr,"%s: ",progname);
	    perror("buf malloc()");
	    exit(1);
	}
	DP(("---size= %dx%d\n", lbiff_data.maxRows, lbiff_data.width));

	cmd_buf = (char*)malloc(strlen(lbiff_data.cmd) +
				strlen(lbiff_data.file) + 10);
	if (cmd_buf == NULL) {
	    fprintf(stderr,"%s: ",progname);
	    perror("cmd_buf malloc()");
	    exit(1);
	}

	sprintf(cmd_buf,lbiff_data.cmd,  lbiff_data.file,  lbiff_data.width);
	DP(("---cmd= %s\n",cmd_buf));
    }

    /*
    ** execute the command, read the results, then set the contents of X window
    */
    if ((p= popen(cmd_buf,"r")) == NULL) {
	fprintf(stderr,"%s: ",progname);
	perror("popen");
	exit(1);
    }
    if ((size= fread(buf,1,bufsize,p)) < 0) {
	fprintf(stderr,"%s: ",progname);
	perror("fread");
	exit(1);
    }
    if ((status= pclose(p)) != 0) {
	fprintf(stderr,"%s: scanCommand failed\n",progname);
	exit(status);
    }

    buf[size] = '\0';				/* null-terminate it! */

    DP(("scanned:\n%s\n",buf));
    return buf;
}


/*************\
|*  setXbuf  *|  reformats X window and tells Xt what the text will be
\*************/
void
#ifdef	FUNCPROTO
setXbuf( char *s )
#else
setXbuf(s)
    char *s;
#endif
{
    Arg 	args[1];
    int 	i,
                len = strlen(s);
    Dimension 	w= 0,
                h= 1;
    Dimension	tmp_w = 0;
    static int	fontWidth, fontHeight;
    static int	borderWidth= -1;

    DP(("setXbuf\n"));
    if (borderWidth == -1)
      initStaticData(&borderWidth,&fontHeight,&fontWidth);

    /*
    ** Set label text to the message scan list
    */
    XtSetArg(args[0],XtNlabel,s);
    XtSetValues(textBox,args,1);

    /*
    ** If we're NOT running with -bottom, we're all done since the widget
    ** resizes automatically.
    */
    if (!lbiff_data.bottom)
      return;

    /*
    ** Unfortunately, when running with -bottom, we need to explicitly
    ** resize the widget.  To do this, we get its width & height and 
    ** multiply by the font size.
    */

    /*
    ** count rows and columns
    */
    for (i=0; i < len-1; i++) {
	if (s[i] == '\n') {			/* new line: clear width */
	    ++h;
	    tmp_w = 0;
	} else {
	    ++tmp_w;
	    if (tmp_w > w)			/* monitor highest width */
	      w = tmp_w;
	}
    }

    if (h > lbiff_data.maxRows)			/* cut to fit max wid/hgt */
      h = lbiff_data.maxRows;
    if (w > lbiff_data.width)
      w = lbiff_data.width;
    DP(("geom= %dx%d (%dx%d pixels)\n",w,h,w*fontWidth,h*fontHeight));

    /*
    ** Set widget to given size, plus some leeway.
    */
    XtResizeWidget(topLevel,w*fontWidth+6,h*fontHeight+4,borderWidth);
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
    if (fs == NULL) {
	fprintf(stderr,"%s: unknown font!\n",progname);
	exit(1);
    }

    *bw	   = tmp;
    *fontW = fs->max_bounds.width;
    *fontH = fs->max_bounds.ascent + fs->max_bounds.descent;

    DP(("font= %dx%d,  borderWidth= %d\n",*fontH,*fontW,*bw));
}


/************\
|*  Shrink  *|  get StructureNotify events, popdown if iconified
\************/
void
#ifdef	FUNCPROTO
Shrink( Widget w, caddr_t data, XEvent *e )
#else
Shrink(w, data, e)
    Widget w;
    caddr_t data;
    XEvent *e;
#endif
{
    if (e->type == UnmapNotify && visible)
      Popdown();
}


/*************\
|*  Popdown  *|  kill window
\*************/
void
Popdown()
{
    DP(("++Popdown()\n"));
    if (lbiff_data.bottom) {
	XtUnrealizeWidget(topLevel);
	XSync(XtDisplay(topLevel), False);
    } else {
	XtUnmapWidget(topLevel);
    }

    visible = 0;
}


/***********\
|*  Popup  *|  bring window up
\***********/
void
Popup()
{
    DP(("++Popup()\n"));
    XBell(XtDisplay(topLevel),lbiff_data.volume - 100);

    if (lbiff_data.bottom) {
	XtRealizeWidget(topLevel);
	XSync(XtDisplay(topLevel), False);
    } else {
	XtMapWidget(topLevel);
    }

    visible = 1;
}
