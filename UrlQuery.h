/*
League Overseer
    Copyright (C) 2013-2015 Vladimir Jimenez & Ned Anderson

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __URL_QUERY_H__
#define __URL_QUERY_H__

#include <string>

#include "bzfsAPI.h"

#include "LeagueOverseer-Version.h"

class UrlQuery
{
    public:
        UrlQuery();
        UrlQuery(bz_BaseURLHandler* handler, const char* url);

        UrlQuery& set(std::string field, int value);
        UrlQuery& set(std::string field, bz_ApiString value);
        UrlQuery& set(std::string field, std::string value);
        UrlQuery& set(std::string field, const char* value);

        void submit();

        UrlQuery operator=(const UrlQuery& rhs);

    private:
        std::string queryDefault = "apiVersion=" + std::to_string(API_VERSION);

        bz_BaseURLHandler* _handler;
        const char*        _URL;
        std::string        _query;

        UrlQuery& query(std::string field, const char* value);
};

#endif