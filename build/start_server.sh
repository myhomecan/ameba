#!/bin/sh
echo
echo ' *** starting server on port 2000'
echo
(sleep 1; echo
echo ' *** to update enter'
echo '     ATSO=<IP>,2000'
echo
echo) &

wine DownloadServer.exe 2000 application/Debug/bin/ota.bin
