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
 * Static prototypes.
 */

static int encode_subject(
        const CAN_context* ctx, librdf_node* subject, CAN_Buffer* subj_buf);

static int encode_preds(
        const CAN_context* ctx, librdf_node* subject, CAN_Buffer* pred_buf);

static int encode_pred_with_objects(
        const CAN_context* ctx, librdf_node* pred_node, librdf_node* subject,
        CAN_Buffer* pred_buf_cur);

static int encode_object(
        const CAN_context* ctx, librdf_node* object, CAN_Buffer* obj_buf);

static inline int array_insert_ordered(
        CAN_NodeArray* array, librdf_node* ins_item);

static inline int serialize(
        const CAN_context* ctx, librdf_node* node, CAN_Buffer* node_buf);

static inline bool subj_array_contains(
        const CAN_NodeArray* subjects, librdf_node* el);

static void print_bytes(const unsigned char *bs, const size_t size);


/*
 * Public API.
 */

int CAN_canonicize(
        librdf_world* world, librdf_model* model, CAN_Buffer* buf)
{
    size_t i, subj_sz, subj_buf_sz;

    CAN_context _ctx;
    CAN_context* ctx = &_ctx;

    CAN_NodeArray visited_nodes;
    ctx->visited_nodes = &visited_nodes;

    ctx->world = world;
    ctx->model = model;

    size_t capacity = 0;
    /* Initialize a buffer array for the subjects. */
    CAN_NodeArray _subjects;
    CAN_NodeArray* subjects = &_subjects;
    CAN_BufferArray _ser_subjects;
    CAN_BufferArray* ser_subjects = &_ser_subjects;

    librdf_stream* stream;
    librdf_statement *stmt, *q_stmt;
    librdf_node* subject;

    cork_array_init(subjects);
    cork_array_init(ser_subjects);
    q_stmt = librdf_new_statement(ctx->world);
    stream = librdf_model_find_statements(ctx->model, q_stmt);
    librdf_free_statement(q_stmt);
    if(!stream)  {
        fprintf(stderr, "librdf_model_get_targets failed to return iterator for searching\n");
        return(1);
    }
    cork_buffer_init(buf);

    /* Canonicized subject as a byte buffer. */
    //cork_array_ensure_size(subjects, librdf_model_size(model));
    subj_buf_sz = librdf_model_size(ctx->model);
    CAN_Buffer* subj_buf = cork_malloc(
            sizeof(CAN_Buffer) * subj_buf_sz);
    i = 0;
    while(!librdf_stream_end(stream)) {
        cork_array_init(ctx->visited_nodes);
        cork_buffer_init(subj_buf + i);
        //printf("i: %lu\n", i);

        /* Get the statement (triple) */
        stmt = librdf_stream_get_object(stream);
        if(!stmt) {
            fprintf(stderr, "librdf_stream_get_statement returned null\n");
            break;
        }
        /*
        fputs("matched statement: ", stdout);
        librdf_statement_print(stmt, stdout);
        fputc('\n', stdout);
        */
        subject = librdf_statement_get_subject(stmt);
        fputs("matched subject: ", stdout);
        librdf_node_print(subject, stdout);
        fputc('\n', stdout);

        if(!subj_array_contains(subjects, subject)) {
            /* Original subject that gets passed down the call stack. */
            ctx->orig_subj = subject;

            encode_subject(ctx, subject, (subj_buf + i));

            printf("Canonicized subject: ");
            fwrite((subj_buf + i)->buf, 1, (subj_buf + i)->size, stdout);
            fputc('\n', stdout);

            cork_array_append(subjects, subject);
            array_insert_ordered(ser_subjects, subj_buf + i);

            capacity += (subj_buf + i)->size + 2;
        } else {
            printf("Duplicate subject: ");
            librdf_node_print(subject, stdout);
            fputc('\n', stdout);
        }

        cork_array_done(ctx->visited_nodes);
        librdf_free_node(subject);

        i++;
        librdf_stream_next(stream);
    }

    librdf_free_statement(stmt);
    cork_array_done(subjects);

    cork_buffer_ensure_size(buf, capacity);
    subj_sz = cork_array_size(ser_subjects);
    printf("Number of serialized subjects: %lu\n", subj_sz);
    for (i = 0; i < subj_sz; i++){
        //printf("i: %lu\n", i);
        CAN_Buffer* el = cork_array_at(ser_subjects, i);
        printf("Buffer in array: ");
        fwrite(el->buf, 1, el->size, stdout);
        fputc('\n', stdout);
        cork_buffer_append(buf, CAN_S_START, 1);
        cork_buffer_append_copy(buf, el);
        cork_buffer_append(buf, CAN_S_END, 1);

        cork_buffer_done(el);
        el = NULL;
    }

    // Free the source pointers first...
    for (i = 0; i < subj_buf_sz; i++) {
        if (subj_buf + i != NULL) {
            printf("Freeing subject buffer item at %p.\n", (void*)subj_buf + i);
            cork_buffer_done(subj_buf + i);
        }
    }
    printf("Freeing subject buffer at %p.\n", (void*)subj_buf);
    cork_free(subj_buf, sizeof(CAN_Buffer) * subj_buf_sz);
    printf("Freed encoded subjects.\n");

    // ...then the ordered array.
    cork_array_done(ser_subjects);
    printf("Freed encoded subjects array.\n");

    librdf_free_stream(stream);
    ctx = NULL;

    return(0);
}

/*
 * Static serialize functions.
 */

static int encode_subject(
        const CAN_context* ctx, librdf_node* subject, CAN_Buffer* subj_buf)
{
    CAN_Buffer _pred_buf;
    CAN_Buffer* pred_buf = &_pred_buf;

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
    cork_buffer_init(pred_buf);
    encode_preds(ctx, subject, pred_buf);
    cork_buffer_append_copy(subj_buf, pred_buf);

    cork_buffer_done(pred_buf);

    return(0);
}


static int encode_preds(
        const CAN_context* ctx, librdf_node* subject, CAN_Buffer* pred_buf)
{
    // Ordered, de-duplicated array of predicate nodes.
    CAN_NodeArray _pred_array;
    CAN_NodeArray* pred_array = &_pred_array;

    // temporary buffer for individual serialized nodes that are added to
    // string iteratively.
    CAN_Buffer* pred_tmp_buf = cork_new(CAN_Buffer);

    librdf_iterator* props_it = librdf_model_get_arcs_out(ctx->model, subject);

    // Build ordered and unique array of predicates.
    cork_array_init(pred_array);
    while(!librdf_iterator_end(props_it)){
        // Librdf returns duplicate predicates, we need to deduplicate them.
        array_insert_ordered(
                pred_array, librdf_iterator_get_object(props_it));
        librdf_iterator_next(props_it);
    }
    librdf_free_iterator(props_it);

    printf("Got %lu predicates.\n", pred_array->size);

    // Build byte buffer from serialized predicates + objects.
    for(size_t i = 0; i < pred_array->size; i++) {
        printf("Processing predicate #%lu of %lu.\n", i, pred_array->size);
        cork_buffer_init(pred_tmp_buf);
        printf("Ordering predicate #%lu.\n", i);
        encode_pred_with_objects(
                ctx, cork_array_at(pred_array, i), subject, pred_tmp_buf);

        // Append start of predicate block.
        cork_buffer_append(pred_buf, CAN_P_START, 1);
        // Append ordered predicate buffer.
        // TODO: Add directly in encode_pred_with_objects?:
        cork_buffer_append_copy(pred_buf, pred_tmp_buf);
        // Append end of predicate block.
        cork_buffer_append(pred_buf, CAN_P_END, 1);
        printf("Canonicized predicate: %s\n", (char*)pred_buf->buf);
    }
    printf("Done serializing predicates.\n");

    // Free temp buffer and its pointer.
    cork_buffer_done(pred_tmp_buf);
    cork_delete(CAN_Buffer, pred_tmp_buf);
    // Free ordered array.
    cork_array_done(pred_array);

    return(0);
}


static int encode_pred_with_objects(
        const CAN_context* ctx, librdf_node* pred_node, librdf_node* subject,
        CAN_Buffer* pred_buf_cur)
{
    // Ordered, de-duplicated array of object nodes.
    CAN_NodeArray obj_array_s;
    CAN_NodeArray* obj_array = &obj_array_s;

    // temporary buffer for individual serialized nodes that are added to
    // string iteratively.
    CAN_Buffer* obj_tmp_buf = cork_new(CAN_Buffer);

    librdf_iterator* obj_it = librdf_model_get_targets(
            ctx->model, subject, pred_node);

    // Build ordered array of serialized objects.
    cork_array_init(obj_array);
    while (!librdf_iterator_end(obj_it)){
        // Librdf returns duplicate predicates, we need to deduplicate them.
        array_insert_ordered(
                obj_array, librdf_iterator_get_object(obj_it));
        librdf_iterator_next(obj_it);
    }
    librdf_free_iterator(obj_it);

    printf("Got %lu objects.\n", obj_array->size);

    // Serialize the predicate first.
    serialize(ctx, pred_node, pred_buf_cur);
    /*
    printf("Predicate node to encode at %p.\n", (void*)pred_node);
    printf("N3 predicate: %s\n", (char*)(pred_buf_cur)->buf);
    */

    // Build byte buffer from serialized objects.
    for(size_t i = 0; i < obj_array->size; i++) {
        cork_buffer_init(obj_tmp_buf);
        encode_object(ctx, cork_array_at(obj_array, i), obj_tmp_buf);
        printf("Canonicized object: %s\n", (char*)(obj_tmp_buf)->buf);
        // FIXME This messes up the string in Valgrind, go figure...
        //cork_buffer_ensure_size(pred_buf_cur, (obj_tmp_buf)->size + 2);

        // Append serialized object to object buffer array.
        cork_buffer_append(pred_buf_cur, CAN_O_START, 1);
        printf("Added start block.\n");
        cork_buffer_append_copy(pred_buf_cur, obj_tmp_buf);
        printf("Added object buffer.\n");
        cork_buffer_append(pred_buf_cur, CAN_O_END, 1);
        printf("Added end block.\n");

        printf("Canonicized object: %s\n", (char*)obj_tmp_buf->buf);
    }
    printf("Done serializing objects.\n");

    // Free temp buffer and its pointer.
    cork_buffer_done(obj_tmp_buf);
    cork_delete(CAN_Buffer, obj_tmp_buf);
    // Free ordered array.
    cork_array_done(obj_array);
    printf("Freed obj_array.\n");
    printf("Serialized predicate with object: ");
    print_bytes(pred_buf_cur->buf, pred_buf_cur->size);

    return(0);
}


static int encode_object(
        const CAN_context* ctx, librdf_node* object, CAN_Buffer* obj_buf)
{
    if (librdf_node_get_type(object) == LIBRDF_NODE_TYPE_BLANK) {
        encode_subject(ctx, object, obj_buf);
    } else {
        serialize(ctx, object, obj_buf);
    }

    return(0);
}


/*
 * Static helper functions.
 */

static inline int array_insert_ordered(
        CAN_NodeArray* array, librdf_node* ins_item)
{
    unsigned int i, j;
    int comp;
    size_t ar_sz = cork_array_size(array);
    CAN_Buffer* check_item;

    if (ar_sz == 0) {
        printf("Inserting first element in array.\n");
        cork_array_append(array, ins_item);
        printf(
                "Appended pointer @%p to array item @%p\n",
                ins_item, cork_array_at(array, 0));
        return(0);
    }

    for (i = 0; i < ar_sz; i++) {
        check_item = cork_array_at(array, i);
        printf("Check item @: %p\n", check_item->buf);
        comp = memcmp(
                ins_item->buf, check_item->buf,
                MAX(ins_item->size, check_item->size));
        printf("Comp: %d.\n", comp);
        if (comp == 0) {
            // Term is duplicate. Exit without inserting.
            printf("Duplicate term. Skipping.\n");
            return(0);
        } else if (comp > 0 || i == 0) {
            // Inserted item is greater than current one. Inserting after.
            // Shift all item past this one by one slot.
            printf("Inserting at position %d.\n", i + 1);
            cork_array_ensure_size(array, ar_sz + 1);
            ar_sz++;
            printf("i = %u\n", i);
            // cork_array_at(array, i + 1) = cork_buffer_new();
            for(j = ar_sz - 1; j > i; j--) {
                printf("j = %u\n", j);
                cork_array_at(array, j) = cork_array_at(array, j - 1);
            }
            cork_array_append(array, ins_item);
            printf("Done shifting ordered array items.\n");
            printf("New ordered array size: %lu.\n", cork_array_size(array));
            return(0);
        }
    }
    printf("*** Could not insert element in array!\n");
    exit(1);
}

static int inline serialize(
        const CAN_context* ctx, librdf_node* node, CAN_Buffer* node_buf)
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
