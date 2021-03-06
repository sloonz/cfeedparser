package feedparser

// #cgo pkg-config: libxml-2.0 glib-2.0
// #include "feedparser.h"
// static Entry *getEntry(Feed *feed, int entry) { return feed->entries[entry]; }
import "C"
import (
	"io/ioutil"
	"net/http"
	"net/http/httputil"
	"net/url"
	"strings"
	"time"
)

type Error string

func (e Error) Error() string {
	return strings.TrimSpace(string(e))
}

type Author struct {
	Name, Email, Uri, Text string
}

type Entry struct {
	Id, Title, Link, Summary, Content             string
	PublicationDate, ModificationDate             string
	PublicationDateParsed, ModificationDateParsed time.Time
	Subtitle, LinkTitle                           string
	Enclosure                                     string
	Author                                        Author
}

type Feed struct {
	Entries                                       []Entry
	Id, Title, Link, Description                  string
	PublicationDate, ModificationDate             string
	PublicationDateParsed, ModificationDateParsed time.Time
	Subtitle, LinkTitle                           string
	Author                                        Author
}

func parseDate(date string) time.Time {
	for _, layout := range []string{time.RFC822, time.RFC822Z, time.RFC3339, time.RFC1123, time.RFC850, time.RubyDate, time.UnixDate, time.ANSIC} {
		parsed, err := time.Parse(layout, date)
		if err == nil {
			return parsed
		}
	}
	return time.Time{}
}

func parseEntry(entry *Entry, centry *C.Entry) {
	entry.Id = C.GoString(centry.id)
	entry.Title = C.GoString(centry.title)
	entry.Link = C.GoString(centry.link)
	entry.Summary = C.GoString(centry.summary)
	entry.Content = C.GoString(centry.content)
	entry.PublicationDate = C.GoString(centry.publication_date)
	entry.ModificationDate = C.GoString(centry.modification_date)
	entry.PublicationDateParsed = parseDate(entry.PublicationDate)
	entry.ModificationDateParsed = parseDate(entry.ModificationDate)
	entry.Subtitle = C.GoString(centry.subtitle)
	entry.LinkTitle = C.GoString(centry.link_title)
	entry.Enclosure = C.GoString(centry.enclosure)
	entry.Author.Name = C.GoString(centry.author.name)
	entry.Author.Email = C.GoString(centry.author.email)
	entry.Author.Uri = C.GoString(centry.author.uri)
	entry.Author.Text = C.GoString(centry.author.text)
}

func parseFeed(cfeed *C.Feed) (feed *Feed) {
	feed = new(Feed)
	feed.Id = C.GoString(cfeed.id)
	feed.Title = C.GoString(cfeed.title)
	feed.Link = C.GoString(cfeed.link)
	feed.Description = C.GoString(cfeed.description)
	feed.PublicationDate = C.GoString(cfeed.publication_date)
	feed.ModificationDate = C.GoString(cfeed.modification_date)
	feed.PublicationDateParsed = parseDate(feed.PublicationDate)
	feed.ModificationDateParsed = parseDate(feed.ModificationDate)
	feed.Subtitle = C.GoString(cfeed.subtitle)
	feed.LinkTitle = C.GoString(cfeed.link_title)
	feed.Author.Name = C.GoString(cfeed.author.name)
	feed.Author.Email = C.GoString(cfeed.author.email)
	feed.Author.Uri = C.GoString(cfeed.author.uri)
	feed.Author.Text = C.GoString(cfeed.author.text)
	feed.Entries = make([]Entry, int(cfeed.entries_size))
	for i := 0; i < int(cfeed.entries_size); i++ {
		parseEntry(&feed.Entries[i], C.getEntry(cfeed, C.int(i)))
	}
	return feed
}

func ParseString(data string) (*Feed, error) {
	parser := C.feed_parser_new()
	defer C.feed_parser_free(parser)

	feed := C.feed_parser_parse_string(parser, C.CString(data), C.int(len(data)))
	if feed == nil {
		return nil, Error(C.GoString(C.feed_parser_get_error(parser)))
	}
	defer C.feed_free(feed)
	return parseFeed(feed), nil
}

func ParseFile(file string) (*Feed, error) {
	parser := C.feed_parser_new()
	defer C.feed_parser_free(parser)

	feed := C.feed_parser_parse_file(parser, C.CString(file))
	if feed == nil {
		return nil, Error(C.GoString(C.feed_parser_get_error(parser)))
	}
	defer C.feed_free(feed)
	return parseFeed(feed), nil
}

func ParseURL(u *url.URL) (*Feed, error) {
	resp, err := http.Get(u.String())
	if err != nil && err != httputil.ErrPersistEOF {
		return nil, err
	}

	if resp.StatusCode/100 == 3 {
		loc, err := url.Parse(resp.Header["Location"][0])
		if err != nil {
			return nil, err
		}
		if loc.Scheme == "" {
			if (len(loc.Path) == 0 || loc.Path[0] != byte('/')) && u.Path != "" {
				idx := strings.LastIndex(u.Path, "/")
				if idx != -1 {
					loc.Path = u.Path[:idx] + "/" + loc.Path
				}
			}
			loc.Scheme = u.Scheme
			loc.Host = u.Host
		}
		return ParseURL(loc)
	}

	data, err := ioutil.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	return ParseString(string(data))
}
