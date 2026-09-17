What is xlbiff
==============

  Is "you have mail" not quite enough detail?  Is a per-message
  notification too much?  Welcome to xlbiff, the X Literate Biff.

  Xlbiff presents enough information to tell you: Is this new mail worth
  reading right now?  And it stops distracting you once you decide.

  Xlbiff waits in the background, monitoring your mailbox file or IMAP
  server (or running any state-monitoring script you configure).
  When a new message arrives, it invokes the MH scan(1) command (or
  any status-generating script) and pops up a window with the output.
  If more mail arrives (or the monitored state changes), xlbiff scans
  again and resizes its preview window accordingly.

  You can customize scan(1) to show any header and/or content; by
  default xlbiff displays the From, Subject and beginning of the
  message content.  Xlbiff can also beep or play a custom sound.

  Clicking the left mouse button anywhere in the window causes it to
  vanish.  It will also vanish if the mailbox becomes empty.  Xlbiff
  stays out of your way when there is no new mail and pops up only
  when something requests your attention.

  Included with xlbiff is mailbox-preview, a command-line program
  that peeks at an IMAP mailbox and uses scan to display a summary,
  one line per new message.   Local mailboxes are also supported:
  Maildir directories, MH folders, mbox files and MMDF files.
  Use mailbox-preview stand-alone on the command line or as the
  back-end of xlbiff.


Advantages (or, why yet another biff?)
==========

  Xlbiff:
   + occupies no screen real estate until mail comes in
   + supports scripts for checking mail
   + has configurable screen location, color, and font
   + can notify by bell, sound file and/or keyboard LED
   + shows all new messages in one, easy-to-dismiss window
   + lets you click anywhere on it; no trying to select a tiny "x"


Installing
==========

  To compile, you will need the run-time library and development
  header files for the following libraries: Xaw, Xt, Xrandr, X11.

  To run xlbiff in its default configuration, you will need nmh
  ("new MH") and Python 3 installed.

  When you have these dependencies installed, build xlbiff:

      ./configure
      make && make install

  The configured default mail file template is /var/mail/%s.
  If this is incorrect for your system, set CONFIG_MAILPATH when
  running configure, e.g.:

      ./configure CONFIG_MAILPATH=/somewhere/mail/%s

  To see a list of the flags that control where "make install"
  installs various files:

      ./configure --help

  If you are building from a source repository, without "configure"
  and "Makefile.in" files, you will need the GNU Autotools: autoconf
  and automake.  With those packages installed, you can create the
  needed files with this command:

      autoreconf -i


Documentation
=============

  The complete guide to using and configuring xlbiff is in xlbiff.man.
  Using and configuring mailbox-preview is documented in mailbox-preview.man.
  After installing xlbiff, you can view the formatted manuals with

      man xlbiff
      man mailbox-preview


Author
======

  Ed Santiago <ed@edsantiago.com>

  The latest version of xlbiff may be found at

   <https://github.com/edsantiago/xlbiff/>
