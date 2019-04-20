#include <stdio.h>
#include <stdlib.h>

#include <redland.h>

int CAN_canonicize(const unsigned char* graph, CAN_Buffer* buf)
{
    librdf_world* world;
    librdf_storage *storage;
    librdf_model* model;
    librdf_uri* uri;
    librdf_stream* stream;
    librdf_statement *stmt, *q_stmt;
    librdf_node* subject;
    CAN_Buffer* buf;
    size_t subj_buf_sz, subj_buf_sz2;


    librdf_world_open(world=librdf_new_world());

    model=librdf_new_model(world, storage=librdf_new_storage(world, "memory", null, null), null);

    uri=librdf_new_uri_from_filename(world, argv[1]);
    librdf_model_load(model, uri, null, null, null);
    librdf_free_uri(uri);

    q_stmt = librdf_new_statement(world);
    stream = librdf_model_find_statements(model, q_stmt);
    if(!stream)  {
        fprintf(stderr, "librdf_model_get_targets failed to return iterator for searching\n");
      return(1);
    }
    while(!librdf_stream_end(stream)) {
        librdf_node *target;

        stmt = librdf_stream_get_object(stream);
        if(!stmt) {
            fprintf(stderr, "librdf_stream_get_statement returned null\n");
            break;
        }
        fputs("matched statement: ", stdout);
        librdf_statement_print(stmt, stdout);
        fputc('\n', stdout);
        fputs("matched subject: ", stdout);
        subject = librdf_statement_get_subject(stmt);
        librdf_node_print(subject, stdout);
        fputc('\n', stdout);

        subj_buf_sz = librdf_node_encode(subject, null, 0);
        subj_buf = malloc(subj_buf_sz);

        printf("subject length: %d\n", subj_buf_sz);
        librdf_node_encode(subject, subj_buf, subj_buf_sz);
        printf("subject (%d):", subj_buf_sz);
        fwrite(subj_buf, 1, subj_buf_sz, stdout);
        fputc('\n', stdout);

        free(subj_buf);

        librdf_stream_next(stream);
    }

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