#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <libcork/core.h>
#include <libcork/ds.h>
#include <redland.h>

#include "rdf_canon.h"

/* An ordered, contiguous array of librdf node structs. */
typedef cork_array(librdf_node*) CAN_node_array;

/* An ordered, contiguous array of serialize nodes. */
typedef cork_array(struct cork_buffer*) CAN_buf_array;

/*
 * From benchmark: https://pastebin.com/azzuk072
 * (Daniel Stutzbach's insertion sort)
 */
static inline void sort_array(int *d, const unsigned int sz){
    int i, j;
    for (i = 1; i < sz; i++) {
        int tmp = d[i];
        for (j = i; j >= 1 && tmp < d[j-1]; j--)
            d[j] = d[j-1];
        d[j] = tmp;
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
    size_t capacity = 0;
    /* Initialize a buffer array for the subjects. */
    CAN_node_array subjects;
    cork_array_init(&subjects);
    cork_array(struct cork_buffer*) ser_subjects;
    cork_array_init(&ser_subjects);
    struct cork_buffer* encoded_subj;

    librdf_stream* stream;
    librdf_statement *stmt, *q_stmt;
    librdf_node* subject;

    q_stmt = librdf_new_statement(world);
    stream = librdf_model_find_statements(model, q_stmt);
    if(!stream)  {
        fprintf(stderr, "librdf_model_get_targets failed to return iterator for searching\n");
        return(1);
    }
    cork_buffer_init(buf);

    while(!librdf_stream_end(stream)) {
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
            librdf_node orig_subj;
            memcpy(&orig_subj, subject, sizeof(librdf_node));

            struct cork_hash_table* visited_nodes = cork_hash_table_new(0, 0);

            encoded_subj = encode_subject(
                    model, subject, &orig_subj, visited_nodes);
            cork_array_append(&subjects, subject);
            cork_array_append(&ser_subjects, encoded_subj);

            capacity += encoded_subj->size + 2;
        }

        librdf_stream_next(stream);
    }

    /* TODO sort subjects. */

    cork_buffer_ensure_size(buf, capacity);
    size_t subj_size = cork_array_size(&ser_subjects);
    for(size_t i = 0; i < subj_size; i++){
        cork_buffer_append(buf, CAN_S_START, 1);
        cork_buffer_append_copy(buf, cork_array_at(&ser_subjects, i));
        cork_buffer_append(buf, CAN_S_END, 1);
    }

    cork_buffer_free(encoded_subj);

    librdf_free_stream(stream);

    return(0);
}


struct cork_buffer* encode_subject(librdf_model* model, librdf_node* subject,
        librdf_node* orig_subj, struct cork_hash_table* visited_nodes)
{
    /* Canonicized subject as a byte buffer. */
    unsigned char* can_term_addr = NULL;
    size_t can_term_size;
    struct cork_buffer* can_buf = cork_buffer_new();

    can_term_size = librdf_node_encode(subject, NULL, 0);
    librdf_node_encode(subject, can_term_addr, can_term_size);
    printf("Encoded node: %s", (char*)can_term_addr);
    cork_buffer_append(can_buf, can_term_addr, can_term_size);

    printf("subject (%lu):", can_term_size);
    fwrite(can_term_addr, 1, can_term_size, stdout);
    fputc('\n', stdout);

    return(can_buf);
}
