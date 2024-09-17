all: comcli comserver

comcli: comcli.c
	gcc -Wall -Wextra -std=c11 comcli.c -o comcli

comserver: comserver.c
	gcc -Wall -Wextra -std=c11 comserver.c -o comserver

clean:
	rm -f comcli comserver
