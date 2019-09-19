Introduction
============

Eiger Detector
--------------

Eiger X-ray hybrid photon counting detectors can produce frames rates in the kilohertz range 
with continuous readout.  The detector can be controlled by using the Dectris Simplon API 
which implements a RESTful API over HTTP.  Images produced by the detector can be captured 
directly from a ZeroMQ stream; these images are compressed using one of the compression 
options avaialble on the detector.  A separate monitor (live view) image can be retrieved 
from the detector using a Simplon HTTP request.  The monitor image is transferred as a TIFF
file (not compressed) and is available at a much reduced rate.

This Eiger Detector software stack provides applications for the following:

* Controlling the detector parameters and acquisition.
* Monitoring the detector status.
* Reading and displaying the live view image.
* Receiving images from the ZeroMQ data stream.
* Manipulating and writing image data to disk (using HDF5).
* Writing associated meta data to disk (using HDF5).
* Controlling the destination of individual images to allow multiple writing applications be present in the deployment.

The ability to alter the destination of individual frames provides scalability of the software 
stack.  As frame rates get higher more writing nodes can be introduced to the system and these 
nodes can reside on different physical hardware.  More details of the scalabililty are provided
in the deployment section below.

All of the applications mentioned above are handled through a single point of control (the 
odin control server) and this provides a simple RESTful API to interact with the whole stack.
The odin control server also serves a set of static HTML pages by default which provide a 
web based GUI that can be used without the need to integrate the software stack into another
framework.  All that is required is a standard browser.  There is also an EPICS based client 
available from Diamond Light Source....... TBC

Odin Detector
-------------

Deployment
----------

