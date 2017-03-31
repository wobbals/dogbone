//
//  main.c
//  pup
//
//  Created by Charley Robinson on 3/11/17.
//
//

#include "unistd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "network_source.h"
#include "pup_dogbone.h"
#include "barc.h"

int main(int argc, const char * argv[]) {
  // todo: move these inside a barc global initializer
  av_register_all();
  av_register_all();
  avfilter_register_all();
  MagickWandGenesis();

    struct pup_dogbone_s* dogbone;
    pup_dogbone_alloc(&dogbone);
  
  struct barc_s* barc;
  barc_alloc(&barc);
  struct barc_config_s config;
  config.css_preset = "auto";
  config.video_framerate = 30;
  config.out_width = 640;
  config.out_height = 480;
  config.output_path = "/tmp/dogbone.mp4";
  barc_read_configuration(barc, &config);
  struct network_source_s* net_source = pup_dogbone_get_network_source(dogbone);
  struct barc_source_s barc_source;
  barc_source.media_stream = network_source_get_stream(net_source);
  barc_add_source(barc, &barc_source);
  int ret = barc_open_outfile(barc);
  if (ret) {
    printf("failed to open archive outfile");
    return ret;
  }

  for (int i = 0; i < 10; i++) {
    printf("waiting %d...\n", i);
    sleep(1);
  }

  double end_time = 10;
  double global_clock = 0;

  while (!ret && end_time > global_clock) {
    ret = barc_tick(barc);
    global_clock = barc_get_current_clock(barc);
    printf("{\"progress\": {\"complete\": %f, \"total\": %f }}\n",
           global_clock * 1000, end_time * 1000);
  }

  if (ret) {
    printf("problem in barc main loop. closing outfile and aborting. ret=%d",
           ret);
    // don't let this condition stop us from closing the file.
  }
  int fret = barc_close_outfile(barc);
  if (fret) {
    printf("failed to finalize container (ret %d", fret);
  }
  return ret & fret;
}
