/* Copyright (c) 2010-2013, Simon Lipp
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

typedef struct {
    char *id;
    char *title;
    char *link;
    char *summary;
    char *content;
    char *publication_date;
    char *modification_date;
    char *subtitle;
    char *link_title;
    char *enclosure;
    struct {
        char *name;
        char *email;
        char *uri;
        char *text;
    } author;
} Entry;

typedef struct {
    Entry **entries;
    int entries_size;
    char *title;
    char *subtitle;
    char *description;
    char *link;
    char *link_title;
    char *id;
    char *publication_date;
    char *modification_date;
    struct {
        char *name;
        char *email;
        char *uri;
        char *text;
    } author;
} Feed;

typedef struct _FeedParser FeedParser;

FeedParser *feed_parser_new();
Feed *feed_parser_parse_string(FeedParser *parser, const char *data, int size);
Feed *feed_parser_parse_file(FeedParser *parser, const char *file);
char *feed_parser_get_error(FeedParser *parser);
void feed_parser_free(FeedParser *parser);
void feed_free(Feed *feed);
