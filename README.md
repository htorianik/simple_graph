## Simple graph 

### Abstract
Once I was writing C++ code to test performance boost by SIMP and parallel execution. 
When I finished two sources with small optimization in the second, I decided to comare their performance. 
I googled `cli build plot`, google gave me these two nice libraries: 
(cli-plot)[https://github.com/Tabcorp/cli-plot] 
(cli-graph)[https://github.com/mcastorina/graph-cli]. 
But, in my honour opinion, there is one big problem in each one: nodejs and python corresponding. 
I don't want to deal with 20+ Mb dependecies when I just need to compare performace of two programs. 
So I decided to write me own. 
Requirements to release first version was so: 
* Modern and high-level technology.
* Easy to install, use and then uninstall. 
* Opensource.
* Zero cost abstraction. No huge runtime, native execution.
* Available on all popular OS.
* Cli interface as simple and as UNIX-idimatically as possible.
* It is possible to compare arbitary amount of csv-files.

This way I made my simple-graph (next `sg`) utility.

### Building
That's it!
```bash
$ git clone https://github.com/D34DStone/simple_graph && cd simple_graph
$ ./configure && make && make install
```

To uninstall:
```bash
$ make uninstall
```

### Using 
To watch short info about utility just run it with `--help` option: 
```bash
$ sg --help
simple_graph, version 0.1.0
usage: simple_graph [fname ...] [-a <integer>] [-x <integer>] [-y <integer>] [-s <char>] [-h]
```

In the root of the repository I have two test files: `double.csv` and `float.csv`. To compare them you should simple run
```bash
$ sg *.csv
```
The result (version 0.1.0) will so: 
![double-float-comparison](https://github.com/D34DStone/simple_graph/blob/master/doc/0.1.0-screen-float-double.png?raw=true)

Also, some customization is available: 
```bash
$ sg *.csv --average 10 --res_x 1024 --res_y 256
```
![double-float-comparison-advanced](https://github.com/D34DStone/simple_graph/blob/master/doc/0.1.0-screen-float-double-advanced.png?raw=true)
