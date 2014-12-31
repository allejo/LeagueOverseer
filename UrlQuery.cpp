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

#include "UrlQuery.h"

UrlQuery::UrlQuery() {}

UrlQuery::UrlQuery(bz_BaseURLHandler* handler, const char* url)
{
    _handler = handler;
    _URL = url;
    _query = queryDefault;
}

UrlQuery& UrlQuery::set(std::string field, int value)
{
    return query(field, std::to_string(value).c_str());
}

UrlQuery& UrlQuery::set(std::string field, bz_ApiString value)
{
    return query(field, value.c_str());
}

UrlQuery& UrlQuery::set(std::string field, std::string value)
{
    return query(field, value.c_str());
}

UrlQuery& UrlQuery::set(std::string field, const char* value)
{
    return query(field, value);
}

void UrlQuery::submit()
{
    bz_addURLJob(_URL, _handler, _query.c_str()); // Send off the URL job
    _query = queryDefault;                        // Reset the query so this object can be reused
}

UrlQuery UrlQuery::operator=(const UrlQuery& rhs)
{
    _handler = rhs._handler;
    _URL = rhs._URL;
    _query = rhs._query;

    return *this;
}

UrlQuery& UrlQuery::query(std::string field, const char* value)
{
    _query += "&" + field + "=" + std::string(bz_urlEncode(value));
    return *this;
}