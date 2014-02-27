Appendix: Generating This Documentation
=======================================

Requirements
------------

This documentation is created with `Sphinx <http://sphinx-doc.org>`_ and `Breathe <http://michaeljones.github.io/breathe/index.html>`_. Here are the required tools:

1. Doxygen is required. `Install <http://www.stack.nl/~dimitri/doxygen/download.html#srcbin>`_ it for your platform.

2. The easiest way to install all the tools is with `Python Package Index (PyPI) <http://pypi.python.org>`_. Just run this and it will install Sphinx, Breathe, and all the required tools if they are not installed::

   $ sudo pip install breathe

3. Otherwise if PyPI is not available, consult Sphinx and Breathe sites for installation instructions and you may need to install these manually:

  - `Sphinx <http://sphinx-doc.org>`_
  - `Breathe <http://michaeljones.github.io/breathe/index.html>`_
  - docutils
  - Pygments


Rendering The Documentation
------------------------------
The main source of the documentation is currently the '''Trac''' pages at https://trac.pjsip.org/repos/wiki/pjsip-doc/index. The copies in SVN are just copies for backup.

To render the documentation as HTML in `_build/html` directory::

  $ cd $PJDIR/doc/pjsip-book
  $ python fetch_trac.py
  $ make
  
To build PDF, run::

  $ make latexpdf


How to Use Integrate Book with Doxygen
--------------------------------------
Quick sample::

  will be rendered like this:
  +++++++++++++++++++++++++++

  This is how to quote a code with syntax coloring:

  .. code-block:: c++

       pj::AudioMediaPlayer *player = new AudioMediaPlayer;
       player->createPlayer("announcement.wav");

  There are many ways to refer a symbol: 

  * A method: :cpp:func:`pj::AudioMediaPlayer::createPlayer()`
  * A method with alternate display: :cpp:func:`a method <pj::AudioMediaPlayer::createPlayer()>`
  * A class :cpp:class:`pj::AudioMediaPlayer`
  * A class with alternate display: :cpp:class:`a class <pj::AudioMediaPlayer>`

  For that links to work, we need to display the link target declaration (a class or method) 
  somewhere in the doc, like this:
  
  .. doxygenclass:: pj::AudioMediaPlayer
        :path: xml
        :members:

  Alternatively we can display a single method declaration like this:

  .. doxygenfunction:: pj::AudioMediaPlayer::createPlayer()
        :path: xml
        :no-link:

  We can also display class declaration with specific members.
  
  For more info see `Breathe documentation <http://michaeljones.github.io/breathe/domains.html>`_

     
.. default-domain:: cpp

will be rendered like this:
+++++++++++++++++++++++++++

This is how to quote a code with syntax coloring:

.. code-block:: c++

       pj::AudioMediaPlayer *player = new AudioMediaPlayer;
       player->createPlayer("announcement.wav");

There are many ways to refer a symbol: 

* A method: :cpp:func:`pj::AudioMediaPlayer::createPlayer()`
* A method with alternate display: :cpp:func:`a method <pj::AudioMediaPlayer::createPlayer()>`
* A class :cpp:class:`pj::AudioMediaPlayer`
* A class with alternate display: :cpp:class:`a class <pj::AudioMediaPlayer>`

For that links to work, we need to display the link target declaration (a class or method) somewhere in the doc, like this:
  
   .. doxygenclass:: pj::AudioMediaPlayer
        :path: xml
        :members:

Alternatively we can display a single method declaration like this:

   .. doxygenfunction:: pj::AudioMediaPlayer::createPlayer()
        :path: xml
        :no-link:

We can also display class declaration with specific members.

For more info see `Breathe documentation <http://michaeljones.github.io/breathe/domains.html>`_



