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

/* Resizable byte buffer. */
typedef struct cork_buffer CAN_Buffer;

/* An ordered, contiguous array of librdf node structs. */
typedef cork_array(librdf_node*) CAN_NodeArray;

/* An ordered, contiguous array of serialize nodes. */
typedef cork_array(CAN_Buffer*) CAN_BufferArray;

//static void print_triple(void* user_data, raptor_statement* triple);

typedef struct CAN_context {
    librdf_world* world;
    raptor_world* raptor_world;
    librdf_model* model;
    CAN_NodeArray* visited_nodes;
    librdf_node* orig_subj;
} CAN_context;


/* * * FUNCTION PROTOTYPES * * */

//CAN_Buffer* CAN_canonicize(librdf_world* world, librdf_model* model);
int CAN_canonicize(librdf_world* world, librdf_model* model, CAN_Buffer* buf);

int encode_subject(
    CAN_context* ctx, librdf_node* subject, CAN_Buffer* subj_buf
);

int encode_preds(
    CAN_context* ctx, librdf_node* subject, CAN_Buffer* pred_buf
);

int encode_object(
    CAN_context* ctx, librdf_node* object, CAN_Buffer* obj_buf
);

int serialize(
        CAN_context* ctx, librdf_node* node, CAN_Buffer* node_buf);

#endif /* _RDF_CANON_H */
