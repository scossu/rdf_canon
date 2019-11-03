#ifndef _RDF_CANON_H
#define _RDF_CANON_H

#include <libcork/ds.h>
#include <redland.h>

/* * * CONSTANTS * * */

/* Start of subject block.*/
#define CAN_S_START "{"

/* End of subject block.*/
#define CAN_S_END "}"

/* Start of predicate block.*/
#define CAN_P_START "("

/* End of predicate block.*/
#define CAN_P_END ")"

/* Start of object block.*/
#define CAN_O_START "["

/* End of object block.*/
#define CAN_O_END "]"

/* Blank node symbol.*/
#define CAN_BNODE "*"

/** Original subject node.
 *
 * This is an addition to the original logic to address an edge case brought up
 * by prof. Miguel Ceriani with self-referential blank nodes.
 */
#define CAN_ORIG_S "!"

#define CAN_EMPTY "-"


/* * * TYPEDEFS * * */

/**
 * Resizable byte buffer.
 */
typedef struct cork_buffer CAN_Buffer;

/*
 * An ordered, contiguous array of librdf node structs.
 */
typedef cork_array(librdf_node*) CAN_NodeArray;

/*
 * Environment context that gets passed around functions.
 */
typedef struct CAN_Context {
    librdf_world* world;
    librdf_model* model;
    CAN_NodeArray* visited_nodes;
    librdf_node* orig_subj;
} CAN_Context;


/* * * FUNCTION PROTOTYPES * * */

int CAN_canonicize(librdf_world* world, librdf_model* model, CAN_Buffer* buf);

#endif /* _RDF_CANON_H */
