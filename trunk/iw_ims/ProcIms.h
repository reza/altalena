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



#include "Ims.h"

using namespace std;

namespace ivrworx
{
	enum ImsSessionState
	{
		IMS_INITIAL,
		IMS_ALLOCATED,
		IMS_TICKING,
		IMS_STOPPED,
		IMS_TORNDOWN
	};

	
	struct StreamingCtx
	{
		StreamingCtx();

		~StreamingCtx();

		ImsSessionState state;

		AudioStream *stream;

		LpHandlePair session_handler;

		// used to send responses as it is 
		// temporary process used to run transaction
		//
		IwMessagePtr last_user_request;

		int  port;

		RtpProfile *profile;

		BOOL loop;

		ImsHandle ims_handle;

	};

	typedef 
	shared_ptr<StreamingCtx> StreamingCtxPtr;

	struct ImsOverlapped:
		public OVERLAPPED
	{
		ImsHandle ims_handle_id;

		int dtmf;
	};

	class ProcIms :
		public LightweightProcess
	{

	public:
		ProcIms(IN LpHandlePair pair, IN Configuration &conf);

		void real_run();

		virtual ~ProcIms(void);

	protected:

		

		virtual void AllocatePlaybackSession(IwMessagePtr msg);

		virtual void ModifySession(IwMessagePtr msg);

		virtual void StartPlayback(IwMessagePtr msg);

		virtual void StopPlayback(IwMessagePtr msg);

		virtual void SendDtmf(IwMessagePtr msg);

		virtual void TearDown(IwMessagePtr msg);

		virtual ApiErrorCode StartTicking(StreamingCtxPtr ctx);

		virtual ApiErrorCode StopTicking(StreamingCtxPtr ctx);

		virtual void TearDown(StreamingCtxPtr ctx);

		virtual void UponPlaybackStopped(ImsOverlapped* args);

		virtual void UponDtmfEvent(ImsOverlapped* args);

		virtual void TearDownAllSessions();

	private:


		void InitCodecs();

		void CleanUpResources();

		CnxInfo _localMedia;

		PortManager _portManager;

		MSTicker *_ticker;

		RtpProfile *_avProfile;

		typedef	
		map<ImsHandle, StreamingCtxPtr> StreamingCtxsMap;
		StreamingCtxsMap _streamingObjectSet;

		typedef
		map<string, PayloadType*> PayloadTypeMap;
		PayloadTypeMap _payloadTypeMap;

		typedef 
		map<string, int> PayloadIndexMap;
		PayloadIndexMap _payloadIndexMap;

		Configuration &_conf;

		IocpInterruptorPtr _iocpPtr;

		OrtpEvQueue *_rtp_q;

		HANDLE _rtpWorkerShutdownEvt;

		HANDLE _rtpWorkerHandle;

	};

	
	

}

