/* Copyright (c) 2010, Simon Lipp
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

// ~ pkg: libxml-2.0 glib-2.0
// ~ cflags: -O4 -Wall
// ~ out: _feedparser.so
// ~ ldflags: -shared

#include <string.h>
#include <ctype.h>
#include <stdio.h>

#include <glib.h>
#include <libxml/parser.h>

#include "feedparser.h"

#define min(x, y) (((x) < (y)) ? (x) : (y))

struct _Author {
    char *name;
    char *email;
    char *uri;
    char *text;
};

struct _FeedParser {
    char *error;
    GSList *entries; // previous entries
    Feed *feed;
    Entry *entry; // current entry
    GString *text; // current level-1-tag content
    GString *author_text; // current author attribute (email...) content
    
    int feed_level;
    int entry_level;
    int author_level;
    int dump_xml;
    int base64;
    
    struct _Author *current_author;
};

#define PUBDATE_TAGS "issued", "published", "created"
#define MODDATE_TAGS "pubdate", "date", "modified", "updated"
#define DATE_TAGS PUBDATE_TAGS, MODDATE_TAGS

static const char *known_feed_tags[] = {"title", "link", "id", "author", "subtitle", "abstract",
    "description", "managingeditor", "creator", "summary",
    DATE_TAGS, NULL};
static const char *known_entry_tags[] = {"link", "title", "creator", "author", "body",
    "link", "id", "guid", "description", "summary", "content", "encoded", "abstract",
    "fullitem", "subtitle",
    DATE_TAGS, NULL};
static const char *known_author_tags[] = {"name", "email", "uri", "url", "homepage", NULL};

static const char *ignored_namespaces[] = {
    "http://schemas.pocketsoap.com/rss/myDescModule/",
    "http://search.yahoo.com/mrss/",
    NULL
};

static int in_array(const char **array, const char *string);
static inline int is_feed(const char *c_name) { return !strcasecmp(c_name, "rss") || !strcasecmp(c_name, "channel") || !strcasecmp(c_name, "feed"); }
static inline int is_entry(const char *c_name) { return !strcasecmp(c_name, "item") || !strcasecmp(c_name, "entry"); }
static inline int is_author(const char *c_name) { return !strcasecmp(c_name, "managingeditor") || !strcasecmp(c_name, "author") || !strcasecmp(c_name, "creator"); }
static inline int is_pubdate(const char *c_name) { static const char *a[] = {PUBDATE_TAGS, NULL}; return in_array(a, c_name); }
static inline int is_moddate(const char *c_name) { static const char *a[] = {MODDATE_TAGS, NULL}; return in_array(a, c_name); }
static inline int is_id(const char *c_name) { return !strcasecmp(c_name, "guid") || !strcasecmp(c_name, "id"); }
static inline int is_summary(const char *c_name) { return !strcasecmp(c_name, "description") || !strcasecmp(c_name, "summary") || !strcasecmp(c_name, "abstract"); }
static inline int is_content(const char *c_name) { return !strcasecmp(c_name, "fullitem") || !strcasecmp(c_name, "body") || !strcasecmp(c_name, "content") || !strcasecmp(c_name, "encoded"); }
static inline int is_uri(const char *c_name) { return !strcasecmp(c_name, "uri") || !strcasecmp(c_name, "url") || !strcasecmp(c_name, "homepage"); }

static int empty(const char *s)
{
    if(s == NULL || *s == 0)
        return 1;
    while(*s && isspace(*s)) s++;
    return (*s == 0);
}

static int in_array(const char **array, const char *string)
{
    while(*array) {
        if(!strcasecmp(*array, string)) {
            return 1;
        } else {
            array++;
        }
    }
    return 0;
}

#define PARSER ((FeedParser*)parser)

static void process_start_document(void *parser)
{
    free(PARSER->error);
    PARSER->error = NULL;
    
    PARSER->feed = malloc(sizeof(Feed));
    memset(PARSER->feed, 0, sizeof(Feed));
    
    PARSER->entries = NULL;
    
    PARSER->feed_level = -1;
    PARSER->entry_level = -1;
    PARSER->author_level = -1;
    PARSER->dump_xml = 0;
    PARSER->base64 = 0;
}

static void process_end_document(void *parser)
{
    GSList *entry;
    int i;
    
    if(PARSER->feed) { /* can be set to null by process_error */
        PARSER->feed->entries_size = g_slist_length(PARSER->entries);
        PARSER->feed->entries = calloc(sizeof(Entry*), PARSER->feed->entries_size);
        for(entry = PARSER->entries, i = 0; entry; entry = entry->next, i++) {
            PARSER->feed->entries[PARSER->feed->entries_size - 1 - i] = entry->data;
        }
    }
    
    g_slist_free(PARSER->entries);
    PARSER->entries = NULL;
}

static void process_error(void *parser, const char *msg,...)
{
    GString *s = g_string_new("");
    va_list ap;
    va_start(ap, msg);
    g_string_vprintf(s, msg, ap);
    va_end(ap);
    PARSER->error = g_string_free(s, 0);
    
    feed_free(PARSER->feed);
    PARSER->feed = NULL;
    
    g_slist_free(PARSER->entries);
    PARSER->entries = NULL;
}

static void process_characters(void *parser, const xmlChar *data, int size)
{
    char *escaped = NULL;
    if(PARSER->dump_xml) {
        escaped = g_markup_escape_text((const char*)data, size);
        data = (const xmlChar*)escaped;
        size = strlen(escaped);
    }
    
    if(PARSER->author_text)
        g_string_append_len(PARSER->author_text, (const char*)data, size);
    else if(PARSER->text)
        g_string_append_len(PARSER->text, (const char*)data, size);
    
    if(escaped)
        free(escaped);
}

#define NEXT_ATTRIBUTE c_attrname = *c_attrs++, c_attrns = *c_attrs++, c_attrnsurl = *c_attrs++, c_attrvalstart = *c_attrs++, c_attrvalend = *c_attrs++
#define EACH_ATTRIBUTE if(attributes) for(c_attrs = (const char**)attributes, i = 0, NEXT_ATTRIBUTE; i < nb_attributes; i++, NEXT_ATTRIBUTE)

static void find_link(int nb_attributes, const xmlChar **attributes, char **href, char **title)
{
    const char **c_attrs = (const char**)attributes;
    const char *c_attrname, *c_attrvalstart, *c_attrvalend, *c_attrns, *c_attrnsurl;
    
    char *rel = NULL;
    int i;
    
    *href = NULL;
    *title = NULL;

    EACH_ATTRIBUTE {
        if(!strcasecmp(c_attrname, "href"))
            *href = strndup(c_attrvalstart, c_attrvalend - c_attrvalstart);
        if(!strcasecmp(c_attrname, "title"))
            *title = strndup(c_attrvalstart, c_attrvalend - c_attrvalstart);
        else if(!strcasecmp(c_attrname, "rel"))
            rel = strndup(c_attrvalstart, c_attrvalend - c_attrvalstart);
    }
    
    if(empty(*title)) {
        free(*title);
        *title = NULL;
    }
    
    if(empty(*href)) {
        free(*href);
        *href = NULL;
    }
    
    if(!empty(rel) && strcasecmp(rel, "alternate")) {
        free(*href);
        free(*title);
        *href = NULL;
        *title = NULL;
    }
    
    free(rel);
}

static int is_base64(int nb_attributes, const xmlChar **attributes)
{
    const char **c_attrs = (const char**)attributes;
    const char *c_attrname, *c_attrvalstart, *c_attrvalend, *c_attrns, *c_attrnsurl;
    int i;
    
    EACH_ATTRIBUTE {
        if(!strcasecmp(c_attrname, "mode") && !strncasecmp(c_attrvalstart, "base64", c_attrvalend - c_attrvalstart))
            return 1;
        if(!strcasecmp(c_attrname, "type")) {
            if(!strncasecmp(c_attrvalstart, "text/", min(5, c_attrvalend - c_attrvalstart)))
                return 0;
            if(!strncasecmp(c_attrvalend - 3, "xml", min(3, c_attrvalend - c_attrvalstart)))
                return 0;
            if(!strncasecmp(c_attrvalend - 4, "html", min(4, c_attrvalend - c_attrvalstart)))
                return 0;
            return 1;
        }
    }
    return 0;
}

static void fix_author(struct _Author *author, char **text)
{
    if(!empty(author->name) || !empty(author->email)) {
        if(!empty(author->name) && !empty(author->email)) {
            author->text = g_strdup_printf("%s (%s)", author->name, author->email);
        } else if(!empty(author->name)) {
            author->text = strdup(author->name);
        } else {
            author->text = strdup(author->email);
        }
        free(*text);
        *text = NULL;
    }
}

static char *unpack_text(FeedParser *parser)
{
    char *text = g_string_free(parser->text, 0), *buf;
    unsigned int _unused;
    
    if(parser->base64 && *text) {
        buf = text;
        text = (char*)g_base64_decode(buf, &_unused);
        free(buf);
    }
    
    parser->text = NULL;
    parser->dump_xml = 0;
    parser->base64 = 0;
    
    return text;
}

static void process_start_element(void *parser,
    const xmlChar *name, const xmlChar *prefix, const xmlChar *uri,
    int nb_namespaces, const xmlChar **namespaces,
    int nb_attributes, int nb_defaulted, const xmlChar **attributes)
{
    int i;
    const char *c_name = (const char*)name;
    const char **c_attrs = (const char**)attributes;
    const char *c_attrname, *c_attrvalstart, *c_attrvalend, *c_attrns, *c_attrnsurl;
    char *buf, *escaped;
    
    if(uri == NULL)
        uri = (const xmlChar*)"";
    
    if(PARSER->feed_level != -1)
        PARSER->feed_level++;
    if(PARSER->entry_level != -1)
        PARSER->entry_level++;
    if(PARSER->author_level != -1)
        PARSER->author_level++;
    
    /* Outside anything: wait for a <channel> element */
    if(PARSER->feed_level == -1) {
        if(is_feed(c_name) && !in_array(ignored_namespaces, (const char*)uri)) {
            PARSER->feed_level = 0;
            find_link(nb_attributes, attributes, &PARSER->feed->link, &PARSER->feed->link_title);
            EACH_ATTRIBUTE {
                if(!strcasecmp(c_attrname, "lastmod"))
                    PARSER->feed->modification_date = strndup(c_attrvalstart, c_attrvalend - c_attrvalstart);
            }
        }
        
        /* In RDF, items are outside the channel. Deal with that */
        if(is_entry(c_name) && !in_array(ignored_namespaces, (const char*)uri)) {
            PARSER->feed_level = 1;
        } else {
            return;
        }
    }
    
    /* Directly inside a channel: wait for an known element */
    if(PARSER->feed_level == 1) {
        if(is_entry(c_name) && !in_array(ignored_namespaces, (const char*)uri)) {
            PARSER->entry = malloc(sizeof(Entry));
            memset(PARSER->entry, 0, sizeof(Entry));
            PARSER->entry_level = 0;
            find_link(nb_attributes, attributes, &PARSER->entry->link, &PARSER->entry->link_title);
            EACH_ATTRIBUTE {
                if(!strcasecmp(c_attrname, "lastmod"))
                    PARSER->entry->modification_date = strndup(c_attrvalstart, c_attrvalend - c_attrvalstart);
                if(!strcasecmp(c_attrname, "about"))
                    PARSER->entry->id = strndup(c_attrvalstart, c_attrvalend - c_attrvalstart);
            }
        } else if(in_array(known_feed_tags, c_name) && !in_array(ignored_namespaces, (const char*)uri)) {
            PARSER->text = g_string_new("");
            PARSER->base64 = is_base64(nb_attributes, attributes);
            
            if(!strcasecmp(c_name, "link") && PARSER->feed->link == NULL) {
                find_link(nb_attributes, attributes, &PARSER->feed->link, &PARSER->feed->link_title);
            } else if(is_author(c_name)) {
                PARSER->author_level = 0;
                PARSER->current_author = (struct _Author*)&PARSER->feed->author;
            }
        }
        
        /* both rss > channel and rss can be a feed (feed = list of entries). Handle that */
        if(is_feed(c_name) && !in_array(ignored_namespaces, (const char*)uri))
            PARSER->feed_level = 0;
        return;
    }
    
    /* Directly inside an entry: wait for a known element */
    if(PARSER->entry_level == 1) {
        if(in_array(known_entry_tags, c_name) && !in_array(ignored_namespaces, (const char*)uri)) {
            PARSER->text = g_string_new("");
            PARSER->base64 = is_base64(nb_attributes, attributes);
            if(is_author(c_name)) {
                PARSER->author_level = 0;
                PARSER->current_author = (struct _Author*)&PARSER->entry->author;
            } else if(!strcasecmp(c_name, "link") && PARSER->entry->link == NULL) {
                find_link(nb_attributes, attributes, &PARSER->entry->link, &PARSER->entry->link_title);
            }
        }
        return;
    }
    
    /* Inside an entry's author, with a known element */
    if(PARSER->author_level == 1 && in_array(known_author_tags, c_name) && !in_array(ignored_namespaces, (const char*)uri)) {
        PARSER->author_text = g_string_new("");
        PARSER->dump_xml = 0;
        return;
    }
    
    /* Other cases (unknown element of entry.author, or entry_level > 1):
     * If we're recording text, that means that we're getting an unknown tag
     * inside a known tag. That means that the known tag contains HTML or XML
     */
    if(PARSER->text) {
        if(!PARSER->dump_xml) {
            buf = g_string_free(PARSER->text, 0);
            escaped = g_markup_escape_text(buf, -1);
            PARSER->text = g_string_new(escaped);
            PARSER->dump_xml = 1;
            free(buf);
            free(escaped);
        }
        
        g_string_append_c(PARSER->text, '<');
        g_string_append(PARSER->text, c_name);
        EACH_ATTRIBUTE {
            escaped = g_markup_escape_text(c_attrvalstart, c_attrvalend - c_attrvalstart);
            
            g_string_append_c(PARSER->text, ' ');
            g_string_append(PARSER->text, c_attrname);
            g_string_append(PARSER->text, "=\"");
            g_string_append(PARSER->text, escaped);
            g_string_append_c(PARSER->text, '"');
            
            free(escaped);
        }
        g_string_append_c(PARSER->text, '>');
        return;
    }
}

static void process_end_element(void *parser, const xmlChar *name, const xmlChar *prefix, const xmlChar *uri)
{
    const char *c_name = (const char*)name;
    char *text;
    
    /* End of feed: nothing to do */
    if(PARSER->feed_level <= 0) {
        if(PARSER->feed_level == 0)
            PARSER->feed_level--;
        return;
    }
    
    /* End of a feed attribute */
    if(PARSER->feed_level == 1 && PARSER->text) {
        text = unpack_text(PARSER);
        
        if(is_author(c_name)) 
            fix_author(PARSER->current_author, &text);
        
        if(empty(text)) {
            free(text);
        } else if(!strcasecmp(c_name, "title") && PARSER->feed->title == NULL) {
            PARSER->feed->title = text;
        } else if(!strcasecmp(c_name, "subtitle") && PARSER->feed->subtitle == NULL) {
            PARSER->feed->subtitle = text;
        } else if(is_summary(c_name) && PARSER->feed->description == NULL) {
            PARSER->feed->description = text;
        } else if(!strcasecmp(c_name, "link") && PARSER->feed->link == NULL) {
            PARSER->feed->link = text;
        } else if(!strcasecmp(c_name, "id") && PARSER->feed->id == NULL) {
            PARSER->feed->id = text;
        } else if(is_pubdate(c_name) && PARSER->feed->publication_date == 0) {
            PARSER->feed->publication_date = text;
        } else if(is_moddate(c_name) && PARSER->feed->modification_date == 0) {
            PARSER->feed->modification_date = text;
        } else if(is_author(c_name) && PARSER->feed->author.text == NULL) {
            PARSER->feed->author.text = text;
        } else {
            free(text);
        }
        
        PARSER->feed_level--;
        return;
    }
    
    /* End of entry: add it to previous entries */
    if(PARSER->entry_level == 0) {
        PARSER->entries = g_slist_prepend(PARSER->entries, PARSER->entry);
        PARSER->entry = NULL;
        PARSER->feed_level--;
        PARSER->entry_level--;
        return;
    }
    
    /* End of an author property: fill it in current author */
    if(PARSER->author_level == 1 && PARSER->author_text) {
        text = g_string_free(PARSER->author_text, 0);
        PARSER->author_text = NULL;
        
        if(empty(text)) {
            free(text);
        } else if(!strcasecmp(c_name, "name") && PARSER->current_author->name == NULL) {
            PARSER->current_author->name = text;
        } else if(!strcasecmp(c_name, "email") && PARSER->current_author->email == NULL) {
            PARSER->current_author->email = text;
        } else if(is_uri(c_name) && PARSER->current_author->uri == NULL) {
            PARSER->current_author->uri = text;
        } else {
            free(text);
        }
        
        PARSER->author_level--;
        PARSER->feed_level--;
        if(PARSER->entry_level >= 0)
            PARSER->entry_level--;
        
        return;
    }
    
    /* End of an entry property: fill it in current entry */
    if(PARSER->entry_level == 1 && PARSER->text) {
        text = unpack_text(PARSER);

        if(is_author(c_name)) 
            fix_author(PARSER->current_author, &text);
        
        if(empty(text)) {
            free(text);
        } else if(!strcasecmp(c_name, "title") && PARSER->entry->title == NULL) {
            PARSER->entry->title = text;
        } else if(!strcasecmp(c_name, "subtitle") && PARSER->entry->subtitle == NULL) {
            PARSER->entry->subtitle = text;
        } else if(is_author(c_name) && PARSER->entry->author.text == NULL) {
            PARSER->entry->author.text = text;
        } else if(is_pubdate(c_name) && PARSER->entry->publication_date == 0) {
            PARSER->entry->publication_date = text;
        } else if(is_moddate(c_name) && PARSER->entry->modification_date == 0) {
            PARSER->entry->modification_date = text;
        } else if(!strcasecmp(c_name, "link") && PARSER->entry->link == NULL) {
            PARSER->entry->link = text;
        } else if(is_id(c_name) && PARSER->entry->id == NULL) {
            PARSER->entry->id = text;
        } else if(is_summary(c_name) && PARSER->entry->summary == NULL) {
            PARSER->entry->summary = text;
        } else if(is_summary(c_name) && PARSER->entry->content == NULL) {
            PARSER->entry->content = text;
        } else if(is_content(c_name) && PARSER->entry->content == NULL) {
            PARSER->entry->content = text;
        } else {
            free(text);
        }
        
        PARSER->entry_level--;
        PARSER->feed_level--;
        PARSER->author_level = -1;
        
        return;
    }
    
    /* Unknown end tag inside a known tag */
    if(PARSER->text) {
        if(!PARSER->dump_xml)
            abort(); /* Should never happen */
        g_string_append_printf(PARSER->text, "</%s>", c_name);
    }
    
    if(PARSER->entry_level >= 0)
        PARSER->entry_level--;
    if(PARSER->feed_level >= 0)
        PARSER->feed_level--;
    if(PARSER->author_level >= 0)
        PARSER->author_level--;
}

static void process_whitespace(void *parser, const xmlChar *spaces, int len)
{
}

static xmlSAXHandler sax_handler = {
    .internalSubset = NULL,
    .isStandalone = NULL,
    .hasInternalSubset = NULL,
    .hasExternalSubset = NULL,
    .resolveEntity = NULL,
    .getEntity = NULL,
    .entityDecl = NULL,
    .notationDecl = NULL,
    .attributeDecl = NULL,
    .elementDecl = NULL,
    .unparsedEntityDecl = NULL,
    .setDocumentLocator = NULL,
    .startDocument = process_start_document,
    .endDocument = process_end_document,
    .startElement = NULL,
    .endElement = NULL,
    .reference = NULL,
    .characters = process_characters,
    .ignorableWhitespace = process_whitespace,
    .processingInstruction = NULL,
    .comment = NULL,
    .warning = NULL,
    .error = process_error,
    .fatalError = NULL,
    .getParameterEntity = NULL,
    .cdataBlock = NULL,
    .startElementNs = NULL,
    .initialized = XML_SAX2_MAGIC,
    .startElementNs = process_start_element,
    .endElementNs = process_end_element,
    .serror = NULL
};

FeedParser *feed_parser_new() 
{
    FeedParser *parser = malloc(sizeof(FeedParser));
    memset(parser, 0, sizeof(FeedParser));
    return parser;
}

char *feed_parser_get_error(FeedParser *parser)
{
    return parser->error;
}

Feed *feed_parser_parse_string(FeedParser *parser, const char *data, int size)
{
    if(xmlSAXUserParseMemory(&sax_handler, parser, data, size) == 0) {
        return parser->feed;
    } else {
        return NULL;
    }
}

Feed *feed_parser_parse_file(FeedParser *parser, const char *path)
{
    if(xmlSAXUserParseFile(&sax_handler, parser, path) == 0) {
        return parser->feed;
    } else {
        return NULL;
    }
}

static void free_entry(Entry *entry)
{
    if(entry == NULL)
        return;
    free(entry->id);
    free(entry->title);
    free(entry->link);
    free(entry->content);
    free(entry->publication_date);
    free(entry->modification_date);
    free(entry->author.name);
    free(entry->author.email);
    free(entry->author.uri);
    free(entry->author.text);
    free(entry);
}

void feed_free(Feed *feed)
{
    int i;
    if(feed == NULL)
        return;
    for(i = 0; i < feed->entries_size; i++)
        free_entry(feed->entries[i]);
    free(feed->entries);
    free(feed);
}

void feed_parser_free(FeedParser *parser)
{
    if(parser == NULL)
        return;
    free(parser->error);
    free(parser);
}
