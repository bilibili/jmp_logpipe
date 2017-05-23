/*
 * bbLog_parser.h
 *
 *  Created on: 2016年2月2日
 *      Author: magicbear
 */

#ifndef SRC_CMD_PARSER_H_
#define SRC_CMD_PARSER_H_

typedef struct
{
	char *user;
	size_t user_len;
	char *hostname;
	size_t hostname_len;
	char *workdir;
	size_t workdir_len;
	char *cmd;
	size_t cmd_len;
	char *args;
	size_t args_len;
} bb_cmdline_t;

int cmd_parseLine(char *p, char *pe, bb_cmdline_t *plog);

#endif /* SRC_CMD_PARSER_H_ */
