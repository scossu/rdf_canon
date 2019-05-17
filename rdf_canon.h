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

/* Generic byte buffer structure with an address and size. * /
typedef struct CAN_Buffer {
    unsigned char* addr;
    size_t sz;
} CAN_Buffer;
 */

/* An ordered, contiguous array of librdf node structs. */
//typedef cork_array(librdf_node*) CAN_node_array;

/* An ordered, contiguous array of librdf node structs. */
typedef cork_array(librdf_node*) CAN_node_array;

/* An ordered, contiguous array of serialize nodes. */
typedef cork_array(struct cork_buffer*) CAN_buffer_array;

//static void print_triple(void* user_data, raptor_statement* triple);

typedef struct CAN_context {
    librdf_world* world;
    raptor_world* raptor_world;
    librdf_model* model;
    CAN_node_array* visited_nodes;
    librdf_node* orig_subj;
} CAN_context;


/* * * FUNCTION PROTOTYPES * * */

//struct cork_buffer* CAN_canonicize(librdf_world* world, librdf_model* model);
int CAN_canonicize(librdf_world* world, librdf_model* model, struct cork_buffer* buf);

int encode_subject(
    CAN_context* ctx, librdf_node* subject, struct cork_buffer* subj_buf
);

int encode_preds(
    CAN_context* ctx, librdf_node* subject, struct cork_buffer* pred_buf
);

int encode_object(
    CAN_context* ctx, librdf_node* object, struct cork_buffer* obj_buf
);

int serialize(
        CAN_context* ctx, librdf_node* node, struct cork_buffer* node_buf);

#endif /* _RDF_CANON_H */
