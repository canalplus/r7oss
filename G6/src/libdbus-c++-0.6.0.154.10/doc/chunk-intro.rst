.. fragment: general description of a component
   expected by ./overview.rst

.. .....................................................................
   The first sentence should act as a short description for the component and be emphasized (=between stars).
   You can also add the rationale, aka explain why this component exists.
   (do not confuse with the rationale of its API)
   There can be several paragraphs explaining what the component is about, but no sections.
   .....................................................................

*libdbus-c++ offers a c++ object oriented API which simplifies the applications' usage of the DBus communication system*

This library is a wrapper around the C library needed to access the DBus communication system. It hides the relative complexity of the C library by providing a tool which, starting from a description of the DBus interface provided by the application, generates most of the boilerplate code specific to the usage of the DBus system. The result is a simplified object oriented API, closer to the application's domain.

