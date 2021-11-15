#include "common.h"
#include <libavformat/avformat.h>
#include "trait/frame_sink.h"
#define PORT 8081

struct screen_exporter {
    struct sc_frame_sink frame_sink;

    unsigned sink_count;
    AVFrame *frame;
    int sock;
};

void
screen_exporter_init(struct screen_exporter *screen_exporter);
