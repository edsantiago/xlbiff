#! /usr/bin/python3
# Unit test for mailbox-preview.py

import importlib
mailbox_preview = importlib.import_module('mailbox-preview')

import sys
import unittest

class TestMailboxPreview(unittest.TestCase):
    def test_flatten_imap_response(self):
        self.assertEqual(
            r'(Some text--"and \"quotes\" and a \\backslash")',
            mailbox_preview.flatten_imap_response(
                [(b'(Some text--{18}',
                  rb'and "quotes" and a \backslash'),
                 b')']))

    def test_parse_imap_bodystructure(self):
        self.assertRaises(
            ValueError,
            mailbox_preview.parse_imap_bodystructure_raises,
            ("This" "String" "Will" "Not" "Parse"))
        substituted = mailbox_preview.parse_imap_bodystructure(
            "This" "String" "Will" "Not" "Parse")
        self.assertEqual("text", substituted[0])
        self.assertEqual("ascii", substituted[5])
        self.assertEqual(
            ("text", "html", ("charset", "UTF-8"), None, None,
             "quoted-printable", 512, 17),
            mailbox_preview.parse_imap_bodystructure(
                '1 (BODY ("text" "html" ("charset" "UTF-8") NIL NIL '
                '"quoted-printable" 512 17))'))

    def test_extract_imap_preview_fuzzy_header(self):
        self.assertEqual(
            'From: testing harness\r\n'
            'Subject: your test\r\n'
            '\r\n'
            'the \"preview\" text',
            mailbox_preview.extract_imap_message_parts(
                [(b'1 (PREVIEW (FUZZY "the \\\"preview\\\" text") '
                  b'BODY[HEADER.FIELDS (FROM SUBJECT)] {45}',
                  b'From: testing harness\r\n'
                  b'Subject: your test\r\n\r\n'),
                 b')'], 'utf-8'))

    def test_extract_imap_preview_header(self):
        self.assertEqual(
            'From: testing harness\r\n'
            'Subject: your test\r\n'
            '\r\n'
            'the \"preview\" text',
            mailbox_preview.extract_imap_message_parts(
                [(b'1 (PREVIEW "the \\\"preview\\\" text" '
                  b'BODY[HEADER.FIELDS (FROM SUBJECT)] {45}',
                  b'From: testing harness\r\n'
                  b'Subject: your test\r\n\r\n'),
                 b')'], 'utf-8'))

    def test_extract_imap_preview_fuzzy_only(self):
        self.assertEqual(
            'the \"preview\" text',
            mailbox_preview.extract_imap_message_parts(
                [(b'1 (PREVIEW (FUZZY {18}',
                  b'the \"preview\" text'),
                 b')'], 'utf-8'))

    def test_extract_imap_preview_only(self):
        self.assertEqual(
            'the \"preview\" text',
            mailbox_preview.extract_imap_message_parts(
                [(b'1 (PREVIEW {18}',
                  b'the \"preview\" text'),
                 b')'], 'utf-8'))

    def test_extract_imap_header_body(self):
        self.assertEqual(
            'This is the body of the message',
            mailbox_preview.extract_imap_message_parts(
                [(b'1 BODY[1.1] {31}',
                  b'This is the body of the message'),
                 b')'], 'utf-8'))

    def test_clean_preview(self):
        # all the same: shortened
        self.assertEqual('=====', mailbox_preview.clean_preview('======'))
        # different: not shortened
        self.assertEqual('=-=-=-=', mailbox_preview.clean_preview('=-=-=-='))


if __name__ == '__main__':
    # modify test runner to output to stdout, because autopkgtest fails
    # tests that write to stderr
    unittest.main(testRunner=unittest.TextTestRunner(stream=sys.stdout))
