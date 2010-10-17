CFeedParser -- fast feed parser
===============================

## Presentation

CFeedParser is a feed parser (yeah!) written in C with Python
bindings. It is heavily inspired by the excellent [Universal Feed
Parser](http://www.feedparser.org/) and, in fact, share some code with
it. Differences with Universal Feed Parser are :

 * far less features :

CFeedParser only support the direct extraction of most used elements
(title, link, id, summary, content and author). It does not extract other
elements (like copyright, source, generator,...), nor it extract elements
attributes (like language or content type), does not export metadata
(like encoding or feed format), know nothing about fancy things like
microformats, and does not “sanitize” the HTML content.

CFeedParser support all formats supported by Universal Feed Parser (I
reused a great part of Universal Feed Parser unit tests), as long as
they are well-formed.

 * no support for illformed feeds :

CFeedParser does not try to fix feeds that are not real XML.

 * fast :

CFeedParser is amazingly fast. If fact, that’s the reason it was written
— Universal Feed Parser was too slow for one on my usage. For example,
let’s try to parse all my feeds (35) with CParserFeed, then its Python
bindings, and then with Universal Feed Parser.

` $ time ./cfeedparser-test feeds/* > /dev/null 0,21s user 0,01s system
94% cpu 0,239 total `

` $ time ./cfeedparser.py feeds/* > /dev/null 0,76s user 0,05s system 95%
cpu 0,844 total `

` $ time ./feedparser.py feeds/* > /dev/null 8,42s user 0,04s system 99%
cpu 8,525 total `

Yes, the Python bindings are 10 times faster than Universal Feed Parser,
and CFeedParser 40 times faster !

## Dependencies

libxml-2.0 and glib-2.0. That’s all.

## The Python bindings

The Python bindings are designed to be a subset of Universal Feed
Parser. There is three ways to use them:

 * Standalone: you have the same features and performances than
   CFeedParser.
 * With Universal Feed Parser date parsing and encoding detection
   capabilites: you have reduced performances (~ 2 times slower), but the
   dates are parsed, and it support more encodings. To enable this mode,
   just put fp\_date.py and fp\_encoding.py in the same directory that
   cfeedparser.py and then use `cfeedparser.parse` function.
 * With Universal Feed Parser: in addition with the preceding mode, you
   can add Universal Feed Parser in your path, and all ill-formed feeds
   will automatically be handled by Universal Feed Parser (you have an
   overhead only for ill-formed feeds)

