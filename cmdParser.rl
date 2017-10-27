#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cmdParser.h"

int cmd_parseLine(char *p, char *pe, bb_cmdline_t *plog)
{
	char *orig_st = p;
	const char *eof;
	char *fieldStart = NULL;
	char *lastAssign = "null";
	int successField = 0;
	int cs;
	eof = pe;

	memset(plog, 0, sizeof(bb_cmdline_t));
	/* Ragel code */
	// $http_host||$remote_addr||$msec||$status||$request_length||$bytes_sent||"$request"||"$http_referer"||"$http_user_agent"||
	//   $upstream_cache_status||$upstream_status||$request_time||$track_id/$cookie_DedeUserID.$cookie_sid/$http_x_cache_server||$upstream_response_time||$upstream_addr
	//
	//		name = space* %markNameStart
	//			   (alnum+ %markNameEnd <: space*)+
	//			   %nameAction ;
	%%{
        machine bbLog;
    
        write data;
		action markFieldStart         { fieldStart = p; }
		action assignUser             { successField++; lastAssign = (char *)"user"; plog->user = fieldStart; plog->user_len = p - fieldStart; }
		action assignHostname         { successField++; lastAssign = (char *)"hostname"; plog->hostname = fieldStart; plog->hostname_len = p - fieldStart; }
		action assignWorkdir          { successField++; lastAssign = (char *)"workdir"; plog->workdir = fieldStart; plog->workdir_len = p - fieldStart; }
		action assignCmd              { successField++; lastAssign = (char *)"cmd"; plog->cmd = fieldStart; plog->cmd_len = p - fieldStart; }
		action assignArgs             { successField++; lastAssign = (char *)"args"; plog->args = fieldStart; plog->args_len = p - fieldStart; }

		main := space*
				%markFieldStart ([a-z_A-Z])+ %assignUser "@"
				%markFieldStart [a-zA-Z_0-9\-]+ %assignHostname ":"
				%markFieldStart [^#$]+ %assignWorkdir [\#\$] space+
				%markFieldStart (any - space)+ %assignCmd any* %assignArgs [\r\n]
				;

		# Initialize and execute.
		write init;
		write exec;
	}%%
	       // fprintf(stderr, "  user: %.*s\n",  plog->user_len, plog->user);
	       // fprintf(stderr, "  workdir: %.*s\n",  plog->workdir_len, plog->workdir);
	       // fprintf(stderr, "  cmd: %.*s\n",  plog->cmd_len, plog->cmd);
	if (cs < bbLog_first_final) {
	    if (fieldStart != NULL)
	    {
		   // fprintf(stderr, "\033[31mInvalid log fmt:\033[0m %.*s (error: %d %d)\n", (int)(pe-orig_st), orig_st, bbLog_error, bbLog_en_main);
	    //    fprintf(stderr, " matched to : %.*s  field: %s   fieldStart: %p  success: %d\n",  (int)(pe-p), p, lastAssign, fieldStart, successField);
	    //    if (plog->workdir != NULL)
	    //    {
	    //    	fprintf(stderr, "workdir = %.*s\n", plog->workdir_len, plog->workdir);
	    //    }
	    //    fprintf(stderr, " [%.*s]\n",  (int)(pe-orig_st), orig_st);
	    }
		return 0;
	}
	return 1;
}

