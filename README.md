WHAT IS XLBIFF
==============

  Is "you have mail" not quite enough detail?  Is a per-message
  notification too much?  Welcome to xlbiff, the X Literate Biff.

  Xlbiff presents enough information to let you decide, Do I want to
  read this new mail now?  And it stops distracting you once you decide.

  Xlbiff waits in the background, monitoring your mailbox file (or
  running your custom check-mail script).  When a new message arrives,
  it invokes the MH scan(1) command (or your custom mail-scanning
  script) and pops up a window with the output (typically the From and
  Subject line of each new message).  If more mail arrives, xlbiff
  scans again and resizes its preview window accordingly.

  Clicking the left mouse button anywhere in the window causes it to
  vanish.  It will also vanish if the mailbox becomes empty.  Xlbiff
  stays out of your way when there is no new mail and pops up only
  when something requests your attention.


ADVANTAGES (or, Why Yet Another Biff?)
==========

  Xlbiff:
   + occupies no screen real estate until mail comes in
   + supports scripts for checking mail
   + has configurable screen location, color, and font
   + can notify by bell and/or keyboard LED
   + shows all new messages in one, easy-to-dismiss window
   + lets you click anywhere on it; no trying to select a tiny "X"


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


AUTHOR
======

  Ed Santiago <ed@edsantiago.com>

  The latest version of xlbiff may be found at

   <https://github.com/edsantiago/xlbiff/>
