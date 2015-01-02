lib_LTLIBRARIES = LeagueOverseer.la

LeagueOverseer_la_SOURCES = \
	LeagueOverseer.h \
	LeagueOverseer-Core.cpp \
	LeagueOverseer-Events.cpp \
	LeagueOverseer-Functions.cpp \
	LeagueOverseer-GeneralCallbacks.cpp \
	LeagueOverseer-Helpers.h \
	LeagueOverseer-Helpers.cpp \
	LeagueOverseer-SlashCommands.cpp \
	LeagueOverseer-Version.h \
	LeagueOverseer-WebAPI.cpp \
	ConfigurationOptions.h \
	ConfigurationOptions.cpp \
	MatchEvent.h \
	MatchEvent-Capture.h \
	MatchEvent-Capture.cpp \
	MatchEvent-Join.h \
	MatchEvent-Join.cpp \
	MatchEvent-Kill.h \
	MatchEvent-Kill.cpp \
	MatchEvent-Part.h \
	MatchEvent-Part.cpp \
	MatchEvent-Substitute.h \
	MatchEvent-Substitute.cpp \
	UrlQuery.h \
	UrlQuery.cpp
LeagueOverseer_la_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/plugins/plugin_utils
LeagueOverseer_la_LDFLAGS = -module -avoid-version -shared
LeagueOverseer_la_LIBADD = $(top_builddir)/plugins/plugin_utils/libplugin_utils.la

AM_CPPFLAGS = $(CONF_CPPFLAGS)
AM_CFLAGS = $(CONF_CFLAGS)
AM_CXXFLAGS = $(CONF_CXXFLAGS)

EXTRA_DIST = \
	README.txt \
	LeagueOverseer.def \
	LeagueOverseer.sln \
	LeagueOverseer.vcxproj

MAINTAINERCLEANFILES =	\
	Makefile.in