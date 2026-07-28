/* gonzo-detritus.cpp -- see gonzo-detritus.h for the module contract. */

#include "gonzo-detritus.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define STATUS_PATH "/run/detritus/status.json"
#define SUPPORTED_SCHEMA_VERSION 1
#define MAX_TRACKED_CANDIDATES 64

typedef struct {
    pid_t pid;
    gint  coldness_pct;
} candidate_entry_t;

static struct {
    gboolean loaded;             /* has any snapshot ever successfully parsed */
    gdouble  psi_avg10;
    gboolean frozen;
    gchar    frozen_name[64];
    gulong   trickle_bytes_interval;
    gdouble  mem_rate_kb_per_sec;
    gulong   gonzocache_resident_kb;
    candidate_entry_t candidates[MAX_TRACKED_CANDIDATES];
    gint     n_candidates;
} g_snapshot = { FALSE, -1.0, FALSE, "", 0, -1.0, 0, {}, 0 };

/* Find the value following "key": in buf, starting the search at buf.
 * Returns a pointer just past the colon, or NULL if key isn't found in
 * this buffer. Scoped to this file's own known schema -- not a general
 * JSON key lookup (it doesn't respect nested object boundaries, which
 * is fine here because status.json's top-level keys are all unique
 * strings that don't recur inside the candidates array's objects). */
static const char *find_key_value(const char *buf, const char *key)
{
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    const char *p = strstr(buf, pattern);
    if (!p) return NULL;
    p += strlen(pattern);
    while (*p == ' ' || *p == '\t') p++;
    return p;
}

static gboolean parse_bool_at(const char *p)
{
    return strncmp(p, "true", 4) == 0;
}

static gboolean extract_string_at(const char *p, gchar *out, gsize out_size)
{
    if (*p != '"') return FALSE;
    p++;
    gsize i = 0;
    while (*p && *p != '"' && i + 1 < out_size) {
        out[i++] = *p++;
    }
    out[i] = '\0';
    return TRUE;
}

/* Parse the candidates array. Scoped to the exact object shape
 * write_status_file() emits: { "pid": N, "name": "...", "rss_kb": N,
 * "coldness_pct": N }. Order of keys within each object is assumed
 * fixed (matches the writer); if the writer's field order ever
 * changes this parser must change with it -- that coupling is
 * accepted deliberately in exchange for not linking a JSON library
 * for a single, small, same-project schema. */
static void parse_candidates(const char *buf)
{
    g_snapshot.n_candidates = 0;
    const char *arr = find_key_value(buf, "candidates");
    if (!arr) return;

    const char *p = arr;
    while ((p = strstr(p, "\"pid\":")) != NULL &&
           g_snapshot.n_candidates < MAX_TRACKED_CANDIDATES) {
        long pid = 0;
        gint coldness = -1;

        const char *pidval = find_key_value(p, "pid");
        if (pidval) pid = strtol(pidval, NULL, 10);

        const char *coldval = find_key_value(p, "coldness_pct");
        /* coldness_pct may appear for a later candidate before we've
         * advanced p -- bound the search to roughly this object by
         * capping the scan distance, since find_key_value has no
         * concept of object boundaries. 200 bytes comfortably covers
         * one candidate object at this schema's field widths. */
        if (coldval && (coldval - p) < 200) {
            coldness = (gint)strtol(coldval, NULL, 10);
        }

        candidate_entry_t *e = &g_snapshot.candidates[g_snapshot.n_candidates];
        e->pid = (pid_t)pid;
        e->coldness_pct = coldness;
        g_snapshot.n_candidates++;

        p += 6; /* past this "pid": to search for the next one */
    }
}

gboolean gonzo_detritus_refresh(void)
{
    FILE *f = fopen(STATUS_PATH, "r");
    if (!f) return FALSE;  /* daemon not installed/running -- expected, not an error */

    char buf[16384];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    if (n == 0) return FALSE;
    buf[n] = '\0';

    const char *ver = find_key_value(buf, "schema_version");
    if (!ver) return FALSE;
    long schema_version = strtol(ver, NULL, 10);
    if (schema_version != SUPPORTED_SCHEMA_VERSION) {
        /* Known incompatible schema -- fail safe rather than misread
         * fields at the wrong offsets. Keep the previous snapshot
         * rather than clearing it, per this module's stale-tolerant
         * contract; a version mismatch is a persistent condition, not
         * a one-cycle glitch, so the display will consistently show
         * stale-but-plausible data until the mismatch is fixed, which
         * is more useful than blanking every row. */
        return FALSE;
    }

    const char *avg10 = find_key_value(buf, "psi_avg10");
    if (avg10) g_snapshot.psi_avg10 = strtod(avg10, NULL);

    const char *trickle = find_key_value(buf, "trickle_bytes_interval");
    g_snapshot.trickle_bytes_interval = trickle ? strtoul(trickle, NULL, 10) : 0;

    const char *rate = find_key_value(buf, "mem_rate_kb_per_sec");
    if (rate) g_snapshot.mem_rate_kb_per_sec = strtod(rate, NULL);

    const char *gcres = find_key_value(buf, "gonzocache_resident_kb");
    g_snapshot.gonzocache_resident_kb = gcres ? strtoul(gcres, NULL, 10) : 0;

    const char *frozen = find_key_value(buf, "frozen");
    if (frozen) g_snapshot.frozen = parse_bool_at(frozen);

    if (g_snapshot.frozen) {
        const char *fname = find_key_value(buf, "frozen_name");
        if (fname) extract_string_at(fname, g_snapshot.frozen_name,
                                      sizeof(g_snapshot.frozen_name));
    } else {
        g_snapshot.frozen_name[0] = '\0';
    }

    parse_candidates(buf);

    g_snapshot.loaded = TRUE;
    return TRUE;
}

gint gonzo_detritus_get_coldness_pct(pid_t pid)
{
    if (!g_snapshot.loaded) return -1;
    for (gint i = 0; i < g_snapshot.n_candidates; i++) {
        if (g_snapshot.candidates[i].pid == pid) {
            return g_snapshot.candidates[i].coldness_pct;
        }
    }
    return -1;
}

gdouble gonzo_detritus_get_psi_avg10(void)
{
    return g_snapshot.loaded ? g_snapshot.psi_avg10 : -1.0;
}

gboolean gonzo_detritus_is_frozen(void)
{
    return g_snapshot.loaded && g_snapshot.frozen;
}

const gchar *gonzo_detritus_get_frozen_name(void)
{
    return g_snapshot.loaded ? g_snapshot.frozen_name : "";
}

gboolean gonzo_detritus_is_available(void)
{
    return g_snapshot.loaded;
}

gulong gonzo_detritus_get_trickle_bytes_interval(void)
{
    return g_snapshot.loaded ? g_snapshot.trickle_bytes_interval : 0;
}

gulong gonzo_detritus_get_gonzocache_resident_kb(void)
{
    return g_snapshot.loaded ? g_snapshot.gonzocache_resident_kb : 0;
}

gdouble gonzo_detritus_get_mem_rate_kb_per_sec(void)
{
    return g_snapshot.loaded ? g_snapshot.mem_rate_kb_per_sec : -1.0;
}
