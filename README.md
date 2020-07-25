## Simple graph 

### Abstract
#### Motivation
Once I was writing a C++ code to test the performance boost by SIMP and parallel execution. 
When I finished two sources with a small optimization in the second, I decided to compare 
their performance. I googled `cli build plot`, google gave me these two nice libraries: 
[cli-plot](https://github.com/Tabcorp/cli-plot) and
[cli-graph](https://github.com/mcastorina/graph-cli). 
But, in my humble opinion, there is one big problem in each one: nodejs and python corresponding. 
I don't want to deal with 20+ Mb of dependecies, third-party runtimes when I just need to compare 
performace of two programs. So I decided to write such program by my own.

#### About
What we try to achieve:
- Modern and high-level technology (as C++ 17).
- Easy to install, execute, uninstall.
- No huge runtime, native execution. Size of the binary file excepted to be less then 1 Mb.
- Easy to build and distribute in any popular platform.
- UNIX-like cli interface.

### Installation
#### Building from source
##### Requirements
- [SDL2](https://www.libsdl.org/download-2.0.php)
- [SDL2\_ttf](https://www.libsdl.org/projects/SDL_ttf/)

##### Compiling
```bash
$ git clone https://github.com/D34DStone/simple_graph && cd simple_graph
$ ./configure && make && make install
```
That's it! No external package managers, no any special runtimes. 

#### Uninstall
```bash
$ make uninstall
```

### Using (0.1.0)
#### Help
To watch a short info about the utility just run it with `--help` option: 
```bash
$ sg --help
simple_graph, version 0.1.0
usage: simple_graph [fname ...] [-a <integer>] [-x <integer>] [-y <integer>] [-s <char>] [-h]
```

#### Graphing
In the root of the repository I have two test files: `double.csv` and `float.csv`. To compare them you should simple run
```bash
$ sg *.csv
```
![double-float-comparison](https://github.com/D34DStone/simple_graph/blob/master/doc/0.1.0-screen-float-double.png?raw=true)

Also, some customization is available: 
```bash
$ sg *.csv --average 10 --res_x 1024 --res_y 256
```
![double-float-comparison-advanced](https://github.com/D34DStone/simple_graph/blob/master/doc/0.1.0-screen-float-double-advanced.png?raw=true)

### TODO
- Add option to render graph to the window or to the .png file.
- Add possibility to choose what column of csv use as x axis and what as y.
- Add validation of uint32\_t cli-option value and check security of cli.
- Display axis values.
- Refactor code.
