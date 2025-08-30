#pragma once

#include "mkv_defs.h"
#include "mkv_element_handlers.h"

// @param p Depending on the element type, it should be cast to different types.
// @param data_len The length of the data.
// @return 0 on success.
typedef int (*ele_handler_func_t)(mkv_ctx_t *ctx, void *p, size_t data_len);

struct id_type_map_t
{
    uint64_t id;
    element_type_t type;
    const char *desc;

    // Finer element handler, called after parsing value. If NULL, not handling.
    ele_handler_func_t ele_handler;
};
static const struct id_type_map_t type_map[] = {
    { 0xEC, ELE_BINARY, "Void" },

    { 0x1a45dfa3, ELE_MASTER, "Header" },
    { 0x4286, ELE_UINT, "EBMLVersion" },
    { 0x42f7, ELE_UINT, "EBMLReadVersion" },
    { 0x42f2, ELE_UINT, "EBMLMaxIDLength" },
    { 0x42f3, ELE_UINT, "EBMLMaxSizeLength" },
    { 0x4282, ELE_ASCII, "DocType" },
    { 0x4287, ELE_UINT, "DocTypeVersion" },
    { 0x4285, ELE_UINT, "DocTypeReadVersion" },

    { 0x18538067, ELE_MASTER, "Segment", ele_segment },

    { 0x114D9B74, ELE_MASTER, "SeekHead" },
    { 0x4DBB, ELE_MASTER, "Seek" },
    { 0x53AB, ELE_BINARY, "SeekID" },
    { 0x53AC, ELE_UINT, "SeekPosition" },

    { 0x1549A966, ELE_MASTER, "Info" },
    { 0x73A4, ELE_BINARY, "SegmentUUID", ele_segment_uuid },
    { 0x7384, ELE_UTF8, "SegmentFilename" },
    { 0x2AD7B1, ELE_UINT, "TimestampScale", ele_timestamp_scale },
    { 0x4D80, ELE_UTF8, "MuxingApp" },
    { 0x5741, ELE_UTF8, "WritingApp" },
    { 0x4489, ELE_FLOAT, "Duration" },

    { 0x1654AE6B, ELE_MASTER, "Tracks" },
    { 0xAE, ELE_MASTER, "TrackEntry", ele_track_entry },
    { 0xD7, ELE_UINT, "TrackNumber", ele_track_number },
    { 0x73C5, ELE_UINT, "TrackUID" },
    { 0x9C, ELE_UINT, "FlagLacing" },
    { 0x22B59C, ELE_ASCII, "Language" },
    { 0x86, ELE_ASCII, "CodecID", ele_track_codecid },
    { 0x83, ELE_UINT, "TrackType", ele_track_type },
    { 0x23E383, ELE_UINT, "DefaultDuration" },
    { 0xE0, ELE_MASTER, "Video" },
    { 0xB0, ELE_UINT, "PixelWidth" },
    { 0xBA, ELE_UINT, "PixelHeight" },
    { 0x9A, ELE_UINT, "FlagInterlaced" },
    { 0x55B0, ELE_MASTER, "Colour" },
    { 0x63A2, ELE_BINARY, "CodecPrivate", ele_track_codec_private },
    { 0xE1, ELE_MASTER, "Audio" },

    { 0x1941A469, ELE_MASTER, "Attachments" },
    { 0x1043A770, ELE_MASTER, "Chapters" },

    { 0x1254C367, ELE_MASTER, "Tags" },
    //{ 0x7373, ELE_MASTER, "Tag" },

    { 0x1F43B675, ELE_MASTER, "Cluster", ele_cluster },
    { 0xE7, ELE_UINT, "Timestamp", ele_cluster_timestamp }, // abs ts
    { 0xA3, ELE_BINARY, "SimpleBlock", ele_simple_block },

    { 0x1C53BB6B, ELE_MASTER, "Cues" },
    { 0xBB, ELE_MASTER, "CuePoint" },
    { 0xB3, ELE_UINT, "CueTime" },
    { 0xB7, ELE_MASTER, "CueTrackPositions" },
    { 0xF7, ELE_UINT, "CueTrack" },
    { 0xF1, ELE_UINT, "CueClusterPosition" },
    { 0xF0, ELE_UINT, "CueRelativePosition" },

    { 0xBF, ELE_BINARY, "CRC32" },

    { 0, ELE_UNKNOWN, "unknown" }
};
