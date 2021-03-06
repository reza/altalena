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
#include "IvrWORX.h"
#include "AbstractOffer.h"
#include "Credentials.h"

using namespace System::Collections::Generic;

namespace ivrworx
{

namespace interop 
{
	public interface class IGenericOfferAnswerSession 
	{
	public:

		 void CleanDtmfBuffer();

		 ApiErrorCode WaitForDtmf(
			OUT String ^%signal, 
			IN  Int32 timeout);

		 ApiErrorCode RejectCall();

		 ApiErrorCode HangupCall();

		 ApiErrorCode BlindXfer(
			IN  String ^destination_uri);

		 void WaitTillHangup();

		 String^ Dnis();

		 String^ Ani();

		 AbstractOffer^ LocalOffer();

		 AbstractOffer^ RemoteOffer();

		 ApiErrorCode MakeCall(
			IN  String^					destinationUri, 
			IN  AbstractOffer^			localOffer,
			IN  Credentials^			credentials, 
			IN OUT  MapOfAnyInterop^	keyValueMap,
			IN Int32					ringTimeout);

		 ApiErrorCode ReOffer(
			IN  AbstractOffer^			localOffer,
			IN OUT  MapOfAnyInterop^	keyValueMap,
			IN Int32					ringTimeout);

		 ApiErrorCode Answer(
			IN  AbstractOffer^			localOffer,
			IN  OUT  MapOfAnyInterop^	keyValueMap,
			IN Int32					ringTimeout);

		 String^ DtmfBuffer();

	};

}
}
