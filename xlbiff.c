static char rcsid[]= "$Id: xlbiff.c,v 1.16 1991/08/21 16:33:09 santiago Exp $";
/*\
|* xlbiff  --  X Literate Biff
|*
|*
|*	Copyright (c) 1991 by Eduardo Santiago Munoz
|*
|*	This software may be distributed under the terms of the GNU General
|* 	Public License.  I wrote it, not DEC, so don't bother suing them.
|*
|* DESCRIPTION
|*
|* 	Now that that's out of the way -- xlbiff is yet another biff
|*	utility.  It lurks around, polling a mail file until its size
|*	changes.  When this happens, it pops up a window containing
|*	a `scan' of the contents of the mailbox.  See README for details.
|*
|*	Author:		Eduardo Santiago Munoz,  santiago@pa.dec.com
|* 	Created:	20 August 1991
\*/

#include "patchlevel.h"

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


/*
** if compiled with -DDEBUG *and* run with debugging on, this does lots
** of useless/useful printfs.
*/
#ifdef	DEBUG
#undef	DEBUG
#define	DEBUG(x)	if (lbiff_data.debug) printf x
#else
#define DEBUG(x)
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
void	Shrink(Widget, caddr_t, XEvent*);
void	initStaticData(int*,int*,int*);
void	setXbuf(char*);

/*****************************************************************************\
**                                 globals				     **
\*****************************************************************************/
Widget	topLevel,textBox;		/* my widgets			*/
jmp_buf	myjumpbuf;			/* for longjmp()ing after timer	*/
int	visible;			/* is window visible?		*/
char	default_file[80];		/* default filename		*/
char	*progname;			/* my program name		*/

typedef struct {
    Boolean	debug;			/* print out useful stuff 	*/
    char	*file;			/* file to monitor size of 	*/
    char	*cmd;			/* command to execute		*/
    int		update;			/* update interval, in seconds	*/
    int		width;			/* number of columns across	*/
    int		maxRows;		/* max# of lines in display	*/
    Boolean	fit;			/* fit display to widest line?	*/
    int		volume;			/* bell volume, 0-100 percent	*/
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
    { "maxRows", "maxRows", XtRInt, sizeof(int),
	offset(maxRows), XtRString, "20"},
    { "fit", "Fit", XtRBoolean, sizeof(Boolean),
	offset(fit), XtRString, "false"},
    { "volume", "Volume", XtRInt, sizeof(int),
	offset(volume), XtRString, "100"}
};

static XrmOptionDescRec	optionDescList[] = {
    { "-debug", "*debug",	XrmoptionNoArg,		(caddr_t) "true"},
    { "-file",	"*file",	XrmoptionSepArg,	(caddr_t) NULL},
    { "-update","*update",	XrmoptionSepArg,	(caddr_t) NULL},
    { "-volume","*volume",	XrmoptionSepArg,	(caddr_t) NULL},
    { "-width", "*width",	XrmoptionSepArg,	(caddr_t) NULL}
};

static char *fallback_resources[] = {
    "XLbiff*font:	-*-clean-bold-r-normal--13-130-75-75-c-80-iso8859-1",
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
main( int argc, char *argv[] )
{
    char *username;
    XtAppContext app_context;

    progname = argv[0];
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

    textBox = XtVaCreateManagedWidget("text",
				      commandWidgetClass,
				      topLevel,
				      NULL);

    XtGetApplicationResources(topLevel, &lbiff_data,
			      xlbiff_resources, XtNumber(xlbiff_resources),
			      (ArgList)NULL,0);


    XtAddCallback(textBox,XtNcallback, Popdown, textBox);
    XtAppAddActions(app_context,lbiff_actions,XtNumber(lbiff_actions));
    XtAddEventHandler(topLevel,StructureNotifyMask,False,Shrink,(caddr_t)NULL);

    /*
    ** Check command line arguments
    */
    if (argc > 1) {
	if (!strncmp(argv[1],"-v",2)) {
	    printf("xlbiff version %s, patchlevel %d\n",VERSION,PATCHLEVEL);
	    exit(0);
	} else if (argv[1][0] != '-') {
	    lbiff_data.file = argv[1];
	} else 
	  Usage();
    }

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
    setjmp(myjumpbuf);
    checksize();
    signal(SIGALRM,handler);
    alarm(lbiff_data.update);

    /*
    ** main program loop  --  mostly just loops forever waiting for events
    **
    ** note that we will continually be interrupted by the timeout code
    */
    if (visible) {
	XtAppMainLoop(app_context);
    } else {
	while (1) sleep(1000);
    }
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
"    -geometry =+x+y                    x,y coords of window",
"    -width width                       width of window, in characters",
"    -file file                         file to watch",
"    -update seconds                    how often to check for mail",
"    -volume percentage                 how loud to ring the bell",
"    -bg color                          background color",
"    -fg color                          foreground color",
NULL};
    char **s;

    fprintf(stderr,"usage:\t%s [-options ...]\n", progname);
    for (s= help_message; *s; s++)
      fprintf(stderr, "%s\n", *s);
    fprintf(stderr,"\n");
    exit(1);
}


/**********\
|*  Exit  *|  called via callback, exits the program
\**********/
void
Exit()
{
    DEBUG(("++exit()\n"));
    exit(0);
}


/***************\
|*  checksize  *|  checks mail file to see if size has changed
\***************/
checksize()
{
    static int mailsize = 0;
    struct stat mailstat;

    DEBUG(("++checksize()..."));
    stat(lbiff_data.file,&mailstat);
    if (mailstat.st_size != mailsize) {
	DEBUG(("changed size: %d -> %d\n",mailsize,mailstat.st_size));
	mailsize = mailstat.st_size;
	if (mailsize == 0) {
	    Popdown();
	} else {
	    if (visible)
	      Popdown();
	    setXbuf(doScan());
	    Popup();
	}
    } else {
	DEBUG(("ok\n"));
    }
}


/*************\
|*  handler  *|  handles SIGALRM, checks mail file, and resets alarm
\*************/
void
handler()
{
    longjmp(myjumpbuf,1);
}


/************\
|*  doScan  *|  invoke MH ``scan'' command to examine mail messages
|************
|*	This routine looks at the mail file to see if it 
\*/
char *
doScan()
{
    static char	cmd_buf[200];
    static char *buf = NULL;
    static int	bufsize;
    FILE 	*p;
    size_t	size;

    DEBUG(("++doScan()\n"));

    /*
    ** Initialise display buffer to #rows * #cols
    ** Initialise command string
    */
    if (buf == NULL) {
	bufsize = lbiff_data.width * lbiff_data.maxRows;
	if ((buf= (char*)malloc(bufsize)) == NULL) {
	    fprintf(stderr,"error in malloc\n");
	    exit(1);
	}
	DEBUG(("---size= %dx%d\n", lbiff_data.maxRows, lbiff_data.width));

	sprintf(cmd_buf,lbiff_data.cmd,  lbiff_data.file,  lbiff_data.width);
	DEBUG(("---cmd= %s\n",cmd_buf));
    }

    /*
    ** execute the command, read the results, then set the contents of X window
    */
    if ((p= popen(cmd_buf,"r")) == NULL) {
	perror("popen");
	exit(1);
    }
    if ((size= fread(buf,1,bufsize,p)) < 0) {
	perror("fread");
	exit(1);
    }
    pclose(p);
    buf[size] = '\0';

    DEBUG(("scanned: %s\n",buf));
    return buf;
}


/*************\
|*  setXbuf  *|  reformats X window and tells X what the text will be
\*************/
void
setXbuf(char *s)
{
    Arg 	args[1];
    int 	i,
                len = strlen(s);
    Dimension 	w= 0,
                h= 1;
    int 	tmp_w = 0;
    static int	fontWidth, fontHeight;
    static int	borderWidth= -1;

    DEBUG(("setXbuf\n"));
    if (borderWidth == -1)
      initStaticData(&borderWidth,&fontHeight,&fontWidth);

    /*
    ** count rows and columns
    */
    for (i=0; i < len-1; i++) {
	if (s[i] == '\n') {
	    ++h;
	    tmp_w = 0;
	} else {
	    ++tmp_w;
	    if (tmp_w > w)
	      w = tmp_w;
	}
    }

    if (h > lbiff_data.maxRows)			/* cut to fit */
      h = lbiff_data.maxRows;
    if (w > lbiff_data.width)
      w = lbiff_data.width;
    if (lbiff_data.fit == False)
      w = lbiff_data.width;
    DEBUG(("geom= %dx%d (%dx%d pixels)\n",w,h,w*fontWidth,h*fontHeight));

    /*
    ** Set widget to given size, plus some leeway.  Set label text.
    */
    XtResizeWidget(topLevel,w*fontWidth+6,h*fontHeight+4,borderWidth);
    XtSetArg(args[0],XtNlabel,s);
    XtSetValues(textBox,args,1);
}


/********************\
|*  initStaticData  *|  initializes font size & borderWidth
\********************/
void
initStaticData(int *bw, int *fontH, int *fontW)
{
    Arg		args[2];
    XFontStruct *fs;
    int tmp;

    DEBUG(("++initStaticData..."));
    XtSetArg(args[0],XtNfont,&fs);
    XtSetArg(args[1],XtNborderWidth,&tmp);
    XtGetValues(textBox, args, 2);
    if (fs == NULL) {
	fprintf(stderr,"unknown font!\n");
	exit(1);
    }

    *bw	   = tmp;
    *fontW = fs->max_bounds.width;
    *fontH = fs->max_bounds.ascent + fs->max_bounds.descent;

    DEBUG(("font= %dx%d,  borderWidth= %d\n",*fontH,*fontW,*bw));
}


/************\
|*  Shrink  *|  get StructureNotify events, popdown if iconized
\************/
void
Shrink(Widget w, caddr_t data, XEvent *e)
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
    DEBUG(("++Popdown()\n"));
    XtUnrealizeWidget(topLevel);
    XSync(XtDisplay(topLevel),0);

    visible = 0;
}


/***********\
|*  Popup  *|  bring window up
\***********/
void
Popup()
{
    DEBUG(("++Popup()\n"));
    XBell(XtDisplay(topLevel),lbiff_data.volume - 100);
    XtRealizeWidget(topLevel);
    XSync(XtDisplay(topLevel),0);

    visible = 1;
}
