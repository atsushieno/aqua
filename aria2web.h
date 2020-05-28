

/* These functions should be part of the public API */

typedef struct aria2web_tag aria2web;
/* The value and velocity could be float, but webaudio-controls seems to handle them in integer. */
typedef void(* aria2web_control_change_callback)(void* context, int cc, int value);
typedef void(* aria2web_note_callback)(void* context, int key, int velocity);
typedef void(* aria2web_window_close_callback)(void* context);

aria2web* aria2web_create();
void aria2web_start(aria2web* context, void* parentWindow = nullptr);
void aria2web_set_control_change_callback(aria2web* a2w, aria2web_control_change_callback callback, void* context);
void aria2web_set_note_callback(aria2web* a2w, aria2web_note_callback callback, void* context);
void aria2web_set_window_close_callback(aria2web* a2w, aria2web_window_close_callback callback, void* context);
void aria2web_stop(aria2web* context);
void* aria2web_get_native_widget(aria2web* instance);

/* The rest of the code is private */

#ifdef ARIA2WEB_IMPL

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <sys/mman.h>
#include <pthread.h>

#include <webview.h>
#include <httpserver.h>


void* a2w_run_http_server(void* context);
void* a2w_run_webview_loop(void* context);

void log_debug(const char* s) { puts(s); }

int a2w_get_http_server_port_number()
{
	/* FIXME: allocate it dynamically. */
	return 37564;
}

typedef struct aria2web_tag {
	pthread_t webview_thread;
	pthread_t http_server_thread;
	void* webview{nullptr};
	void* parent_window{nullptr};
	void* webview_widget{nullptr};
	bool http_server_started{false};
	bool webview_ready{false};
	void* component_ref{nullptr};
	aria2web_control_change_callback control_change_callback{nullptr};
	void* cc_callback_context{nullptr};
	aria2web_note_callback note_callback{nullptr};
	void* note_callback_context{nullptr};
	aria2web_window_close_callback window_close_callback{nullptr};
	void* window_close_callback_context{nullptr};

	void start(void* parentWindow = nullptr)
	{
		pthread_create(&http_server_thread, NULL, a2w_run_http_server, this);
		struct timespec tm;
		tm.tv_sec = 0;
		tm.tv_nsec = 1000;
		while (!http_server_started)
			nanosleep(&tm, NULL);
		parent_window = parentWindow;
		pthread_create(&webview_thread, NULL, a2w_run_webview_loop, this);
		tm.tv_sec = 0;
		tm.tv_nsec = 1000;
		while (!webview_ready)
			nanosleep(&tm, NULL);
	}

	void stop()
	{
		webview_destroy(webview);
		pthread_cancel(&webview_thread);
		pthread_cancel(&http_server_thread);
	}

};

aria2web* aria2web_create() { return new aria2web(); }

void aria2web_free(aria2web* instance) { delete instance; }

void aria2web_start(aria2web* instance, void* parentWindow) { instance->start(parentWindow); }

void aria2web_stop(aria2web* instance) { instance->stop(); }

void* aria2web_get_native_widget(aria2web* instance) {
	return instance->webview_widget;
}

void uri_unescape_in_place(char* p) {
	int dst = 0;
	for (int i = 0; p[i]; i++) {
		if (p[i] == '%') {
			if (p[i + 1] && p[i + 2]) {
				p[dst++] = (p[i + 1] - 0x30) * 16 + p[i + 2] - 0x30;
				i += 2;
			}
			else
				p[dst++] = '%';
		}
		else
			p[dst++] = p[i];
	}
	p[dst] = NULL;
}

void uri_strip_query_string(char *s) {
	for (int i = 0; s[i]; i++)
		if (s[i] == '?') {
			s[i] = NULL;
			return;
		}
}

const char* mime_types[] = {"text/html", "application/xml", "image/png" };

const char* get_mime_type_from_filename(char* filename) {
	std::string s{filename};
	if (s.size() > 4 && s.compare(s.size() - 4, 4, ".xml") == 0)
		return mime_types[1];
	if (s.size() > 4 && s.compare(s.size() - 4, 4, ".png") == 0)
		return mime_types[2];
	return mime_types[0];
}

void handle_request(struct http_request_s* request) {
	struct http_response_s* response = http_response_init();

	struct http_string_s target = http_request_target(request);
	char* targetPath = (char*) calloc(target.len + 2, 1);
	/* FIXME: make sure that the requested resource is under pwd. */
	targetPath[0] = '.';
	memcpy(targetPath + 1, target.buf, target.len);
	targetPath[target.len + 1] = NULL;
	/* log_debug(targetPath); */
	uri_unescape_in_place(targetPath);
	uri_strip_query_string(targetPath);
	
	const char* mimeType = get_mime_type_from_filename(targetPath);

	int fd = open(targetPath, O_RDONLY);
	free(targetPath);
	if (fd == -1) {
		puts("... was NOT found.");
		http_response_status(response, 404);
		http_respond(request, response);
	} else {
		struct stat st;
		fstat(fd, &st);
		void* content = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
		assert(content);
		
		http_response_status(response, 200);
		http_response_header(response, "Content-Type", mimeType);
		http_response_body(response, (const char*) content, st.st_size);
		http_respond(request, response);
		munmap(content, st.st_size);
	}
}

void* a2w_run_http_server(void* context) {
	auto a2w = (aria2web*) context;
	struct http_server_s* server = http_server_init(37564, handle_request);
	a2w->http_server_started = TRUE;
	http_server_listen(server);
	return nullptr;
}

void aria2web_set_control_change_callback(aria2web* a2w, aria2web_control_change_callback callback, void* callbackContext)
{
	a2w->control_change_callback = callback;
	a2w->cc_callback_context = callbackContext;
}

void aria2web_set_note_callback(aria2web* a2w, aria2web_note_callback callback, void* callbackContext)
{
	a2w->note_callback = callback;
	a2w->note_callback_context = callbackContext;
}

void aria2web_set_window_close_callback(aria2web* a2w, aria2web_window_close_callback callback, void* callbackContext)
{
	a2w->window_close_callback = callback;
	a2w->window_close_callback_context = callbackContext;
}

void parse_js_two_array_items(const char* req, int* ret1, int* ret2)
{
	assert(req[0] == '[');
	int end = strlen(req);
	assert(req[end - 1] == ']');
	char* ptr = strchr(req, ',');
	assert(ptr != nullptr);
	int pos = ptr - req;
	assert(pos > 0);
	*ret1 = 0;
	for (int i = 1; i < pos; i++)
		*ret1 = *ret1 * 10 + req[i] - '0';
	*ret2 = 0;
	for (int i = pos + 1; i < end - 1; i++)
		*ret2 = *ret2 * 10 + req[i] - '0';
}

void webview_callback_window_close(const char *seq, const char *req, void *arg) {
	fprintf(stderr, "CLOSED\n");
	auto a2w = (aria2web*) arg;
	if (a2w && a2w->window_close_callback)
		a2w->window_close_callback(a2w->window_close_callback_context);
}

void webview_callback_control_change(const char *seq, const char *req, void *arg) {
	int cc = 0, val = 0;
	parse_js_two_array_items(req, &cc, &val);

	auto a2w = (aria2web*) arg;
	if (a2w && a2w->control_change_callback)
		a2w->control_change_callback(a2w->cc_callback_context, cc, val);
	else {
		log_debug("control change callback is invoked");
		log_debug(seq);
		log_debug(req);
		log_debug(arg != NULL ? "not null" : "null");
	}
}

void webview_callback_note(const char *seq, const char *req, void *arg) {
	int key = 0, vel = 0;
	parse_js_two_array_items(req, &key, &vel);

	auto a2w = (aria2web*) arg;
	if (a2w && a2w->note_callback)
		a2w->note_callback(a2w->note_callback_context, key, vel);
	else {
		log_debug("note callback is invoked");
		log_debug(seq);
		log_debug(req);
		log_debug(arg != NULL ? "not null" : "null");
	}
}

void on_dispatch(webview_t* w, void* context) {
	// FIXME: add gtk window close callback here.
	printf("on_dispatch: %d\n", webview_get_window(w));
}

void* a2w_run_webview_loop(void* context) {
	char* urlfmt = "http://localhost:%d/index.html";
	int port = a2w_get_http_server_port_number();
	char* url = strdup(urlfmt);
	sprintf(url, urlfmt, port);

	auto a2w = (aria2web*) context;
	void* w = a2w->webview = webview_create(true, nullptr);
	//webview_set_title(w, "Aria2Web embedded example");
	webview_set_size(w, 1200, 450, WEBVIEW_HINT_NONE);
	webview_navigate(w, url);
	free(url);
	webview_bind(w, "ControlChangeCallback", webview_callback_control_change, context);
	webview_bind(w, "NoteCallback", webview_callback_note, context);
	webview_bind(w, "Aria2WebWindowCloseCallback", webview_callback_window_close, context);
	webview_dispatch(w, on_dispatch, context);
	a2w->webview_ready = TRUE;
	webview_run(w);
	return nullptr;
}

#endif /* ARIA2WEB_IMPL */
