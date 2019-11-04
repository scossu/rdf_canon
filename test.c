#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

#include <redland.h>
#include "./rdf_canon.h"

int main(int argc, char* argv[]) {
    CAN_Context* ctx = CAN_context_new();

    librdf_uri* uri;
    unsigned char* string;

    uri = librdf_new_uri_from_filename(ctx->world, argv[1]);
    librdf_model_load(ctx->model, uri, NULL, NULL, NULL);

    string=librdf_model_to_string(ctx->model, uri, "ntriples", NULL, NULL);
    if(!string) {
        printf("Failed to serialize model\n");
        exit(1);
    }

    printf("Made a %d byte string\n", (int)strlen((char*)string));
    printf("RDF: %s\n", string);
    free(string);

    CAN_Buffer canon;
    CAN_canonicize(ctx, &canon);

    printf("Canonicized graph: ");
    fwrite(canon.buf, 1, canon.size, stdout);
    fputc('\n', stdout);

    cork_buffer_done(&canon);
    printf("Freed buffer.\n");

    librdf_free_uri(uri);
    printf("Freed URI.\n");

    CAN_context_free(ctx);

#ifdef LIBRDF_MEMORY_DEBUG
  librdf_memory_report(stderr);
#endif
    return(0);
}

