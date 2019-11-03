#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>

#include <libcork/core.h>
#include <libcork/ds.h>
#include <redland.h>

#include "rdf_canon.h"

/*
 * Local typedefs.
 */

/*
 * A pair made by a librdf node and its turtle serialization, as a buffer.
 *
 * This is convenient to use in node sorting functions where a node needs
 * to be serialized in order to be compared for sorting, and wants to be kept
 * with the original node for future use in an iteration.
 */
typedef struct CAN_NodePair {
    librdf_node* node;
    CAN_Buffer* s_node;
} CAN_NodePair;

/*
 * An ordered, contiguous array of node pairs.
 */
typedef cork_array(CAN_NodePair*) CAN_NodePairArray;


/*
 * Static prototypes.
 */

//static void npair_array_init_f(void* el, void* user_data);

static void npair_array_done_f(void* el, void* user_data);

static int encode_subject(
        const CAN_Context* ctx, librdf_node* subject, CAN_Buffer* subj_buf);

static int encode_preds(
        const CAN_Context* ctx, librdf_node* subject, CAN_Buffer* pred_buf);

static int encode_pred_with_objects(
        const CAN_Context* ctx, librdf_node* pred_node, librdf_node* subject,
        CAN_Buffer* pred_buf_cur);

/* TODO Move to tests.
typedef cork_array(char*) CHArray;
static inline int test_array_insert_ordered(CHArray* array, char* ins_item);
void test_insert();
*/

static inline int nparray_insert_ordered(
        const CAN_Context* ctx, CAN_NodePairArray* array, librdf_node* ins_item);

static inline int serialize(
        const CAN_Context* ctx, librdf_node* node, CAN_Buffer* node_buf);

static inline bool subj_array_contains(
        const CAN_NodeArray* subjects, librdf_node* el);

static void print_bytes(const unsigned char *bs, const size_t size);


/*
 * Public API.
 */

CAN_Context* CAN_context_new()
{
    CAN_Context* ctx = cork_new(CAN_Context);

    ctx->world = librdf_new_world();
    librdf_world_open(ctx->world);

    ctx->storage = librdf_new_storage(ctx->world, "memory", NULL, NULL);

    ctx->model = librdf_new_model(ctx->world, ctx->storage, NULL);

    ctx->visited_nodes = cork_new(CAN_NodeArray);
    cork_array_init(ctx->visited_nodes);

   return(ctx);
}


void CAN_context_free(CAN_Context* ctx)
{
    cork_array_done(ctx->visited_nodes);
    cork_delete(CAN_NodeArray, ctx->visited_nodes);

    librdf_free_model(ctx->model);
    printf("Freed model.\n");
    librdf_free_storage(ctx->storage);
    printf("Freed storage.\n");

    librdf_free_world(ctx->world);
    printf("Freed world.\n");

    cork_delete(CAN_Context, ctx);
}


int CAN_canonicize(CAN_Context* ctx, CAN_Buffer* buf)
{
    CAN_NodePairArray subj_array;
    cork_array_init(&subj_array);
    cork_array_set_done(&subj_array, &npair_array_done_f);

    CAN_Buffer* subj_tmp_buf = cork_new(CAN_Buffer);

    librdf_statement* q_stmt = librdf_new_statement(ctx->world);
    librdf_stream* stream = librdf_model_find_statements(ctx->model, q_stmt);
    librdf_free_statement(q_stmt);
    if(!stream)  {
        fprintf(stderr, "librdf_model_get_targets returned no iterator.\n");
        return(1);
    }
    cork_buffer_init(buf);

    // Build array of ordered, unique subject nodes.
    librdf_statement* stmt;
    while(!librdf_stream_end(stream)) {
        //printf("i: %lu\n", i);

        // Get the statement (triple).
        stmt = librdf_stream_get_object(stream);
        if(!stmt) {
            fprintf(stderr, "librdf_stream_get_statement returned null\n");
            break;
        }
        fputs("matched statement: ", stdout);
        librdf_statement_print(stmt, stdout);
        fputc('\n', stdout);

        librdf_node* subj = librdf_statement_get_subject(stmt);
        nparray_insert_ordered(
                ctx, &subj_array, subj);

        librdf_stream_next(stream);
    }

    librdf_free_statement(stmt);
    librdf_free_stream(stream);

    // Serialize the subjects and, iteratively, the whole statements.
    for (size_t i = 0; i < subj_array.size; i++){
        librdf_node* subject = cork_array_at(&subj_array, i)->node;
        cork_buffer_init(subj_tmp_buf);
        /* Original subject that gets passed down the call stack. */
        ctx->orig_subj = subject;

        encode_subject(ctx, subject, subj_tmp_buf);

        printf("Canonicized subject: ");
        fwrite(subj_tmp_buf->buf, 1, subj_tmp_buf->size, stdout);
        fputc('\n', stdout);

        cork_buffer_append(buf, CAN_S_START, 1);
        cork_buffer_append_copy(buf, subj_tmp_buf);
        cork_buffer_append(buf, CAN_S_END, 1);

    }

    // Free the source pointers first...
    cork_buffer_done(subj_tmp_buf);
    cork_free(subj_tmp_buf, sizeof(CAN_Buffer));
    printf("Freed encoded subjects.\n");

    // ...then the ordered array.
    cork_array_done(&subj_array);
    printf("Freed subjects array.\n");

    return(0);
}

/*
 * Static serialize functions.
 */

static int encode_subject(
        const CAN_Context* ctx, librdf_node* subject, CAN_Buffer* subj_buf)
{
    CAN_Buffer pred_buf;

    if (librdf_node_get_type(subject) == LIBRDF_NODE_TYPE_BLANK) {
        if (subj_array_contains(ctx->visited_nodes, subject)) {
            if (librdf_node_equals(subject, ctx->orig_subj)) {
                cork_buffer_set(subj_buf, CAN_ORIG_S, 1);
                return(0);
            } else {
                cork_buffer_clear(subj_buf);
            }
        } else {
            cork_array_append(ctx->visited_nodes, subject);
            cork_buffer_set(subj_buf, CAN_BNODE, 1);
        }
    } else {
        serialize(ctx, subject, subj_buf);
        printf("Encoded node: %s\n", (char*)subj_buf->buf);
    }
    cork_buffer_init(&pred_buf);
    encode_preds(ctx, subject, &pred_buf);
    cork_buffer_append_copy(subj_buf, &pred_buf);

    cork_buffer_done(&pred_buf);

    return(0);
}


static int encode_preds(
        const CAN_Context* ctx, librdf_node* subject, CAN_Buffer* pred_buf)
{
    // Ordered, de-duplicated array of predicate nodes.
    CAN_NodePairArray _pred_array;
    CAN_NodePairArray* pred_array = &_pred_array;

    librdf_iterator* props_it = librdf_model_get_arcs_out(ctx->model, subject);

    // Build ordered and unique array of predicates.
    cork_array_init(pred_array);
    cork_array_set_done(pred_array, &npair_array_done_f);
    while(!librdf_iterator_end(props_it)){
        // Librdf returns duplicate predicates, we need to deduplicate them.
        nparray_insert_ordered(
                ctx, pred_array, librdf_iterator_get_object(props_it));
        librdf_iterator_next(props_it);
    }
    librdf_free_iterator(props_it);

    printf("Got %lu predicates.\n", pred_array->size);

    // Build byte buffer from serialized predicates + objects.
    for(size_t i = 0; i < pred_array->size; i++) {
        printf("Processing predicate #%lu of %lu.\n", i, pred_array->size);
        printf("Ordering predicate #%lu.\n", i);
        encode_pred_with_objects(
                ctx, cork_array_at(pred_array, i)->node,
                subject, cork_array_at(pred_array, i)->s_node);

        // Append start of predicate block.
        cork_buffer_append(pred_buf, CAN_P_START, 1);
        // Append ordered predicate buffer.
        // TODO: Add directly in encode_pred_with_objects?:
        cork_buffer_append_copy(pred_buf, cork_array_at(pred_array, i)->s_node);
        // Append end of predicate block.
        cork_buffer_append(pred_buf, CAN_P_END, 1);
        printf("Canonicized predicate: %s\n", (char*)pred_buf->buf);
    }
    printf("Done serializing predicates.\n");

    // Free ordered array.
    cork_array_done(pred_array);

    return(0);
}


static int encode_pred_with_objects(
        const CAN_Context* ctx, librdf_node* pred_node, librdf_node* subject,
        CAN_Buffer* pred_buf_cur)
{
    // Ordered, de-duplicated array of object nodes.
    CAN_NodePairArray _obj_array;
    CAN_NodePairArray* obj_array = &_obj_array;

    // temporary buffer for individual serialized nodes that are added to
    // string iteratively.
    //CAN_Buffer* obj_tmp_buf = cork_new(CAN_Buffer);

    librdf_iterator* obj_it = librdf_model_get_targets(
            ctx->model, subject, pred_node);

    // Build ordered array of serialized objects. The nodes are serialized
    // as they are sorted and saved for the next loop.
    cork_array_init(obj_array);
    cork_array_set_done(obj_array, &npair_array_done_f);
    while (!librdf_iterator_end(obj_it)){
        // Librdf returns duplicate predicates, we need to deduplicate them.
        nparray_insert_ordered(
                ctx, obj_array, librdf_iterator_get_object(obj_it));
        librdf_iterator_next(obj_it);
    }
    librdf_free_iterator(obj_it);

    printf("Got %lu objects.\n", obj_array->size);

    /*
    printf("Predicate node to encode at %p.\n", (void*)pred_node);
    printf("N3 predicate: %s\n", (char*)(pred_buf_cur)->buf);
    */

    // Build byte buffer from serialized objects.
    for(size_t i = 0; i < obj_array->size; i++) {
        CAN_NodePair* npair_cur = cork_array_at(obj_array, i);
        if (librdf_node_get_type(npair_cur->node) == LIBRDF_NODE_TYPE_BLANK) {
            encode_subject(ctx, npair_cur->node, npair_cur->s_node);
        }
        printf("Canonicized object: ");
        print_bytes(npair_cur->s_node->buf, npair_cur->s_node->size);
        // FIXME This messes up the string in Valgrind, go figure...
        //cork_buffer_ensure_size(pred_buf_cur, (npair_cur->s_node->size) + 2);

        // Append serialized object to object buffer array.
        cork_buffer_append(pred_buf_cur, CAN_O_START, 1);
        printf("Added start block.\n");
        cork_buffer_append_copy(pred_buf_cur, npair_cur->s_node);
        printf("Added object buffer.\n");
        cork_buffer_append(pred_buf_cur, CAN_O_END, 1);
        printf("Added end block.\n");
    }
    printf("Done serializing objects.\n");

    // Free ordered array.
    cork_array_done(obj_array);
    printf("Freed obj_array.\n");
    printf("Serialized predicate with object: ");
    print_bytes(pred_buf_cur->buf, pred_buf_cur->size);

    return(0);
}


/*
 * Static helper functions.
 */

/**
 * Initialize underlying NodePair element in array.
 */
/*static void npair_array_init_f(void* el, void* user_data)
{
    CAN_NodePair* np = (CAN_NodePair*)el;
    CAN_Context* ctx = (CAN_Context*)user_data;

    serialize(ctx, np->node, np->s_node);
}
*/

/**
 * Free the underlying array element.
 */
static void npair_array_done_f(void* user_data, void* el)
{
    if (el != NULL) {
        CAN_NodePair* np = ((CAN_NodePair**)el)[0];
        printf("Freeing elements in NodePair @ %p\n", np);
        /*
        if (np->node != NULL) {
            librdf_free_node(np->node);
        }
        */
        if (np->s_node != NULL) {
            printf("Freeing buffer in NodePair @ %p: %s\n", np->s_node, (char*)np->s_node->buf);
            cork_buffer_free(np->s_node);
        }
        //cork_delete(CAN_NodePair, np);
    }
}


/**
 * Allocate a new CAN_NodePair and initialize its elements.
 */
static inline CAN_NodePair* np_new_from_node(
        const CAN_Context* ctx, librdf_node* node)
{
    CAN_NodePair* np = cork_new(CAN_NodePair);
    np->node = node;
    np->s_node = cork_buffer_new();
    serialize(ctx, np->node, np->s_node);
    printf("Allocated new NodePair @ %p\n", np);
    printf("Allocated buffer in NodePair @ %p\n", np->s_node);

    return(np);
}


/**
 * Insert a node in an ordered array of NodePairs.
 *
 * The node is serialized and store in the `s_node` part of the struct.
 *
 * Deduplication and sorting of the node is done based on the serialized node
 * string.
 *
 * The array items must be freed with `cork_delete(CAN_NodePair, <item>)`.
 */
static inline int nparray_insert_ordered(
        const CAN_Context* ctx, CAN_NodePairArray* array, librdf_node* ins_item)
{
    CAN_NodePair* new_np = np_new_from_node(ctx, ins_item);

    if (array->size == 0) {
        printf("Inserting first element in array: %s.\n", (char*)new_np->s_node->buf);
        cork_array_append(array, new_np);
        //printf("new_np: %p array np: %p\n", new_np, cork_array_at(array, 0));
        //printf("new_np->s_node: %p array np->s_node: %p\n", new_np->s_node, cork_array_at(array, 0)->s_node);
        return(0);
    }

    for (unsigned int i = 0; i < array->size; i++) {
        CAN_Buffer* check_item = cork_array_at(array, i)->s_node;
        printf("Check item @: %p\n", check_item->buf);
        int comp = memcmp(
                new_np->s_node->buf, check_item->buf,
                MAX(new_np->s_node->size, check_item->size));
        printf("Compare %s with %s.\n", (char*)new_np->s_node->buf, (char*)check_item->buf);
        printf("Comp: %d.\n", comp);
        if (comp == 0) {
            // Term is duplicate. Exit without inserting.
            printf("Duplicate term. Skipping.\n");
            return(0);

        } else if (comp < 0) {
            // Inserted item is greater than current one. Inserting after.
            // Make room in the array for shuffling and for the new element.
            cork_array_ensure_size(array, array->size + 1);
            array->size++;
            printf("i = %u\n", i);
            // Shift all item past this one by one slot.
            unsigned int j;
            for(j = array->size - 1; j > i; j--) {
                printf("j = %u\n", j);
                cork_array_at(array, j) = cork_array_at(array, j - 1);
            }
            printf("Inserting at position %d.\n", j + 1);
            cork_array_at(array, j) = new_np;
            printf("Done shifting ordered array items.\n");
            printf("New ordered array size: %lu.\n", cork_array_size(array));
            return(0);

        } else if (i == array->size - 1){
            printf("Inserting at the end.\n");
            cork_array_append(array, new_np);
            return(0);

        }
        // If comp > 0 and not the end, continue until a greater or equal term
        // is found.
    }
    printf("*** Could not insert element in array!\n");
    exit(1);
}


/* TODO Move to tests.
static inline void list(CHArray* a)
{
    for(size_t i = 0; i < cork_array_size(a); i++) {
        printf("Element %lu: %s\n", i, cork_array_at(a, i));
    }
}

void test_insert()
{
    CHArray _a;
    CHArray* a = &_a;
    cork_array_init(a);

    char *str1 = "a", *str2 = "b", *str3 = "b", *str4 = "c", *str5 = "9";

    printf("Inserting %s\n", str3);
    test_array_insert_ordered(a, str3);
    list(a);
    printf("Inserting %s\n", str1);
    test_array_insert_ordered(a, str1);
    list(a);
    printf("Inserting %s\n", str2);
    test_array_insert_ordered(a, str2);
    list(a);
    printf("Inserting %s\n", str4);
    test_array_insert_ordered(a, str4);
    list(a);
    printf("Inserting %s\n", str5);
    test_array_insert_ordered(a, str5);
    list(a);
}

static inline int test_array_insert_ordered(CHArray* array, char* ins_item)
{
    if (array->size == 0) {
        printf("Inserting first element in array: %s.\n", ins_item);
        cork_array_append(array, ins_item);
        //printf("new_np: %p array np: %p\n", new_np, cork_array_at(array, 0));
        //printf("new_np->s_node: %p array np->s_node: %p\n", new_np->s_node, cork_array_at(array, 0)->s_node);
        return(0);
    }

    for (unsigned int i = 0; i < array->size; i++) {
        char* check_item = cork_array_at(array, i);
        int comp = strcmp(ins_item, check_item);
        printf("Compare %s with %s.\n", ins_item, check_item);
        printf("Comp: %d.\n", comp);
        if (comp == 0) {
            // Term is duplicate. Exit without inserting.
            printf("Duplicate term. Skipping.\n");
            return(0);
        } else if (comp < 0) {
            // Inserted item is less than current one. Inserting before.

            // Make room in the array for shuffling and for the new element.
            cork_array_ensure_size(array, array->size + 1);
            array->size++;
            printf("i = %u\n", i);
            // Shift all item past this one by one slot.
            unsigned int j;
            for(j = array->size - 1; j > i; j--) {
                printf("j = %u\n", j);
                cork_array_at(array, j) = cork_array_at(array, j - 1);
            }
            printf("Inserting at position %d.\n", j + 1);
            cork_array_at(array, j) = ins_item;
            printf("Done shifting ordered array items.\n");
            printf("New ordered array size: %lu.\n", cork_array_size(array));
            return(0);
        } else if (i == array->size - 1){
            printf("Inserting at the end.\n");
            cork_array_append(array, ins_item);
            return(0);
        }
        // If comp > 0 and not the end, continue until a greater or equal term is found.
    }
    printf("*** Could not insert element in array!\n");
    exit(1);
}
*/


static int inline serialize(
        const CAN_Context* ctx, librdf_node* node, CAN_Buffer* node_buf)
{
    raptor_iostream* iostr = raptor_new_iostream_to_string(
            librdf_world_get_raptor(ctx->world), &(node_buf->buf),
            &(node_buf->size), malloc);
    librdf_node_write(node, iostr);
    raptor_iostream_write_end(iostr);
    raptor_free_iostream(iostr);

    return(0);
}


static inline bool subj_array_contains(
        const CAN_NodeArray* subjects, librdf_node* el)
{
    size_t i;
    size_t ct = cork_array_size(subjects);
    for(i = 0; i < ct; i++)
    {
        if (librdf_node_equals(cork_array_at(subjects, i), el) != 0)
            return(true);
    }
    return(false);
}

/**
 * Print a byte string of a given length in a human-readable format.
 *
 * The string is printed in Python style: printable characters are output
 * literally, and non-printable ones as hex sequences.
 */
static void print_bytes(const unsigned char *bs, const size_t size) {
    for (size_t i = 0; i < size; i++) {
        if isprint(bs[i]) {
            fputc(bs[i], stdout);
        } else {
            printf("\\x%02x", bs[i]);
        }
    }
    printf("\n");
}
