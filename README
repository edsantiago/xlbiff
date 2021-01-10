WHAT IS IT?
===========

  Is xbiff just not quite enough for you?

  This is xlbiff, the X Literate Biff.

  Xlbiff lurks in the background, monitoring your mailbox file (normally
  /var/mail/_yourusername_).  When something shows up there, it
  invokes the MH scan(1) command and displays the output in a window.
  If more mail comes in, it scans again and resizes accordingly.

  Clicking the left mouse button anywhere in the window causes it to
  vanish.  It will also vanish if you inc(1) and the mailbox becomes
  empty.  Xlbiff is modeled after xconsole -- its job is to sit invisibly
  and pop up only when something demands your attention.


ADVANTAGES (or, Why Yet Another Biff?)
==========

  Xlbiff:
   + occupies no screen real estate until mail comes in
   + lets you preview new mail to decide if you want to read it immediately
   + is easy to make go away


INSTALLING
==========

      ./configure
      make && make install

  The configured default mail file template is /var/mail/%s.
  If this is incorrect for your system, set CONFIG_MAILPATH when
  running configure, e.g.:

      ./configure CONFIG_MAILPATH=/somewhere/mail/%s

  To see a list of the flags that control where "make install"
  installs various files:

      ./configure --help

  If you have a source distribution without "configure" and
  "Makefile.in" files, you can create them with this command:

      autoreconf -i

CUSTOMIZING
===========

  You may want to tweak some values in the app-defaults file and/or add
  some resources to your .Xdefaults.  You also probably want to tell your
  window manager not to put borders or titlebars or whatever around the
  xlbiff window.  Finally, don't forget to add an "l" to that xbiff
  invocation in your .Xlogin!

  Note that an MH format file, xlbiff.form, is included.  This form:

   - omits message number, which is meaningless in this context
   - omits message size, since `scan -file` can't figure it out
   - puts a "*" next to the message if your name is on the To: list
     (before you say "no duh", think about mailing lists and cc's)
   - displays the date in a friendly format
   - packs as much subject & body into one line as possible.

  Xlbiff.form was stolen & hacked from Jerry Peek's excellent Nutshell
  book on MH & xmh.

  There are also two sample scripts, Bcheck and Bscan, intended to be
  used in conjunction.  These are for checking mail in "bulk" maildrops.
  See README.bulk for more info.


THINGS TO BE AWARE OF
=====================

  Xlbiff invokes `scan -file xxx` by default, and thus requires MH 6.7
  or above.  If your MH isn't at this level, you can find the latest
  version on ftp.ics.uci.edu:/pub/mh.  If you don't feel like upgrading,
  there's a really, really ugly kludge you can do with inc:

	inc -notruncate -silent -file %s -form xlbiff.form          	\
                -width %d -audit /tmp/xlbiff.$$ +.null >/dev/null 2>&1; \
                cat /tmp/xlbiff.$$;                                     \
                rm /tmp/xlbiff.$$ >/dev/null 2>&1;                      \
                rmf +.null >/dev/null 2>&1

  Note that this is truly bletcherous and will give you lots of headaches
  when new mail comes in and your MH context gets screwed up.

  If you're a Berkeley mail person, you can set scanCommand to:

	echo x | mail | grep "^.[NU]"

  but it sure does look ugly.  Perhaps there are .mailrc settings
  to make it look more decent?

  If you use Elm you may want to use `frm` as your scanCommand.  Some
  people seem to be very satisfied with the `fromwho` package, posted
  by jearls@blackbird.csc.calpoly.edu to comp.sources.unix volume 25,
  available on gatekeeper.dec.com or ftp.uu.net or other places Archie
  surely knows about.

  Also be aware that problems have been detected with xlbiff reading a
  mail file on an NFS-mounted directory.  Xlbiff is not guaranteed to
  work under these conditions.


PORTING (or, if INSTALLING didn't work)
=======================================

  Xlbiff should be pretty portable.  It was developed in 1991 on a
  DECstation 5000/200 running Ultrix 4.2 using cc and gcc; it has
  been known to run on Linux, Solaris, and others.

  Xlbiff requires X11R5 or beyond and works best with MH 6.7 and up,
  or nmh 1.1.

  I *think* most portability bugs have been ironed out, but of course
  there are always a few left.  If fixes are required, or if you add
  any whizzy features (see TODO for a partial list), please mail them
  to me, <ed@edsantiago.com>, and I will wrap them up for future releases.


BACKGROUND
==========

  I wrote xlbiff because I don't always want to read my mail.  Regular
  xbiff lets me know when I have mail but not what it is -- this makes
  me have to inc(1) just so I can decide if I want to bother reading it.
  Xlbiff gives me a much better indication of whether or not I want to
  take time to read a new message.  If I don't, I just click it away
  and continue with what I'm doing.

  Before xlbiff, I wrote a crude hack using perl and xmessage.  It worked,
  but I had always wanted to do it right.


HISTORY
=======

  The following describes changes made to xlbiff throughout its history.
  Where applicable, changes are attributed to the people who submitted
  the ideas and, in many cases, patches and source code.

 * Version 4.5  (7 Dec 2020)
   + handle screen size changes with RANDR, so "xlbiff -bottom"
     stays at screen bottom

 * Version 4.4  (1 Nov 2020)
   + scanCommand works even if user has not run mh-install

 * Version 4.3  (7 Sept 2020)
   + Removed "international: True" X resource.
     This was added in 4.2 and changed font resources incompatibly.
     Users for whom "international" was useful, or who had already updated
     their resources to handle it, can get the 4.2 behavior back by adding
     the following line to an X Resources file or -xrm argument:
     *international: true
   + Update default font resource with the font's current name
   + Changed default mail path directory to /var/mail from /usr/spool/mail
   + Changed manual page name to xlbiff.1 from xlbiff.1x
   + Install xlbiff.form to /usr/local/share/mh, not /usr/new/lib/mh
   + Developers: Build proceess changed from Imake to Autotools.
   + Developers: Assume modern C library and compiler.

 * Version 4.2  (16 May 2017)
   + Primitive i18n support
   + minor code cleanup (whitespace, warnings)
   + Again, update email addresses and URLs

 * Version 4.1  (8 Nov 2003)
   + improved Bcheck and Bscan
   + updated email addresses, URLs
   + Packaged for Debian release

 * Version 4.0  (2 June 1994 for X11R6 contrib tape)
   + expand functionality of "checkCommand"
   + include "bulk" scripts, taking advantage of checkCommand
   + general cleanup

   From Stephen Gildea <gildea@x.org>
   + don't call XBell() if volume is zero (workaround for bug
     in certain X terminals).

 * Version 3.1  (unreleased)

   From Bjoern Stabell <bjoerns@stud.cs.uit.no>
   + add *mailerCommand resource and functionality

 * Version 3.0 (released 26 Oct 92)
   + miscellaneous bug fixes

   From Gary Weimer <weimer@ssd.kodak.com>
   + add "sound" resource -- allows playing any sound on popup.

   From Neil O'Sullivan <Neil.OSullivan@eng.sun.com>
   + add "fade" resource -- pops window down after certain time.

 * Version 2.1 BETA

   From  Daren W Latham <dwl@mentat.udev.cdc.com>
   + add -refresh and -led options.
   + handle WM_DELETE_WINDOW.

   From Peter Fischman <peterf@ima.isc.com>
   + Add some comments to Makefile.std to help out SYSV'ers

   From Stephen Gildea <gildea@expo.lcs.mit.edu>
   + Lots of cleanup in the default resources.

   From Yuan Liu <liu@cs.UMD.EDU>
   + Handle remapping better, don't vanish when WM restarts.

   From me:
   + add ErrExit routine, do some more cleanup.
   + output error message if scan fails, but don't die.
   + add ledPopdown option.
   + add checkCommand option (after lots of pestering by Mark!)

 * Version 2.0 submitted for X11R5 contrib tape  Fri  4 Oct 1991
   + no change, just up rev level

 * Version 1.5 submitted for testing  Thu  3 Oct 1991

   From  Jhon Honce <honce@spso.gsfc.nasa.gov>
   + change 'maxWidth/maxHeight' to 'columns/rows'.  Bottom option
        was broken and I hadn't even noticed!

   From me:
   + I need to track revisions better...

 * Version 1.4 submitted for testing  Thu  3 Oct 1991

   From  Arthur Castonguay <arthurc@doe.carleton.ca>
   + ignore stat() errors, treat them as empty file.

   From Stephen Gildea <gildea@expo.lcs.mit.edu>
   + use XtPop{up,down} instead of {,un}mapWidget.

   From several or myself
   + change .width resource to .maxWidth to fix strange bug.
   + yet more portability fixes and cleanup.

 * Version 1.3 submitted for testing  Tue  1 Oct 1991

   From Stephen Gildea <gildea@expo.lcs.mit.edu>
   + use allowShellResize and {,un}map instead of {,un}realize so
        it tracks position changes.
   + clean up .ad file
   + raise window to top of stack on new mail.
   + add 'resetSaver' option.

   From Mike Schroeder <mike@geronimo.pcs.com>
   + don't pop up if results of scanCommand are null.  This is
        useful for mail(1)-based users since the mailbox size will
        normally be >0 but that may not indicate new mail.

   From several
   + remove ANSI C dependencies.
   + use XtAppAddTimeOut() instead of alarm()/sleep()/setjmp().
   + clean up a bit.

   From me
   + add 'bottom' resource to preserve old functionality if you
        want to keep xlbiff at the bottom of your screen.

  * Version 1.0 posted to alt.sources  Thu 19 Sep 1991


AUTHOR
======

  Ed Santiago <ed@edsantiago.com>

  The latest version of xlbiff may be found at

   <https://github.com/edsantiago/xlbiff/>
