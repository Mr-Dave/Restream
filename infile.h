#ifndef _INCLUDE_INFILE_H_
#define _INCLUDE_INFILE_H_

    int infile_init(ctx_restream *restrm);
    void infile_close(ctx_restream *restrm);
    void infile_wait(ctx_restream *restrm);

#endif
