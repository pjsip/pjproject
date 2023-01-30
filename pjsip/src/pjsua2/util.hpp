/*
 * Copyright (C) 2013 Teluu Inc. (http://www.teluu.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <pjsua2/types.hpp>
#include <pjsua2/media.hpp>
#include <string>

#define PJ2BOOL(var) ((var) != PJ_FALSE)

namespace pj
{
using std::string;

inline pj_str_t str2Pj(const string &input_str)
{
    pj_str_t output_str;
    output_str.ptr = (char*)input_str.c_str();
    output_str.slen = input_str.size();
    return output_str;
}

inline string pj2Str(const pj_str_t &input_str)
{
    if (input_str.ptr && input_str.slen>0)
        return string(input_str.ptr, input_str.slen);
    return string();
}

class AudioMediaHelper : public AudioMedia
{
public:
    void setPortId(int port_id) { id = port_id; }
};

class VideoMediaHelper : public VideoMedia
{
public:
    void setPortId(int port_id) { id = port_id; }
};

} // namespace pj
