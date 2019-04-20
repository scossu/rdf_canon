#ifndef _RDF_CANON_H
#define _RDF_CANON_H

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

#define CAN_EMPTY ""


/* * * TYPEDEFS * * */

/* A single token representing a term. */
typedef unsigned char CAN_Token;

/* Generic byte buffer structure with an address and size. */
typedef struct CAN_Buffer {
    unsigned char* addr;
    size_t sz;
} CAN_Buffer;

//static void print_triple(void* user_data, raptor_statement* triple);


/* * * FUNCTION PROTOTYPES * * */

int CAN_canonicize(const unsigned char const * graph, CAN_Buffer* buf);

#endif /* _RDF_CANON_H */
