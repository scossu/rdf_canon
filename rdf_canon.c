#include <stdio.h>
#include <stdlib.h>

#include <libcork/ds.h>
#include <redland.h>

int CAN_canonicize(librdf_world* world, librdf_model* graph, cork_buffer* buf)
{
    cork_hash_table* subjects;
    librdf_stream* stream;
    librdf_statement *stmt, *q_stmt;
    librdf_node* subject;
    cork_buffer* subj_buf = cork_buffer_new();

    q_stmt = librdf_new_statement(world);
    stream = librdf_model_find_statements(graph, q_stmt);
    if(!stream)  {
        fprintf(stderr, "librdf_model_get_targets failed to return iterator for searching\n");
        return(1);
    }
    /* Must be preallocated. */
    cork_buffer_init(buf);

    while(!librdf_stream_end(stream)) {
        /* Original subject that gets passed down recursive calls. */
        librdf_node orig_subj;

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

        subj_buf_sz = librdf_node_encode(subject, NULL, 0);
        cork_buffer_ensure_size(subj_buf, subj_buf_sz)

        /*printf("subject length: %d\n", subj_buf_sz);*/
        librdf_node_encode(subject, subj_buf->buf, subj_buf->size);
        printf("subject (%d):", subj_buf_sz);
        fwrite(subj_buf, 1, subj_buf_sz, stdout);
        fputc('\n', stdout);

        cork_buffer_done(subj_buf);

        librdf_stream_next(stream);
    }
    cork_buffer_free(subj_buf);

    librdf_free_stream(stream);
    librdf_free_model(model);
    librdf_free_storage(storage);

    librdf_free_world(world);

    return(0);

#ifdef librdf_memory_debug
  librdf_memory_report(stderr);
#endif
    return(0);
}
