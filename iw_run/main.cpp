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

#include "stdafx.h"

using namespace boost::assign;

using namespace JadedHoboConsole;
namespace con = JadedHoboConsole;

#define IW_WRONG_NUMBER_OF_ARGUMENTS -1
#define IW_ERROR_PARSING_CONF		 -2


#define  IW_DEFAULT_BOOT_TIME		15000
#define  IW_DEFAULT_KEEP_ALIVE_TIME 60000

namespace ivrworx
{
	class ProcSystemStarter: 
		public LightweightProcess
	{
	private:

		
		typedef list<LpHandlePair>
		HandlePairList;

		HandlePairList proc_handlepairs;

		HandlesVector selected_handles;

	public:
		ProcSystemStarter(LpHandlePair pair,ConfigurationPtr conf)
			:LightweightProcess(pair,"ProcSystemStarter"),
			_conf(conf)
		{

		}

		void real_run()
		{
			START_FORKING_REGION;

			FactoryPtrList factories_list = 
				list_of
				//(ProcFactoryPtr(new Live555RtpProxyFactory	()))
				(ProcFactoryPtr(new ResipStackFactory		()))
// 				(ProcFactoryPtr(new SqlFactory		()))
// 				(ProcFactoryPtr(new M2ImsFactory	()))
// 				(ProcFactoryPtr(new UniMrcpFactory	()))
// 				(ProcFactoryPtr(new OpalFactory		()))
//				(ProcFactoryPtr(new IvrFactory		()));
;

			if (factories_list.size() == 0)
			{
				LogInfo("No processes to boot, exiting.");
				return;
					
			}

			HandlePairList proc_handlepairs;

			HandlesVector selected_handles;
			selected_handles.push_back(_inbound);


			BOOL all_booted = TRUE;
			for (FactoryPtrList::iterator i = factories_list.begin(); 
				i !=  factories_list.end(); 
				++i)
			{
				// listen to process shutdown event
				DECLARE_NAMED_HANDLE(shutdown_handle);
				DECLARE_NAMED_HANDLE_PAIR(proc_pair);

				AddShutdownListener(proc_pair,shutdown_handle);
				try 
				{
					LightweightProcess *p = (*i)->Create(proc_pair,_conf);
					string name = p->Name();

					LogInfo("Booting process name:" << name << ", service uri:" << p->ServiceId());


					FORK(p);
					if (IW_FAILURE(WaitTillReady(MilliSeconds(IW_DEFAULT_BOOT_TIME), proc_pair)))
					{
						LogCrit("Cannot start proc:" << name);
						all_booted = FALSE;
						break;
					};

				} 
				catch (exception &e)
				{
					LogWarn("Exception during botting one of the processes, what:" << e.what());
					goto exit;
				}
				

				proc_handlepairs.push_back(proc_pair);
				selected_handles.push_back(shutdown_handle);
				
			};

			if (all_booted == FALSE)
			{
				goto exit;
			}

	
			I_AM_READY;

			
			{
				LpHandlePtr handle = ivrworx::GetHandle(_conf->GetString("ivr/sip_service"));

				
				HandleId service_handle_id = handle->GetObjectUid();
				SipMediaCallPtr first_call =
					SipMediaCallPtr(new SipMediaCall(forking, service_handle_id));

				SipMediaCallPtr second_call =
					SipMediaCallPtr(new SipMediaCall(forking, service_handle_id));

				BridgedMediaCallSession b;
				ApiErrorCode res = b.MakeBridgedCall(
					first_call,
					"sip:24001@192.168.150.3",
					second_call, 
					"sip:6095@192.168.150.3");

				return;
			}

			

			


			while(true)
			{
				
				IwMessagePtr msg;
				int index = 0;

				ApiErrorCode code = 
					SelectFromChannels(selected_handles,MilliSeconds(IW_DEFAULT_KEEP_ALIVE_TIME),index,msg);

				
				if (code == API_TIMEOUT)
				{
					for (HandlePairList::iterator i = proc_handlepairs.begin();
						i!= proc_handlepairs.end();
						++i)
					{
						ApiErrorCode ping_res = Ping(*i);
						if (IW_FAILURE(ping_res))
						{
							LogWarn("One of processes did not respond in timely fashion to keep alive request, consider restarting the application.")
						}

					}
					
					continue;

				} else
				{
					
					LogWarn("One of the process terminated unexpectedly. Shutting down all procesees.");
					break;
				}
			}

exit:

			// shutdown in reverse order (pop) first one
			for (HandlePairList::reverse_iterator i = proc_handlepairs.rbegin(); 
				i!= proc_handlepairs.rend();
				++i)
			{
				LogInfo("Shutting down proc:" << (*i).inbound->HandleName());
				Shutdown(Seconds(15), (*i));
			}

			
			END_FORKING_REGION;

		}

	private:

		ConfigurationPtr _conf;

	};

}

using namespace ivrworx;
int _tmain(int argc, _TCHAR* argv[])
{
	wstring conf_file;

	if (argc < 2)
	{
		cerr << "Usage: ivrworx.exe [configuration_file]" << std::endl;
		cerr << "\tStarts lightweight IVR application." << std::endl;
		cerr << "Trying default conf.json file..." << std::endl;

		conf_file = L"conf.json";
	} 
	else
	{
		conf_file = argv[1];
	}

	WSADATA dat;
	if (WSAStartup(MAKEWORD(2,2),&dat)!=0)
	{
		cerr << "Error starting up WIN socket, err:" << ::GetLastError();
		return -1;
	}

	MMRESULT timer_res = 
		timeBeginPeriod(1);

	if (timer_res!= TIMERR_NOERROR)
	{
		cerr << "Cannot set timer resolution to 1 res:" << timer_res << endl;
		return -2;
	}

	int res = 0;
	try
	{
		ApiErrorCode err_code = API_SUCCESS;
		ConfigurationPtr conf = 
			ConfigurationFactory::CreateJsonConfiguration(WStringToString(conf_file),err_code);

		if (IW_FAILURE(err_code))
		{
			return IW_ERROR_PARSING_CONF;
		}


		InitLog(conf);
		LogInfo(">>>>>> IVRWORX START <<<<<<");

		Start_CPPCSP();

		START_FORKING_REGION;

		DECLARE_NAMED_HANDLE_PAIR(starter_pair);

		FORK(new ProcSystemStarter(starter_pair, conf));

		END_FORKING_REGION;

		End_CPPCSP();


		LogInfo(">>>>>> IVRWORX END <<<<<<");
		ExitLog();

	} 
	catch (exception e)
	{
		cerr << endl << "Exception caught during program execution e:" << e.what() << endl;
		res = -3;
	}

	timeBeginPeriod(1);
	WSACleanup();

	return res;
	
}


