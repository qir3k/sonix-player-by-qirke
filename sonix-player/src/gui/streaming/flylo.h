#ifndef FLYLO_H
#define FLYLO_H

// The stations behind "Flylo favs", in one place.
//
// They are not in the radio directory, so nothing has ever given them a uuid:
// the id in this table is theirs, stable for as long as the entry is, and it is
// what a favourite is kept by. The interface lists them on the radio page; the
// phone's streaming tab lists the same ones through SonixLink. Two copies of
// this table is one copy too many -- the two lists drifted apart once already,
// and a station edited in one of them went missing from the other.

typedef struct {
	const char *id;
	const char *name;
	const char *url;
	const char *icon;
} flylo_station_t;

static const flylo_station_t FLYLO_STATIONS[] = {
	{"ilove-chillhop", "I Love Chillhop", "https://streams.ilovemusic.de/iloveradio17.mp3",
	 "https://ilovemusic.de/favicon.png"},
	{"ilove-chillhop-2", "ChillHop", "http://stream01.iloveradio.de/iloveradio17.mp3",
	 "https://ilovemusic.de/favicon.png"},
	{"fluxfm-chillhop", "Flux Fm Chillhop", "https://streams.fluxfm.de/Chillhop/mp3-320/streams.fluxfm.de/",
	 "https://www.fluxfm.de/assets/favicons/apple-icon-180x180.png"},
	{"le-mellotron", "Le Mellotron", "https://listen.radioking.com/radio/477719/stream/534044",
	 "https://cdn.prod.website-files.com/62288d6c9e6c33efb4e67d74/"
	 "622895c09e6c33d606e6e822_Icone-webclip.png"},
	{"zeno-lofi", "Lo-Fi Hip-Hop Radio", "https://stream.zeno.fm/0r0xa792kwzuv",
	 "https://zeno.fm/icon-192x192.png"},
};
#define FLYLO_STATION_COUNT (int)(sizeof(FLYLO_STATIONS) / sizeof(FLYLO_STATIONS[0]))

#endif /* FLYLO_H */
