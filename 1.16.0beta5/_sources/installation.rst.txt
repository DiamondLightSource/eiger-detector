Installation
============

Installing From Source
----------------------

Installation from source requires several tools and external packages to be available as 
well as the building and installation of three Odin and Eiger specific software packages.
This guide assumes the the installation is taking place on a CentOS 7 (or RHEL 7) operating
system.  The source can also be built for other operating systems;
any operating system specific package installation commands should simply be replaced with
the equivalent command for the chosen operating system.

Prerequisites
*************

The Odin and Eiger software requires standard C, C++ and python development tools to be 
present.  The following libraries and tools are also required:

* boost (add link)
* cmake (add link)
* log4cxx (add link)
* ZeroMQ (add link)
* pcap (add link)
* Cython (add link)
* Python Pip (add link)
* Python Virtual Environment (add link)

All of the above packages can be installed from the default package manager Yum (add link) 
with the following commands::

    yum install -y epel-release
    yum groupinstall -y 'Development Tools'
    yum install -y boost boost-devel
    yum install -y cmake
    yum install -y log4cxx-devel
    yum install -y zeromq3-devel
    yum install -y libpcap-devel
    yum install -y python2-pip
    yum install -y python-virtualenv
    yum install -y Cython

For all remaining sections of this installation guide it is assumed that source files are 
going to be located in the directory::

    /home/eiger/src

and the installed files will be located in the directory::

    /home/eiger/prefix

File Writing Libraries
**********************

As well as the tools and libraries mentioned above there are two libraries required for the writing of 
data to disk and for compression of that data should it be required.  The files are saved 
using the HDF5 file format and it is higly recommended to install from source a release of 
the HDF5 library which is at version 1.10.4 or greater::

    cd /home/eiger/src
    curl -L -O https://www.hdfgroup.org/ftp/HDF5/releases/hdf5-1.10/hdf5-1.10.4/src/hdf5-1.10.4.tar.bz2
    tar -jxf hdf5-1.10.4.tar.bz2
    mkdir -p /home/eiger/src/build-hdf5-1.10
    cd /home/eiger/src/build-hdf5-1.10
    /home/eiger/src/hdf5-1.10.4/configure --prefix=/home/eiger/prefix
    make >> /home/eiger/src/hdf5build.log 2>&1
    make install

Odin-Data supports writing compressed data files using the Blosc high performance compressor optimised for
binary data.  The Blosc compressor is supplied as an optional odin-data plugin and requires the Blosc 
compression library to be installed.  The following instructions will install the Blosc library::

    cd /home/eiger/src
    curl -L -s -o c-blosc-1.14.2.tar.gz -O https://github.com/Blosc/c-blosc/archive/v1.14.2.tar.gz
    tar -zxf c-blosc-1.14.2.tar.gz
    mkdir -p /home/eiger/src/build-blosc
    cd /home/eiger/src/build-blosc
    cmake /home/eiger/src/c-blosc-1.14.2/ -DCMAKE_INSTALL_PREFIX=/home/eiger/prefix
    make >> /home/eiger/src/bloscbuild.log 2>&1
    make install

Installing Odin-Control
***********************

*Section TBD*

Installing Odin-Data
********************

*Section TBD*

Installing Eiger-Detector
*************************

*Section TBD*

Installation Using Docker
-------------------------

*Section TBD*
