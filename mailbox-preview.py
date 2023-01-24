#! /usr/bin/python3
# Display one line for each of the last N new messages in an IMAP mailbox.
# Designed for use with xlbiff.
# By Stephen Gildea, November 2021.

"""
This script does three things:

1. Reads an IMAP mailbox without changing it.  In particular, fetches
with PEEK, so messages are not marked as Seen on the IMAP server.

2. Writes out a shortened version of each new message in MMDF format,
where they can be piped through "scan -file -", giving a nice listing
of the messages.  The output of "scan" is highly customizable;
see the --form option of this script.

Minimizes network traffic by fetching only the headers that would
contribute to "scan" output and enough body to fill out the scan line.
Decodes enough MIME to set the body part to actual content, so that
"scan", which is not MIME-aware, will not end up displaying MIME
headers or boundary strings.

3. Runs the shortened messages through "scan".


Optional first argument is an optional name of IMAP server (default "imap"),
optionally followed by a colon and a mailbox name (default "inbox")
Usage:
  mailbox-preview [[server][:mailbox]]
  mailbox-preview [[server][:mailbox]] --check oldsize

If the first argument is instead the name of a file under a
top-level directory that exists, it is treated as the name of a
local mail drop, and that file is used instead of doing IMAP.

With --check, output is the new size to pass us next time, and
exit value is per comments in the function do_check_and_exit, below.

To use with xlbiff, add these resources to XLbiff:

*checkCommand: mailbox-preview %s --check %d
*scanCommand:  mailbox-preview %s --width %d --max-messages %d 2>&1

When reporting interoperability problems with an IMAP server,
include the output of
  mailbox-preview server --imap-debug=4 --quiet 2>&1 | grep -v " LOGIN "
"""

import argparse
import os
import re
import subprocess
import sys

# There may be disagreement about how to number multipart/alternative.
# Set this to whatever your IMAP server does.
# If we find any servers that need this set to True, turn it into a flag.
PLAIN_ALTERNATIVE_IS_LAST = False

# This script has many options; we import modules only as we need them.
#pylint: disable=import-outside-toplevel

def main():
    """The main program.  See comments above."""
    argparser = argparse.ArgumentParser(
        description='Display one line for each of the last N messages '
        'in an IMAP mailbox.')
    argparser.add_argument(
        'server_mailbox', metavar='[server][:mailbox]',
        nargs='?', default='',
        help='default server hostname is "imap"; default mailbox is "inbox"')
    argparser.add_argument(
        '--check', dest='old_size', type=int, default=-1,
        help='check for mailbox change instead of scanning; see xlbiff(1)')
    argparser.add_argument(
        '--width', type=int, default=os.getenv("COLUMNS", "80"),
        help='width in characters to pass to "scan"; default is $COLUMNS')
    argparser.add_argument(
        '--max-messages', metavar='N', type=int, default=20,
        help='display only the last N (default 20) messages; 0 to display all')
    argparser.add_argument(
        '--form', default='xlbiff.form',
        help='format file for "scan" in place '
        "of xlbiff's default form; see mh-format(5). Default is xlbiff.form")
    argparser.add_argument(
        '--scanproc', default='/usr/bin/mh/scan',
        help='file name of the "scan" program to pipe the messages through; '
        'if set explicitly to an empty string, the messages will be '
        'output raw (probably useful only for debugging)')
    argparser.add_argument(
        '--quiet', action='store_true',
        help='do not output the messages (probably useful only for debugging)')
    argparser.add_argument(
        '--client-preview', action='store_true',
        help='do not use the IMAP PREVIEW command internally '
        '(probably useful only for testing)')
    argparser.add_argument(
        '--imap-debug', metavar='LEVEL', type=int, default=0,
        help='set imaplib debug level; 4 displays the IMAP protocol messages')

    args = argparser.parse_args()

    # This treatment of a local filename matches what xlbiff would do without
    # any checkCommand/scanCommand specified.  We support it here for
    # two reasons:
    # First, so that the user can switch between IMAP and local by merely
    # changing the file.  Second, nmh 1.7.1 and earlier cannot scan a Maildir.
    mailbox_is_maildir = False
    possible_local_file = re.match(r'(?P<topdir>/[^/]+)/.+$',
                                   args.server_mailbox)
    if possible_local_file:
        if os.path.isdir(possible_local_file.group('topdir')):
            if os.path.isdir(args.server_mailbox):
                # a directory is assumed to be a Maildir
                mailbox_is_maildir = True
            else:
                do_local_mailbox_file_and_exit(args)

    if mailbox_is_maildir:
        import mailbox
        maildir = mailbox.Maildir(args.server_mailbox)
        unseen_list = get_maildir_unseen_list(maildir)
    else:
        imap_server_name = 'imap'
        imap_mailbox_name = 'inbox'
        parsed_arg = re.match(
            r'(?P<host>\[[^]]+\]|[^[:]*)?(?::(?P<mailbox>.*))?',
            args.server_mailbox)
        if parsed_arg.group('host'):
            imap_server_name = parsed_arg.group('host')
        if parsed_arg.group('mailbox'):
            imap_mailbox_name = parsed_arg.group('mailbox')

        imapc = connect_to_imap(imap_server_name, args.imap_debug,
                                argparser.prog)
        if args.client_preview:
            server_preview_available = False
        else:
            server_preview_available = is_imap_server_preview_available(imapc)

        imapc.select(mailbox=imap_mailbox_name, readonly=True)
        unseen_list = get_imap_unseen_list(imapc)

    unseen_msg_count = len(unseen_list)

    # --check
    if args.old_size >= 0:
        do_check_and_exit(args.old_size, unseen_msg_count)

    if args.scanproc and not args.quiet:
        # By 2024, Python 3.6 may be rare enough that we can
        # replace universal_newlines=True with text=True
        scan_proc = subprocess.Popen([args.scanproc,
                                      '-file', '-',
                                      '-form', args.form,
                                      '-width', str(args.width)],
                                     stdin=subprocess.PIPE,
                                     universal_newlines=True)
        sys.stdout = scan_proc.stdin

    start_msg_index = 0
    if args.max_messages > 0 and unseen_msg_count > args.max_messages:
        start_msg_index += unseen_msg_count - args.max_messages
    for msg_index in range(start_msg_index, unseen_msg_count):
        cur_msg = unseen_list[msg_index]
        if mailbox_is_maildir:
            if not args.quiet:
                output_message(maildir.get_bytes(cur_msg))
        else:
            if server_preview_available:
                body_part = 'PREVIEW'
                encoding = 'utf-8'  # per RFC 8970
            else:
                # The summary will be more interesting if we can find
                # message content and not just multipart MIME headers.
                _ok, structure_data = imapc.fetch(cur_msg, b'BODY')
                part_to_request, encoding = get_displayable_part(structure_data)
                body_part = f'BODY.PEEK[{part_to_request}]<0.700>'

            header_part = 'BODY.PEEK[HEADER.FIELDS (DATE SUBJECT FROM TO CC)]'
            parts = (header_part, body_part)
            _ok, fetch_data = imapc.fetch(
                cur_msg,
                b'(' + ' '.join(parts).encode('UTF-8') + b')')
            # Return value is a 2-tuple:
            # ('OK', parts)

            if not args.quiet:
                output_imap_message_parts(fetch_data, encoding)

    if not mailbox_is_maildir:
        imapc.logout()


def do_local_mailbox_file_and_exit(args):
    """This treatment of a local filename matches what xlbiff would do
without any checkCommand/scanCommand specified.
It does not honor the max-messages parameter.
"""
    if args.old_size >= 0:
        try:
            new_size = os.path.getsize(args.server_mailbox)
        except OSError:
            new_size = 0
        do_check_and_exit(args.old_size, new_size)
    if os.path.exists(args.server_mailbox):
        # run the same default scanCommand xlbiff would
        subprocess.run([args.scanproc,
                        '-file', args.server_mailbox,
                        '-form', args.form,
                        '-width', str(args.width)],
                       check=True)
    sys.exit(0)


def do_check_and_exit(old_size, new_size):
    """Implement --check mode."""
    try:
        print(new_size)         # output the size to pass us next time
        sys.stdout.close()      # cause any EPIPE to happen now
    except BrokenPipeError:
        # If our caller has exited, the write
        # may fail.  Ignore their ignoring us.
        pass
    if new_size == old_size: sys.exit(1) # no change
    if new_size == 0: sys.exit(2)        # no longer new mail
    sys.exit(0)                          # new mail


def connect_to_imap(imap_server_name, debug_flag, progname):
    """Returns an imaplib connection object."""
    import imaplib
    import netrc
    import socket

    netrc_object = None
    netrc_error = None
    netrc_auth = None
    imap_username = ''
    imap_password = ''
    try:
        netrc_object = netrc.netrc()
        netrc_auth = netrc_object.authenticators(imap_server_name)
        if netrc_auth:
            imap_username = netrc_auth[0]
            imap_password = netrc_auth[2]
    except (OSError, netrc.NetrcParseError) as err:
        # Do not surface the error unless we fail to log in.
        # Maybe we won't need a .netrc file.
        netrc_error = err
    if not imap_username:
        import getpass
        imap_username = getpass.getuser()

    imaplib.Debug = debug_flag

    try:
        imapc = imaplib.IMAP4_SSL(imap_server_name)
    except socket.gaierror as sockerr:
        print(f'{progname}: Cannot connect to IMAP server host '
              f'{imap_server_name!r}: {sockerr}',
              file=sys.stderr)
        sys.exit(2)
    has_cram_md5 = 'AUTH=CRAM-MD5' in imapc.capabilities
    try:
        if has_cram_md5:
            imapc.login_cram_md5(imap_username, imap_password)
        else:
            imapc.login(imap_username, imap_password)
    except imapc.error as autherr:
        print(f'{progname}: Login as {imap_username!r} '
              f'to IMAP server {imap_server_name!r}: {autherr}',
              file=sys.stderr)
        if netrc_error is not None:
            print(f'{progname}: Could not use .netrc file: {netrc_error}',
                  file=sys.stderr)
        elif not netrc_auth:
            print(f'{progname}: '
                  f'~/.netrc is missing entry for machine {imap_server_name!r}',
                  file=sys.stderr)
        else:
            print(f'{progname}: '
                  f'Used entry in ~/.netrc for machine {imap_server_name!r}',
                  file=sys.stderr)
        sys.exit(2)
    return imapc

def is_imap_server_preview_available(imapc):
    """Return whether the IMAP server has the PREVIEW capability."""
    # capability() is not documented, so perhaps not a stable interface
    try:
        # Check for capabilities the server already sent us.
        _ok, post_login_capabilities = imapc.response('CAPABILITY')
        # Documentation disagrees with implementation about the return value.
        if post_login_capabilities in (None, [None]):
            _ok, post_login_capabilities = imapc.capability()
        capability_list = post_login_capabilities[-1].decode().split()
    except NameError:
        capability_list = []
    # Dovecot 2.3.15 dropped the "=FUZZY", in conformance with RFE 8970.
    return 'PREVIEW' in capability_list or 'PREVIEW=FUZZY' in capability_list

def get_imap_unseen_list(imapc):
    """Return a list of unseen messages in the currently selected mailbox."""
    _ok, unseen_message_resp = imapc.search(None, b'(UNSEEN)')
    return unseen_message_resp[0].split()


def Maildir_get_flags(maildir, key):
    """Return as a string the flags that are set on the keyed message.
This is the same as maildir.get(key).get_flags() but much faster,
because it does not open the message file.
This method should exist in the mailbox.Maildir class.
"""
    subpath = maildir._lookup(key) # using internal method!
    if maildir.colon in subpath:
        info = subpath.split(maildir.colon)[-1]
        if info.startswith('2,'):
            return info[2:]
    return ''


def get_maildir_unseen_list(maildir):
    """Return a list of unseen messages in the Maildir."""
    # This textual sort will fail in the year 2286.
    sorted_keys = sorted(maildir.keys())
    unseen_list = [key for key in sorted_keys
                   # S is for Seen
                   if 'S' not in Maildir_get_flags(maildir, key)]
    return unseen_list


def get_displayable_part(imap_body_list):
    """Accepts the BODY response from IMAP, returns the MIME part to request."""
    imap_body_string = flatten_imap_response(imap_body_list)
    mime_parts_list = parse_imap_bodystructure(imap_body_string)
    return select_displayable_part(mime_parts_list)

def flatten_imap_response(imap_cmdresp):
    """Flattens a list of IMAP response strings
(bytes or lists of bytes) into one string."""
    # IMAP-tools, on GitHub, parses this better.
    total_response = b''
    for fragment in imap_cmdresp:
        # All but the last in the list will be lists of two strings,
        # the first having a "{length}" at the end
        # (literal braces) giving the length of the second.
        if isinstance(fragment, tuple):
            fragment0_match = re.match(rb'(?P<text>.*){[0-9]+}$', fragment[0])
            fragment = fragment0_match.group('text') + fragment[1]
        total_response += fragment
    return total_response.decode(errors='ignore')

def parse_imap_bodystructure(imap_body_string):
    """Parses the result of a BODY or BODYSTRUCTURE response.
Returns a list of the MIME parts."""
    # IMAP-tools, on GitHub, parses this better.
    import ast
    # strip off leading message number and following space
    body_list_as_string = re.sub(r'^[^ ]+ \(BODY[^ ]* ', '(', imap_body_string)
    body_list_as_string = re.sub(r'\bNIL\b', 'None', body_list_as_string)
    # We have to add commas to convert from Lisp syntax to Python syntax
    # Adding a comma at all spaces also adds them inside quoted string;
    # this is wrong but we don't look at the multi-word quoted strings.
    body_list_as_string = re.sub(r'([) ] *)', r'\1, ', body_list_as_string)
    body_list = ast.literal_eval(body_list_as_string)
    # We had extra nesting
    return body_list[0][0]

def select_displayable_part(body_list):
    """Returns a tuple of 2 strings: the part number, and its encoding."""
    parts = []
    # unwrap multipart types looking for the one we want to display
    while isinstance(body_list[0], tuple):
        if body_list[-1] == "alternative" and PLAIN_ALTERNATIVE_IS_LAST:
            # MIME standard says the last element of alternative type will be
            # the simplest.
            parts += [f'{len(body_list)-1}']
            body_list = body_list[-2]
        else:
            # take the first element of any other multipart type
            parts += ['1']
            body_list = body_list[0]
    if len(parts) == 0:
        parts = ['1']
    part_to_fetch = '.'.join(parts)
    encoding = body_list[5]
    return (part_to_fetch, encoding)


def output_message(message_bytes):
    """Output the bytes of a message.  Does not handle MIME nor encoding."""
    print('\1\1\1\1')           # MMDF delimiter
    # Todo: figure out actual encoding
    print(decode_body_part(message_bytes, 'utf-8'))
    print('\1\1\1\1')           # MMDF delimiter


def output_imap_message_parts(parts, body_encoding):
    """Writes one mesasge to stdout, in MMDF format."""
    # parts is list:
    # [msg1, msg2, ..., ')']
    # each message has a 2-tuple entry, and at the end is literal ')'
    # each message 2-tuple:
    # (imap-metadata contents)
    # If we asked for PREVIEW, the imap-metadata is
    #     msgnum (PREVIEW (FUZZY "preview text ...")
    #     BODY[HEADER.FIELDS (DATE SUBJECT FROM TO CC)] {nnn}
    # and the contents is the headers.
    # Or, could be imap-metadata is
    #     msgnum (PREVIEW (FUZZY {nnn}
    # and the contents is the preview (followed by literal ')'),
    # and the next imap-metadata is
    #     BODY[HEADER.FIELDS (DATE SUBJECT FROM TO CC)] {nnn}
    # If we didn't ask for PREVIEW, the first imap-metadata is
    #     msgnum (BODY[HEADER.FIELDS (DATE SUBJECT FROM TO CC)] {nnn}
    # and the second is
    #     BODY[1.1]<0> {500}
    print('\1\1\1\1')           # MMDF delimiter
    header = ""
    preview = ""
    body = ""
    for part in parts:
        if not isinstance(part, tuple): # the close paren
            continue

        preview_literal_match = re.search(
            rb'\(PREVIEW \([A-Z]+ "(?P<text>.*?[^\\])"\)', part[0])
        if preview_literal_match:
            preview = preview_literal_match.group('text').decode(
                body_encoding, errors='ignore')
            # remove backslashes used for quoting quotes
            preview = re.sub(r'\\([\'"])', r'\1', preview)

        preview_next_match = re.search(
            rb'\(PREVIEW \([A-Z]+ {[0-9]+}$', part[0])
        if preview_next_match:
            preview = part[1].decode(body_encoding, errors='ignore')

        header_next_match = re.search(
            rb'BODY\[HEADER\.FIELDS', part[0])
        if header_next_match:
            header = part[1].decode(errors='ignore')

        body_next_match = re.search(
            rb'BODY\[[0-9.]+\]', part[0])
        if body_next_match:
            decoded_part = decode_body_part(part[1], body_encoding)
            body += decoded_part

    preview = clean_preview(preview)
    body = clean_preview(body)
    # Either preview or body will be non-empty
    print(f'{header}{preview}{body}')
    print('\1\1\1\1')           # MMDF delimiter

def decode_body_part(body_part, body_encoding):
    """Returns body_part decoded per body_encoding."""
    if body_encoding.lower() == "base64":
        import base64
        encoded = re.sub(b'\r\n', b'', body_part)
        # The message may have been truncated; truncate it more
        # to an encoded byte boundary
        x4len = len(encoded) - len(encoded) % 4
        return base64.b64decode(encoded[:x4len]).decode(errors='ignore')
    if body_encoding.lower() == "quoted-printable":
        import quopri
        return quopri.decodestring(body_part).decode(errors='ignore')
    if body_encoding.lower() == "7bit":
        return body_part.decode('ascii', errors='ignore')
    if body_encoding.lower() == "8bit":
        return body_part.decode('latin1', errors='ignore')
    return body_part.decode(errors='ignore')

def clean_preview(preview):
    """Some messages have lots of *'s or ='s as visual spacers.  Elide them."""
    # reduce more than 5 repeated characters to just 5
    return re.sub(r'((.)\2\2\2\2)\2+', r'\1', preview)


if __name__ == '__main__':
    main()
