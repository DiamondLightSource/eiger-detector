.. Eiger Detector documentation master file, created by
   sphinx-quickstart on Wed Sep 18 13:15:15 2019.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

Eiger Detector
==============

Eiger Detector is a software solution for the control of and acquisition of data
from a Dectris Eiger detector.  This module is built on top of the detector
framework `odin-detector`_ which has been created for ease of integration, scalability,
and highly efficient data acquisition.  The module does not require a particular
control middleware to act as a client, providing instead a RESTful API over HTTP.

A web based GUI is supplied with the module as a default client to allow immediate testing
once the software has been built.

There is also a separate software repository `ADOdin`_ that contains an EPICS client application 
which can be used to control this software stack.


Documentation
-------------

The following documentation is available for Eiger Detector:

* The :doc:`introduction <introduction>` section provides an overview of the various software components and a guide to the odin-detector framework.
* The :doc:`quickstart <quickstart>` guide will show you how to run up the software with very little detail on each step.  This guide focuses on getting running quickly and uses a simulator to show the software working without the need for any hardware.
* The :doc:`installation <installation>` instructions explain in detail the steps necessary to install the software module on a native linux OS.
* The :doc:`user guide <userguide>` demonstrates how to operate the detector with the software.
* The :doc:`reference <reference>` documentation provides full details of the interactions between components and can be used to create a client for control of the Eiger Detector module.


Other
-----

.. toctree::
  :maxdepth: 2
  :caption: Contents:

  introduction
  quickstart
  installation
  userguide
  reference


Indices and tables
==================

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`

.. _odin-detector:
    https://github.com/odin-detector

.. _ADOdin:
    https://github.com/DiamondLightSource/ADOdin
