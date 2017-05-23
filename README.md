# jmp_logpipe

A tool for jumper analyize log and send the command to remove UDP server for aduit. And this tool will denied for lszrz protocol to prevent a large log for storage.

### Usage

ssh xxx | jmp_logpipe -s &lt;target udp ip&gt; -p <target udp port&gt; -H <hostname for logging&gt; -u <username for logging&gt; [-k <pid for kill with found lszrz protocol&gt;] -a

### Options

  -a, --append              append to the given FILEs, do not overwrite

  -i, --ignore-interrupts   ignore interrupt signals

  -s, --server              post target ip

  -p, --port                post target UDP port

  -u, --user                post to target user

  -H, --host                post to target host

  -k, --kill-pid            badlist command kill process

### Requirment

  Require for coreutils, change the Makefile COREUTIL to the coreutils source code path.

