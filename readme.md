#FTP_LibUring_Server
Study project aimed to implement minimal FTP server application using the modern io_uring Linux asynchronous programming API.

#Build
1. Clone, install dependencies (Boost, liburing)
2. Run CMake, specifying the root of your Boost installation:
```CMake -DBOOST_ROOT=<your-root> .```
3. Optionally, you may specify additional parameters, e.g. compiler, compiler keys and makefile generator.
4. Build the program with the compiler and makefile processor you specified. Typically, ```make .```

#How to use
Launch the program, specifying the desired port and number of threads for it to use.

#Notice
This project was completed only in educational purposes and may contain lots of mistakes.
It is very likely that the project will receive no furhter support.