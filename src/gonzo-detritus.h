/* gonzo-detritus.h -- read-only client for detritus's status.json
 *
 * Contract (matches the writer's contract in detritus.c):
 *   Guarantees -- returns the most recent successfully-parsed snapshot;
 *     never blocks; never throws; a read failure or parse failure
 *     silently falls back to "no data" (coldness_pct lookups return -1)
 *     rather than propagating an error, since the daemon being absent
 *     or momentarily mid-write is an expected, not exceptional, state.
 *   Assumes   -- caller calls gonzo_detritus_refresh() at most once per
 *     UI refresh cycle (not once per process) -- this is a shared,
 *     cycle-scoped read, matching detritus's own once-per-cycle write.
 *   Refuses   -- never writes to the status file; this is a read-only
 *     consumer, matching the file's own read-only contract.
 *
 * This module never re-derives coldness_pct, PSI, or any other value --
 * it only parses and passes through what detritus already computed.
 * Recomputing any of this here would create a second, independently
 * drifting implementation of logic detritus already owns (Ch.6:
 * single source of truth).
 */
#ifndef _GONZO_DETRITUS_H_
#define _GONZO_DETRITUS_H_

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Re-reads /run/detritus/status.json. Safe to call every refresh cycle;
 * internally cheap (one small file read + scoped parse). If the file is
 * absent, unreadable, or fails to parse, the previous successfully-
 * parsed snapshot is kept (not cleared) -- a single missed read (e.g.
 * caught mid-rename) should not cause every row to flicker to "no
 * data" for one cycle. Returns TRUE if a new snapshot was loaded. */
gboolean gonzo_detritus_refresh(void);

/* Returns the coldness percentage (0-100) for pid from the most recent
 * snapshot, or -1 if there is no data for this pid (daemon not
 * running, snapshot never successfully loaded, or this pid absent
 * from detritus's own candidate list -- e.g. below its RSS floor or
 * on its skip list). Sourced directly from the kernel's own
 * Referenced/Rss page-table accounting (see detritus.c's
 * select_cold_victims()), not a userspace-derived estimate. -1 must
 * never be displayed or treated as "definitely 0% cold". */
gint gonzo_detritus_get_coldness_pct(pid_t pid);

/* Snapshot-level fields for the resources-tab pressure readout. All
 * return sentinel values (-1.0 / FALSE / empty string) if no snapshot
 * has ever successfully loaded. */
gdouble  gonzo_detritus_get_psi_avg10(void);
gboolean gonzo_detritus_is_frozen(void);
const gchar *gonzo_detritus_get_frozen_name(void);  /* "" if not frozen */
gboolean gonzo_detritus_is_available(void);          /* has any snapshot ever loaded? */

/* Bytes marked MADV_COLD by detritus's proactive idle-trickle thread
 * since the previous status.json publish -- an activity rate, not a
 * running total. Returns 0 (not a negative sentinel) when no snapshot
 * has ever loaded, since "the daemon isn't running" and "the daemon
 * is running but has nothing idle to trickle right now" both
 * legitimately mean "zero bytes trickled" from the caller's point of
 * view. Callers that need to distinguish those two cases should check
 * gonzo_detritus_is_available() separately. */
gulong gonzo_detritus_get_trickle_bytes_interval(void);

/* GonzoCache's real page-cache residency (KB) across whatever it
 * preloaded this session, as measured by detritus via mincore() --
 * see detritus.c's read_gonzocache_resident_kb(). Returns 0 (not a
 * negative sentinel) when unavailable, since "GonzoCache isn't
 * installed" and "GonzoCache preloaded nothing" both legitimately
 * mean zero from the caller's point of view. */
gulong gonzo_detritus_get_gonzocache_resident_kb(void);

/* Rate of |MemAvailable| change between the two most recent publishes,
 * in KB/s -- the actual "how fast is RAM filling up or emptying"
 * signal, magnitude only (direction is not reported). Returns -1.0 if
 * no snapshot has ever loaded, matching psi_avg10's sentinel
 * convention, since unlike trickle bytes, "no data" and "rate is
 * genuinely zero" are meaningfully different states worth
 * distinguishing here. */
gdouble gonzo_detritus_get_mem_rate_kb_per_sec(void);

#ifdef __cplusplus
}
#endif

#endif /* _GONZO_DETRITUS_H_ */
