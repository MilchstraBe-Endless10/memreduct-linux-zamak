/* SPDX-License-Identifier: GPL-3.0-or-later
 * Production-only inert runtime hooks required by the shared Zamak controller.
 * No environment variables, CI markers, fixture rows, fake authorization, or
 * test capture code are present in this translation unit.
 */
#include <stdint.h>

long long mr_runtime_mode(void) { return 0; }
long long mr_runtime_timeout_ms(void) { return 0; }
long long mr_runtime_sample_failure_requested(void) { return 0; }
long long mr_runtime_consume_sample_failure_trigger(void) { return 0; }
long long mr_runtime_log_canvas_font(void) { return 0; }
long long mr_runtime_start_settings(void) { return 0; }
long long mr_runtime_start_balanced_picker(void) { return 0; }
long long mr_runtime_widget_preview_resource(void) { return 0; }
long long mr_runtime_schedule_capture(void) { return 0; }
long long mr_runtime_selected_group(void) { return 0; }
long long mr_runtime_clean_simulation_enabled(void) { return 0; }
long long mr_runtime_xproc_simulation_enabled(void) { return 0; }
long long mr_runtime_xproc_candidate_count(void) { return 0; }
long long mr_runtime_xproc_candidate_pid(long long index) { (void)index; return -1; }
long long mr_runtime_xproc_candidate_start_time(long long index) { (void)index; return -1; }
long long mr_runtime_xproc_candidate_uid(long long index) { (void)index; return -1; }
long long mr_runtime_xproc_candidate_bytes(long long index) { (void)index; return -1; }
long long mr_runtime_xproc_candidate_scope_bytes(long long index) { (void)index; return -1; }
long long mr_runtime_xproc_candidate_rss_bytes(long long index) { (void)index; return -1; }
long long mr_runtime_xproc_candidate_private_resident_bytes(long long index) { (void)index; return -1; }
long long mr_runtime_xproc_candidate_regions(long long index) { (void)index; return 0; }
const char *mr_runtime_xproc_candidate_name(long long index) { (void)index; return ""; }
const char *mr_runtime_xproc_candidate_app_name(long long index) { (void)index; return ""; }
long long mr_runtime_xproc_candidate_state(long long index) { (void)index; return 0; }
long long mr_runtime_xproc_candidate_identity_token(long long index) { (void)index; return -1; }
long long mr_runtime_xproc_candidate_scope_hash(long long index) { (void)index; return -1; }
long long mr_runtime_xproc_candidate_region_digest(long long index) { (void)index; return -1; }
long long mr_runtime_xproc_request_id_new(void) { return 0; }
long long mr_runtime_xproc_audit_confirmation(long long request_id, long long pid,
                                                long long start_time, long long uid,
                                                long long scope_hash, long long identity_hash,
                                                long long region_digest)
{
    (void)request_id;
    (void)pid;
    (void)start_time;
    (void)uid;
    (void)scope_hash;
    (void)identity_hash;
    (void)region_digest;
    return 1;
}
long long mr_runtime_xproc_start(long long request_id, long long mode, long long pid,
                                 long long start_time, long long uid,
                                 long long identity_hash, long long scope_hash,
                                 long long region_digest, long long reviewed_bytes,
                                 long long reviewed_regions, long long max_bytes)
{
    (void)request_id;
    (void)mode;
    (void)pid;
    (void)start_time;
    (void)uid;
    (void)identity_hash;
    (void)scope_hash;
    (void)region_digest;
    (void)reviewed_bytes;
    (void)reviewed_regions;
    (void)max_bytes;
    return 3; /* fail closed; production never enables the simulated path */
}
long long mr_runtime_xproc_poll(void) { return -2; }
long long mr_runtime_xproc_cancel(void) { return 1; }
long long mr_runtime_xproc_stage(void) { return 0; }
long long mr_runtime_xproc_result_mode(void) { return 0; }
long long mr_runtime_xproc_result_advised_bytes(void) { return 0; }
long long mr_runtime_xproc_result_requested_bytes(void) { return 0; }
long long mr_runtime_xproc_result_regions(void) { return 0; }
long long mr_runtime_xproc_result_call_count(void) { return 0; }
long long mr_runtime_mock_stage(void) { return 0; }
long long mr_runtime_clean_simulation_start(long long mode)
{
    (void)mode;
    return 3; /* never launches a fake or privileged helper */
}
void mr_runtime_emit(const char *line) { (void)line; }
