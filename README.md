Prerequisites:
Netcat (NC) required. 

First run the makefile with command `make`. 

This will compile battle.c to create battle executable program

Run the program 

`./battle`

To join as a client from clientside:

```nc serverIP portnumber```

The port is set to 54770 

To join with server set to localhost:


`nc localhost 54770`
