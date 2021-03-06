/*
*	The Altalena Project File
*	Copyright (C) 2009  Boris Ouretskey
*
*	This library is free software; you can redistribute it and/or
*	modify it under the terms of the GNU Lesser General Public
*	License as published by the Free Software Foundation; either
*	version 2.1 of the License, or (at your option) any later version.
*
*	This library is distributed in the hope that it will be useful,
*	but WITHOUT ANY WARRANTY; without even the implied warranty of
*	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
*	Lesser General Public License for more details.
*
*	You should have received a copy of the GNU Lesser General Public
*	License along with this library; if not, write to the Free Software
*	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#pragma once

#include "Configuration.h"
#include "IwBase.h"

using namespace std;
using namespace json_spirit;

namespace ivrworx
{

	class JSONConfiguration :
		public Configuration
	{
	public:

		JSONConfiguration();

		virtual ~JSONConfiguration(void);
		
		ApiErrorCode InitFromFile(IN const string &filename);

		virtual int GetInt(const string &name);

		virtual string GetString(const string &name);

		virtual BOOL GetBool(const string &name);

		virtual BOOL HasOption(const string &name);

		virtual void GetArray(IN const string &name, OUT list<any> &out_list);

	protected:

		const Value& QueryValue(IN const string &name, const Object &parent_object);

	private:

		Value _rootValue;
	};

}

