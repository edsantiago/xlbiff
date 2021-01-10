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


AUTHOR
======

  Ed Santiago <ed@edsantiago.com>

  The latest version of xlbiff may be found at

   <https://github.com/edsantiago/xlbiff/>
