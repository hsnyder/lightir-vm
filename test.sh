set -e
cc -g lightir.c -o lightir.exe
./lightir.exe as example-loop.lightir > example-loop.bin
./lightir.exe as example-io.lightir > example-io.bin
