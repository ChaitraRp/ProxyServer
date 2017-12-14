#Contents
There is one folder:
-	proxyServer

proxyServer has the following contents:
- proxyserver.c
- Makefile
- gethostbyname_test.c (A helper file)
- blocked_sites.txt
- DNSCache.txt

-------------------------------------------------------------------------------------

#How to run the code:
To run: type make on command line from proxyServer folder
To clean the object file: run make clean on command line from webServer folder

- ./webproxy 10001 45
where 10001 port number and 45 is optional cache timeout value if not provided it will set default cache time out to 20 second

-------------------------------------------------------------------------------------

#What has been implemented
- Proxy server that supports multiple client connections
- Multithreading via fork()
- Only supports GET method
- Parsing client requests into HTTP and URL structs
- Caching and cache timeout (MD5 of requested URL)
       ----> if cache is found, then serve request via cached page depending on cache expiry timeout
	   ----> if cache is not found, then send request to server and serve the response sent by the server
- Hostname IP Address cache (DNSCache.txt - no expiration)
- Blocked websites (blocked_sites.txt)
- Error handling (400, 501)


-------------------------------------------------------------------------------------
#Extra credit
- Link Prefetch (link on every page that begins with href=" is collected and stored in cache folder)