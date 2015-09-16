Example Module Documentation
----------------------------

.. include:: replace.txt
.. highlight:: cpp

.. heading hierarchy:
   ------------- Chapter
   ************* Section (#.#)
   ============= Subsection (#.#.#)
   ############# Paragraph (no number)

This is a suggested outline for adding new module documentation to |ns3|.
See ``src/click/doc/click.rst`` for an example.

The introductory paragraph is for describing what this code is trying to
model.

For consistency (italicized formatting), please use |ns3| to refer to
ns-3 in the documentation (and likewise, |ns2| for ns-2).  These macros
are defined in the file ``replace.txt``.

Model Description
*****************

The source code for the new module lives in the directory ``src/sdn``.

Add here a basic description of what is being modeled.

Design
======

Briefly describe the software design of the model and how it fits into 
the existing ns-3 architecture. 

Scope and Limitations
=====================

What can the model do?  What can it not do?  Please use this section to
describe the scope and limitations of the model.

References
==========

Add academic citations here, such as if you published a paper on this
model, or if readers should read a particular specification or other work.

Usage
*****

This section is principally concerned with the usage of your model, using
the public API.  Focus first on most common usage patterns, then go
into more advanced topics.

Building New Module
===================

Include this subsection only if there are special build instructions or
platform limitations.

Helpers
=======

What helper API will users typically use?  Describe it here.

Attributes
==========

What classes hold attributes, and what are the key ones worth mentioning?

Output
======

What kind of data does the model generate?  What are the key trace
sources?   What kind of logging output can be enabled?

Advanced Usage
==============

Go into further details (such as using the API outside of the helpers)
in additional sections, as needed.

Examples
========

What examples using this new code are available?  Describe them here.

Troubleshooting
===============

Add any tips for avoiding pitfalls, etc.

Validation
**********

Describe how the model has been tested/validated.  What tests run in the
test suite?  How much API and code is covered by the tests?  Again, 
references to outside published work may help here.

***********************************************************


This page shows the absolute minimum to get libfluid running.

    We are assuming Ubuntu 12.04 for the steps below. Detailed instructions for other distributions (Fedora only for now) are available in other pages: A quick intro to libfluid_base and A quick intro to libfluid_msg.

Install the dependencies:
$ sudo apt-get install autoconf libtool build-essential pkg-config
$ sudo apt-get install libevent-dev libssl-dev

Clone the libfluid repository:
$ git clone https://github.com/OpenNetworkingFoundation/libfluid.git
$ cd libfluid
$ ./bootstrap.sh

The bootstrap script will clone both libraries that form libfluid: libfluid_base, which deals with OpenFlow control channel, and libfluid_msg, which provides classes for building and parsing OpenFlow messages. The bootstraping process will also checkout the repositories to stable versions of both libraries.

Build and install libfluid_base:
$ cd libfluid_base
$ ./configure --prefix=/usr
$ make
$ sudo make install

    For more information, see A quick intro to libfluid_base.

Build and install libfluid_msg:
$ cd libfluid_msg
$ ./configure --prefix=/usr
$ make
$ sudo make install

