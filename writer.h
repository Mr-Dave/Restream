#ifndef _INCLUDE_WRITER_H_
#define _INCLUDE_WRITER_H_

    int writer_packet_flush(ctx_restream *restrm);
    int writer_init_video(ctx_restream *restrm, int indx);
    int writer_init_audio(ctx_restream *restrm, int indx);
    int writer_init_open(ctx_restream *restrm);
    int writer_init(ctx_restream *restrm);
    void writer_close_encoder(ctx_restream *restrm);
    void writer_close(ctx_restream *restrm);
    void writer_rescale_frame(ctx_restream *restrm);
    void writer_rescale_enc_pkt(ctx_restream *restrm);
    int writer_packet_sendpkt(ctx_restream *restrm);
    int writer_packet_encode(ctx_restream *restrm);
    void writer_packet(ctx_restream *restrm);

#endif
