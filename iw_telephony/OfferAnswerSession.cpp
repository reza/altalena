#include "StdAfx.h"
#include "OfferAnswerSession.h"


#pragma push_macro("SendMessage")
#undef SendMessage







using namespace ivrworx;

namespace ivrworx
{

IW_TELEPHONY_API IwStackHandle 
GenerateCallHandle()
{
	static volatile int index = 10000;
	return ::InterlockedIncrement((LONG*)&index);
};

GenericOfferAnswerSession::GenericOfferAnswerSession(IN ScopedForking &forking):
	_serviceHandleId(IW_UNDEFINED),
	_iwCallHandle(IW_UNDEFINED),
	_hangupDetected(FALSE),
	_handlerPair(HANDLE_PAIR),
	_dtmfChannel(new LpHandle()),
	_hangupChannel(new LpHandle()),
	_callState(CALL_STATE_UNKNOWN),
	_uac(TRUE),
	_forking(forking)
{
	FUNCTRACKER;

	CALL_RESET_STATE(CALL_STATE_UNKNOWN);


}


GenericOfferAnswerSession::GenericOfferAnswerSession(IN ScopedForking &forking, 
	IN HandleId stack_handle_id):
	_serviceHandleId(stack_handle_id),
	_iwCallHandle(IW_UNDEFINED),
	_hangupDetected(FALSE),
	_handlerPair(HANDLE_PAIR),
	_dtmfChannel(new LpHandle()),
	_hangupChannel(new LpHandle()),
	_callState(CALL_STATE_UNKNOWN),
	_uac(TRUE),
	_forking(forking)
{
	FUNCTRACKER;

	CALL_RESET_STATE(CALL_STATE_UNKNOWN);

	StartActiveObjectLwProc(forking,_handlerPair,"GenericOfferAnswerSession Session Handler");

}

GenericOfferAnswerSession::GenericOfferAnswerSession(IN ScopedForking &forking,
	IN HandleId stack_handle_id,
	IN shared_ptr<MsgCallOfferedReq> offered_msg):
	_serviceHandleId(stack_handle_id),
	_iwCallHandle(offered_msg->stack_call_handle),
	_handlerPair(offered_msg->call_handler_inbound),
	_dtmfChannel(new LpHandle()),
	_hangupChannel(new LpHandle()),
	_callState(CALL_STATE_UNKNOWN),
	_uac(FALSE),
	_forking(forking)
{
	FUNCTRACKER;

	InitFromIncoming(forking, offered_msg);

}

void 
GenericOfferAnswerSession::InitFromIncoming(IN ScopedForking &forking, IN shared_ptr<MsgCallOfferedReq> offered_msg)
{
	FUNCTRACKER;

	CALL_RESET_STATE(CALL_STATE_INITIAL_OFFERED);
	StartActiveObjectLwProc(forking,_handlerPair,__FUNCTION__);

	_dnis	= offered_msg->dnis;
	_ani	= offered_msg->ani;
	_remoteOffer = offered_msg->remoteOffer;

}


IW_TELEPHONY_API ApiErrorCode
SubscribeToIncomingCalls(IN LpHandlePtr stackIncomingHandle, IN LpHandlePtr listenerHandle)
{
	FUNCTRACKER;

	MsgCallSubscribeReq *msg = new MsgCallSubscribeReq();
	msg->listener_handle = listenerHandle;

	IwMessagePtr response = NULL_MSG;


	// wait for ok or nack
	ApiErrorCode res = GetCurrRunningContext()->DoRequestResponseTransaction(
		stackIncomingHandle,
		IwMessagePtr(msg),
		response,
		Seconds(5),
		"SubscribeToIncomingCalls TXN");


	if (res == API_TIMEOUT)
	{
		// just to be sure that timeout-ed call
		// eventually gets hanged up
		LogWarn("SubscribeToIncomingCalls - Timeout.");
		return API_TIMEOUT;
	}

	if (IW_FAILURE(res))
	{
		LogWarn("SubscribeToIncomingCalls - failure res:" << res);
		return API_FAILURE;
	}

	switch (response->message_id)
	{
	case MSG_CALL_SUBSCRIBE_ACK:
		{
			res = API_SUCCESS;
			break;
		}
	default:
		{
			res = API_SERVER_FAILURE;
		}
	}


	return res; 

}



IW_TELEPHONY_API ApiErrorCode  
GenericOfferAnswerSession::Accept(IN const string& service, IN csp::Time timeout)
{
	FUNCTRACKER;

	// register cross wide map of accepting handles if needed
	// the pyrotechnics's is used not to miss any calls between
	// object created.
	AcceptingHandlesMap *handles_map = (AcceptingHandlesMap *)
		GetCurrRunningContext()->GetAppData()->get("handles_map@GenericOfferAnswerSession::Accept").get();

    if (handles_map == NULL )
	{
		LogDebug("GenericOfferAnswerSession::Accept map has been created.");

		handles_map = new AcceptingHandlesMap();
		GetCurrRunningContext()->GetAppData()->set("handles_map@GenericOfferAnswerSession::Accept",
			shared_ptr<AcceptingHandlesMap>(handles_map));
	}

	// check if there some handle that accepts the calls

	AcceptingHandlesMap::iterator iter  = 
		handles_map->find(service);

	DECLARE_NAMED_HANDLE(listener_handle);


	if (iter == handles_map->end())
	{
		LpHandlePtr service_handle = ivrworx::GetHandle(service);

		_serviceHandleId = service_handle->GetObjectUid();

		if (!service_handle)
			return API_UNKNOWN_DESTINATION;

		ApiErrorCode res = SubscribeToIncomingCalls(service_handle,listener_handle);
		if (IW_FAILURE(res))
		{
			LogDebug("Error subscribing err:" << res);
			return res;
		}

		(*handles_map)[service] = listener_handle;
	}

	//
	// Message Loop
	// 
	ApiErrorCode res = API_SUCCESS;
	IwMessagePtr msg = listener_handle->Wait(timeout,res);
	if (IW_FAILURE(res))
	{
		return res;
	}

	switch (msg->message_id)
	{
	case MSG_CALL_OFFERED:
		{

			shared_ptr<MsgCallOfferedReq> call_offered = 
				shared_polymorphic_cast<MsgCallOfferedReq> (msg);

			InitFromIncoming(_forking,call_offered);
			break;
		}
	default:
		{
			return API_UNKNOWN_RESPONSE;
		}
	}
	return  API_SUCCESS;
}


const string&
GenericOfferAnswerSession::Dnis()
{
	return _dnis;
}

const string&
GenericOfferAnswerSession::Ani()
{
	return _ani;
}


void
GenericOfferAnswerSession::ResetState(CallState state, const char *state_str)
{
	LogDebug("GenericOfferAnswerSession::ResetState iwh:" << _iwCallHandle << ", transition from state:" << _callState << " to state:" << state << ", " << state_str);
	_callState = state;
}
ApiErrorCode 
GenericOfferAnswerSession::ReOffer(
							 IN const AbstractOffer  &localOffer,
							 IN OUT  MapOfAny &keyValueMap,
							 IN csp::Time	  ringTimeout)
{
	FUNCTRACKER;

	LogDebug("GenericOfferAnswerSession::ReOffer offer:\n" << localOffer.body);

	if (localOffer.body.empty())
	{
		return API_WRONG_PARAMETER;
	}

	if (_callState != CALL_STATE_CONNECTED)
	{
		return API_WRONG_STATE;
	}

	CALL_RESET_STATE(CALL_STATE_IN_DIALOG_OFFERED);

	IwMessagePtr response = NULL_MSG;

	MsgCallReofferReq *msg		= new MsgCallReofferReq();
	msg->localOffer			    = localOffer;
	msg->stack_call_handle		= _iwCallHandle;
	msg->optional_params		= keyValueMap;

	ApiErrorCode res = API_SUCCESS;

	// wait for ok or nack
	res = GetCurrRunningContext()->DoRequestResponseTransaction(
		_serviceHandleId,
		IwMessagePtr(msg),
		response,
		ringTimeout,
		"GenericOfferAnswerSession::ReOffer TXN");


	if (res == API_TIMEOUT)
	{
		// just to be sure that timeout-ed call
		// eventually gets hanged up
		LogWarn("GenericOfferAnswerSession::ReOffer - Timeout.");
		HangupCall();
		return API_TIMEOUT;
	}

	if (IW_FAILURE(res))
	{
		LogWarn("GenericOfferAnswerSession::ReOffer - failure res:" << res);
		HangupCall();
		return API_SERVER_FAILURE;
	}


	switch (response->message_id)
	{
	case MSG_CALL_REOFFER_ACK:
		{
			_localOffer		= localOffer;
			break;
		}
	default:
		{
			CALL_RESET_STATE(CALL_STATE_CONNECTED);
			return API_SERVER_FAILURE;
		}
	}

	shared_ptr<MsgCallReofferAck> make_call_ok = 
		dynamic_pointer_cast<MsgCallReofferAck>(response);

	LogDebug("GenericOfferAnswerSession::ReOffer remote:" << make_call_ok->remoteOffer.body);

	_remoteOffer = make_call_ok->remoteOffer;
	
	CALL_RESET_STATE(CALL_STATE_CONNECTED);
	
	return res;

}

ApiErrorCode
GenericOfferAnswerSession::MakeCall(IN const string   &destination_uri, 
	IN const AbstractOffer &offer,
	IN const Credentials &credentials,
	IN OUT MapOfAny &key_value_map,
	IN csp::Time	  ring_timeout)
{
	FUNCTRACKER;

	LogDebug("GenericOfferAnswerSession::MakeCall dst:" << destination_uri 
		<< ", type:" << offer.GetType() << ", subtype:" << offer.GetSubType() 
		<< ", offer:\n" << offer.body );

	if (_callState != CALL_STATE_UNKNOWN)
	{
		LogWarn("GenericOfferAnswerSession::MakeCall - wrong state");
		return API_FAILURE;
	}

	CALL_RESET_STATE(CALL_STATE_OFFERING);


	IwMessagePtr response = NULL_MSG;

	MsgMakeCallReq *msg = new MsgMakeCallReq();
	msg->destination_uri		= destination_uri;
	msg->localOffer			    = offer;
	msg->call_handler_inbound	= _handlerPair.inbound;
	msg->stack_call_handle		= _iwCallHandle;
	msg->optional_params		= key_value_map;
	msg->credentials			= credentials;

	_localOffer					= offer;

	ApiErrorCode res = API_SUCCESS;
	
	// wait for ok or nack
	res = GetCurrRunningContext()->DoRequestResponseTransaction(
		_serviceHandleId,
		IwMessagePtr(msg),
		response,
		ring_timeout,
		"GenericOfferAnswerSession::MakeCall TXN");
	

	if (res == API_TIMEOUT)
	{
		// just to be sure that timeout-ed call
		// eventually gets hanged up
		LogWarn("GenericOfferAnswerSession::MakeCall - Timeout.");
		HangupCall();
		return API_TIMEOUT;
	}

	if (IW_FAILURE(res))
	{
		LogWarn("GenericOfferAnswerSession::MakeCall - failure res:" << res);
		CALL_RESET_STATE(CALL_STATE_UNKNOWN);
		return API_SERVER_FAILURE;
	}


	switch (response->message_id)
	{
	case MSG_MAKE_CALL_OK:
		{
			break;
		}
	case MSG_MAKE_CALL_NACK:
		{
			CALL_RESET_STATE(CALL_STATE_UNKNOWN);
			return API_SERVER_FAILURE;
			break;
		}
	default:
		{
			throw;
		}
	}

	shared_ptr<MsgMakeCallOk> make_call_ok = 
		dynamic_pointer_cast<MsgMakeCallOk>(response);

	LogDebug("GenericOfferAnswerSession::MakeCall remote:" << make_call_ok->remoteOffer.body);

	_remoteOffer = make_call_ok->remoteOffer;
	_iwCallHandle = make_call_ok->stack_call_handle;
	_dnis = destination_uri;
	_ani  = make_call_ok->ani;

	if (!_localOffer.body.empty())
		CALL_RESET_STATE(CALL_STATE_CONNECTED);
	else
		CALL_RESET_STATE(CALL_STATE_REMOTE_OFFERED);
	

	return res;

}

ApiErrorCode
GenericOfferAnswerSession::Answer(IN const AbstractOffer &offer,
						 IN const MapOfAny &key_value_map,
						 IN csp::Time	  ring_timeout)
{
	FUNCTRACKER;

	LogDebug("enericOfferAnswerSession::Answer iwh:" << _iwCallHandle <<
		" type=" << offer.type << ", body=" << offer.body);

	if (_callState != CALL_STATE_INITIAL_OFFERED && 
		_callState != CALL_STATE_REMOTE_OFFERED		)
	{
		LogWarn("Answer:: wrong call state:" << _callState << ", iwh:" << _iwCallHandle);
		return API_WRONG_STATE;
	}


	_localOffer = offer;

	IwMessagePtr response = NULL_MSG;

	if (_uac)
	{
		MsgNewCallConnected *ack	= new MsgNewCallConnected();
		ack->stack_call_handle	= _iwCallHandle;
		ack->localOffer		    = offer;


		ApiErrorCode res = GetCurrRunningContext()->SendMessage(
			_serviceHandleId,
			IwMessagePtr(ack));

		CALL_RESET_STATE(CALL_STATE_CONNECTED);

		
		return res;
	}

	MsgCalOfferedAck *ack	= new MsgCalOfferedAck();
	ack->stack_call_handle	= _iwCallHandle;
	ack->localOffer		    = offer;


	ApiErrorCode res = GetCurrRunningContext()->DoRequestResponseTransaction(
		_serviceHandleId,
		IwMessagePtr(ack),
		response,
		MilliSeconds(GetCurrRunningContext()->TransactionTimeout()),
		"Accept GenericOfferAnswerSession TXN");

	if (IW_FAILURE(res))
	{
		return res;
	}



	

	switch (response->message_id)
	{
	case MSG_CALL_CONNECTED:
		{
			shared_ptr<MsgNewCallConnected> make_call_sucess = 
				dynamic_pointer_cast<MsgNewCallConnected>(response);

			_iwCallHandle = make_call_sucess->stack_call_handle;

			break;
		}
	default:
		{
			HangupCall();
			return API_SERVER_FAILURE;
		}
	}

	CALL_RESET_STATE(CALL_STATE_CONNECTED);

	return res;
}


GenericOfferAnswerSession::~GenericOfferAnswerSession(void)
{
	FUNCTRACKER;

	StopActiveObjectLwProc();

	HangupCall();

}

string 
GenericOfferAnswerSession::DtmfBuffer()
{
	return _dtmfBuffer.str();

}

void 
GenericOfferAnswerSession::UponDtmfEvt(IwMessagePtr ptr)
{
	FUNCTRACKER;

	shared_ptr<MsgCallDtmfEvt> dtmf_evt = 
		dynamic_pointer_cast<MsgCallDtmfEvt> (ptr);

	_dtmfBuffer << dtmf_evt->signal;

	// just proxy the event
	_dtmfChannel->Send(ptr);

}

void 
GenericOfferAnswerSession::CleanDtmfBuffer()
{
	FUNCTRACKER;

	_dtmfBuffer.str("");

	// the trick we are using
	// to clean the buffer is just replace the handle
	// with  new one
	_dtmfChannel->Poison();
	_dtmfChannel = LpHandlePtr(new LpHandle());

	LogDebug("GenericOfferAnswerSession::CleanDtmfBuffer iwh:" << _iwCallHandle);

}


ApiErrorCode 
GenericOfferAnswerSession::WaitForDtmf(OUT string &signal, IN const Time timeout)
{
	FUNCTRACKER;

	// just proxy the event
	int handle_index= IW_UNDEFINED;
	IwMessagePtr response = NULL_MSG;

	ApiErrorCode res = GetCurrRunningContext()->WaitForTxnResponse(
		assign::list_of(_dtmfChannel)(_hangupChannel),
		handle_index,
		response, 
		timeout);

	if (IW_FAILURE(res))
	{
		return res;
	}

	if (handle_index == 1)
	{
		return API_HANGUP;
	}

	shared_ptr<MsgCallDtmfEvt> dtmf_evt = 
		dynamic_pointer_cast<MsgCallDtmfEvt> (response);

	signal = dtmf_evt->signal;

	LogDebug("GenericOfferAnswerSession::WaitForDtmf iwh:" << _iwCallHandle << " received signal:" << signal);

	return API_SUCCESS;

}



void 
GenericOfferAnswerSession::UponActiveObjectEvent(IwMessagePtr ptr)
{
	FUNCTRACKER;

	switch (ptr->message_id)
	{
	case MSG_CALL_HANG_UP_EVT:
	 {

		 UponCallTerminated(ptr);
		 break;
	 }
	case MSG_CALL_DTMF_EVT:
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

void 
GenericOfferAnswerSession::UponCallTerminated(IwMessagePtr ptr)
{
	_hangupChannel->Send(new MsgCallHangupEvt());
	CALL_RESET_STATE(CALL_STATE_TERMINATED);
}


ApiErrorCode
GenericOfferAnswerSession::RejectCall()
{
	FUNCTRACKER;

	LogDebug("GenericOfferAnswerSession::RejectCall iwh:" << _iwCallHandle );

	MsgCallOfferedNack *msg = new MsgCallOfferedNack();
	msg->stack_call_handle = _iwCallHandle;

	ApiErrorCode res = GetCurrRunningContext()->SendMessage(_serviceHandleId,msg);
	return res;

}

ApiErrorCode
GenericOfferAnswerSession::HangupCall()
{

	FUNCTRACKER;

	LogDebug("GenericOfferAnswerSession::HangupCall iwh:" << _iwCallHandle);

	if (_iwCallHandle == IW_UNDEFINED || 
		_callState == CALL_STATE_UNKNOWN || 
		_callState == CALL_STATE_TERMINATED)
	{
		return API_SUCCESS;
	}

	MsgHangupCallReq *msg = new MsgHangupCallReq(_iwCallHandle);
	msg->stack_call_handle = _iwCallHandle;

	ApiErrorCode res = GetCurrRunningContext()->SendMessage(_serviceHandleId,msg);

	_iwCallHandle = IW_UNDEFINED;
	_hangupDetected = TRUE;

	return res;
}

AbstractOffer
GenericOfferAnswerSession::RemoteOffer() 
{
	return _remoteOffer;
	
}

AbstractOffer
GenericOfferAnswerSession::LocalOffer() 
{
	return _localOffer;
}

int 
GenericOfferAnswerSession::StackCallHandle() const 
{ 
	return _iwCallHandle; 
}

void
GenericOfferAnswerSession::WaitTillHangup()
{
	FUNCTRACKER;

	LogDebug("GenericOfferAnswerSession::WaitTillHangup iwh:" << _iwCallHandle);

	ApiErrorCode res = API_TIMEOUT;
	IwMessagePtr response;
	while (res == API_TIMEOUT)
	{
		res = GetCurrRunningContext()->WaitForTxnResponse(_hangupChannel,response,Seconds(100));
	}

}

ApiErrorCode
GenericOfferAnswerSession::BlindXfer(IN const string &destination_uri)
{
	FUNCTRACKER;

	LogDebug("GenericOfferAnswerSession::BlindXfer iwh:" << _iwCallHandle << " dest:" << destination_uri);

	if (_iwCallHandle == IW_UNDEFINED)
	{
		return API_FAILURE;
	}


	IwMessagePtr response = NULL_MSG;

	MsgCallBlindXferReq *msg = new MsgCallBlindXferReq();
	msg->destination_uri = destination_uri;
	msg->stack_call_handle = _iwCallHandle;

	ApiErrorCode res = GetCurrRunningContext()->DoRequestResponseTransaction(
		_serviceHandleId,
		IwMessagePtr(msg),
		response,
		Seconds(60),
		"Blind Xfer GenericOfferAnswerSession TXN");

	if (res != API_SUCCESS)
	{
		return res;
	}

	switch (response->message_id)
	{
	case MSG_CALL_BLIND_XFER_ACK:
		{
			CALL_RESET_STATE(CALL_STATE_TERMINATED);
			break;
		}
	case MSG_CALL_BLIND_XFER_NACK:
		{
			res = API_SERVER_FAILURE;
			break;
		}
	default:
		{
			return API_SERVER_FAILURE;
		}
	}
	return res;

}


}

#pragma pop_macro("SendMessage")