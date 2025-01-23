# NEC

## How to run the code

All the settings for the run (including the name of the problem file to use) are configured directly in the source code.
There's no commandline arguments.

You open the `NEC2/main.cpp` file and in the header correct the values of the constants as you wish,
then recompile the code.

### Windows

On Windows the code is opening as is in the Visual Studio (this is how I wrote it in the first place). In Visual Studio 2022 Community Edition, just open the NEC.sln file that's it. Click on the green "run" button in the middle of the toolbar above. It should run and produce the results according to the settings at the top of the main.cpp file.

### Linux/Mac

Install the base C++ compiler toolchain.

You need C++20 support, nothing else.

The project doesn't have any dependencies apart from the stdlib.
Compiler executable should be `g++` this is configured in the Makefile.

Having the `g++` available in the commandline, you go to the NEC2 subfolder and run:

```shell
make
```

It creates an executable called `main`. Then you run this executable:

```shell
./main
```

That's all.
