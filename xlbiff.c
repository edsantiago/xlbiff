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
|*	Author:		Eduardo Santiago, ed@edsantiago.com
|* 	Created:	20 August 1991
|*	Last Updated:	16 May 2017
|*
|*    Copyright 1994, 2017 Eduardo Santiago
|*    SPDX-License-Identifier: MIT
 */

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>
#include <X11/Xaw/Command.h>
#include <X11/Xos.h>
#include <X11/extensions/Xrandr.h>

#include <unistd.h>
#include <stdlib.h>
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
#if defined(SYSV) || defined(SVR4)
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


/*
** if compiled with -DDEBUG *and* run with debugging on, this does lots
** of useless/useful printfs.
*/
#ifdef DEBUG
#define DP(x) if (lbiff_data.debug) printf x
#else
#define DP(x)
#endif

/*
** This defines the file we need to monitor.  If not defined explicitly
** on the command line, we pick this default.
*/
#ifndef XLBIFF_MAILPATH
#define XLBIFF_MAILPATH "/var/mail/%s"
#endif

/*****************************************************************************\
**                                prototypes                                 **
\*****************************************************************************/
char		*doScan();
void		Popdown(), Popup();
void		Usage();
extern char	*getlogin();

void	Shrink(Widget, XtPointer, XEvent*, Boolean*);
void	handler(XtPointer, XtIntervalId*);
void	initStaticData(int*, int*, int*);
void	Exit(Widget, XEvent*, String*, Cardinal*);
void	Mailer(Widget, XEvent*, String*, Cardinal*);
void	lbiffUnrealize(), lbiffRealize(char*);
void	getDimensions(char*, Dimension*, Dimension*);
void	toggle_key_led(int);
void	init_randr();
void	ErrExit(Boolean, char*);
Bool	CheckEvent(Display*, XEvent*, XPointer);
waitType popen_nmh(char *cmd, int bufsize, char **buf_out, size_t *size_out);

/*****************************************************************************\
**                                 globals				     **
\*****************************************************************************/
extern int errno;

Widget	topLevel, textBox;		/* my widgets			*/
XtAppContext app_context;		/* application context		*/
Boolean	visible;			/* is window visible?		*/
Boolean hasdata;			/* Something is to be displayed */
char	*default_file;			/* default filename		*/
char	*progname;			/* my program name		*/
struct timeval acknowledge_time = {0};	/* time window was acknowledged	*/
struct timeval popup_time = {0};	/* time window was popped up	*/

static Atom wm_delete_window;		/* for handling WM_DELETE	*/

typedef struct {
    Boolean	debug;			/* print out useful stuff 	*/
    char	*file;			/* file to monitor size of 	*/
    char	*checkCmd;		/* command to run for check     */
    char	*cmd;			/* command to run for output	*/
    char	*mailerCmd;		/* command to read mail		*/
    float	update;			/* update interval, in seconds	*/
    float	fade;			/* popdown interval, in seconds */
    int		columns;		/* number of columns across	*/
    int		rows;			/* max# of lines in display	*/
    int		volume;			/* bell volume, 0-100 percent	*/
    Boolean	bottom;			/* Put window at window bottom  */
    Boolean	resetSaver;		/* reset screensaver on popup   */
    float	refresh;		/* seconds before reposting msg	*/
    int		led;			/* led number to light up	*/
    Boolean	ledPopdown;		/* turn off LED on popdown?	*/
    char	*sound;			/* Sound file to use		*/
} AppData, *AppDataPtr;
AppData lbiff_data;

#define offset(field) XtOffset(AppDataPtr, field)

float default_update_secs = 15.0f;
float default_fade_secs = 0.0f;
float default_refresh_secs = 1800.0f;

static XtResource xlbiff_resources[] = {
    {"debug", "Debug", XtRBoolean, sizeof(Boolean),
     offset(debug), XtRImmediate, False},
    {"file", "File", XtRString, sizeof(String),
     offset(file), XtRString, NULL},
    {"checkCommand", "CheckCommand", XtRString, sizeof(String),
     offset(checkCmd), XtRString, NULL},
    {"scanCommand", "ScanCommand", XtRString, sizeof(String),
     offset(cmd), XtRString, "scan -file %s -width %d 2>&1"},
    {"mailerCommand", "MailerCommand", XtRString, sizeof(String),
     offset(mailerCmd), XtRString, NULL },
    {"update", "Interval", XtRFloat, sizeof(float),
     offset(update), XtRFloat, &default_update_secs},
    {"fade", "Fade", XtRFloat, sizeof(float),
     offset(fade), XtRFloat, &default_fade_secs},
    {"columns", "Columns", XtRInt, sizeof(int),
     offset(columns), XtRImmediate, (XtPointer)80},
    {"rows", "Rows", XtRInt, sizeof(int),
     offset(rows), XtRImmediate, (XtPointer)20},
    {"sound", "Sound", XtRString, sizeof(String),
     offset(sound), XtRString, ""},
    {"volume", "Volume", XtRInt, sizeof(int),
     offset(volume), XtRImmediate, (XtPointer)100},
    {"bottom", "Bottom", XtRBoolean, sizeof(Boolean),
     offset(bottom), XtRImmediate, False},
    {"resetSaver", "ResetSaver", XtRBoolean, sizeof(Boolean),
     offset(resetSaver), XtRImmediate, False},
    {"refresh", "Refresh", XtRFloat, sizeof(float),
     offset(refresh), XtRFloat, &default_refresh_secs},
    {"led", "Led", XtRInt, sizeof(int),
     offset(led), XtRImmediate, (XtPointer)0},
    {"ledPopdown", "LedPopdown", XtRBoolean, sizeof(Boolean),
     offset(ledPopdown), XtRImmediate, False}
};

static XrmOptionDescRec optionDescList[] = {
    {"-bottom",      ".bottom",      XrmoptionNoArg,	(XtPointer) "true"},
    {"+bottom",      ".bottom",      XrmoptionNoArg,	(XtPointer) "false"},
    {"-debug",       ".debug",	      XrmoptionNoArg,	(XtPointer) "true"},
    {"-file",	      ".file",        XrmoptionSepArg,	(XtPointer) NULL},
    {"-rows",        ".rows",	      XrmoptionSepArg,	(XtPointer) NULL},
    {"-columns",     ".columns",     XrmoptionSepArg,	(XtPointer) NULL},
    {"-update",      ".update",      XrmoptionSepArg,	(XtPointer) NULL},
    {"-fade",	      ".fade",        XrmoptionSepArg,	(XtPointer) NULL},
    {"-volume",      ".volume",      XrmoptionSepArg,	(XtPointer) NULL},
    {"-resetSaver",  ".resetSaver",  XrmoptionNoArg,	(XtPointer) "true"},
    {"+resetSaver",  ".resetSaver",  XrmoptionNoArg,	(XtPointer) "false"},
    {"-refresh",     ".refresh",     XrmoptionSepArg,	(XtPointer) NULL},
    {"-led",         ".led",         XrmoptionSepArg,	(XtPointer) NULL},
    {"-ledPopdown",  ".ledPopdown",  XrmoptionNoArg,	(XtPointer) "true"},
    {"+ledPopdown",  ".ledPopdown",  XrmoptionNoArg,	(XtPointer) "false"},
    {"-sound",	      ".sound",       XrmoptionSepArg,  (XtPointer) NULL},
    {"-scanCommand", ".scanCommand", XrmoptionSepArg,	(XtPointer) NULL},
    {"-mailerCommand",".mailerCommand",XrmoptionSepArg,(XtPointer) NULL},
    {"-checkCommand",".checkCommand",XrmoptionSepArg,  (XtPointer) NULL}
};

static char *fallback_resources[] = {
    "*Font: -*-clean-bold-r-normal--13-130-75-75-c-80-iso646.1991-*",
    "*Geometry: +0-0",
    NULL
};

static XtActionsRec lbiff_actions[] = {
    {"exit", Exit},
    {"mailer", Mailer},
    {"popdown", Popdown}
};


/*****************************************************************************\
**                                  code                                     **
\*****************************************************************************/

/**********\
|*  main  *|
\**********/
int main(int argc, char *argv[]) {
    progname = argv[0];

    XtSetLanguageProc(NULL, NULL, NULL);
    topLevel = XtVaAppInitialize(&app_context,
                                 "XLbiff",
                                 optionDescList, XtNumber(optionDescList),
                                 &argc, argv,
                                 fallback_resources,
                                 XtNallowShellResize, True,
                                 NULL);

    XtGetApplicationResources(topLevel, &lbiff_data,
                              xlbiff_resources, XtNumber(xlbiff_resources),
                              (ArgList)NULL, 0);

#ifndef DEBUG
    if (lbiff_data.debug)
        fprintf(stderr, "%s: DEBUG support not compiled in, sorry\n", progname);
#endif

    /*
    ** Check command line arguments
    */
    if (argc > 1) {
        if (!strncmp(argv[1], "-version", strlen(argv[1]))) {
            fprintf(stderr, "%s version %s\n", progname, VERSION);
            exit(0);
        } else if (!strncmp(argv[1], "-help", strlen(argv[1]))) {
            Usage();
        } else if (argv[1][0] != '-') {
            lbiff_data.file = argv[1];
        } else {
            fprintf(stderr,
                    "%s: no such option \"%s\", type '%s -help' for help\n",
                    progname, argv[1], progname);
            exit(1);
        }
    }

    /*
    ** If no data file was explicitly given, make our best guess
    */
    if (lbiff_data.file == NULL) {
        char *username = getlogin();

        if (username == NULL || username[0] == '\0') {
            struct passwd *pwd = getpwuid(getuid());

            if (pwd == NULL) {
                fprintf(stderr, "%s: cannot get username\n", progname);
                exit(1);
            }
            username = pwd->pw_name;
        }

        // -2 for the "%s" removed by formatting, +1 for the NUL.
        size_t mailpath_file_size =
            strlen(XLBIFF_MAILPATH) - 2 + strlen(username) + 1;
        default_file = (char *)malloc(mailpath_file_size);
        if (default_file == NULL)
            ErrExit(True, "default_file malloc()");

        snprintf(default_file, mailpath_file_size, XLBIFF_MAILPATH, username);
        default_file[mailpath_file_size - 1] = '\0';
        lbiff_data.file = default_file;
    }
    DP(("file= %s\n", lbiff_data.file));

    if (lbiff_data.cmd == NULL || lbiff_data.cmd[0] == '\0') {
        fprintf(stderr, "%s: empty scanCommand will not work\n", progname);
        exit(1);
    }

    /*
    ** Fix DISPLAY environment variable, might be needed by subprocesses
    */
    {
        char *envstr =
            (char *)malloc(strlen("DISPLAY=") + 1 +
                           strlen(XDisplayString(XtDisplay(topLevel))));

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
                      (XtEventHandler)Shrink, (XtPointer)NULL);

    XtOverrideTranslations(
        topLevel, XtParseTranslationTable("<Message>WM_PROTOCOLS: exit()"));

    wm_delete_window =
        XInternAtom(XtDisplay(topLevel), "WM_DELETE_WINDOW", False);

    toggle_key_led(False);

    /*
    ** check to see if there's something to do, pop up window if necessary,
    ** and set up alarm to wake us up again every so often.
    */
    handler(NULL, NULL);

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
void Usage() {
    static char *help_message[] = {
        "where options include:",
        "    -version                   display xlbiff version number",
        "    -display host:dpy          X server to contact",
        "    -geometry +x+y             x,y coords of window",
        "    -rows height               height of window, in lines",
        "    -columns width             width of window, in characters",
        "    -file file                 file to watch",
        "    -update seconds            how often to check for mail",
        "    -fade seconds              lifetime of unmodified window",
        "    -volume percentage         how loud to ring the bell",
        "    -bg color                  background color",
        "    -fg color                  foreground color",
        "    -refresh seconds           seconds before re-posting window",
        "    -led ledNum                keyboard LED to light up",
        "    -ledPopdown                turn off LED when popped down",
        "    -scanCommand command       command to interpret and display",
        "    -checkCommand command      command used to check for change",
        "    -mailerCommand command     command used to read mail",
        NULL};
    char **s;

    printf("usage:\t%s  [-options ...]  [file to watch]\n", progname);
    for (s = help_message; *s; s++)
        printf("%s\n", *s);
    printf("\n");
    exit(1);
}

// Returns true if the difference between newtime and oldtime is
// greater than interval_seconds.
int time_passed(struct timeval *newtime, struct timeval *oldtime,
                float interval_seconds) {
    float timediff_secs = newtime->tv_sec - oldtime->tv_sec +
                          (newtime->tv_usec - oldtime->tv_usec) * 1e-6;
    return timediff_secs > interval_seconds;
}

/**********\
|*  Exit  *|  called via callback, exits the program
\**********/
void Exit(Widget w, XEvent *event, String *params, Cardinal *num_params) {
    DP(("++Exit()\n"));

    if (event->type == ClientMessage) {
        if (event->xclient.data.l[0] != wm_delete_window) {
            DP(("received client message that was not delete_window\n"));
            XBell(XtDisplay(w), 0);
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
 */
void checksize() {
    static int mailsize = 0;
    struct stat mailstat;
    int pop_window = False;
    struct timeval tp;

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
        waitType     status;
        int          outbuf_size = 80;
        static char *outbuf;
        static char *cmd_buf;
        static int   previous;

        if (cmd_buf == NULL) {
            cmd_buf = (char *)malloc(strlen(lbiff_data.checkCmd) +
                                     strlen(lbiff_data.file) + 10);
            if (cmd_buf == NULL)
                ErrExit(True, "scan command buffer malloc()");
        }
        if (outbuf == NULL) {
            outbuf = (char *)malloc(outbuf_size);
            if (outbuf == NULL)
                ErrExit(True, "check output buffer malloc()");
        }

        sprintf(cmd_buf, lbiff_data.checkCmd, lbiff_data.file, previous);
        DP(("++checkCommand= %s\n", cmd_buf));

        status = popen_nmh(cmd_buf, outbuf_size, &outbuf, NULL);
        previous = atol(outbuf);
        DP(("checkCommand returns %d\n", previous));
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
        if (stat(lbiff_data.file, &mailstat) != 0) {
            DP(("stat() failed, errno=%d.  Assuming filesize=0!\n", errno));
            mailstat.st_size = 0;
        }
    }

    /*
    ** If it's changed size, take appropriate action.
    */
    if (mailstat.st_size != mailsize) {
        DP(("changed size: %d -> %d\n", mailsize, (int)mailstat.st_size));
        mailsize = mailstat.st_size;
        pop_window = True;
    } else if (!visible && lbiff_data.refresh && mailsize != 0) {
        /*
        ** If window has been popped down, check if it's time to refresh
        */
        if (gettimeofday(&tp, NULL) != 0) {
            ErrExit(True, "gettimeofday() in checksize()");
        } else {
            if (time_passed(&tp, &acknowledge_time, lbiff_data.refresh)) {
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
        if (gettimeofday(&tp, NULL) != 0) {
            ErrExit(True, "gettimeofday() in checksize()");
        } else if (lbiff_data.fade > 0) {
            if (time_passed(&tp, &popup_time, lbiff_data.fade)) {
                DP(("fade time (%f) reached\n", lbiff_data.fade));
                lbiffUnrealize();
            }
        }
    }

    if (pop_window) {
        if (mailsize == 0) {
            hasdata = False;
            toggle_key_led(False);
            lbiffUnrealize();
        } else {				/* something was added? */
            char *s = doScan();

            if (strlen(s) != 0) {		/* is there anything new? */
                if (hasdata) /* ESM && isvisible? ESM */
                    lbiffUnrealize();		/* pop down if it's up    */
                hasdata = True;
                toggle_key_led(True);
                lbiffRealize(s);		/* pop back up */
            }
        }
    } else {
        DP(("no change (still %d)\n", mailsize));
    }
}


/************\
|*  Mailer  *|  called via callback, starts a mailer
\************/
void Mailer(Widget w, XEvent *event, String *params, Cardinal *num_params) {
    int system_return;
    DP(("++Mailer()\n"));

    Popdown();
    if (lbiff_data.mailerCmd != NULL && lbiff_data.mailerCmd[0] != '\0') {
        DP(("---mailerCmd = %s\n", lbiff_data.mailerCmd));
        system_return = system(lbiff_data.mailerCmd);
        if (system_return == 0) {
            DP(("---mailerCmd completed successfully\n"));
        } else {
            fprintf(stderr, "mailer command \"%s\" returned %d (%#x)\n",
                    lbiff_data.mailerCmd, system_return, system_return);
        }
    }
    checksize();
    Popup();
}


/*************\
|*  handler  *|  Checks mail file and reschedules itself to do so again
\*************/
void handler(XtPointer closure, XtIntervalId *id) {
    checksize();
    long int update_msecs = lbiff_data.update * 1000.0f + 0.5f;
    XtAppAddTimeOut(app_context, update_msecs, handler, NULL);
}


/************\
|*  doScan  *|  invoke MH ``scan'' command to examine mail messages
|************
|* This routine looks at the mail file and parses the contents. It
|* does this by invoking scan(1) or some other user-defined function.
 */
char *doScan() {
    static char *cmd_buf;
    static char *buf = NULL;
    static int   bufsize;
    static char  scan_fail_msg[] = "\n---->>>> scanCommand failed <<<<<----\n";
    size_t       size;
    waitType     status;

    DP(("++doScan()\n"));

    /*
    ** Initialise display buffer to #rows * #cols
    ** Initialise command string
    */
    if (buf == NULL) {
        /* +1 for the newline */
        bufsize = (lbiff_data.columns + 1) * lbiff_data.rows;

        buf = (char *)malloc(bufsize + sizeof(scan_fail_msg) + 1);
        if (buf == NULL)
            ErrExit(True, "text buffer malloc()");

        DP(("---size= %dx%d\n", lbiff_data.rows, lbiff_data.columns));

        cmd_buf = (char *)malloc(strlen(lbiff_data.cmd) +
                                 strlen(lbiff_data.file) + 10);
        if (cmd_buf == NULL)
            ErrExit(True, "command buffer malloc()");

        sprintf(cmd_buf, lbiff_data.cmd, lbiff_data.file, lbiff_data.columns);
        DP(("---cmd= %s\n", cmd_buf));
    }

    /*
    ** Execute the command, read the results, then set the contents of window.
    */
    status = popen_nmh(cmd_buf, bufsize, &buf, &size);
    if (waitCode(status) != 0) {
        strcpy(buf + size, scan_fail_msg);
        size += strlen(scan_fail_msg);
    }

    buf[size] = '\0';				/* null-terminate it! */

    DP(("scanned:\n%s\n", buf));
    return buf;
}


/****************\
|*  CheckEvent  *|
\****************/
Bool CheckEvent(Display *d, XEvent *e, XPointer arg) {
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
void Shrink(Widget w, XtPointer data, XEvent *e, Boolean *b) {
#ifdef DEBUG
    char *type_str;
    switch (e->type) {
    case UnmapNotify: type_str = "UnmapNotify"; break;
    case MapNotify: type_str = "MapNotify"; break;
    case ReparentNotify: type_str = "ReparentNotify"; break;
    case ConfigureNotify: type_str = "ConfigureNotify"; break;
    default: type_str = "event type";
    }
    DP(("++Shrink(%s %d)\n", type_str, e->type));
#endif
    if (e->type != MapNotify && e->type != UnmapNotify) {
        return;
    }
    int event_seen = 0;
    Window win = e->xmap.window;

    memcpy((char *)&lastEvent, (char *)e, sizeof(XEvent));

    XSync(XtDisplay(w), False);

    while (XCheckIfEvent(XtDisplay(w), &lastEvent, CheckEvent,
                         (XPointer)win)) {
        event_seen = 1;
    }

    if (!event_seen) {
        return;
    }

    if (lastEvent.type == UnmapNotify && visible) {
        Popdown();
    } else if (lastEvent.type == MapNotify && hasdata) {
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
void Popdown() {
    struct timeval tp;

    DP(("++Popdown()\n"));

    if (visible) {
        XtPopdown(topLevel);
        XSync(XtDisplay(topLevel), False);
    }
    visible = False;

    /*
    ** Remember when we were popped down so we can refresh later
    */
    if (gettimeofday(&tp, NULL) != 0)
        ErrExit(True, "gettimeofday() in lbiffUnrealize()");

    acknowledge_time = tp;

    if (lbiff_data.ledPopdown)		/* Turn off LED if so requested */
        toggle_key_led(False);
}


void Popup() {
    struct timeval tp;

    DP(("++Popup()\n"));

    /*
    ** Remember when we were popped up so we can fade later
    */
    if (gettimeofday(&tp, NULL) != 0)
        ErrExit(True, "gettimeofday() in Popup()");
    popup_time = tp;

    if (hasdata && !visible) {
        XtPopup(topLevel, XtGrabNone);
        XSync(XtDisplay(topLevel), False);
    }
    visible = True;
}


/********************\
|*  lbiffUnrealize  *|  kill window
\********************/
void lbiffUnrealize() {
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
void lbiffRealize(char *s) {
    Arg args[4];
    int n;
    static int first_time = 1;

    DP(("++lbiffRealize()\n"));

    /*
    ** Set the contents of the window
    */
    n = 0;
    XtSetArg(args[n], XtNlabel, s); n++;
    XtSetValues(textBox, args, n);

    /*
    ** If running with *bottom, we need to tell the widget what size it
    ** is before realize()ing it.  This is so the WM can position it
    ** properly at the bottom of the screen.
    */
    if (lbiff_data.bottom) {
        Dimension width, height;

        getDimensions(s, &width, &height);

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
        (void)XSetWMProtocols(XtDisplay(topLevel), XtWindow(topLevel),
                              &wm_delete_window, 1);
        init_randr();
        first_time = 0;
    }

    if (lbiff_data.sound[0] == '\0') {
        /*
        ** No, the following is not a typo, nor is it redundant code.
        ** Apparently there is one X terminal that beeps whenever XBell()
        ** is called, even with volume zero.
        */
        if (lbiff_data.volume > 0) {
            XBell(XtDisplay(topLevel), lbiff_data.volume - 100);
            DP(("---sound= %s\n", "XBell default"));
        }
    } else {
        static char *sound_buf;
        int system_return;

        /*
        ** Initialise sound string
        */
        if (sound_buf == NULL) {
            sound_buf = (char *)malloc(strlen(lbiff_data.sound) + 10);
            if (sound_buf == NULL)
                ErrExit(True, "sound_buf malloc()");

            sprintf(sound_buf, lbiff_data.sound, lbiff_data.volume);
            DP(("---sound= %s\n", sound_buf));
        }
        system_return = system(sound_buf);
        if (system_return != 0) {
            fprintf(stderr, "sound command \"%s\" returned %d (%#x)\n",
                    sound_buf, system_return, system_return);
        }
    }

    if (lbiff_data.resetSaver)
        XResetScreenSaver(XtDisplay(topLevel));

    visible = True;
}


/*******************\
|*  getDimensions  *|  get width x height of text string
\*******************/
void getDimensions(char *s, Dimension *width, Dimension *height) {
    Dimension tmp_width;
    int i, len = strlen(s);
    static int fontWidth, fontHeight;
    static int borderWidth = -1;

    tmp_width = *width = *height = 1;

    if (borderWidth == -1)
        initStaticData(&borderWidth, &fontHeight, &fontWidth);

    /*
    ** count rows and columns
    */
    for (i = 0; i < len - 1; i++) {
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

    DP(("geom= %dx%d chars (%dx%d pixels)\n", *width, *height,
                                              *width * fontWidth,
                                              *height * fontHeight));

    *width  *= fontWidth;  *width  += 6;	/* convert to pixels 	  */
    *height *= fontHeight; *height += 4;	/* and add a little fudge */
}


/********************\
|*  initStaticData  *|  initializes font size & borderWidth
\********************/
void initStaticData(int *bw, int *fontH, int *fontW) {
    Arg args[2];
    XFontStruct *fs = NULL;
    int tmp = 0;

    DP(("++initStaticData..."));
    XtSetArg(args[0], XtNfont, &fs);
    XtSetArg(args[1], XtNborderWidth, &tmp);
    XtGetValues(textBox, args, 2);
    if (fs == NULL)
        ErrExit(False, "unknown font");

    *bw = tmp;
    *fontW = fs->max_bounds.width;
    *fontH = fs->max_bounds.ascent + fs->max_bounds.descent;

    DP(("font= %dx%d,  borderWidth= %d\n", *fontH, *fontW, *bw));
}


/********************\
|*  toggle_key_led  *|  toggle a keyboard LED on and off
\********************/
void toggle_key_led(int flag) {
    XKeyboardControl keyboard;

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
 */
void ErrExit(Boolean errno_valid, char *s) {
    if (errno_valid)
        fprintf(stderr, "%s: %s: %s\n", progname, s, strerror(errno));
    else
        fprintf(stderr, "%s: %s\n", progname, s);

    toggle_key_led(False);

    XCloseDisplay(XtDisplay(topLevel));

    exit(1);
}

waitType popen_simple(char *cmd, int bufsize, char **buf_out,
                      size_t *size_out) {
    FILE *p;
    size_t read_size;
    waitType status;
    /*
    ** Execute the command and read the results.
    ** If there is data remaining in the pipe, read it in (and throw it away)
    ** so our exit status is correct (eg, not "Broken pipe").
    */
    if ((p = popen(cmd, "r")) == NULL)
        ErrExit(True, "popen");
    read_size = fread(*buf_out, 1, bufsize, p);
    if (read_size == bufsize) {
        --read_size;              // leave room for NUL we add later
        char junkbuf[100];
        while (fread(junkbuf, 1, 100, p) > 0)
            ;	/* Keep reading until no more left */
    }
    (*buf_out)[read_size] = '\0';

#ifdef INTWAITTYPE
    status = pclose(p);
#else
    status.w_status = pclose(p);
#endif

    if (size_out) {
        *size_out = read_size;
    }
    return status;
}

/***************\
|*  popen_nmh  *|  invoke a command to check or scan
|***************
|*      Executes a command, returning its stdout.
|*      Does nmh initialization if necessary, so nmh commands such as "scan"
|*      work even if the user has not run install-mh.
|*      Handling this case here means xlbiff works out of the box.
 */
#define PROFILE_TEMPLATE "xlbiff-mh-profile-XXXXXX"
waitType popen_nmh(char *cmd, int bufsize, char **buf_out, size_t *size_out) {
    waitType status;
    char *profile_name;
    char *tmpdir_name;
    char slash_tmp[] = "/tmp";
    int namelen;
    int profile_fd;
    FILE *profile_stream;

    status = popen_simple(cmd, bufsize, buf_out, size_out);
    if (waitCode(status) == 0) {
        return status;
    }
    /* If an MH profile file is missing, scan (and all other nmh programs)
     * give the error "Doesn't look like nmh is installed.
     * Run install-mh to do so."
     * This misleadingly implies the nmh package is not installed
     * on the system, but in fact means that the user does not have
     * a ~/.mh_profile file created yet.
     * We handle this case so that xlbiff works out of the box.
     */
    if (strstr(*buf_out, "Doesn't look like nmh is installed.") == NULL) {
        /* failed for some other reason */
        return status;
    }

    /* Create a temporary MH profile file */

    tmpdir_name = getenv("TMPDIR");
    if (tmpdir_name == NULL || strlen(tmpdir_name) == 0)
        tmpdir_name = slash_tmp;
    namelen = strlen(tmpdir_name) + 1 + strlen(PROFILE_TEMPLATE) + 1;
    profile_name = (char *)malloc(namelen);
    if (profile_name == NULL)
        ErrExit(True, "profile_name malloc()");
    strcpy(profile_name, tmpdir_name);
    strcat(profile_name, "/");
    strcat(profile_name, PROFILE_TEMPLATE);

    mode_t old_mask = umask(077);
    profile_fd = mkstemp(profile_name);
    if (profile_fd < 0)
        ErrExit(True, "profile_name mkstemp()");
    umask(old_mask);
    DP(("created profile_name = %s\n", profile_name));
    profile_stream = fdopen(profile_fd, "w");
    fprintf(profile_stream, "Path: %s\nWelcome: disable\n", tmpdir_name);
    fclose(profile_stream);

    setenv("MH", profile_name, /*overwrite=*/1);

    /* try again to run the command */

    status = popen_simple(cmd, bufsize, buf_out, size_out);

    /* Remove our temporary profile so that if the user later runs install-mh,
     * we will notice and use their profile.
     */
    unsetenv("MH");
    unlink(profile_name);
    free(profile_name);

    return status;
}

/*
 * For -bottom option, we need to know if the height of the screen changes,
 * because we may be positioned relative to the bottom of the screen.
 * Use the RANDR extension to learn about screen size changes.
 */

/* helper function to handle RANDR screen change events */
void handle_screen_change(Widget w, XtPointer client_data, XEvent *event,
                          Boolean *continue_to_dispatch) {
    DP(("++screen_change\n"));
    int old_screen_width = WidthOfScreen(XtScreen(w));
    int old_screen_height = HeightOfScreen(XtScreen(w));
    // Pass the event back to Xlib, where it can update
    // the Display structure with the new screen dimensions.
    XRRUpdateConfiguration(event);
    int new_screen_width = WidthOfScreen(XtScreen(w));
    int new_screen_height = HeightOfScreen(XtScreen(w));
    if (old_screen_height != new_screen_height ||
        old_screen_width != new_screen_width) {
        DP(("screen dimensions changed: %dx%d -> %dx%d\n", old_screen_width,
            old_screen_height, new_screen_width, new_screen_height));
        // reinterpret our geometry
        if (visible) {
            DP(("screen_change: widget is visible, so letting it move\n"));
            XtUnrealizeWidget(w);
            XtRealizeWidget(w);
        }
    }
}

/* helper function to tell Xt where to dispatch RANDR screen change events */
Boolean dispatch_screen_change(XEvent *event) {
    return XtDispatchEventToWidget(topLevel, event);
}

/*
 * Initialize RANDR extension to track when the bottom of the screen changes.
 */
void init_randr() {
    int event1, error1;
    if (!XRRQueryExtension(XtDisplay(topLevel), &event1, &error1)) {
        DP(("XRRQueryExtension returned False\n"));
        return;
    }
    // provide a handler for RANDR screen change events
    XtInsertEventTypeHandler(topLevel, event1 + RRScreenChangeNotify,
                             /*select_data=*/NULL, handle_screen_change,
                             /*client_data=*/NULL, XtListTail);
    // register a function to tell Xt dispatcher which widget has the handler
    XtSetEventDispatcher(XtDisplay(topLevel), event1 + RRScreenChangeNotify,
                         dispatch_screen_change);
    // tell RANDR we want to receive screen configuration events
    XRRSelectInput(XtDisplay(topLevel), XRootWindowOfScreen(XtScreen(topLevel)),
                   RRScreenChangeNotifyMask);
}
