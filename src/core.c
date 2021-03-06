// core.c
#include <string.h>
#include <stdlib.h>
#include "core.h"
#include "console.h"

static session *ses = NULL;		// the global session for gtorrent
static int (*resp)(gt_alert *);		// session callback function
static alert_deque *alerts;		// vector of session alerts to pop
static gt_torrent *tbase;		// base pointer to next torrent

static void gt_core_trntlist_destroy(gt_torrent **);
static void gt_core_alert_convert(gt_alert *, alert *);

int gt_core_is_maglink(const char *url) {
	return !strncmp("magnet:", url, 7);
}

char *gt_core_get_savepath(char *str) {
	// TODO: WIN32 specific functions
	if (!getenv("HOME")) return NULL;
	char *home = getenv("HOME");
	strcpy(str, home);
	strcat(str, "/Downloads");
	return str;
}

session *gt_core_get_session(void) {
	return ses;
}

void gt_core_session_start(int x, int y) {
	ses = lt_session_create();
	resp = NULL;
	alerts = lt_alert_deque_create();
	tbase = NULL;

	lt_session_listen_on(ses, x, y);
	// for debug
	lt_session_set_alert_mask(ses, error_notification|status_notification
		|progress_notification);
}

void gt_core_session_end(void) {
	lt_alert_deque_destroy(alerts);
	resp = NULL;
	
	if (tbase != NULL)
		gt_core_trntlist_destroy(&tbase);
	lt_session_destroy(ses);
	ses = NULL;
}

static void gt_core_trntlist_destroy(gt_torrent **p) {
	if ((*p)->next != NULL)
		gt_core_trntlist_destroy(&(*p)->next);
	(*p)->next = NULL;
	lt_session_remove_torrent(ses, (*p)->th);
//	gt_trnt_destroy(*p); TODO: fix
}

void gt_core_session_set_callback(int (*f)(gt_alert *)) {
	resp = f;
}

void gt_core_session_remove_callback(void) {
	resp = NULL;
}

int gt_core_session_add_torrent(gt_torrent *t) {
	gt_torrent **p;
	
	for (p = &tbase; *p != t && *p != NULL; p = &(*p)->next);
	if (*p == t) {
		Console.error("%s: attempting to add duplicate torrent.",
				"gt_core_session_add_torrent");
		return 0;
	} else
		Console.debug("Added a torrent successfully.");
	*p = t;
	t->next = NULL;
	t->th = lt_session_add_torrent(ses, t->tp);
	return 1;
}

gt_torrent *gt_core_session_remove_torrent(gt_torrent *t) {
	gt_torrent **p;

	for (p = &tbase; *p != t && *p != NULL; p = &(*p)->next);
	if (*p == NULL) {
		Console.error("%s: Attempt to remove non-existent torrent.",
				"gt_core_session_remove_torrent");
		return NULL;
	}
	*p = (*p)->next;
	t->next = NULL;
	lt_session_remove_torrent(ses, t->th);
	return t;
}

int gt_core_session_update(void) {
	if (ses == NULL)
		return -1;
	static gt_alert gtorrent_alert;
	alert *al;
	uint64_t i;

	gtorrent_alert.ses = NULL;
	gtorrent_alert.trnt = NULL;
	gtorrent_alert.al = NULL;
	gtorrent_alert.type = other_alert;
	
	lt_session_pop_alerts(ses, alerts);
	for (i=0; (al=lt_alert_deque_get(alerts, i)) != NULL; ++i) {
		gt_core_alert_convert(&gtorrent_alert, al);

		if (gtorrent_alert.trnt != NULL
		 && gtorrent_alert.trnt->call != NULL)
			gtorrent_alert.trnt->call(&gtorrent_alert);
		else if (gtorrent_alert.trnt == NULL
		 && resp != NULL)
			(*resp)(&gtorrent_alert);
	}

	return 1;	// TODO: check for session validity
}

static void gt_core_alert_convert(gt_alert *ga, alert *a) {
	torrent_handle *thandle;
	gt_torrent *gtp;

	ga->ses = ses;
	ga->trnt = NULL;

	if (lt_alert_is_torrent_alert(a)) {
		// has a torrent_handle member; search for match
		thandle = lt_alert_get_torrent_handle(a);
		for (gtp = tbase; gtp != NULL; gtp = gtp->next)
			if (lt_trnt_handle_equal(thandle, gtp->th)) {
				ga->trnt = gtp;
				break;
			}
	}

	ga->type = lt_alert_get_type(a);
}
