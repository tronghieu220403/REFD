#include "mp3.h"
// GetMp3Types.cpp

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/mem.h>
}

using std::string;
using std::vector;

// If you are on Windows headers, UCHAR is usually unsigned char.
#ifndef UCHAR
using UCHAR = unsigned char;
#endif

namespace type_iden
{
    // In-memory IO state for FFmpeg.
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

    // Decode all audio packets and require zero decode errors.
    // Return {"mp3"} only if every frame decodes cleanly; otherwise return empty.
    vector<string> GetMp3Types(const std::span<UCHAR>& data) {
        vector<string> types;
        if (data.size() < 4) return types;

        // Silence FFmpeg logs (optional).
        av_log_set_level(AV_LOG_QUIET);

        // Allocate format context and custom AVIO context.
        AVFormatContext* fmt_ctx = avformat_alloc_context();
        if (!fmt_ctx) return types;

        const int kAvioBufSize = 1 << 14;  // 16KB IO buffer
        uint8_t* avio_buf = static_cast<uint8_t*>(av_malloc(kAvioBufSize));
        if (!avio_buf) {
            avformat_free_context(fmt_ctx);
            return types;
        }

        BufferData bd{ reinterpret_cast<const unsigned char*>(data.data()), data.size(), 0 };

        // Create AVIO with read + seek from memory.
        AVIOContext* avio_ctx =
            avio_alloc_context(avio_buf, kAvioBufSize, 0, &bd, &ReadPacket, nullptr, &Seek);
        if (!avio_ctx) {
            av_free(avio_buf);
            avformat_free_context(fmt_ctx);
            return types;
        }

        fmt_ctx->pb = avio_ctx;
        fmt_ctx->flags |= AVFMT_FLAG_CUSTOM_IO;

        // Open "input" from our custom IO. Let FFmpeg probe the format.
        if (avformat_open_input(&fmt_ctx, nullptr, nullptr, nullptr) < 0) {
            avio_context_free(&avio_ctx);  // also frees avio_ctx->buffer
            avformat_free_context(fmt_ctx);
            return types;
        }

        // Parse stream info (detects audio stream and codec parameters).
        if (avformat_find_stream_info(fmt_ctx, nullptr) < 0) {
            avformat_close_input(&fmt_ctx);
            if (avio_ctx) avio_context_free(&avio_ctx);
            return types;
        }

        // Find best audio stream and associated decoder.
        AVCodec* codec = nullptr;
        const int stream_index =
            av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, (const AVCodec**)&codec, 0);
        if (stream_index < 0 || codec == nullptr) {
            avformat_close_input(&fmt_ctx);
            if (avio_ctx) avio_context_free(&avio_ctx);
            return types;
        }

        // Strong check: require MP3 decoder (accept both mp3 and mp3float decoders).
        if (codec->id != AV_CODEC_ID_MP3) {
            // Not an MP3 stream; treat as not-mp3 (empty).
            avformat_close_input(&fmt_ctx);
            if (avio_ctx) avio_context_free(&avio_ctx);
            return types;
        }

        AVCodecContext* codec_ctx = avcodec_alloc_context3(codec);
        if (!codec_ctx) {
            avformat_close_input(&fmt_ctx);
            if (avio_ctx) avio_context_free(&avio_ctx);
            return types;
        }

        if (avcodec_parameters_to_context(codec_ctx, fmt_ctx->streams[stream_index]->codecpar) < 0) {
            avcodec_free_context(&codec_ctx);
            avformat_close_input(&fmt_ctx);
            if (avio_ctx) avio_context_free(&avio_ctx);
            return types;
        }

        if (avcodec_open2(codec_ctx, codec, nullptr) < 0) {
            avcodec_free_context(&codec_ctx);
            avformat_close_input(&fmt_ctx);
            if (avio_ctx) avio_context_free(&avio_ctx);
            return types;
        }

        // Decode all packets; any error means "corrupt".
        AVPacket* pkt = av_packet_alloc();
        AVFrame* frame = av_frame_alloc();
        if (!pkt || !frame) {
            if (pkt) av_packet_free(&pkt);
            if (frame) av_frame_free(&frame);
            avcodec_free_context(&codec_ctx);
            avformat_close_input(&fmt_ctx);
            if (avio_ctx) avio_context_free(&avio_ctx);
            return types;
        }

        bool had_any_frame = false;
        bool all_ok = true;

        // Read and decode loop.
        for (;;) {
            int rr = av_read_frame(fmt_ctx, pkt);
            if (rr == AVERROR_EOF) {
                break;  // end of stream
            }
            if (rr < 0) {
                all_ok = false;  // read error
                break;
            }

            if (pkt->stream_index == stream_index) {
                // Send packet to decoder
                int ret = avcodec_send_packet(codec_ctx, pkt);
                if (ret < 0) {
                    all_ok = false;
                    av_packet_unref(pkt);
                    break;
                }

                // Receive as many frames as available for this packet
                while (true) {
                    ret = avcodec_receive_frame(codec_ctx, frame);
                    if (ret == AVERROR(EAGAIN)) {
                        // Need more packets to continue decoding; not an error.
                        break;
                    }
                    if (ret == AVERROR_EOF) {
                        // Decoder fully flushed; no more frames now.
                        break;
                    }
                    if (ret < 0) {
                        // Any actual decode error => corrupt.
                        all_ok = false;
                        break;
                    }
                    // Successfully decoded a frame.
                    had_any_frame = true;
                    av_frame_unref(frame);
                }
                if (!all_ok) {
                    av_packet_unref(pkt);
                    break;
                }
            }

            av_packet_unref(pkt);
        }

        // Flush the decoder to drain delayed frames.
        if (all_ok) {
            int ret = avcodec_send_packet(codec_ctx, nullptr);  // flush signal
            if (ret >= 0) {
                while (true) {
                    ret = avcodec_receive_frame(codec_ctx, frame);
                    if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) break;
                    if (ret < 0) {
                        all_ok = false;
                        break;
                    }
                    had_any_frame = true;
                    av_frame_unref(frame);
                }
            }
            else {
                all_ok = false;
            }
        }

        // Cleanup
        av_frame_free(&frame);
        av_packet_free(&pkt);
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&fmt_ctx);
        if (avio_ctx) avio_context_free(&avio_ctx);  // also frees avio buffer

        // Decision: valid MP3 only if at least one decoded frame and zero errors.
        if (all_ok && had_any_frame) {
            types.emplace_back("mp3");
        }
        return types;
    }
}