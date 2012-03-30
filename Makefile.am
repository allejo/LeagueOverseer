lib_LTLIBRARIES = leagueOverSeer.la

leagueOverSeer_la_SOURCES = \
			    Events.cpp \
			    Rejoin.h \
			    Rejoin.cpp \
			    SlashCommands.cpp \
			    URLJobs.cpp \
			    Cleanup.cpp \
			    auxfuncts.cpp \
			    leagueOverSeer.h \
			    leagueOverSeer.cpp
leagueOverSeer_la_CXXFLAGS= -I$(top_srcdir)/include -I$(top_srcdir)/plugins/plugin_utils
leagueOverSeer_la_LDFLAGS = -module -avoid-version -shared
leagueOverSeer_la_LIBADD = $(top_builddir)/plugins/plugin_utils/libplugin_utils.la

AM_CPPFLAGS = $(CONF_CPPFLAGS)
AM_CFLAGS = $(CONF_CFLAGS)
AM_CXXFLAGS = $(CONF_CXXFLAGS)

EXTRA_DIST = \
	README.txt \
	leagueOverSeer.def \
	leagueOverSeer.sln \
	leagueOverSeer.vcproj

MAINTAINERCLEANFILES =	\
	Makefile.in
