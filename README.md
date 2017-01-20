# Ameba SDK

To build it change path to arm-none-eabi toolchain in build/application.mk

after that

```
cd build
make (or gmake on FreeBSD)
```

To upload binary over OTA

```
cd build
./start_server.sh
```

and on your board type `ATSO=A.B.C.D,2000` where A.B.C.D is IP of your machine

