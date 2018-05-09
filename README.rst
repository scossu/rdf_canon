========
RDF Hash
========

This libray generates a canonical serialization and cryptographic hash of a RDF
graph.

The logic is drawn from a `publication
<http://ceur-ws.org/Vol-1259/method2014_submission_1.pdf>`_ by Edzard Höfig and
Ina Schieferdecker.

**Note:** This implementation represents RDF identifiers in their full N3
syntax: fully qualified IRIs are represented with angle brackets, literals
with double quotes, and so on, e.g.::

    <http://ex.org/a> # An IRI
    "Ahoy"^^<http://www.w3.org/2001/XMLSchema#string> # A string literal
    3 # An xsd:int literal

**Note:** there has been further development of this logic since the original
publication. Most notably, prof. Miguel Ceriani discovered two edge cases that
invalidate the logic. This implementation attempts to resolve the first one but
the second one will require further research. See `related discussion
<https://groups.google.com/d/msg/fedora-tech/8pemDHNvbvc/KLp5633jBgAJ>`_.

