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

#include "StdAfx.h"
#include "OpalFactory.h"
#include "ProcOpalH323.h"
#include "H323Call.h"

namespace ivrworx
{
	IMediaCallPtr 
	OpalProvider::CreateCall(ScopedForking &forking, shared_ptr<MsgCallOfferedReq> msg)
	{
		return IMediaCallPtr(new H323Call(forking,msg));

	}

	IMediaCallPtr 
	OpalProvider::CreateCall(ScopedForking &forking)
	{
		return IMediaCallPtr(new H323Call(forking));

	}

	const string& 
	OpalProvider::protocolId()
	{
		static string protocol_id = "h323";
		return protocol_id;
	}

	
	OpalFactory::OpalFactory(void)
	{
	}

	OpalFactory::~OpalFactory(void)
	{
	}

	LightweightProcess *
	OpalFactory::Create(LpHandlePair pair, Configuration &conf)
	{
		return new ProcOpalH323(conf,pair);
	}

}

