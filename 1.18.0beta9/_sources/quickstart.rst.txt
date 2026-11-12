Quickstart Guide
================

This Quickstart guide will show you how to run a software stack containing
a full set of control applications for an Eiger detector.  The default 
settings will run a simulator in place of real hardware to allow immediate 
demonstration and testing.  Replacing the simulator with a real detector 
requires minimal configuration changes and will be explained in the step
by step instructions.

.. _prereq:

Prerequisites
-------------

The quickstart runs the Eiger applications in a containerized setup.  
`Docker <https://docs.docker.com/install/>`_ is required to run the 
software stack using this guide.  To install from source without 
containers read the :doc:`installation <installation>` instructions.

Step By Step
------------

1) Make sure you have read the :ref:`prereq`
2) Clone the eiger-detector github repository::

    git clone https://github.com/dls-controls/eiger-detector.git

3) Change into the docker directory present in the module::

    cd eiger-detector/docker

4) Build the docker image from the Dockerfile present in that directory::

    sudo docker build --tag eiger .

5) Run the tests ...... TBC
6) Run the eiger simulator and detector applications
7) Open a browser on your host machine and point it to localhost port 8888

You should see the eiger-detector home page.  Click on the Home tab and the page 
should look similar to the image below.

Additional Details
------------------

The Dockerfile contains a long list of build instructions that were executed
when the container image was built (step 4 above).  These lines equate to the
installation steps that would be necessary to build the software stack from 
source, and each step is explained in detail in the :doc:`installation <installation>`
instructions.

This default build of the software stack executes all applications on the same
machine, running four instances each of the FrameProcessor and FrameReceiver 
applications.  It also runs a control and data acquisition simulator ........

ADD INSTRUCTIONS ON REPLACING THE SIMULATOR WITH HARDWARE 
