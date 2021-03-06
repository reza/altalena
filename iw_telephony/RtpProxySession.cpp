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
#include "RtpProxySession.h"

#pragma push_macro("SendMessage")
#undef SendMessage


namespace ivrworx
{
	RtpProxySession::RtpProxySession(ScopedForking &forking,HandleId handle_id):
	_rtpProxyHandleId(handle_id),
	_handle(IW_UNDEFINED),
	_bridgedHandle(IW_UNDEFINED),
	_handlerPair(HANDLE_PAIR),
	_dtmfChannel(new LpHandle())
	{
		StartActiveObjectLwProc(forking,_handlerPair,"RtpProxySession Session Handler");
	}

	RtpProxySession::~RtpProxySession(void)
	{
		TearDown();
	}

	ApiErrorCode 
	RtpProxySession::Allocate(const AbstractOffer &remoteOffer)
	{
		FUNCTRACKER;

		if (_handle != IW_UNDEFINED)
		{
			return API_FAILURE;
		}

		DECLARE_NAMED_HANDLE_PAIR(session_handler_pair);

		MsgRtpProxyAllocateReq *msg = new MsgRtpProxyAllocateReq();
		msg->offer		= remoteOffer;
		msg->handler	= _handlerPair.inbound;


		IwMessagePtr response = NULL_MSG;
		ApiErrorCode res = GetCurrRunningContext()->DoRequestResponseTransaction(
			_rtpProxyHandleId,
			IwMessagePtr(msg),
			response,
			MilliSeconds(GetCurrRunningContext()->TransactionTimeout()),
			"Allocate RTP Connection TXN");

		if (res != API_SUCCESS)
		{
			LogWarn("Error allocating rtp connection " << res);
			return res;
		}

		switch (response->message_id)
		{
		case MSG_RTP_PROXY_ACK:
			{
				shared_ptr<MsgRtpProxyAck> ack 
					= dynamic_pointer_cast<MsgRtpProxyAck> (response);

				_handle = ack->rtp_proxy_handle;
				_localOffer = ack->offer;

				LogDebug("RtpProxySession::Allocate allocated rtpid:" << _handle);
				
				return API_SUCCESS;
			}
		
		default:
			{
				LogDebug("RtpProxySession::Allocate failed");
				return API_FAILURE;
			}
		}

	}

	ApiErrorCode 
	RtpProxySession::TearDown()
	{
		FUNCTRACKER;

		if (_handle == IW_UNDEFINED)
		{
			return API_SUCCESS;
		}

		MsgRtpProxyDeallocateReq *req = 
			new MsgRtpProxyDeallocateReq();

		req->rtp_proxy_handle = _handle;

		GetCurrRunningContext()->SendMessage(_rtpProxyHandleId,IwMessagePtr(req));

		_handle = IW_UNDEFINED;
		_bridgedHandle = IW_UNDEFINED;
		
		return API_SUCCESS;
	}

	ApiErrorCode 
	RtpProxySession::Modify(const AbstractOffer &remoteOffer)
	{
		LogDebug("RtpProxySession::Modify  rtph:" << _handle);

		if (_handle == IW_UNDEFINED)
		{
			return API_WRONG_STATE;
		}

		MsgRtpProxyModifyReq *req = 
			new MsgRtpProxyModifyReq();

		req->rtp_proxy_handle = _handle;
		req->offer	  = remoteOffer;
		

		IwMessagePtr response = NULL_MSG;
		ApiErrorCode res = GetCurrRunningContext()->DoRequestResponseTransaction(
			_rtpProxyHandleId,
			IwMessagePtr(req),
			response,
			MilliSeconds(GetCurrRunningContext()->TransactionTimeout()),
			"Modify RTP Connection TXN");

		if (res != API_SUCCESS)
		{
			LogWarn("RtpProxySession::Modify - Error modifying rtp connection " << res);
			return res;
		}

		switch (response->message_id)
		{
		case MSG_RTP_PROXY_ACK:
			{
				shared_ptr<MsgRtpProxyAck> ack 
					= dynamic_pointer_cast<MsgRtpProxyAck> (response);

				return API_SUCCESS;
			}

		default:
			{
				return API_FAILURE;
			}
		};

	}

	RtpProxyHandle 
	RtpProxySession::RtpHandle()
	{
		return _handle;
	}

	void 
	RtpProxySession::CleanDtmfBuffer()
	{
		FUNCTRACKER;

		_dtmfBuffer.str("");

		// the trick we are using
		// to clean the buffer is just replace the handle
		// with  new one
		_dtmfChannel->Poison();
		_dtmfChannel = LpHandlePtr(new LpHandle());

	}


	ApiErrorCode 
	RtpProxySession::WaitForDtmf(OUT string &signal, IN const Time timeout)
	{
		FUNCTRACKER;

		// just proxy the event
		int handle_index= IW_UNDEFINED;
		IwMessagePtr response = NULL_MSG;

		ApiErrorCode res = GetCurrRunningContext()->WaitForTxnResponse(
			assign::list_of(_dtmfChannel),
			handle_index,
			response, 
			timeout);

		if (IW_FAILURE(res))
		{
			return res;
		}

		shared_ptr<MsgRtpProxyDtmfEvt> dtmf_evt = 
			dynamic_pointer_cast<MsgRtpProxyDtmfEvt> (response);

		signal = dtmf_evt->signal;

		LogDebug("RtpProxySession::WaitForDtmf received signal:" << signal);

		return API_SUCCESS;

	}

	string  
	RtpProxySession::DtmfBuffer()
	{
		return _dtmfBuffer.str().c_str();
	}

	void 
	RtpProxySession::UponDtmfEvt(IN IwMessagePtr ptr)
	{
		FUNCTRACKER;

		shared_ptr<MsgRtpProxyDtmfEvt> dtmf_evt = 
			dynamic_pointer_cast<MsgRtpProxyDtmfEvt> (ptr);

		_dtmfBuffer << dtmf_evt->signal;
		// just proxy the event
		_dtmfChannel->Send(ptr);
	}

	ApiErrorCode 
	RtpProxySession::Bridge(IN const RtpProxySession &dest, BOOL fullDuplex)
	{
		FUNCTRACKER;

		LogDebug("RtpProxySession::Bridge src:" << _handle << ", dst:" << dest._handle << ", fullduplex:" << fullDuplex);

		if (_handle == IW_UNDEFINED)
		{
			return API_WRONG_STATE;
		}

		_bridgedHandle = IW_UNDEFINED;

		DECLARE_NAMED_HANDLE_PAIR(session_handler_pair);

		MsgRtpProxyBridgeReq *msg = new MsgRtpProxyBridgeReq();
		msg->output_conn = dest._handle;
		msg->rtp_proxy_handle = _handle;
		msg->full_duplex = fullDuplex;


		IwMessagePtr response = NULL_MSG;
		ApiErrorCode res = GetCurrRunningContext()->DoRequestResponseTransaction(
			_rtpProxyHandleId,
			IwMessagePtr(msg),
			response,
			MilliSeconds(GetCurrRunningContext()->TransactionTimeout()),
			"Bridge RTP Connection TXN");

		if (res != API_SUCCESS)
		{
			LogWarn("Error bridging rtp connection " << res);
			return res;
		}

		switch (response->message_id)
		{
		case MSG_RTP_PROXY_ACK:
			{
				shared_ptr<MsgRtpProxyAck> ack 
					= dynamic_pointer_cast<MsgRtpProxyAck> (response);

				_bridgedHandle = dest._handle;

				return API_SUCCESS;
			}
		default:
			{
				return API_FAILURE;
			}
		}
	}

	void 
	RtpProxySession::UponActiveObjectEvent(IwMessagePtr ptr)
	{
		FUNCTRACKER;

		switch (ptr->message_id)
		{
		case MSG_RTP_PROXY_DTMF_EVT:
			{
				UponDtmfEvt(ptr);
				break;
			}
		default:
			{

			}
		}

		ActiveObject::UponActiveObjectEvent(ptr);

	}

	AbstractOffer 
	RtpProxySession::LocalOffer()
	{
		return _localOffer;
	}

	AbstractOffer 
	RtpProxySession::RemoteOffer()
	{
		return _remoteOffer;
	}

}

#pragma pop_macro("SendMessage")

