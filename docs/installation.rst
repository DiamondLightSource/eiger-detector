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

As well as the tools and libraries above there are two libraries required for the writing of 
data to disk and for compression of that data should it be required.  The files are saved 
using the HDF5 file format and it is higly recommended to install from source a release of 
the library which is at version 1.10.4 or greater::

    cd <path to source>


Installation Using Docker
-------------------------
