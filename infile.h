#ifndef _INCLUDE_INFILE_H_
#define _INCLUDE_INFILE_H_

    int infile_init_decoder(ctx_restream *restrm, int stream_index);
    int infile_init(ctx_restream *restrm);
    void infile_close(ctx_restream *restrm);
    void infile_wait(ctx_restream *restrm);
    void infile_rescale_pkt(ctx_restream *restrm);

#endif
