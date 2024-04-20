#include "common.h"
#include <assert.h>

int main(int argc, char *argv[]) {
    Options opts;
    if (ParseCommandLine(argc, argv, opts)) {
        if (opts.transmit) {
            transmit(opts);
        } else if(opts.receive) {
            receive(opts);
        } else {
            assert(0);
        }
    }
    return 0;
}

