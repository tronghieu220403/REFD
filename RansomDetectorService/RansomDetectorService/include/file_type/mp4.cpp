#include "mp4.h"
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/error.h>
#include <libavutil/mem.h>
#include <libavutil/imgutils.h>
}

namespace type_iden
{
    // Custom read context for memory buffer
    struct BufferData {
        const unsigned char* base;  // start of buffer
        size_t size;                // total size
        size_t pos;                 // current read position
    };

    // FFmpeg read callback: read from the memory buffer.
    static int ReadPacket(void* opaque, uint8_t* buf, int buf_size) {
        auto* bd = reinterpret_cast<BufferData*>(opaque);
        if (bd->pos >= bd->size) return AVERROR_EOF;
        size_t remaining = bd->size - bd->pos;
        size_t to_copy = remaining < static_cast<size_t>(buf_size) ? remaining
            : static_cast<size_t>(buf_size);
        if (to_copy > 0) {
            memcpy(buf, bd->base + bd->pos, to_copy);
            bd->pos += to_copy;
        }
        return static_cast<int>(to_copy);
    }

    // FFmpeg seek callback: support random access and size query.
    static int64_t Seek(void* opaque, int64_t offset, int whence) {
        auto* bd = reinterpret_cast<BufferData*>(opaque);
        if (whence == AVSEEK_SIZE) {
            return static_cast<int64_t>(bd->size);
        }
        int64_t new_pos = 0;
        if (whence == SEEK_SET) {
            new_pos = offset;
        }
        else if (whence == SEEK_CUR) {
            new_pos = static_cast<int64_t>(bd->pos) + offset;
        }
        else if (whence == SEEK_END) {
            new_pos = static_cast<int64_t>(bd->size) + offset;
        }
        else {
            return -1;
        }
        if (new_pos < 0 || static_cast<size_t>(new_pos) > bd->size) return -1;
        bd->pos = static_cast<size_t>(new_pos);
        return new_pos;
    }

    // Main function
    std::vector<std::string> GetMp4Types(const std::span<UCHAR>& data) {
        std::vector<std::string> types;

        if (data.size() < 12) {
            return types;  // too small
        }

        // FFmpeg initialization
        av_log_set_level(AV_LOG_QUIET);
        AVFormatContext* fmt_ctx = avformat_alloc_context();
        if (!fmt_ctx) return types;
        defer{ avformat_free_context(fmt_ctx); };

        const int kAvioBufSize = 1 << 14;  // 16KB IO buffer
        uint8_t* avio_buf = static_cast<uint8_t*>(av_malloc(kAvioBufSize));
        if (!avio_buf) {
            return types;
        }

        BufferData bd{ data.data(), data.size(), 0 };
        // Create AVIO with read + seek from memory.
        AVIOContext* avio_ctx = avio_alloc_context(
            avio_buf, kAvioBufSize, 0, &bd, &ReadPacket, nullptr, &Seek);
        if (!avio_ctx) { return types; }
        defer{
            /* note: the internal buffer could have changed, and be != avio_buf */
            if (avio_ctx->buffer != nullptr)
            {
                av_freep(&avio_ctx->buffer);
            }
            avio_context_free(&avio_ctx);
        };
        AVERROR(EIO);
        fmt_ctx->pb = avio_ctx;
        fmt_ctx->flags |= AVFMT_FLAG_CUSTOM_IO;
        
        // Open "input" from our custom IO. Let FFmpeg probe the format.
        if (avformat_open_input(&fmt_ctx, nullptr, nullptr, nullptr) < 0) {
            return types;  // not valid
        }
        defer{ avformat_close_input(&fmt_ctx); };

        // Parse stream info (detects audio stream and codec parameters).
        if (avformat_find_stream_info(fmt_ctx, nullptr) < 0) {
            return types;
        }

        // Decode all streams fully
        bool decode_ok = true;
        AVPacket* pkt = av_packet_alloc();
        if (pkt == nullptr) { return types; }
        defer{ av_packet_free(&pkt); };

        std::vector<AVCodecContext*> decoders(fmt_ctx->nb_streams, nullptr);

        // Prepare decoder for each stream
        for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
            AVStream* st = fmt_ctx->streams[i];
            const AVCodec* dec = avcodec_find_decoder(st->codecpar->codec_id);
            if (!dec) {
                decode_ok = false;
                break;
            }
            AVCodecContext* dec_ctx = avcodec_alloc_context3(dec);
            if (!dec_ctx) {
                decode_ok = false;
                break;
            }
            if (avcodec_parameters_to_context(dec_ctx, st->codecpar) < 0) {
                avcodec_free_context(&dec_ctx);
                decode_ok = false;
                break;
            }
            if (avcodec_open2(dec_ctx, dec, nullptr) < 0) {
                avcodec_free_context(&dec_ctx);
                decode_ok = false;
                break;
            }
            decoders[i] = dec_ctx;
        }

        // Read packets and decode
        while (decode_ok && av_read_frame(fmt_ctx, pkt) >= 0) {
            AVCodecContext* dec_ctx = decoders[pkt->stream_index];
            if (!dec_ctx) {
                av_packet_unref(pkt);
                continue;
            }
            if (avcodec_send_packet(dec_ctx, pkt) < 0) {
                decode_ok = false;
                break;
            }
            AVFrame* frame = av_frame_alloc();
            if (frame == nullptr) { return types; }
            defer{ av_frame_free(&frame); };

            while (true) {
                int ret = avcodec_receive_frame(dec_ctx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                }
                if (ret < 0) {
                    decode_ok = false;
                    break;
                }
            }
            av_packet_unref(pkt);
        }

        // Cleanup
        for (auto* dec_ctx : decoders) {
            if (dec_ctx) avcodec_free_context(&dec_ctx);
        }

        if (decode_ok) {
            types.push_back("mp4");
        }

        return types;
    }
}
