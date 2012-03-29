#!/usr/bin/python

# Copyright (c) 2010, Simon Lipp
# 
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
# 
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

import os
import ctypes
import UserDict

_libpath = os.path.join(os.path.dirname(__file__) or ".", "libfeedparser.so")
_lib = ctypes.cdll.LoadLibrary(_libpath)

def _copystr(s):
    if s == None:
        return None
    else:
        return unicode(s, 'utf-8')[:].strip()

class _EntryStruct(ctypes.Structure):
    _fields_ = [('id', ctypes.c_char_p),
                ('title', ctypes.c_char_p),
                ('link', ctypes.c_char_p),
                ('summary', ctypes.c_char_p),
                ('content', ctypes.c_char_p),
                ('created', ctypes.c_char_p),
                ('updated', ctypes.c_char_p),
                ('subtitle', ctypes.c_char_p),
                ('link_title', ctypes.c_char_p),
                ('author_name', ctypes.c_char_p),
                ('author_email', ctypes.c_char_p),
                ('author_url', ctypes.c_char_p),
                ('author', ctypes.c_char_p)]

class _FeedStruct(ctypes.Structure):
    _fields_ = [('entries', ctypes.POINTER(ctypes.POINTER(_EntryStruct))),
                ('entries_size', ctypes.c_int),
                ('title', ctypes.c_char_p),
                ('subtitle', ctypes.c_char_p),
                ('description', ctypes.c_char_p),
                ('link', ctypes.c_char_p),
                ('link_title', ctypes.c_char_p),
                ('id', ctypes.c_char_p),
                ('created', ctypes.c_char_p),
                ('updated', ctypes.c_char_p),
                ('author_name', ctypes.c_char_p),
                ('author_email', ctypes.c_char_p),
                ('author_url', ctypes.c_char_p),
                ('author', ctypes.c_char_p)]

class Entry(UserDict.UserDict):
    def __init__(self, struct):
        UserDict.UserDict.__init__(self)
        for fieldname, fieldtype in _EntryStruct._fields_:
            if fieldtype == ctypes.c_char_p:
                self[fieldname] = _copystr(getattr(struct, fieldname))
    
    def __getattr__(self, attr):
        return self[attr]

class Feed(UserDict.UserDict):
    def __init__(self, struct):
        UserDict.UserDict.__init__(self)
        self['entries_size'] = struct.entries_size
        self['entries'] = []
        for i in range(self.entries_size):
            self['entries'].append(Entry(struct.entries[i].contents))
        for fieldname, fieldtype in _FeedStruct._fields_:
            if fieldtype == ctypes.c_char_p:
                self[fieldname] = _copystr(getattr(struct, fieldname))
    
    def __getattr__(self, attr):
        return self[attr]
    
    def __iter__(self):
        return iter(self.entries)
    
    def __len__(self):
        return self.entries_size

class ParseError(Exception):
    pass

class Parser(object):
    _new_parser = _lib.feed_parser_new
    _get_error = _lib.feed_parser_get_error
    _parse_file = _lib.feed_parser_parse_file
    _parse_string = _lib.feed_parser_parse_string
    _parser_free = _lib.feed_parser_free
    _feed_free = _lib.feed_free
    
    def __init__(self):
        self.__ptr = self._new_parser()
    
    def parse_file(self, file):
        return self._convert_feed(self._parse_file(self.__ptr, ctypes.c_char_p(file)))

    def parse_string(self, data):
        if isinstance(data, unicode):
            data = data.encode('utf-8')
        return self._convert_feed(self._parse_string(self.__ptr, ctypes.c_char_p(data), ctypes.c_int(len(data))))
    
    def _convert_feed(self, feedp):
        err = self._get_error(self.__ptr)
        if err:
            raise ParseError(_copystr(err).strip())
        feed = Feed(feedp.contents)
        self._feed_free(feedp)
        return feed
    
    def free(self):
        if self.__ptr:
            self._parser_free(self.__ptr)
        self.__ptr = None
    
    __del__ = free

Parser._new_parser.restype = ctypes.c_void_p
Parser._parse_file.restype = ctypes.POINTER(_FeedStruct)
Parser._parse_string.restype = ctypes.POINTER(_FeedStruct)
Parser._parser_free.restype = None
Parser._feed_free.restype = None
Parser._get_error.restype = ctypes.c_char_p

try:
    import fp_date, fp_encoding
    
    try:
        import feedparser
    except ImportError:
        feedparser = None
    
    def parse(file_stream_or_string):
        def parse_dates(e):
            e['date'] = e['updated'] or e['created']
            for k in ('date', 'updated', 'created'):
                e[k+'_parsed'] = e[k] and fp_date.parse_date(e[k])

        data = fp_encoding.fetch_feed(file_stream_or_string)
        try:
            parser = Parser()
            feed = parser.parse_string(data)
            parser.free()
        except ParseError:
            if feedparser:
                return feedparser.parse(file_stream_or_string)
            else:
                raise
        
        parse_dates(feed)
        for e in feed.entries: parse_dates(e)
        feed['feed'] = feed
        return feed
except ImportError:
    parse = None

if __name__ == "__main__":
    import sys, time

    def mprint(fmt = None, *args):
        if fmt is None:
            print
            return
        
        def _tounicode(s):
            if isinstance(s, str):
                return unicode(s, 'utf-8')
            else:
                return s
        args = tuple(_tounicode(s) for s in args)
        print (fmt % args).encode('utf-8')

    p = Parser()
    for file in sys.argv[1:]:
        print file
        try:
            if parse:
                f = parse(file)
            else:
                f = p.parse_file(file)
        except ParseError, e:
            mprint("Error: %s", str(e))
            continue
        
        mprint("%d entries.", len(f))
        for e in f:
            mprint("------------------------")
            mprint("Subject: %s", e.title)
            mprint("From: %s", e.author)
            mprint("URL: %s (%s)", e.link, e.link_title)
            mprint("ID: %s", e.id)
            mprint("Created: %s", e.created)
            mprint("Modified: %s", e.updated)
            mprint()
            mprint("%s", e.content or e.summary)
        mprint()
        mprint("========================")
