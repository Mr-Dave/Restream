#ifndef _INCLUDE_READER_H_
    #define _INCLUDE_READER_H_

    void *reader(void *parms);
    void reader_start(ctx_restream *restrm);
    void reader_startbyte(ctx_restream *restrm);
    void reader_close(ctx_restream *restrm);
    void reader_end(ctx_restream *restrm);
    void reader_flush(ctx_restream *restrm);
    void reader_init(ctx_restream *restrm);

#endif
