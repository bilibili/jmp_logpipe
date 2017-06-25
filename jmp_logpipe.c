#include <config.h>
#include <sys/types.h>
#include <signal.h>
#include <getopt.h>

#include "system.h"
#include "error.h"
#include "fadvise.h"
#include "xfreopen.h"
#include <stdio.h>
#include "cmdParser.h"
#include <arpa/inet.h>
#include <sys/socket.h>

/* The official name of this program (e.g., no 'g' prefix).  */
#define PROGRAM_NAME "jmp_logpipe"

#define AUTHORS \
  proper_name ("MagicBear")

#define LINE_BUFFER_SIZE    16384

static bool
tee_files (int nfiles, const char **files);

/* If true, append to output files rather than truncating them. */
static bool append;

/* If true, ignore interrupts. */
static bool ignore_interrupts;

char *post_url = NULL;
int port;
char *post_user = NULL;
char *post_host = NULL;
static uint64_t woffset = 0;
static pid_t kpid = 0;

static struct sockaddr_in si_log;
static int i_sockLen;
static int log_fd = 0;

static bool skip_out = false;

typedef struct jumper_log_s jumper_log_t;
struct jumper_log_s {
	uint64_t log_datasize;
	char user[64];
	char host[64];
	char ssh_user[64];
	char hostname[64];
	char workdir[256];
	char cmd[256];
	char *payload;
} __attribute__((packed));

static struct option const long_options[] =
{
  {"append", no_argument, NULL, 'a'},
  {"ignore-interrupts", no_argument, NULL, 'i'},
  {"user", required_argument, NULL, 'u'},
  {"host", required_argument, NULL, 'h'},
  {"server", required_argument, NULL, 's'},
  {"port", required_argument, NULL, 'p'},
  {"kill-pid", required_argument, NULL, 'k'},
  {GETOPT_HELP_OPTION_DECL},
  {GETOPT_VERSION_OPTION_DECL},
  {NULL, 0, NULL, 0}
};

void
usage (int status)
{
  if (status != EXIT_SUCCESS)
    emit_try_help ();
  else
    {
      printf (_("Usage: %s [OPTION]... [FILE]...\n"), PROGRAM_NAME);
      fputs (_("\
Copy standard input to each FILE, and also to standard output.\n\
\n\
  -a, --append              append to the given FILEs, do not overwrite\n\
  -i, --ignore-interrupts   ignore interrupt signals\n\
  -s, --server              post target ip\n\
  -p, --port                post target UDP port\n\
  -u, --user                post to target user\n\
  -H, --host                post to target host\n\
  -k, --kill-pid            badlist command kill process\n\
"), stdout);
      fputs (HELP_OPTION_DESCRIPTION, stdout);
      fputs (VERSION_OPTION_DESCRIPTION, stdout);
      fputs (_("\
\n\
If a FILE is -, copy again to standard output.\n\
"), stdout);
    }
  exit (status);
}

int
main (int argc, char **argv)
{
  bool ok;
  int optc;

  i_sockLen = sizeof(si_log);
  memset((char *) &si_log, 0, sizeof(si_log));

  append = false;
  ignore_interrupts = false;

  while ((optc = getopt_long (argc, argv, "aihs:p:u:H:b:k:", long_options, NULL)) != -1)
    {
      switch (optc)
        {
        case 'a':
          append = true;
          break;

        case 'i':
          ignore_interrupts = true;
          break;

        case 's':
			if ( (log_fd=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
			{
				perror("socket");
				return 1;
			}
		    si_log.sin_family = AF_INET;
		    si_log.sin_port = htons(port ? port : 12345);
		    
		    if (inet_aton(optarg , &si_log.sin_addr) == 0) 
		    {
		        fprintf(stderr, "inet_aton() failed\n");
				return 1;
		    }
          break;

        case 'p':
          port = atoi(optarg);
          si_log.sin_port = htons(port);
          break;

        case 'u':
          post_user = optarg;
          break;

        case 'H':
          post_host = optarg;
          break;

        case 'k':
          kpid = atoi(optarg);
          break;

        case 'h':
          usage(EXIT_SUCCESS);
          break;

        default:
          usage (EXIT_FAILURE);
        }
    }

  if (ignore_interrupts)
    signal (SIGINT, SIG_IGN);

  /* Do *not* warn if tee is given no file arguments.
     POSIX requires that it work when given no arguments.  */

  ok = tee_files (argc - optind, (const char **) &argv[optind]);
  if (close (STDIN_FILENO) != 0)
    perror ("standard input");
  exit (ok ? EXIT_SUCCESS : EXIT_FAILURE);
}


typedef void (*cb_parseLine)(char *, int, uint64_t);

void parse_line_buffer(char *line_buffer, int bytes_read, char *new_buffer, int new_buffer_size, int *new_buffer_off, cb_parseLine callback)
{
	if (bytes_read <= 0) return;

	int i;
	for (i = 0; i < bytes_read; i++)
	{
		if (!skip_out && (line_buffer[i] == '\n' || line_buffer[i] == '\r' || *new_buffer_off + 1 >= new_buffer_size))
		{
			new_buffer[(*new_buffer_off)++] = line_buffer[i];

			char *final_buff = malloc(*new_buffer_off);
			int final_off_t = 0;
			int j;
			// printf("final = %d\n", *new_buffer_off);
			for (j = 0; j < *new_buffer_off; j++)
			{
				if (new_buffer[j] == '\b')
				{
					final_off_t--;
					if (final_off_t < 0) final_off_t = 0;
        } else if (new_buffer[j] == '\x9b' || (j < *new_buffer_off - 1 && new_buffer[j] == '\x1b' && (new_buffer[j+1] == '[')))
        {
          char *p = new_buffer + j + (new_buffer[j] == '\x1b' ? 2 : 1);
          // printf("offset = %zd\n", p - new_buffer);
          while (p - new_buffer < *new_buffer_off && *p >= '0' && *p <='?') p++;
          while (p - new_buffer < *new_buffer_off && (*p == ' ' || *p == '-' || *p == '/')) p++;
          if (*p >= '@' && *p <= '~')
          {
            // printf("skip pos = %zd\n", p - new_buffer);
            j = p - new_buffer;
            continue;
          }
          final_buff[final_off_t++] = new_buffer[j];
				} else if (j < *new_buffer_off - 3 && new_buffer[j] == '\x1b' && (new_buffer[j+1] == '('))
				{
          j += 2;
				} else if (j < *new_buffer_off - 4 && strncmp(new_buffer + j, "\x1b]0;", 4) == 0)
				{
					char *p = new_buffer + j + 4;
					while (p - new_buffer < *new_buffer_off && *p != '\x07') p++;
					if (*p == '\x07')
					{
						// printf("skip pos = %zd\n", p - new_buffer);
						j = p - new_buffer;
						continue;
					}
					final_buff[final_off_t++] = new_buffer[j];
				} else
				{
					final_buff[final_off_t++] = new_buffer[j];
				}
			}
			callback(final_buff, final_off_t, woffset - *new_buffer_off);
			free(final_buff);

			*new_buffer_off = 0;
		} else {
			char partten[19];
			strcpy(partten, "** B00000000000000");
			partten[2] = '\x18';

			if (!skip_out && *new_buffer_off >= 5 && memcmp(new_buffer + *new_buffer_off - 5, partten, 5) == 0)
			{
				skip_out = true;
				if (kpid > 0)
				{
					printf("\r\033[31;1m错误：系统已禁止使用lrzsz，请使用sftp进行文件传输\033[0m\r\n");
					kill(kpid, SIGTERM);
				}
			} else if (skip_out && *new_buffer_off >= 5 && memcmp(new_buffer + *new_buffer_off - 5, partten, 5) == 0)
			{
				skip_out = false;
				*new_buffer_off = 0;
				continue;
			}
			if (skip_out && *new_buffer_off + 1 >= new_buffer_size) 
			{
				*new_buffer_off = 0;
			}
			new_buffer[(*new_buffer_off)++] = line_buffer[i];
		}
	}
}


void onLineParsed(char *cmdStart, int cmdLen, uint64_t line_start_off)
{
	bb_cmdline_t plog;
	if (cmd_parseLine(cmdStart, cmdStart + cmdLen, &plog))
	{
		jumper_log_t  *send_log = malloc(16384);

		memset(send_log, 0, 16384);

		send_log->log_datasize = line_start_off;
		if (post_user != NULL) strcpy(send_log->user, post_user);
		if (post_host != NULL) strcpy(send_log->host, post_host);
		strncpy(send_log->ssh_user, plog.user, plog.user_len > sizeof(send_log->ssh_user) ? sizeof(send_log->ssh_user) : plog.user_len);
		strncpy(send_log->hostname, plog.hostname, plog.hostname_len > sizeof(send_log->hostname) ? sizeof(send_log->hostname) : plog.hostname_len);
		strncpy(send_log->workdir, plog.workdir, plog.workdir_len > sizeof(send_log->workdir) ? sizeof(send_log->workdir) : plog.workdir_len);
		strncpy(send_log->cmd, plog.cmd, plog.cmd_len > sizeof(send_log->cmd) ? sizeof(send_log->cmd) : plog.cmd_len);
		memcpy(&send_log->payload, plog.args, plog.args_len > 16384 - offsetof(jumper_log_t, payload) ? 16384 - offsetof(jumper_log_t, payload) : plog.args_len);

        //send the message
        if (log_fd && sendto(log_fd, send_log, offsetof(jumper_log_t, payload) + (plog.args_len > 16384 - offsetof(jumper_log_t, payload) ? 16384 - offsetof(jumper_log_t, payload) : plog.args_len + 1), 0 , (struct sockaddr *) &si_log, i_sockLen)==-1)
        {
            perror("sendto()");
        }

		// printf("post url: %s\n", post_url);
		// printf("post user: %s\n", post_user);
		// printf("user: %.*s\n", plog.user_len, plog.user);
		// printf("hostname: %.*s\n", plog.hostname_len, plog.hostname);
		// printf("workdir: %.*s\n", plog.workdir_len, plog.workdir);
		// printf("cmd: %.*s\n", plog.cmd_len, plog.cmd);
		// printf("args: %.*s\n", plog.args_len, plog.args);
	}
}

/* Copy the standard input into each of the NFILES files in FILES
   and into the standard output.
   Return true if successful.  */

static bool
tee_files (int nfiles, const char **files)
{
  FILE **descriptors;
  char buffer[BUFSIZ];
  char line_buffer[LINE_BUFFER_SIZE];
  int new_buf_off = 0;
  ssize_t bytes_read;
  int i;
  bool ok = true;
  char const *mode_string = "ab";

  descriptors = (FILE **)calloc (nfiles + 1, sizeof *descriptors);

  /* Move all the names 'up' one in the argv array to make room for
     the entry for standard output.  This writes into argv[argc].  */
  for (i = nfiles; i >= 1; i--)
    files[i] = files[i - 1];

  if (O_BINARY && ! isatty (STDIN_FILENO))
    xfreopen (NULL, "rb", stdin);
  if (O_BINARY && ! isatty (STDOUT_FILENO))
    xfreopen (NULL, "wb", stdout);

  fadvise (stdin, FADVISE_SEQUENTIAL);

  /* In the array of NFILES + 1 descriptors, make
     the first one correspond to standard output.   */
  descriptors[0] = stdout;
  files[0] = "standard output";
  setvbuf (stdout, NULL, _IONBF, 0);

  for (i = 1; i <= nfiles; i++)
    {
      descriptors[i] = (STREQ (files[i], "-")
                        ? stdout
                        : fopen (files[i], mode_string));
      if (descriptors[i] == NULL)
        {
          error (0, errno, "%s", files[i]);
          ok = false;
        }
      else
        setvbuf (descriptors[i], NULL, _IONBF, 0);
    }

  while (1)
    {
      bytes_read = read (0, buffer, sizeof buffer);
      if (bytes_read < 0 && errno == EINTR)
        continue;
      if (bytes_read <= 0)
        break;

      /* Write to all NFILES + 1 descriptors.
         Standard output is the first one.  */
	  woffset += bytes_read;
      parse_line_buffer(buffer, bytes_read, line_buffer, LINE_BUFFER_SIZE, &new_buf_off, onLineParsed);

      if (skip_out)
      {
      	woffset -= bytes_read;
      } else
      {      	
	    if (!skip_out)
	    {
	      for (i = 0; i <= nfiles; i++)
	        if (descriptors[i]
	            && fwrite (buffer, bytes_read, 1, descriptors[i]) != 1)
	          {
	            error (0, errno, "%s", files[i]);
	            descriptors[i] = NULL;
	            ok = false;
	          }
	    }
      }

    }

    if (bytes_read == -1)
    {
      perror("read error");
      ok = false;
    }

  /* Close the files, but not standard output.  */
  for (i = 1; i <= nfiles; i++)
    if (!STREQ (files[i], "-")
        && descriptors[i] && fclose (descriptors[i]) != 0)
      {
        error (0, errno, "%s", files[i]);
        ok = false;
      }

  free (descriptors);

  return ok;
}