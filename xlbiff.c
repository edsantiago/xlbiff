/*\
|* xlbiff  --  X Literate Biff
|*
|* This application does mumble
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


#ifdef	DEBUG
#undef	DEBUG
#define	DEBUG(x)	if (lbiff_data.debug) printf x
#else
#define DEBUG(x)
#endif


#if	1
#define	MAILPATH	"/usr/spool/mail/%s"
#else
#define	MAILPATH	"/udir/%s/.mailbox"
#endif


/*
** prototypes
*/
void	handler();
void	Exit();
void	Popdown(Widget,XtPointer,XtPointer);
void	initStaticData(int*,int*,int*);
void	setXbuf(char*);

/*****************************************************************************\
** globals								     *|
\*****************************************************************************/
Widget	topLevel,textBox;		/* my widgets			*/
jmp_buf	myjumpbuf;			/* for longjmp()ing after timer	*/
int	visible;			/* is window visible?		*/
char	default_file[80];		/* default filename		*/


typedef struct {
    Boolean	debug;			/* print out useful stuff 	*/
    char	*file;			/* file to monitor size of 	*/
    char	*cmd;			/* command to execute		*/
    int		update;			/* update interval, in seconds	*/
    int		maxLines;		/* max# of lines in display	*/
} AppData, *AppDataPtr;
AppData		lbiff_data;


#define offset(field)	XtOffset(AppDataPtr,field)

static XtResource	xlbiff_resources[] = {
    { "debug", "Debug", XtRBoolean, sizeof(Boolean),
	offset(debug), XtRString, "false"},
    { "file", "File", XtRString, sizeof(String),
	offset(file), XtRString, NULL},
    { "command", "Command", XtRString, sizeof(String),
	offset(cmd), XtRString, "scan -file %s" },
    { "update", "Interval", XtRInt, sizeof(int),
	offset(update), XtRString, "10"},
    { "maxLines", "MaxLines", XtRInt, sizeof(int),
	offset(maxLines), XtRString, "20"}
};

static XrmOptionDescRec	optionDescList[] = {
    { "-file",	"*file",	XrmoptionSepArg,	(caddr_t) NULL},
    { "-update","*update",	XrmoptionSepArg,	(caddr_t) NULL}
};

static char *fallback_resources[] = {
    "XLbiff*font:	-*-clean-bold-r-normal--13-130-75-75-c-80-iso8859-1",
    NULL
};

static XtActionsRec lbiff_actions[] = {
    {"exit",Exit},
    {"popdown",Popdown}
};



/*************\
|*  Popdown  *|  callback for buttonpress anywhere in text window
\*************/
void
Popdown(Widget w, XtPointer client_data, XtPointer call_data)
{
    DEBUG(("++Popdown()\n"));
    popdown();
}


/**********\
|*  main  *|
\**********/
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

    textBox = XtVaCreateManagedWidget("text",
				      commandWidgetClass,
				      topLevel,
				      NULL);

    XtGetApplicationResources(topLevel, &lbiff_data,
			      xlbiff_resources, XtNumber(xlbiff_resources),
			      (ArgList)NULL,0);


    XtAddCallback(textBox,XtNcallback, Popdown, textBox);
    XtAppAddActions(app_context,lbiff_actions,XtNumber(lbiff_actions));

    if (argc > 1) {
	if (!strncmp(argv[1],"-v",2)) {
	    printf("xlbiff version %s, patchlevel %d\n",VERSION,PATCHLEVEL);
	    exit(0);
	} else if (!strncmp(argv[1],"-h",2)) {
	    printf("usage:\txlbiff\n");
	}
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


/*************\
|*  handler  *|  handles SIGALRM, checks mail file, and resets alarm
\*************/
void
handler()
{
    longjmp(myjumpbuf,1);
}


/************\
|*  doscan  *|  invoke MH ``scan'' command to examine mail messages
|************
|*	This routine looks at the mail file to see if it 
\*/
int
doscan()
{
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

    setXbuf(buf);
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

    DEBUG(("domagic\n"));
    if (borderWidth == -1)
      initStaticData(&borderWidth,&fontHeight,&fontWidth);

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

    if (h > lbiff_data.maxLines)
      h = lbiff_data.maxLines;

    DEBUG(("geom= %dx%d (%dx%d pixels)\n",w,h,w*fontWidth,h*fontHeight));
    XtResizeWidget(topLevel,w*fontWidth+6,h*fontHeight+4,borderWidth);

    XtSetArg(args[0],XtNlabel,s);
    XtSetValues(textBox,args,1);
}


/********************\
|*  initStaticData  *|  initializes data we will need often
\********************/
void
initStaticData(int *bw, int *fontH, int *fontW)
{
    Arg		args[2];
    XFontStruct *fs;
    int tmp;
    XCharStruct	c;

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

    XTextExtents(fs,"foo: bar\nfoof",13,&tmp,&tmp,&tmp,&c);
    printf("got back: %dx%d\n",c.ascent+c.descent,c.width);

    DEBUG(("font= %dx%d,  borderWidth= %d\n",*fontH,*fontW,*bw));
}

/*************\
|*  popdown  *|  kill window
\*************/
popdown()
{
    DEBUG(("++popdown()\n"));
    XtUnrealizeWidget(topLevel);
    XSync(XtDisplay(topLevel),0);

    visible = 0;
}


/***********\
|*  popup  *|  bring window up
\***********/
popup()
{
    DEBUG(("++popup()\n"));
    XBell(XtDisplay(topLevel),0);
    XtRealizeWidget(topLevel);
    XSync(XtDisplay(topLevel),0);

    visible = 1;
}
