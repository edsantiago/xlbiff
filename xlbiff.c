static char rcsid[]= "$Id: xlbiff.c,v 1.39 1991/10/04 04:58:15 santiago Exp $";
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
void		Usage();
void		Exit();
extern char	*getlogin();

#ifdef	FUNCPROTO
void	Shrink(Widget, caddr_t, XEvent*, Boolean*);
void	handler(XtPointer,XtIntervalId*);
void	initStaticData(int*,int*,int*);
void	Popdown(), Popup(char*);
void	getDimensions(char*,Dimension*,Dimension*);
#else
void	Shrink();
void	handler();
void	initStaticData();
void	Popdown(), Popup();
void	getDimensions();
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
	offset(resetSaver), XtRString, "false"}
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
    { "+resetSaver",  ".resetSaver",  XrmoptionNoArg,	(caddr_t) "false"}
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
    XtAddEventHandler(topLevel,StructureNotifyMask,False,
		      (XtEventHandler)Shrink,(caddr_t)NULL);

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
	mailstat.st_size = 0;
    }

    /*
    ** If it's changed size, take appropriate action.
    */
    if (mailstat.st_size != mailsize) {
	DP(("changed size: %d -> %d\n",mailsize,mailstat.st_size));
	mailsize = mailstat.st_size;		/* remember the new size */
	if (mailsize == 0) {			/* mail file got inc'ed  */
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
	DP(("ok\n"));
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
	if (buf == NULL) {
	    fprintf(stderr,"%s: ",progname);
	    perror("buf malloc()");
	    exit(1);
	}
	DP(("---size= %dx%d\n", lbiff_data.rows, lbiff_data.columns));

	cmd_buf = (char*)malloc(strlen(lbiff_data.cmd) +
				strlen(lbiff_data.file) + 10);
	if (cmd_buf == NULL) {
	    fprintf(stderr,"%s: ",progname);
	    perror("cmd_buf malloc()");
	    exit(1);
	}

	sprintf(cmd_buf,lbiff_data.cmd, lbiff_data.file, lbiff_data.columns);
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
    DP(("++Popdown()\n"));
    if (lbiff_data.bottom) {
	XtUnrealizeWidget(topLevel);
    } else {
	XtPopdown(topLevel);
    }

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
    if (fs == NULL) {
	fprintf(stderr,"%s: unknown font!\n",progname);
	exit(1);
    }

    *bw	   = tmp;
    *fontW = fs->max_bounds.width;
    *fontH = fs->max_bounds.ascent + fs->max_bounds.descent;

    DP(("font= %dx%d,  borderWidth= %d\n",*fontH,*fontW,*bw));
}
