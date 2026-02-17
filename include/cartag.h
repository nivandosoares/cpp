#ifndef CARTAG_H
#define CARTAG_H

#include <stddef.h>
#include <stdint.h>

#define CARTAG_MAX_TRACKS 20000
#define CARTAG_PATH_MAX 1024
#define CARTAG_NAME_MAX 256

typedef enum {
    FORMAT_UNKNOWN = 0,
    FORMAT_MP3,
    FORMAT_FLAC,
    FORMAT_WAV,
    FORMAT_AAC,
    FORMAT_M4A,
    FORMAT_OGG,
    FORMAT_WMA
} AudioFormat;

typedef enum {
    ORG_NONE = 0,
    ORG_ARTIST,
    ORG_ALBUM,
    ORG_FLAT
} OrganizeMode;

typedef enum {
    SIM_NONE = 0,
    SIM_GENERIC,
    SIM_FAT,
    SIM_FILENAME
} SimulateMode;

typedef struct {
    char input[CARTAG_PATH_MAX];
    char export_path[CARTAG_PATH_MAX];
    int keep_format;
    int convert_mp3;
    int group_by_format;
    int fix_tags;
    int dedupe;
    int normalize_volume;
    int strip_art;
    int resize_art;
    int extract_art;
    int car_safe;
    int prefix;
    int limit_name;
    int interactive_tui;
    OrganizeMode organize;
    SimulateMode simulate;
} CliOptions;

typedef struct {
    char path[CARTAG_PATH_MAX];
    char rel_path[CARTAG_PATH_MAX];
    char out_path[CARTAG_PATH_MAX];
    char filename[CARTAG_NAME_MAX];
    char artist[128];
    char album[128];
    char title[128];
    char genre[64];
    int track_no;
    int year;
    AudioFormat format;
    uint64_t size_bytes;
    uint64_t quick_hash;
    int duration_seconds;
    int duplicate;
    int unsupported;
    int warning_count;
} AudioTrack;

typedef struct {
    AudioTrack *tracks;
    size_t count;
    size_t capacity;
} TrackList;

typedef struct {
    size_t total_tracks;
    size_t removed_duplicates;
    uint64_t total_duration;
    size_t format_count[8];
} LibraryStats;

int cli_parse(int argc, char **argv, CliOptions *opts);
void cli_print_help(void);

int tui_run(CliOptions *opts);

int fs_scan_audio(const char *root, TrackList *list);
int fs_copy_file(const char *src, const char *dst);
int fs_ensure_directory(const char *path);

AudioFormat audio_detect_format(const char *path);
const char *audio_format_name(AudioFormat fmt);
int audio_can_play_car(AudioTrack *t, int car_safe, char *warn, size_t warn_sz);
int audio_convert_if_needed(AudioTrack *t, const CliOptions *opts, char *warn, size_t warn_sz);

void sanitize_filename(char *name, size_t max_len);
void sanitize_track(AudioTrack *t, int limit_name);

void tags_fix_from_filename(AudioTrack *t);
void tags_standardize(AudioTrack *t);

void organizer_plan(TrackList *list, const CliOptions *opts);
void organizer_apply_prefix(TrackList *list);

void dedupe_mark(TrackList *list, LibraryStats *stats);
void simulate_print(const TrackList *list, SimulateMode mode, LibraryStats *stats);

int downloader_is_url(const char *s);
int downloader_install(char *warn, size_t warn_sz);
int downloader_fetch_audio(const char *url, const char *out_dir, char *warn, size_t warn_sz);

int exporter_run(const TrackList *list, const CliOptions *opts);
void diagnostics_print(const TrackList *list);
void stats_print(const LibraryStats *stats);

#endif
