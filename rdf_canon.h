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

#define CAN_EMPTY \x00


/* * * TYPEDEFS * * */

/* Generic byte buffer structure with an address and size. * /
typedef struct CAN_Buffer {
    unsigned char* addr;
    size_t sz;
} CAN_Buffer;
 */

/* An ordered, contiguous array of librdf node structs. */
//typedef cork_array(librdf_node*) CAN_node_array;

//static void print_triple(void* user_data, raptor_statement* triple);


/* * * FUNCTION PROTOTYPES * * */

//struct cork_buffer* CAN_canonicize(librdf_world* world, librdf_model* model);
int CAN_canonicize(librdf_world* world, librdf_model* model, struct cork_buffer* buf);

struct cork_buffer* encode_subject(librdf_model* model, librdf_node* subject,
        librdf_node* orig_subj, struct cork_hash_table* visited_nodes);

#endif /* _RDF_CANON_H */
