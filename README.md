# librte_pmd_rvif: a DPDK poll mode driver for rvif

This is a DPDK poll mode driver (pmd) for a virtual interface, called rvif, which is made for a virtual switch named [rvs](https://github.com/yasukata/rvs).

For details of rvs and rvif, please visit [https://github.com/yasukata/rvs](https://github.com/yasukata/rvs).

## How to use

### Compilation

#### librte_pmd_rvif.so

Please type the following to compile the program.

```
make
```

The command above will:
- download the source code of DPDK at ([LOCATION_OF_MAKEFILE]/dpdk/dpdk-$(DPDK_VER).tar.xz)
- compile it in ([LOCATION_OF_MAKEFILE]/dpdk/dpdk-$(DPDK_VER))
- install the compiled binaries at [LOCATION_OF_MAKEFILE]/dpdk/install .
- clone the rvs repository at [LOCATION_OF_MAKEFILE]/deps/rvs .
- compile [LOCATION_OF_MAKEFILE]/main.c

DPDK is installed in [LOCATION_OF_MAKEFILE]/dpdk/install; therefore, you do not need the root permission for the installation, and this does not overwrite the existing DPDK library.

#### deps/rvs/apps/fwd/a.out

The following command will generate a binary of the forwarder application.

```
make -C deps/rvs/apps/fwd
```

### Run

We run the testpmd application of DPDK at ```deps/dpdk/install/bin/dpdk-testpmd``` for testing.

First, please create two of 32 MB shared memory files by the following commands.

```
dd if=/dev/zero of=/dev/shm/rvs_shm00 bs=1M count=0 seek=32
```

```
dd if=/dev/zero of=/dev/shm/rvs_shm01 bs=1M count=0 seek=32
```

Terminal/console 1: please execute the following command to launche the forwarder application.

```
./deps/rvs/apps/fwd/a.out -m /dev/shm/rvs_shm00 -m /dev/shm/rvs_shm01
```

Terminal/console 2: please open a terminal/console and type the following to start testpmd with the rxonly mode.

```
sudo LD_LIBRARY_PATH=./deps/dpdk/install/lib/x86_64-linux-gnu LD_PRELOAD=./deps/dpdk/install/lib/x86_64-linux-gnu/dpdk/pmds-23.0/librte_mempool_ring.so.23.0:./librte_pmd_rvif.so ./deps/dpdk/install/bin/dpdk-testpmd -l 0-1 --proc-type=primary --file-prefix=pmd1 --vdev=net_rvif,mac=aa:bb:cc:dd:ee:00,mem=/dev/shm/rvs_shm00 --no-pci -- --nb-cores 1 --forward-mode=rxonly --txq=1 --rxq=1 --txd=512 --rxd=512 --stats-period=1
```

Terminal/console 3: please open a terminal/console and execute the following to launch testpmd with the txonly mode.

```
sudo LD_LIBRARY_PATH=./deps/dpdk/install/lib/x86_64-linux-gnu LD_PRELOAD=./deps/dpdk/install/lib/x86_64-linux-gnu/dpdk/pmds-23.0/librte_mempool_ring.so.23.0:./librte_pmd_rvif.so ./deps/dpdk/install/bin/dpdk-testpmd -l 2-3 --proc-type=primary --file-prefix=pmd2 --vdev=net_rvif,mac=aa:bb:cc:dd:ee:01,mem=/dev/shm/rvs_shm01 --no-pci -- --nb-cores 1 --forward-mode=txonly --txq=1 --rxq=1 --txd=512 --rxd=512 --stats-period=1 --txpkts=64
```

Supposedly, you will see incoming packets in terminal/console 2, which are sent from testpmd in terminal/console 3.

The meaning of the command is as follows.

- ```LD_LIBRARY_PATH=./deps/dpdk/install/lib/x86_64-linux-gnu```: ensure the DPDK library built by us is loaded
- ```LD_PRELOAD=./deps/dpdk/install/lib/x86_64-linux-gnu/dpdk/pmds-23.0/librte_mempool_ring.so.23.0:./librte_pmd_rvif.so```: load ```librte_pmd_rvif.so ``` at the execution time
- ```./deps/dpdk/install/bin/dpdk-testpmd```: path to the application
- ```-l 0-1 --proc-type=primary --file-prefix=pmd1```: the arguments passed to DPDK, and this is the default syntax defined by DPDK
- ```--vdev=net_rvif,mac=aa:bb:cc:dd:ee:00,mem=/dev/shm/rvs_shm00```: request DPDK to create an rvif virtual interface, whose mac address is ```aa:bb:cc:dd:ee:00``` and shared memory file is ```/dev/shm/rvs_shm00```; this part is handled by our librte_pmd_rvif.so
- ```--no-pci```: DPDK does not try to look up PCI devices
- ```--```: separator between the arguments passed to DPDK and the testpmd application
- ```--nb-cores 1 --forward-mode=rxonly --txq=1 --rxq=1 --txd=512 --rxd=512 --stats-period=1```: handled by the testpmd application

## Some points

For modularity, we build ```librte_pmd_rvif.so``` as an independent shared library file, and apply it through LD_PRELOAD.

To use testpmd of DPDK, we specify ```--default-library=share``` for the build option of DPDK; otherwise, the DPDK compilation phase statically links the DPDK library and testpmd, and LD_PRELOAD cannot load our librte_pmd_rvif.so for the execution of it.

https://github.com/yasukata/librte_pmd_rvif/blob/eab680c6e205af6f4aa4453c844063e543b0dbf0/mk/dpdk/1.mk#L15

