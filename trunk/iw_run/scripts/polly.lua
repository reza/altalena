
logger   = assert(ivrworx.LOGGER)
conf     = assert(ivrworx.CONF)
incoming = assert(ivrworx.INCOMING)

mrcp_goodbye = [[<?xml version="1.0"?>
<speak>
  <paragraph>
    <sentence>Thank you for using Polly.</sentence><sentence>And good bye.</sentence>
  </paragraph>
</speak>]]


--
--
--
function validate_res(res)
 return res==ivrworx.API_TIMEOUT or res==ivrworx.API_SUCCESS
end

--
--
--
function getdtmfsandconfirm(prompt_mrcp, termination_digit, interdigit_timeout, max_digits, max_retries)

	local confirmed = false
	local retries	= 0
	local dtmfs     = ""
	
	while (not confirmed) do 
	
		--
		-- Gather input
		-- 
		res, dtmfs = getdtmfs(prompt_mrcp, termination_digit, interdigit_timeout, max_digits, max_retries)
		
		if ((dtmfs == "") or (dtmfs == nil)) then
			return res
		end
		
		if (not validate_res(res)) then
			return res
		end
		
		--
		-- Confirm it
		-- 
		mrcp_input_confirmation = [[<?xml version="1.0"?>
		<speak>
		  <paragraph>
			<sentence>You have entered 
				<say-as interpret-as="characters">]] 
				.. dtmfs .. 
			  [[</say-as>
			 </sentence>
			 <sentence>Please press pound key to confirm, or any other key to enter data again.</sentence>
		  </paragraph>
		</speak>]]


		
		res, conf_dtmfs = getdtmfs(mrcp_input_confirmation, "#", interdigit_timeout, 1, max_retries)
		
		if (res ~= ivrworx.API_TIMEOUT and res ~= ivrworx.API_SUCCESS) then
			return res
		end
		
		logger:logdebug("conf_dtmfs:"..conf_dtmfs..", res:"..res);
		
		if (res == ivrworx.API_SUCCESS and conf_dtmfs == "") then 
		   	confirmed = true
		end	
	
		
	end
	
	
	logger:logdebug("confirmed dtmfs:"..dtmfs..", res:"..res);
	return res, dtmfs
	

end

			
--
-- Utility functions
--
function getdtmfs(prompt_mrcp, termination_digit, interdigit_timeout, max_digits, max_retries)

	local curr_dtmf   = ""
	local dtmf_buffer = ""
	local res         = ivrworx.API_TIMEOUT
	local retries	  = 0
	local confirmed	  = false
	
	
	--
	-- Clean DTMF buffer
	--
	
	incoming:speak(prompt_mrcp,false);
	incoming:cleandtmfbuffer();
	ivrworx.sleep(1000);
	
	
	while ( curr_dtmf ~= termination_digit and string.len(dtmf_buffer) < max_digits and retries < max_retries) do
		--
		-- Wait for DTMF's 
		--
		res,curr_dtmf = incoming:waitfordtmf(interdigit_timeout);
		
		if (res == ivrworx.API_TIMEOUT) then
			--
			-- If timeout play request again
			--
 			incoming:speak(prompt_mrcp,false);
 			ivrworx.sleep(1000);
 			retries = retries + 1;
 			
		elseif (res == ivrworx.API_SUCCESS) then
			--
			-- Add received dtmf to the buffer and stop playing
			--
			incoming:stopspeak(); 
			if (curr_dtmf~=termination_digit) then
				dtmf_buffer = dtmf_buffer..curr_dtmf;
			end
		else 
		
			logger:logwarn("Error receiveing dtmf, err:"..res);
			return res,dtmf_buffer
		end;
	end
		

	logger:logdebug("dtmf_buffer:"..dtmf_buffer..", res:"..res);
	return res, dtmf_buffer
	
end



---
--- Main
---
logger:loginfo("welcome to Polly client");


env = assert(luasql.sqlite3());
conn_string = assert(conf:getstring("polly_db"));
con = assert(env:connect(conn_string));

logger:loginfo("db:"..conn_string.." open");

assert(incoming:answer() == ivrworx.API_SUCCESS);

--
-- Play welcome propmt
--
mrcp = [[<?xml version="1.0"?>
<speak>
  <paragraph>
    <sentence> welcome to polly application.</sentence>
  </paragraph>
</speak>]]

incoming:speak(mrcp,true);
ivrworx.sleep(1000);

--
-- Gather user number
-- 
mrcp = [[<?xml version="1.0"?>
<speak>
  <paragraph>
    <sentence>Please enter your user number, then press pound key.</sentence>
  </paragraph>
</speak>]]

res, dtmf_buffer = getdtmfsandconfirm(mrcp, "#", 10000, 10, 1)
if (not validate_res(res) or dtmf_buffer == "" or dtmf_buffer == nil ) then
	incoming:speak(mrcp_goodbye,true);
	return
end

--
-- Gather user number
-- 

mrcp = [[<?xml version="1.0"?>
<speak>
  <paragraph>
    <sentence>Please enter your four digits, pin number.</sentence>
  </paragraph>
</speak>]]

dtmf_buffer, res = getdtmfs(mrcp, "#", 10000, 4, 1)
if (not validate_res(res) or dtmf_buffer == "" or dtmf_buffer == nil) then
	incoming:speak(mrcp_goodbye,true);
	return
end

