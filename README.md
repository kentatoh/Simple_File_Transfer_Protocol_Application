# Simple_File_Transfer_Protocol_Application
A simple and minimal SFTP application build in C, with lots of bugs

Available commands:  
pwd - to display the current directory of the server that is serving the client  
lpwd - to display the current directory of the client  
dir - to display the file names under the current directory of the server that is serving the client  
ldir - to display the file names under the current directory of the client  
cd directory_pathname - to change the current directory of the server that is serving the client; Must support "." and ".." notations.  
lcd directory_pathname - to change the current directory of the client; Must support "." and ".." notations.  
get filename - to download the named file from the current directory of the remote server and save it in the current directory of the client  
put filename - to upload the named file from the current directory of the client to the current directory of the remote server.  
quit - to terminate the myftp session.  

Bugs:
Logging function is not working.
Max transfer size of 256 bytes.
Lacks of validation
+ tons of other bugs

In collaboration with Kenneth Andita

First created: 25/03/2021
Last updated: 09/04/2021
