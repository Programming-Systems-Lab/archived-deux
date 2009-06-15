README for UIMD
===============

Running the UIMD in Server mode
-------------------------------

1. Log-in as root.

2. Compile the UIMD daemon code in a separate directory using the command
"gcc -lpthread -O2 -o uimd *.c".
Let this directory be called uimd_dir.

3. Edit the uimd.config file as follows: Set the run_mode=server, include
one or more client_ip entries, specify the run_dir which tells UIMD where to
create the log file,
specify the mouse_event_interface and the keyboard_event_interface absolute
paths. The mouse and keyboard event interface information is available in
the file /proc/bus/input/devices file.
Also, specify the server port number. Default values for other settings like
server port etc. may be left unchanged.

4. Run the UIMD executable using the command "./uimd". The config file must
be present in the directory from where the daemon is run.
Check the uimd.log file in uimd_dir directory to see if the server is up and
ready to accept incoming client connections.

5. Start one or more clients which connect to this server.


Running UIMD in Client mode
---------------------------

1. Log-in as root.

2. Make sure that the uinput driver is loaded. This can be checked using
"lsmod | grep uinput" command.
If the driver is not already loaded, load it with "modprobe -v uinput"
command. This driver must be loaded for the UIMD client to function.

3. Compile the UIMD daemon code in a separate directory using the command
"gcc -lpthread -O2 -o uimd *.c".
Let this directory be called uimd_dir.

4. Edit the uimd.config file as follows: Set the run_mode=client, specify
the IP address of the machine
on which the UIMD server daemon will run in the server_ip entry and specify
run_dir which tells UIMD where to create the log file.
Also, specify the server port number. The sever port number should match the
server port number specified in the config file of the UIMD server.
Default values for other settings may be left unchanged.

5. Run the UIMD executable using the command "./uimd". The config file must
be present in the directory from where you run the daemon.

6. Check the uimd.log file in uimd_dir directory to see if the client is up
and ready to connect to the server.
