#  	Feather build guide

[toc]

## Create folders

To make debugging more efficient, I standardized the scripts to make everything relevant to home dir. If you execute everything in section 0, then everything commands mentioned in this guide should work with a copy and paste.

If you use a different folder, then you would probably need cd to the correct directory and rerun everything.

```shell
cd ~
mkdir featherDebug
```



## Dataset preparation

| Name              | Repo                                                         | Miss type     |
| ----------------- | ------------------------------------------------------------ | ------------- |
| histogram         | Phoenix                                                      | False sharing |
| linear-regression | [Phoenix](https://github.com/kozyraki/phoenix/tree/master/phoenix-2.0) | False sharing |
| lu_ncb            | [Splahs2](https://github.com/staceyson/splash2)              | True sharing  |

Clone these two repos and directly use make command to generate target binary program.



```shell
cd ~/featherDebug
git clone https://github.com/kozyraki/phoenix.git
git clone https://github.com/staceyson/splash2
```
Turn on debug symbol in Default.mk

```
vim ~/featherDebug/phoenix/phoenix-2.0/Defines.mk
#Uncomment DEBUG=-g

vim ~/featherDebug/splash2/codes/Makefile.config
# Uncomment CFLAGS := -g3 -pthread -D_POSIX_C_SOURCE=200112

```
Build phonix2.0
```
cd ~/featherDebug/phoenix/phoenix-2.0
make clean #If 
make -j70 all
```

Build lu_ncb

```

```

Download data files

```
mkdir -p ~/featherDebug/phoenix/phoenix-2.0/data
rm -rf ~/featherDebug/phoenix/phoenix-2.0/data/*

cd ~/featherDebug/phoenix/phoenix-2.0/data

wget http://csl.stanford.edu/~christos/data/histogram.tar.gz
wget http://csl.stanford.edu/~christos/data/linear_regression.tar.gz
wget http://csl.stanford.edu/~christos/data/string_match.tar.gz
wget http://csl.stanford.edu/~christos/data/reverse_index.tar.gz
wget http://csl.stanford.edu/~christos/data/word_count.tar.gz
 
tar -xf histogram.tar.gz
tar -xf linear_regression.tar.gz
tar -xf string_match.tar.gz
tar -xf reverse_index.tar.gz
tar -xf word_count.tar.gz

```

## Build Feather

Build hpctoolkit-externals

> Attention: We don’t need to specify –prefix when run configure, because externals only compile some necessary libraries without “make install”.

```
mkdir -p ~/featherDebug/feather
cd ~/featherDebug/feather
git clone https://github.com/WitchTools/hpctoolkit-externals
cd hpctoolkit-externals
ac_cv_prog_cxx_g=yes ./configure
make -j70 all
make distclean
```

```
mkdir ~/featherDebug/feather/bin/debug
cd ~/featherDebug/feather
git clone https://github.com/WitchTools/libmonitor
cd ~/featherDebug/feather/libmonitor
#./configure --enable-debug --prefix=`realpath ~/featherDebug/feather/bin/debug`
./configure --prefix=`realpath ~/featherDebug/feather/bin/release`
make clean
make -j70
make install
```
```
cd ~/featherDebug/feather
git clone https://github.com/WitchTools/PAPI.git
cd PAPI/src
#./configure --prefix=`realpath ~/featherDebug/feather/bin/debug`
./configure --prefix=`realpath ~/featherDebug/feather/bin/release`
make -j70
make install
```


```
cd ~/featherDebug/feather
git clone https://github.com/WitchTools/hpctoolkit
cd ~/featherDebug/feather/hpctoolkit

#./configure --enable-develop --prefix=`realpath ~/featherDebug/feather/bin/debug` --with-externals=`realpath ~/featherDebug/feather/hpctoolkit-externals` --with-libmonitor=`realpath ~/featherDebug/feather/bin/debug` --with-papi=`realpath ~/featherDebug/feather/bin/debug`
./configure --prefix=`realpath ~/featherDebug/feather/bin/release` --with-externals=`realpath ~/featherDebug/feather/hpctoolkit-externals` --with-libmonitor=`realpath ~/featherDebug/feather/bin/release` --with-papi=`realpath ~/featherDebug/feather/bin/release`
make clean
make -j70
make install 
```

## Env Variable

```
#export PATH=`realpath ~/featherDebug/feather/bin/debug/bin`:$PATH
export PATH=`realpath ~/featherDebug/feather/bin/release/bin`:$PATH
```

## Run linear_regression

```
cd ~/featherDebug/phoenix/phoenix-2.0/tests/linear_regression
mkdir -p ~/featherDebug/phoenix/phoenix-2.0/tests/linear_regression/results
```

### With debug

```
rm -rf results/*
hpcrun  --debug -o ./results -e WP_FALSE_SHARING -e MEM_UOPS_RETIRED:ALL_STORES@200 ./linear_regression-pthread ~/featherDebug/phoenix/phoenix-2.0/data/linear_regression_datafiles/key_file_100MB.txt
```

## Run built-in tests

```
cd ~/featherDebug/Feather/microbenchmark
```
### Without debug
```
rm -rf results/*
hpcrun  -o ./results -e WP_FALSE_SHARING -e MEM_UOPS_RETIRED:ALL_STORES@200 ./test1.out
```
### With debug

```
rm -rf results/*
hpcrun  --debug -o ./results -e WP_FALSE_SHARING -e MEM_UOPS_RETIRED:ALL_STORES@200 ./linear_regression-pthread ~/featherDebug/phoenix/phoenix-2.0/data/linear_regression_datafiles/key_file_100MB.txt
```


## Attach gdb

```
sudo su
ps -aux|grep linear_regression
gdb
attach PID

#set HPCRUN_DEBUGGER_WAIT=0
call ((void (*) (void)) hpcrun_continue) () 

#Avoid SIG38
handle SIG37 noprint nostop
handle SIG38 noprint nostop

#Set breakpoints

```

 Without debug

```
rm -rf results/*
hpcrun -o ./results -e WP_FALSE_SHARING -e MEM_UOPS_RETIRED:ALL_STORES@200 ./linear_regression-pthread ~/featherDebug/phoenix/phoenix-2.0/data/linear_regression_datafiles/key_file_100MB.txt
```

set (int)HPCRUN_DEBUGGER_WAIT = 0

# Key Code Snippets

### Event Sampling

Event name: MEM_UOPS_RETIRED:ALL_STORES

linux_perf.c:gen_event_set -> perf_thread_init -> perf_event_open (Called upon each thread creation)

**--->** linux_perf.c:perf_event_handler -> linux_perf.c:record_sample->watchpoint_support:OnSample-> TIME_ELAPSED? -> wathpoint_support: SubscribeWatchpoint -> wathpoint_support: CreateWatchPoint -> perf_event_open (PERF_TYPE_BREAKPOINT) 

Init counter
```
tData.numWatchpointTriggers = 0;
tData.numWatchpointImpreciseIP = 0;
tData.numWatchpointImpreciseAddressArbitraryLength = 0;
tData.numWatchpointImpreciseAddress8ByteLength = 0;
tData.numWatchpointDropped = 0;
tData.numSampleTriggeringWatchpoints = 0;
tData.numInsaneIP = 0;
```

```c++
if( ((curTime-localSharedData.time) > 2 * (curTime-lastTime)) // Sufficient time passed since the last time somebody published
```
perf_thread_fini will send PERF_SIGNAL

**--->**  wathpoint_support: OnWatchPoint (Signal monitor) -> wathpoint_support:  CollectWatchPointTriggerInfo ->   Counter++;

```c++
  if(WatchpointClientActive()){
    OnSample(mmap_data,
             hpcrun_context_pc(context),
             sv->sample_node,
             current->event->metric);
  }
```

### Watchpoint

# Problems


## PreciseIP Problem

```
  char *name;
  int precise_ip_type = perf_skid_parse_event(event_name, &name);
  free(name);

  u64 precise_ip;

  switch (precise_ip_type) {
    case PERF_EVENT_AUTODETECT_SKID: 
            precise_ip = perf_skid_set_max_precise_ip(attr);
	    break;
    case PERF_EVENT_SKID_ERROR:
    case PERF_EVENT_SKID_ARBITRARY:
	    // check the HPCRUN_PRECISE_IP env variable
	    precise_ip = perf_skid_get_precise_ip(attr);
	    break;
    default:
            precise_ip = precise_ip_type;
  }

  attr->precise_ip    = precise_ip;
```

## DisArm Problem

```
#0  DisArm (wpi=0x7f27f8c40210) at sample-sources/watchpoint_support.c:519
#1  0x00007f27fbda002b in DisableWatchpointWrapper (wpi=0x7f27f8c40210) at sample-sources/watchpoint_support.c:1014
#2  0x00007f27fbda027c in OnWatchPoint (signum=37, info=0x7f27f7f464f0, context=0x7f27f7f463c0)
    at sample-sources/watchpoint_support.c:1064
#3  0x00007f27fbb4955d in monitor_signal_handler (sig=37, info=0x7f27f7f464f0, context=0x7f27f7f463c0) at signal.c:220
#4  <signal handler called>
#5  Work (me=0) at test1.cpp:18
#6  <lambda()>::operator() (__closure=<optimized out>) at test1.cpp:27
#7  std::__invoke_impl<void, main()::<lambda()> > (__f=...) at /usr/include/c++/7/bits/invoke.h:60
#8  std::__invoke<main()::<lambda()> > (__fn=...) at /usr/include/c++/7/bits/invoke.h:95
#9  std::thread::_Invoker<std::tuple<main()::<lambda()> > >::_M_invoke<0> (this=0x55f2720bbff8) at /usr/include/c++/7/thread:234
#10 std::thread::_Invoker<std::tuple<main()::<lambda()> > >::operator() (this=0x55f2720bbff8) at /usr/include/c++/7/thread:243
#11 std::thread::_State_impl<std::thread::_Invoker<std::tuple<main()::<lambda()> > > >::_M_run(void) (this=0x55f2720bbff0)
    at /usr/include/c++/7/thread:186
#12 0x00007f27fb80a4c0 in ?? () from /usr/lib/x86_64-linux-gnu/libstdc++.so.6
#13 0x00007f27fbb52501 in monitor_begin_thread (arg=0x7f27fbd60280 <monitor_init_tn_array>) at pthread.c:978
#14 0x00007f27fb3066db in start_thread (arg=0x7f27f8c48700) at pthread_create.c:463
#15 0x00007f27fb02f71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

```



.



 set HPCRUN_DEBUGGER_WAIT=0


 #0  hpcrun_set_ipc_load_map (val=true) at loadmap.c:78
#1  0x00007f3affcc51b2 in process_event_list (self=0x7f3afff0a640 <_witch_obj>, lush_metrics=0) at sample-sources/watchpoint_clients.c:867
#2  0x00007f3affcac7c5 in hpcrun_all_sources_process_event_list (lush_metrics=0) at sample_sources_all.c:256
#3  0x00007f3affca5d75 in hpcrun_init_internal (is_child=false) at main.c:436
#4  0x00007f3affca66bb in monitor_init_process (argc=0x7f3affc831f0 <monitor_argc>, argv=0x7fffbd298fe8, data=0x0) at main.c:805
#5  0x00007f3affa7364a in monitor_begin_process_fcn (user_data=0x0, is_fork=<optimized out>) at main.c:281
#6  0x00007f3affa73ba4 in monitor_main (argc=argc@entry=2, argv=argv@entry=0x7fffbd298fe8, envp=0x7fffbd299000) at main.c:504
#7  0x00007f3aff477bf7 in __libc_start_main (main=0x7f3affa73af0 <monitor_main>, argc=2, argv=0x7fffbd298fe8, init=<optimized out>, fini=<optimized out>, rtld_fini=<optimized out>, 
    stack_end=0x7fffbd298fd8) at ../csu/libc-start.c:310
#8  0x00007f3affa73212 in __libc_start_main (main=<optimized out>, argc=2, argv=0x7fffbd298fe8, init=0x564fda405d30 <__libc_csu_init>, fini=0x564fda405da0 <__libc_csu_fini>, 
    rtld_fini=0x7f3afffeeb40 <_dl_fini>, stack_end=0x7fffbd298fd8) at main.c:555
#9  0x0000564fda4052aa in _start ()

