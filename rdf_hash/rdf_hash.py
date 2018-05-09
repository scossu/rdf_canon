from hashlib import sha256

from rdflib import URIRef, Literal, BNode

__doc__ = """
Generate a canonical serialization and cryptographic hash of a RDF graph.

From the original paper by Edzard Höfig and Ina Schieferdecker, whose
initials are used to name this class:

http://ceur-ws.org/Vol-1259/method2014_submission_1.pdf

**Note:** This implementation represents RDF identifiers in their full N3
syntax: fully qualified IRIs are represented with angle brackets, literals
with double quotes, and so on, e.g.::

    <http://ex.org/a> # An IRI
    "Ahoy"^^<http://www.w3.org/2001/XMLSchema#string> # A string literal
    3 # An xsd:int literal

**Note:** there has been further development of this logic since the original
publication. Most notably, prof. Miguel Ceriani discovered two edge cases that
invalidate the logic. This implementation attempts to resolve the first one but
the second one will require further research. See
https://groups.google.com/d/msg/fedora-tech/8pemDHNvbvc/KLp5633jBgAJ
"""

S_START = b'{'
"""Start of subject block."""

S_END = b'}'
"""End of subject block."""

P_START = b'('
"""Start of predicate block."""

P_END = b')'
"""End of predicate block."""

O_START = b'['
"""Start of object block."""

O_END = b']'
"""End of object block."""

BNODE = b'*'
"""Blank node symbol."""

SELF_REF_BNODE = b'!'
"""
Self-referential blank node.

This is an addition to the original logic to address an edge case brought up by
prof. Miguel Ceriani with self-referential blank nodes.

    Graph 1::

       _:a ex:p _:c .
       _:b ex:p _:c .
       _:c ex:p _:c .

    Graph 2::

       _:a ex:p _:b .
       _:b ex:p _:a .
       _:c ex:p _:c .

    The two RDF graphs represented are different (and not just for the
    used labels) but the algorithm generates in both cases the following
    string (spaces added for readability)::

       { * [ - ( * [ - ] ) ] } { * [ - ( * [ - ] ) ] } { * [ - ] }

"""


def get_hash(gr, hash_fn=sha256):
    """
    Calculate the hash of an RDF graph.

    This is an optional function. A custom hash function can either be passed
    to this function, or the ``serialize`` function can be used to serialize
    the graph into canonical form without hashing it.

    :param rdflib.Graph gr: Input graph.
    :param function hash_fn: Hash function. Defaults to ``hashlib.sha256``.

    :rtype: bytes
    :return: Graph digest.
    """
    return hash_fn(serialize(gr)).digest()


def serialize(gr):
    """
    Generate a canonical serialization of an RDF graph.

    :param rdflib.Graph gr: Input graph.

    :rtype: bytearray
    :return: Serialized graph.
    """
    subj_strings = []
    res = bytearray()
    for subject in set(gr.subjects()):
        visited_nodes = set()
        subj_strings.append(
                _encode_subject(subject, visited_nodes, gr))

    subj_strings.sort()
    for subj_str in subj_strings:
        res += S_START
        res += subj_str
        res += S_END

    return res


def _encode_subject(subject, visited_nodes, gr):
    """
    Encode a subject node.

    :param subject: Subject node to encode
    :type subject: rdflib.URIRef or rdflib.BNode
    :param list visited_nodes: registry of visited blank nodes.
    :param rdflib.Graph gr: Input graph.

    :rtype: bytes
    :return: Encoded bytestring.
    """
    if isinstance(subject, BNode):
        if subject in visited_nodes:
            return b''
        else:
            visited_nodes.add(subject)
            res = bytearray(BNODE)
    else:
        res = bytearray(subject.n3().encode('utf-8'))
    res += _encode_props(subject, visited_nodes, gr)

    return res


def _encode_props(subject, visited_nodes, gr):
    """
    encode properties (predicates + objects) for a subject.

    :param subject: Subject node with properties to encode
    :type subject: rdflib.URIRef or rdflib.BNode
    :param list visited_nodes: registry of visited blank nodes.
    :param rdflib.Graph gr: Input graph.

    :rtype: bytes
    :return: Encoded bytestring.
    """
    res = bytearray()
    preds = sorted(set(gr.predicates(subject)))
    for pred in preds:
        res += P_START
        res += pred.n3().encode('utf-8')
        objs = sorted([
                _encode_object(obj, visited_nodes, gr)
                for obj in set(gr.objects(subject, pred))])
        for obj in objs:
            res += O_START
            res += obj
            res += O_END
        res += P_END
    return res


def _encode_object(obj, visited_nodes, gr):
    """
    Encode an object node.

    :param rdflib.term.Identifier obj: Object node.
    :param list visited_nodes: registry of visited blank nodes.
    :param rdflib.Graph gr: Input graph.

    :rtype: bytes
    :return: Encoded bytestring.
    """
    if isinstance(obj, BNode):
        return _encode_subject(obj, visited_nodes, gr)
    else:
        # This includes both Literal and URIRef cases.
        return obj.n3().encode('utf-8')

