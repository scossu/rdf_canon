#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

#include <redland.h>
#include "./rdf_canon.h"

int main(int argc, char* argv[]) {
    librdf_world* world;
    librdf_model* model;
    librdf_uri* uri;
    unsigned char* string;


    librdf_world_open(world=librdf_new_world());

    librdf_storage* storage = librdf_new_storage(world, "memory", NULL, NULL);

    model=librdf_new_model(world, storage=storage, NULL);

    uri=librdf_new_uri_from_filename(world, argv[1]);
    librdf_model_load(model, uri, NULL, NULL, NULL);

    string=librdf_model_to_string(model, uri, "ntriples", NULL, NULL);
    if(!string) {
        printf("Failed to serialize model\n");
        exit(1);
    }

    printf("Made a %d byte string\n", (int)strlen((char*)string));
    printf("RDF: %s\n", string);
    free(string);

    struct cork_buffer canon;
    CAN_canonicize(world, model, &canon);

    printf("Canonicized graph: ");
    fwrite(canon.buf, 1, canon.size, stdout);
    fputc('\n', stdout);

    cork_buffer_done(&canon);
    printf("Freed buffer.\n");

    librdf_free_uri(uri);
    printf("Freed URI.\n");

    librdf_free_model(model);
    printf("Freed model.\n");
    librdf_free_storage(storage);
    printf("Freed storage.\n");

    librdf_free_world(world);

#ifdef LIBRDF_MEMORY_DEBUG
  librdf_memory_report(stderr);
#endif
    return(0);
}

