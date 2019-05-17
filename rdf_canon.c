#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>

#include <libcork/core.h>
#include <libcork/ds.h>
#include <redland.h>

#include "rdf_canon.h"

/*
 * From benchmark: https://pastebin.com/azzuk072
 * (Daniel Stutzbach's insertion sort)
 */
/*
static inline void sort_terms(cork_array* array){
    int i, j;
    for (i = 1; i < sz; i++) {
        int tmp = d[i];
        for (j = i; j >= 1 && tmp < d[j-1]; j--)
            d[j] = d[j-1];
        d[j] = tmp;
    }
}
*/
static inline void sort_terms(CAN_buffer_array* array){
    int i, j;

    for (i = 1; i < cork_array_size(array); i++) {
        struct cork_buffer* tmp = cork_array_at(array, i);
        for (
            j = i;
            j >= 1 && memcmp(
                tmp->buf, cork_array_at(array, j - 1)->buf, MAX(tmp->size, cork_array_at(array, j - 1)->size)
            ) < 0;
            j--
        )
            cork_array_at(array, j) = cork_array_at(array, j - 1);
        cork_array_at(array, j) = tmp;
    }
}

static bool subj_array_contains(const CAN_node_array* subjects, librdf_node* el)
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


int CAN_canonicize(
        librdf_world* world, librdf_model* model, struct cork_buffer* buf)
{
    CAN_context _ctx;
    CAN_context* ctx = &_ctx;

    CAN_node_array visited_nodes;
    ctx->visited_nodes = &visited_nodes;

    ctx->world = world;
    ctx->model = model;
    ctx->raptor_world = librdf_world_get_raptor(world);

    size_t capacity = 0;
    /* Initialize a buffer array for the subjects. */
    CAN_node_array subjects;
    cork_array_init(&subjects);
    CAN_buffer_array ser_subjects;
    cork_array_init(&ser_subjects);

    librdf_stream* stream;
    librdf_statement *stmt, *q_stmt;
    librdf_node* subject;

    q_stmt = librdf_new_statement(world);
    stream = librdf_model_find_statements(ctx->model, q_stmt);
    librdf_free_statement(q_stmt);
    if(!stream)  {
        fprintf(stderr, "librdf_model_get_targets failed to return iterator for searching\n");
        return(1);
    }
    cork_buffer_init(buf);

    /* Canonicized subject as a byte buffer. */
    //cork_array_ensure_size(&subjects, librdf_model_size(model));
    struct cork_buffer* subj_buf = malloc(
            sizeof(struct cork_buffer) * librdf_model_size(ctx->model));
    size_t i = 0;
    while(!librdf_stream_end(stream)) {
        //struct cork_buffer* subj_buf = encoded_subjs + i;
        cork_array_init(ctx->visited_nodes);
        cork_buffer_init(subj_buf + i);
        printf("i: %lu\n", i);

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

        if(!subj_array_contains(&subjects, subject)) {
            /* Original subject that gets passed down the call stack. */
            ctx->orig_subj = subject;

            encode_subject(ctx, subject, (subj_buf + i));

            printf("Canonicized subject: ");
            fwrite((subj_buf + i)->buf, 1, (subj_buf + i)->size, stdout);
            fputc('\n', stdout);

            cork_array_append(&subjects, subject);
            cork_array_append(&ser_subjects, subj_buf + i);

            capacity += (subj_buf + i)->size + 2;
        } else {
            printf("Duplicate subject: ");
            librdf_node_print(subject, stdout);
            fputc('\n', stdout);
            cork_buffer_done(subj_buf + i);
        }

        cork_array_done(ctx->visited_nodes);
        i++;
        librdf_stream_next(stream);
    }

    librdf_free_statement(stmt);
    cork_array_done(&subjects);

    sort_terms(&ser_subjects);

    cork_buffer_ensure_size(buf, capacity);
    size_t subj_size = cork_array_size(&ser_subjects);
    for(i = 0; i < subj_size; i++){
        //printf("i: %lu\n", i);
        struct cork_buffer* el = cork_array_at(&ser_subjects, i);
        printf("Buffer in array: ");
        fwrite(el->buf, 1, el->size, stdout);
        fputc('\n', stdout);
        cork_buffer_append(buf, CAN_S_START, 1);
        cork_buffer_append_copy(buf, el);
        cork_buffer_append(buf, CAN_S_END, 1);

        cork_buffer_done(el);
        el = NULL;
    }

    free(subj_buf);
    printf("Freed encoded subjects.\n");
    cork_array_done(&ser_subjects);
    printf("Freed encoded subjects array.\n");

    librdf_free_stream(stream);
    ctx = NULL;

    return(0);
}


int encode_subject(
        CAN_context* ctx, librdf_node* subject, struct cork_buffer* subj_buf)
{
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
    struct cork_buffer* pred_buf = cork_buffer_new();
    encode_preds(ctx, subject, pred_buf);
    cork_buffer_append_copy(subj_buf, pred_buf);

    cork_buffer_free(pred_buf);

    return(0);
}


int encode_preds(
    CAN_context* ctx, librdf_node* subject, struct cork_buffer* pred_buf
) {
    librdf_iterator* props_it = librdf_model_get_arcs_out(ctx->model, subject);
    CAN_node_array props_a;
    cork_array_init(&props_a);

    // Build ordered predicates array.
    while(!librdf_iterator_end(props_it)){
        cork_array_append(&props_a, librdf_iterator_get_object(props_it));
        /* TODO Insert sorted. */
        librdf_iterator_next(props_it);
    }

    // Append serialized perdicates and objects to buffer. 
    for(size_t i = 0; i < cork_array_size(&props_a); i++){
        librdf_node* p = cork_array_at(&props_a, i);
        CAN_node_array obj_a;
        cork_array_init(&obj_a);

        // Build ordered array of objects.
        librdf_iterator* obj_it = librdf_model_get_targets(
                ctx->model, subject, p);
        while (!librdf_iterator_end(obj_it)){
            cork_array_append(&obj_a, librdf_iterator_get_object(obj_it));
            librdf_iterator_next(obj_it);
        }
        librdf_free_iterator(obj_it);

        // Start of predicate block.
        cork_buffer_append(pred_buf, CAN_P_START, 1);

        // Append serialized objects to object buffer.
        for(size_t j = 0; j < cork_array_size(&obj_a); j++){
            struct cork_buffer* obj_buf = cork_buffer_new();

            encode_object(ctx, cork_array_at(&obj_a, j), obj_buf);

            cork_buffer_append(pred_buf, CAN_O_START, 1);
            cork_buffer_append_copy(pred_buf, obj_buf);
            cork_buffer_append(pred_buf, CAN_O_END, 1);

            cork_buffer_free(obj_buf);
        }

        // End of predicate block.
        cork_buffer_append(pred_buf, CAN_P_END, 1);

        cork_array_done(&obj_a);
    }
    cork_array_done(&props_a);
    librdf_free_iterator(props_it);
    return(0);
}

int encode_object(
    CAN_context* ctx, librdf_node* object, struct cork_buffer* obj_buf
) {
    if (librdf_node_get_type(object) == LIBRDF_NODE_TYPE_BLANK) {
        encode_subject(ctx, object, obj_buf);
    } else {
        serialize(ctx, object, obj_buf);
    }

    return(0);
}

int serialize(
        CAN_context* ctx, librdf_node* node, struct cork_buffer* node_buf){
    raptor_iostream* iostr = raptor_new_iostream_to_string(
        ctx->raptor_world, &(node_buf->buf), &(node_buf->size), malloc
    );
    int ret = librdf_node_write(node, iostr);
    printf("node buffer write result: %d\n", ret);
    printf("node buffer: %s\n", (char*)node_buf->buf);
    raptor_iostream_write_end(iostr);
    raptor_free_iostream(iostr);

    return(0);
}
